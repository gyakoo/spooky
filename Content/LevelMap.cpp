﻿#include "pch.h"
#include "LevelMap.h"
#include "../Common/DirectXHelper.h"
#include "../Common/DeviceResources.h"
#include "ShaderStructures.h"
#include "CameraFirstPerson.h"
#include "GlobalFlags.h"

using namespace SpookyAdulthood;
using namespace DX;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Provider;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Globalization::DateTimeFormatting;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#pragma region Types and functions
struct SpookyAdulthood::VisMatrix
{
    std::vector<bool> matrix;

    VisMatrix(int nLeaves)
    {
        matrix.resize(nLeaves*nLeaves, false);
    }

    inline bool IsVisible(int n, int i, int j) const
    {
        return matrix[n*j + i];
    }

    inline void SetVisible(int n, int i, int j, bool v)
    {
        matrix[n*j + i] = v;
    }
};

// random element in a set (will get the first with no teleport already)
template<typename T, typename R>
static size_t RandomRoomInSet(const T& roomset, const R& leaves, RandomProvider& rnd)
{
    T::const_iterator it = roomset.begin();
    for (; it != roomset.end(); ++it)
    {
        if (leaves[*it]->m_teleportNdx == -1)
            return *it;
    }
    return *roomset.begin();
}
#pragma endregion

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#pragma region LevelMapGenerationSettings
LevelMapGenerationSettings::LevelMapGenerationSettings()
    : m_randomSeed(RANDOM_DEFAULT_SEED), m_tileSize(1.0f, 1.0f)
    , m_tileCount(20, 20), m_minTileCount(4, 4)
    , m_charRadius(0.25f), m_minRecursiveDepth(3)
    , m_maxRecursiveDepth(5)
    , m_probRoom(0.05f), m_generateThumbTex(true)
    , m_maxTileCount(8,8), m_minForPillars(3,3)
    , m_pillarsProbRange(0.01f, 0.2f) // between 1%-20% of pillars for a room
{
}

void LevelMapGenerationSettings::Validate() const
{
    DX::ThrowIfFalse(m_charRadius > 0.01f);
    const float charDiam = m_charRadius * 2.0f;
    DX::ThrowIfFalse(m_tileSize.x > charDiam && m_tileSize.y > charDiam);
    DX::ThrowIfFalse(m_tileCount.x >= m_minTileCount.x && m_tileCount.y >= m_minTileCount.y);
    DX::ThrowIfFalse(m_minTileCount.x >= 2 && m_minTileCount.y >= 2);
    // left stuff to check for :(
}
//////////////////////////////////////////////////////////////////////////
#pragma endregion

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#pragma region LevelMapBSPPortal
XMUINT2 LevelMapBSPPortal::GetPortalPosition(XMUINT2* opposite/*0*/) const
{
    DX::ThrowIfFalse(m_wallNode->m_type == LevelMapBSPNode::WALL_VERT || m_wallNode->m_type == LevelMapBSPNode::WALL_HORIZ);

    XMUINT2 pos(0, 0);
    switch (m_wallNode->m_type)
    {
    case LevelMapBSPNode::WALL_VERT:
        pos.x = m_wallNode->m_area.m_x0;
        pos.y = m_index;
        if (opposite)
            *opposite = XMUINT2(pos.x - 1, pos.y);
        break;
    case LevelMapBSPNode::WALL_HORIZ:
        pos.x = m_index;
        pos.y = m_wallNode->m_area.m_y0;
        if (opposite)
            *opposite = XMUINT2(pos.x, pos.y - 1);
        break;
    }
    return pos;
}

void LevelMapBSPPortal::GetTransform(XMFLOAT3& pos, float& rotY) const
{
    const XMUINT2 p = GetPortalPosition();
    pos.x = (float)p.x;
    pos.z = (float)p.y;
    pos.y = 0.75f;

    if (m_wallNode->m_type == LevelMapBSPNode::WALL_VERT)
    {
        pos.z += 0.5f;
        rotY = XM_PIDIV2;
    }
    else
    {
        pos.x += 0.5f;
        rotY = 0.0f;
    }
}

LevelMapBSPNodePtr LevelMapBSPPortal::GetOtherLeaf(LevelMapBSPNodePtr l) const
{
    return (l == m_leaves[0]) ? m_leaves[1] : m_leaves[0];
}
#pragma endregion

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#pragma region LevelMap
LevelMap::LevelMap(const std::shared_ptr<DX::DeviceResources>& device)
    : m_root(nullptr)
    , m_device(device)
{
    XMStoreFloat4x4(&m_levelTransform, XMMatrixIdentity());
}

void LevelMap::Generate(const LevelMapGenerationSettings& settings)
{
    settings.Validate();

    Destroy();

    m_root = std::make_shared<LevelMapBSPNode>();
    LevelMapBSPTileArea area(0, settings.m_tileCount.x - 1, 0, settings.m_tileCount.y - 1);
    auto gameRes = m_device->GetGameResources();
    gameRes->m_random.SetSeed(settings.m_randomSeed);
    LevelMapBSPNodePtr lastRoom;
    RecursiveGenerate(m_root, area, settings, 0);        
    GenerateVisibility(settings);
    GenerateCollisionInfo();
    if (settings.m_generateThumbTex)
        GenerateThumbTex(settings.m_tileCount);
    CreateDeviceDependentResources();
    if (m_device && gameRes)
        gameRes->m_levelTime = .0f;
}

void LevelMap::RecursiveGenerate(LevelMapBSPNodePtr& node, LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, uint32_t depth)
{
    node->m_area = area;
    // It's an EMPTY? any dimension is not large enough to be a room
    if (area.SizeX() < settings.m_minTileCount.x ||
        area.SizeY() < settings.m_minTileCount.y)
    {
        node->m_type = LevelMapBSPNode::NODE_EMPTY;
        // leaf, no children
        return;
    }

    auto& random = m_device->GetGameResources()->m_random;
    // Can be a ROOM?
    if ( CanBeRoom(node, area, settings, depth) )
    {
        node->m_type = LevelMapBSPNode::NODE_ROOM;
        node->m_leafNdx = (int)m_leaves.size();
        m_leaves.push_back(node);
        GenerateDetailsForRoom(node, settings);
        // leaf, no children
    }
    else
    {
        // WALL node (split)
        node->m_type = (LevelMapBSPNode::NodeType)(LevelMapBSPNode::WALL_VERT + (depth % 2)); // each depth alternate wall dir
        uint32_t at = 0;
        if (node->m_type == LevelMapBSPNode::WALL_VERT) // random division plane
        {
            at = area.m_x0 + random.Get(0, area.SizeX() - 1);
            node->m_area.m_x0 = node->m_area.m_x1 = at;
        }
        else
        {
            at = area.m_y0 + random.Get(0, area.SizeY() - 1);
            node->m_area.m_y0 = node->m_area.m_y1 = at;
        }
        //if (at == 0) at = 1;
        // get the two sub-areas and subdivide them recursively
        LevelMapBSPTileArea newAreas[2];
        SplitNode(area, at, node->m_type, newAreas);
        for (int i = 0; i < 2; ++i)
        {
            node->m_children[i] = std::make_shared<LevelMapBSPNode>();
            node->m_children[i]->m_parent = node;
            RecursiveGenerate(node->m_children[i], newAreas[i], settings, depth + 1);            
        }
    }
}

static const uint32_t RPDENSITY[LevelMap::RP_MAX] = {
    10, 30, 40, 50, 60, 70, 80, 90, 100
};

void LevelMap::GenerateDetailsForRoom(LevelMapBSPNodePtr& node, const LevelMapGenerationSettings& settings)
{
    auto& random = m_device->GetGameResources()->m_random;    
    node->m_profile = random.GetWithDensity(RPDENSITY, RP_MAX);

    switch (node->m_profile)
    {
        case RP_NORMAL0:
            GeneratePillarsForRoom(node, settings.m_minForPillars, XMFLOAT2(0.01f, 0.3f));
            break;
        case RP_NORMAL1:
            GeneratePillarsForRoom(node, settings.m_minForPillars, XMFLOAT2(0.2f, 0.5f));
            break;
        case RP_GRAVE: break;
        case RP_WOODS: break;
        case RP_BODYPILES: 
            GeneratePillarsForRoom(node, settings.m_minForPillars, XMFLOAT2(0.01f, 0.15f));
            break;
        case RP_GARGOYLES: 
            GeneratePillarsForRoom(node, settings.m_minForPillars, XMFLOAT2(0.01f, 0.1f));
            break;
        case RP_HANDS: 
            GeneratePillarsForRoom(node, settings.m_minForPillars, XMFLOAT2(0.01f, 0.2f));
            break;
        case RP_SCARYMESSAGES: 
            GeneratePillarsForRoom(node, settings.m_minForPillars, XMFLOAT2(0.01f, 0.05f));
            break;
        case RP_PUMPKINFIELD: break;
    }
}

void LevelMap::GeneratePillarsForRoom(LevelMapBSPNodePtr& node, const XMUINT2& minForPillars, const XMFLOAT2& probRange)
{
    // pillars
    auto& random = m_device->GetGameResources()->m_random;
    const auto& area = node->m_area;
    if (area.SizeX() > 2 && area.SizeY() > 2)
    {
        // we don't want pillars next to a door (can block a door)
        uint32_t sx = area.SizeX() - 2;
        uint32_t sy = area.SizeY() - 2;
        if (sx >= minForPillars.x && sy >= minForPillars.y)
        {
            // compute no of pillars to generate and allocate vector
            int areaSize = sx*sy;
            float p = random.GetF(probRange.x, probRange.y);
            int nPillars = (int)(areaSize*p);
            if (nPillars && !node->m_pillars)
            {
                node->m_pillars = std::make_unique<std::vector<XMUINT2>>();
                node->m_pillars->reserve(nPillars);
            }

            // generate a pillar randomly and check if it exists already before adding
            for (int i = 0; i < nPillars; ++i)
            {
                XMUINT2 newPillar;
                newPillar.x = random.Get(area.m_x0 + 1, area.m_x1 - 1);
                newPillar.y = random.Get(area.m_y0 + 1, area.m_y1 - 1);
                auto it = std::find_if(node->m_pillars->begin(), node->m_pillars->end(), [&](const auto& rhs)->bool {return rhs.x == newPillar.x && rhs.y == newPillar.y; });
                if (it == node->m_pillars->end())
                    node->m_pillars->push_back(newPillar);
            }
        }
    }
}

void LevelMap::SplitNode(const LevelMapBSPTileArea& area, uint32_t at, LevelMapBSPNode::NodeType wallDir, LevelMapBSPTileArea* outAreas)
{
    DX::ThrowIfFalse(wallDir == LevelMapBSPNode::WALL_VERT || wallDir == LevelMapBSPNode::WALL_HORIZ);
    outAreas[0] = outAreas[1] = area;
    switch (wallDir)
    {
    case LevelMapBSPNode::WALL_VERT:
        DX::ThrowIfFalse(at <= area.m_x1);
        outAreas[0].m_x0 = area.m_x0; outAreas[0].m_x1 = at - 1;
        outAreas[1].m_x0 = at; outAreas[1].m_x1 = area.m_x1;
        break;

    case LevelMapBSPNode::WALL_HORIZ:
        DX::ThrowIfFalse(at <= area.m_y1);
        outAreas[0].m_y0 = area.m_y0; outAreas[0].m_y1 = at - 1;
        outAreas[1].m_y0 = at; outAreas[1].m_y1 = area.m_y1;
        break;
    }
}

bool LevelMap::CanBeRoom(const LevelMapBSPNodePtr& node, const LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, uint32_t depth)
{
    // not there yet
    if (depth < settings.m_minRecursiveDepth || area.SizeX() >= settings.m_maxTileCount.x || area.SizeY() >= settings.m_maxTileCount.y) 
        return false;

    // minimum enough to be a room
    if (depth >= settings.m_maxRecursiveDepth || area.SizeX() == settings.m_minTileCount.x || area.SizeY() == settings.m_minTileCount.y)
        return true;

    
    auto& random = m_device->GetGameResources()->m_random;
    float dice = random.Get(1, 100)*0.01f;
    return dice < settings.m_probRoom;
}

void LevelMap::Destroy()
{
    ReleaseDeviceDependentResources();
    m_root = nullptr;
    m_cameraCurLeaf = nullptr;
    m_leaves.clear();
    m_teleports.clear();
    m_portals.clear();
    m_leafPortals.clear();
    m_thumbTex.Destroy();
}

void LevelMap::GenerateThumbTex(XMUINT2 tcount, const XMUINT2* playerPos)
{
    if (!m_root) return;
    
    m_thumbTex.Destroy();

    uint32_t w = tcount.x;
    uint32_t h = tcount.y;
    m_thumbTex.m_dim = tcount;
    m_thumbTex.m_sysMem = new uint32_t[w*h];
    ZeroMemory(m_thumbTex.m_sysMem, w*h * sizeof(uint32_t));

    // rooms
    for (auto& room : m_leaves)
    {
        for (uint32_t y = room->m_area.m_y0; y <= room->m_area.m_y1; ++y)
            for (uint32_t x = room->m_area.m_x0; x <= room->m_area.m_x1; ++x)
                m_thumbTex.SetAt(x, y, room->m_tag);

        /*
        if (room->m_pillars)
        {
            for (auto& p : *room->m_pillars)
            {
                m_thumbTex.SetAt(p.x, p.y, 0xff000000);
            }
        }
        */
    }

    // teleports
    for (const auto& tp : m_teleports)
    {
        if (tp.m_open)
        {
            const uint32_t abgr = 0x7700ff00;
            for (int i = 0; i < 2; ++i)
            {
                m_thumbTex.SetAt(tp.m_positions[i].x, tp.m_positions[i].y, abgr);
            }
        }
    }

    // portals    
    for (const auto& p : m_portals)
    {
        XMUINT2 pos = p.GetPortalPosition();
        for (int i = 0; i < 2; ++i)
        {
            m_thumbTex.SetAt(pos.x, pos.y, 0x77000000);
            switch (p.m_wallNode->m_type)
            {
            case LevelMapBSPNode::WALL_HORIZ: --pos.y; break;
            case LevelMapBSPNode::WALL_VERT: --pos.x; break;
            }
        }
    }

    // character
    if ( playerPos)
        m_thumbTex.SetAt(playerPos->x, playerPos->y, 0x77ff00ff);

    // create DX resources for rendering
    m_thumbTex.CreateDeviceDependentResources(m_device);
}

// I know, I know...
void LevelMap::GenerateVisibility(const LevelMapGenerationSettings& settings)
{
    if (m_leaves.size() <= 1) 
        return;

    const int nLeaves = (int)m_leaves.size();

    // vis matrix (i know, should be a bitset only storing half the matrix, whatever...)
    VisMatrix visMatrix(nLeaves);
    LevelMapBSPNodePtr curRoom, otherRoom;
    for (int i = 0; i < nLeaves; ++i)
    {
        visMatrix.SetVisible(nLeaves, i, i, true); // diag

        curRoom = m_leaves[i];
        for (int j = i - 1; j >= 0; --j)
        {
            otherRoom = m_leaves[j];
            if (VisRoomAreContiguous(otherRoom, curRoom))
            {
                visMatrix.SetVisible(nLeaves, i, j, true);
                visMatrix.SetVisible(nLeaves, j, i, true);
            }
        }
    }


    // generate portals
    for (int i = 1; i < nLeaves; ++i)
    {
        for (int j = i - 1; j >= 0; --j)
        {
            if (visMatrix.IsVisible(nLeaves, i, j))
            {
                // generate portal
                VisGeneratePortal(m_leaves[i], m_leaves[j]);
                break;
            }
        }
    }

    // generate disjoint sets
    GenerateTeleports(visMatrix);
}

void LevelMap::GenerateCollisionInfo()
{
    for (auto& room : m_leaves)
    {
        room->GenerateCollisionSegments(*this);
    }
}

bool LevelMap::VisRoomAreContiguous(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB)
{
    auto& areaA = roomA->m_area;
    auto& areaB = roomB->m_area;

    const int diffX = (areaA.m_x0 <= areaB.m_x0)
        ? areaB.m_x0 - areaA.m_x1
        : areaA.m_x0 - areaB.m_x1;

    const int diffY = (areaA.m_y0 <= areaB.m_y0)
        ? areaB.m_y0 - areaA.m_y1
        : areaA.m_y0 - areaB.m_y1;

    const bool touchingCorners = diffX == 1 && diffY == 1;
    if ((diffX >= 2 || diffY >= 2) || touchingCorners )
        return false;

    return true;
}

void LevelMap::VisGeneratePortal(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB)
{
    LevelMapBSPNodePtr parent = roomA->m_parent;
    while (parent)
    {
        if (HasNode(parent, roomB))
        {
            // generate portal for node 'parent' which should be a WALL_X or WALL_Y
            DX::ThrowIfFalse(parent->IsWall());
            LevelMapBSPPortal portal = 
            {
                { roomA, roomB },
                parent,
                VisComputeRandomPortalIndex(roomA->m_area, roomB->m_area, parent->m_type ),
                false
            };
            uint32_t portalNdx = (uint32_t)m_portals.size();
            m_leafPortals.insert(std::make_pair(roomA.get(), portalNdx));
            m_leafPortals.insert(std::make_pair(roomB.get(), portalNdx));
            m_portals.push_back(portal);

            break;
        }
        parent = parent->m_parent;
    }
}

// we assume area1 and area2 are contiguous and wallDir in {WALL_X, WALL_Y}
int LevelMap::VisComputeRandomPortalIndex(const LevelMapBSPTileArea& areaA, const LevelMapBSPTileArea& areaB, LevelMapBSPNode::NodeType wallDir)
{
    // get the intersection range
    uint32_t a = 0, b = 0;
    switch (wallDir)
    {
    case LevelMapBSPNode::WALL_HORIZ:
        a = (std::max)(areaA.m_x0, areaB.m_x0);
        b = (std::min)(areaA.m_x1, areaB.m_x1);
        break;
    case LevelMapBSPNode::WALL_VERT:
        a = (std::max)(areaA.m_y0, areaB.m_y0);
        b = (std::min)(areaA.m_y1, areaB.m_y1);
        break;
    }

    // random cell along the wallDir
    return m_device->GetGameResources()->m_random.Get(a, b);
}

void LevelMap::VisGenerateTeleport(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB)
{
    DX::ThrowIfFailed(roomA->IsLeaf() && roomB->IsLeaf());

    //DX::ThrowIfFalse(roomA->m_teleportNdx == -1 && roomB->m_teleportNdx == -1);
    
    LevelMapBSPTeleport tport =
        {
            {roomA, roomB},
            {GetRandomInArea(roomA), GetRandomInArea(roomB) },
            false
        };
    const int ndx = (int)m_teleports.size();
    m_teleports.push_back(tport);
    roomA->m_teleportNdx = roomB->m_teleportNdx = ndx;
}

bool LevelMap::HasNode(const LevelMapBSPNodePtr& node, const LevelMapBSPNodePtr& lookFor)
{
    if (node == lookFor)
        return true;

    if (!node||node->IsLeaf())
        return false;
    
    for (int i = 0; i < 2; ++i)
    {
        auto child = node->m_children[i];
        if (HasNode(child, lookFor)) 
            return true;
    }
    return false;
}

XMUINT2 LevelMap::GetRandomInArea(const LevelMapBSPNodePtr& node, bool checkNotInPortal/*=true*/)
{
    auto& random = m_device->GetGameResources()->m_random;
    const auto& area = node->m_area;

    XMUINT2 rndPos;
    int iter = 0;
    do
    {
        rndPos = XMUINT2(random.Get(area.m_x0, area.m_x1), random.Get(area.m_y0, area.m_y1));
        ++iter;
    } while (iter < 50 && node->IsPillar(rndPos)); // ugly but set a max iters!
    
    if (checkNotInPortal)
    {
        XMUINT2 ppos;
        for (int i=0;i<(int)m_portals.size(); ++i)
        {
            const auto& p = m_portals[i];
            ppos = p.GetPortalPosition();
            if (rndPos == ppos)
            {
                switch (p.m_wallNode->m_type)
                {
                case LevelMapBSPNode::WALL_VERT:
                    ++rndPos.y; 
                    if (rndPos.y > area.m_y1) rndPos.y = area.m_y0;
                    break;
                case LevelMapBSPNode::WALL_HORIZ:
                    ++rndPos.x;
                    if (rndPos.x > area.m_x1) rndPos.x = area.m_x0;
                    break;
                }
                break;
            }
        }
    }
    return rndPos;
}

void LevelMap::GenerateTeleports(const VisMatrix& visMatrix)
{
    const size_t nLeaves = m_leaves.size();
    if (nLeaves <= 1)
        return;
    
    // generate disjoint sets
    typedef std::set<size_t> RoomSet;
    typedef std::vector<RoomSet> AllRoomSets;

    RoomSet initialSet; for (size_t i = 0; i < nLeaves; ++i) initialSet.insert(i);
    AllRoomSets allRoomSets;
    while (!initialSet.empty())
    {
        size_t roomNdx = *initialSet.begin(); initialSet.erase(initialSet.begin());
        
        std::queue<size_t> q;
        q.push(roomNdx);
        RoomSet roomSet;
        while (!q.empty()/* && !initialSet.empty()*/)
        {
            roomNdx = q.front(); q.pop();
            roomSet.insert(roomNdx);
            for (size_t i = 0; i < nLeaves; ++i)
            {
                if (i == roomNdx) continue;
                if (visMatrix.IsVisible((int)nLeaves, (int)roomNdx, (int)i) && roomSet.find(i) == roomSet.end())
                {
                    initialSet.erase(i);
                    q.push(i);
                    roomSet.insert(i);
                }
            }
        }

        if (!roomSet.empty())
            allRoomSets.push_back(roomSet);
    }

    // if nº sets == 1 return (no teleports, all well communicated)
    if (allRoomSets.size() <= 1) 
        return;

    // generate teleports between sets 2-by-2    
    auto& random = m_device->GetGameResources()->m_random;
    for (size_t i = 1; i < allRoomSets.size(); ++i)
    {
        const RoomSet& a = allRoomSets[i - 1];
        const RoomSet& b = allRoomSets[i];
        
        // gets a random room in both sets
        const size_t roomANdx = RandomRoomInSet(a, m_leaves, random);
        const size_t roomBNdx = RandomRoomInSet(b, m_leaves, random);

        auto& roomA = m_leaves[roomANdx];
        auto& roomB = m_leaves[roomBNdx];
        //if (roomA->m_teleportNdx != -1) { roomA->m_finished = true; roomA->m_tag = 0xffffff22; }
        //if (roomB->m_teleportNdx != -1) { roomB->m_finished = true; roomB->m_tag = 0xffffff22;}
        if ( !roomA->m_finished && !roomB->m_finished )
            this->VisGenerateTeleport(roomA, roomB);
        int asdfi = 0;
    }

    // disconnected?
    for (auto& r : m_leaves)
    {
        auto it = m_leafPortals.find(r.get());
        if (r->m_teleportNdx == -1 && it == m_leafPortals.end())
        {
            int i = r->m_leafNdx;
            i = i;
        }
    }
}

void LevelMap::CreateDeviceDependentResources()
{
    if (!m_root) return;
    // buffers for each room
    for (auto& leaf : m_leaves)
    {
        concurrency::create_task([this, leaf]() {
            leaf->CreateDeviceDependentResources(*this, m_device); 
        });
    }
    auto& loadTexTask = concurrency::create_task([this]() {
        DX::ThrowIfFailed(
            DirectX::CreateWICTextureFromFile(
                m_device->GetD3DDevice(), L"assets\\textures\\atlaslevel.png",
                (ID3D11Resource**)m_atlasTexture.ReleaseAndGetAddressOf(),
                m_atlasTextureSRV.ReleaseAndGetAddressOf()));
    });

}

void LevelMap::ReleaseDeviceDependentResources()
{
    // buffers for each room
    for (auto& leaf : m_leaves)
    {
        leaf->ReleaseDeviceDependentResources();
    }

    m_atlasTexture.Reset();
    m_atlasTextureSRV.Reset();
}

void LevelMap::Update(const DX::StepTimer& timer, const CameraFirstPerson& camera)
{
    auto pos = camera.GetPosition();
    m_cameraCurLeaf = GetLeafAt(pos);
//    if (m_cameraCurLeaf)
//        m_cameraCurLeaf->m_tag = 0xffffffaa;

    if (m_device && m_device->GetGameResources())
        m_device->GetGameResources()->m_levelTime += (float)timer.GetElapsedSeconds();
}

void LevelMap::Render(const CameraFirstPerson& camera)
{
    if (!m_root)
        return;

    if (!RenderSetCommonState(camera))
        return;

    auto context = m_device->GetD3DDeviceContext();
    if (GlobalFlags::DrawLevelGeometry)
    {
        // SINGLE room and connected ones
        if (m_cameraCurLeaf && m_cameraCurLeaf->m_dx && m_cameraCurLeaf->m_dx->m_indexBuffer)
        {
            auto dx = m_cameraCurLeaf->m_dx;
            UINT stride = sizeof(VertexPositionNormalColorTextureNdx);
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, dx->m_vertexBuffer.GetAddressOf(), &stride, &offset);
            context->IASetIndexBuffer(dx->m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
            context->DrawIndexed((UINT)dx->m_indexCount, 0, 0);
        }

        // render ALL rooms 
        // TODO: (c'mon, improve this with visibity bit*h, that's why you did BSP, duh!)
        //for (const auto& room : m_leaves)
        //{
        //    if (!room->m_dx || !room->m_dx->m_indexBuffer)  // not ready
        //        continue;
        //    
        //    UINT stride = sizeof(VertexPositionNormalColorTextureNdx);
        //    UINT offset = 0;
        //    context->IASetVertexBuffers(0, 1, room->m_dx->m_vertexBuffer.GetAddressOf(), &stride, &offset);
        //    context->IASetIndexBuffer(room->m_dx->m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        //    context->DrawIndexed((UINT)room->m_dx->m_indexCount, 0, 0);
        //}
    }


    /* DEBUG LINES */
    if (GlobalFlags::DrawDebugLines)
    {
        for (const auto& room : m_leaves)
        {
            if (!room->m_collisionSegments) continue;
            auto gameRes = m_device->GetGameResources();
            gameRes->m_batch->Begin();
            XMFLOAT3 s, e;
            XMFLOAT4 c(DirectX::Colors::Yellow.f); std::swap(c.x, c.w);
            for (const auto& seg : *room->m_collisionSegments)
            {
                s.x = seg.start.x; s.y = 0.0f; s.z = seg.start.y;
                e.x = seg.end.x; e.y = 0.0f; e.z = seg.end.y;
                if (seg.IsDisabled())
                    s.y = e.y = 0.4f;
                VertexPositionColor v1(s, c), v2(e, c);
                gameRes->m_batch->DrawLine(v1, v2);
            }
            gameRes->m_batch->End();
        }
    }

    // drawing doors
    auto& spr = m_device->GetGameResources()->m_sprite;
    spr.Begin3D(camera);
    XMFLOAT3 dp; 
    float rotY;
    // current SINGLE coors
    auto it = m_leafPortals.find(m_cameraCurLeaf.get());
    while (it != m_leafPortals.end() && it->first == m_cameraCurLeaf.get())
    {
        const auto& d = m_portals[it->second];
        d.GetTransform(dp, rotY);
        spr.Draw3D(d.m_open ? 31 : 24, dp, XMFLOAT2(1, 1.5f), XMFLOAT4(1,1,1,1), false, false, false, rotY);
        ++it;
    }


    // ALL door
    //for (const auto& d : m_portals)
    //{
    //    d.GetTransform(dp, rotY);
    //    spr.Draw3D(d.m_open ? 31 : 24, dp, XMFLOAT2(1, 1.5f), XMFLOAT4(1,1,1,1), false, false, false, rotY);
    //}
    spr.End3D();
}

void LevelMap::RenderMinimap(const CameraFirstPerson& camera)
{

    // UI rendering
    if (GlobalFlags::DrawThumbMap)
    {
        auto sprites = m_device->GetGameResources()->m_sprites.get();
        sprites->Begin(DirectX::SpriteSortMode_Deferred, nullptr, m_device->GetGameResources()->m_commonStates->PointClamp());
        XMFLOAT2 scale;
        switch (GlobalFlags::DrawThumbMap)
        {
        case 1: scale = XMFLOAT2(4, 4); break;
        case 2: scale = XMFLOAT2(10, 10); break;
        }
        sprites->Draw(m_thumbTex.m_textureView.Get(), XMFLOAT2(10, 400), nullptr, Colors::White, 0, XMFLOAT2(0, 0), scale);
        sprites->End();
    }
}


bool LevelMap::RenderSetCommonState(const CameraFirstPerson& camera)
{
    if (!m_atlasTexture || !m_atlasTextureSRV)
        return false;

    // common render state for all rooms
    ModelViewProjectionConstantBuffer cbData ={m_levelTransform, camera.m_view, camera.m_projection};
    auto gameRes = m_device->GetGameResources();
    if (!gameRes->m_readyToRender) return false;
    auto context = m_device->GetD3DDeviceContext();
    context->UpdateSubresource1(gameRes->m_baseVSCB.Get(),0,NULL,&cbData,0,0,0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(gameRes->m_baseIL.Get());
    context->VSSetShader(gameRes->m_baseVS.Get(), nullptr, 0);
    context->VSSetConstantBuffers1(0, 1, gameRes->m_baseVSCB.GetAddressOf(), nullptr, nullptr);
    ID3D11SamplerState* sampler = gameRes->m_commonStates->PointWrap();
    context->PSSetSamplers(0, 1, &sampler);    
    context->PSSetShaderResources(0, 1, m_atlasTextureSRV.GetAddressOf());
    //float t = std::max(std::min(camera.m_rightDownTime*2.0f, 1.f), std::max(gameRes->m_flashScreenTime*0.7f, 0.0f));
    float t = std::max( 0.5f, std::max(gameRes->m_flashScreenTime*0.7f,0.0f));
    if (GlobalFlags::AllLit) t = 1.0f;
    PixelShaderConstantBuffer pscb = { 
        { 16,16, gameRes->m_levelTime,camera.m_aspectRatio }, 
        {t,1.0f-int(GlobalFlags::AllLit), gameRes->m_curDensityMult,0},
        {1,1,1,1} };
    context->UpdateSubresource1(gameRes->m_basePSCB.Get(), 0, NULL, &pscb, 0, 0, 0);
    context->PSSetConstantBuffers(0, 1, gameRes->m_basePSCB.GetAddressOf());
    context->PSSetShader(gameRes->m_basePS.Get(), nullptr, 0);
    context->OMSetDepthStencilState(gameRes->m_commonStates->DepthDefault(), 0);
    context->OMSetBlendState(gameRes->m_commonStates->Opaque(), nullptr, 0xffffffff);
    if ( GlobalFlags::DrawWireframe )
        context->RSSetState(gameRes->m_commonStates->Wireframe());
    else
        context->RSSetState(gameRes->m_commonStates->CullCounterClockwise());
    return true;
}

int LevelMap::GetLeafIndexAt(const XMFLOAT3& pos) const
{
    // linear search (this works so far, do the BSP search later with more time)
    XMUINT2 ipos((UINT)pos.x, (UINT)pos.z);
    int i = -1;
    for (auto room : m_leaves)
    {
        ++i;
        if (room->m_area.Contains(ipos))
            break;
    }
    return i;
}


LevelMapBSPNodePtr LevelMap::GetLeafAtIndex(int index) const
{
    return m_leaves[index];
}


LevelMapBSPNodePtr LevelMap::GetLeafAt(const XMFLOAT3& pos) const
{
    // linear search (this works so far, do the BSP search later with more time)
    XMUINT2 ipos((UINT)pos.x, (UINT)pos.z);
    for (auto room : m_leaves)
    {
        if (room->m_area.Contains(ipos) )
        { 
            return room;
        }
    }
    return nullptr;
}

XMUINT2 LevelMap::GetRandomPosition()
{
    if (!m_root || m_leaves.empty())
        return XMUINT2(0, 0);

    return XMUINT2(m_leaves.front()->m_area.m_x0, m_leaves.front()->m_area.m_y0);
}

XMUINT2 LevelMap::ConvertToMapPosition(const XMFLOAT3& xyz) const
{
    UINT tx = m_thumbTex.m_dim.x;
    UINT ty = m_thumbTex.m_dim.y;
    XMVECTOR maxMap = XMVectorSet((float)tx - 1, 0, (float)ty - 1, 0);
    XMVECTOR camXZ = XMLoadFloat3(&xyz);
    camXZ = XMVectorClamp(camXZ, XMVectorZero(), maxMap);
    XMUINT2 ppos((UINT)XMVectorGetX(camXZ), (UINT)XMVectorGetZ(camXZ));
    return ppos;
}

const SegmentList* LevelMap::GetCurrentCollisionSegments()
{
    if (!m_cameraCurLeaf) return nullptr;
    return m_cameraCurLeaf->m_collisionSegments.get();
}


bool LevelMap::RaycastDir(const XMFLOAT3& origin, const XMFLOAT3& dir, XMFLOAT3& outHit)
{
    // get room where origin is
    // check against all collision segments for that room,
    // if portal hit move origin to portal origin and check again for the room that portal connects with
    const auto& leaf = GetLeafAt(origin);
    if (!leaf) return false;
    if (!leaf->m_collisionSegments) 
        return false;

    XMFLOAT2 origin2D(origin.x, origin.z);
    XMFLOAT2 dir2D(dir.x, dir.z);
    
    XMFLOAT2 minHit, hit;
    float minFrac = FLT_MAX, frac;
    int minCSIndex = -1;
    const int count = (int)leaf->m_collisionSegments->size();
    for (int i =0 ; i < count; ++i )
    {
        const auto& cs = leaf->m_collisionSegments->at(i);
        if (IntersectRaySegment(origin2D, dir2D, cs, hit, frac) && frac < minFrac)
        {
            minFrac = frac;
            minHit = hit;
            minCSIndex = i;
        }
    }
    
    // was there any hit?
    if (minCSIndex != -1)
    {
        const auto& cs = leaf->m_collisionSegments->at(minCSIndex);
        if ( cs.IsPortalOpen() )
        {
            XMFLOAT3 newOrigin3D(minHit.x + dir.x*0.05f, 0.0f, minHit.y+dir.z*0.05f);
            return RaycastDir(newOrigin3D, dir, outHit);
        }
        else
        {
            outHit = XMFLOAT3(minHit.x, 0.0f, minHit.y);
            return true;
        }
    }

    return false;
}

bool LevelMap::RaycastSeg(const XMFLOAT3& origin, const XMFLOAT3& end, XMFLOAT3& outHit, float optRad, float offsHit)
{
    XMFLOAT3 dir2D = XM3Sub(end, origin);
    dir2D.y = 0.0f;
    const float distSq = XM3LenSq(dir2D);
    const float invSq = 1.0f / sqrtf(distSq);
    dir2D.x *= invSq; dir2D.z *= invSq;
    XMFLOAT3 origin2D(origin.x, 0, origin.z);

    // ground?
    float fracToG = 0;
    XMFLOAT3 hitG(0, 0, 0);
    const XMFLOAT3 dir3D = XM3Normalize(XM3Sub(end, origin));
    const bool hitGround = false;// IntersectRayPlane(origin, dir3D, XM3Up(), XM3Zero(), hitG, fracToG);

    // any wall
    bool wasHit = false;
    bool hitWalls = RaycastDir(origin2D, dir2D, outHit);
    if ( hitWalls || hitGround)
    {
        hitG.y = 0.0f;
        XMFLOAT3 toHit = XM3Sub(outHit, origin2D);
        const float lenToHitSq = hitWalls ? XM3LenSq(toHit) : FLT_MAX;
        const float lenToGSq = hitGround ? XM3LenSq(origin, hitG) : FLT_MAX;
        const float compRad = optRad > 0.0f ? (optRad) : distSq;

        wasHit = lenToHitSq <= compRad;
        if (hitGround)
        {
            if (wasHit && lenToGSq < lenToHitSq)
                outHit = hitG;
            else if (!wasHit)
            {
                outHit = hitG;
                wasHit = true;
            }
        }

        if (wasHit && offsHit!=0 && !hitGround)
        {
            outHit = XM3Mad(outHit, dir3D, offsHit);
        }
    }
    return wasHit;
}

LevelMapBSPNodePtr LevelMap::GetBiggestRoom() const
{
    LevelMapBSPNodePtr maxRoom;
    int maxArea = -1;
    for (auto& r: m_leaves)
    {
        const int area = (int)(r->m_area.CountTiles());
        if (area > maxArea)
        {
            maxRoom = r;
            maxArea = area;
        }
    }
    return maxRoom;
}

void LevelMap::ToggleRoomDoors(int roomIndex, bool open)
{
    if (roomIndex == -1 && !m_cameraCurLeaf)
        return;
    
    m_cameraCurLeaf->m_finished = true;
    m_cameraCurLeaf->m_tag = 0xffffff77;

    // disable collision segments for this leaf
    std::vector<CollSegment> portalSegments; 
    portalSegments.reserve(4);
    auto leaf = roomIndex < 0 ? m_cameraCurLeaf : m_leaves[roomIndex];
    if (!leaf) 
        return;
    if (leaf->m_collisionSegments)
    {
        for (auto& collseg : *leaf->m_collisionSegments)
        {
            if (!collseg.IsPortal()) continue;
            collseg.SetDisabled(open);
            portalSegments.push_back(collseg);
        }
    }
    
    // look for all portal objects, mark as open
    {
        auto it = m_leafPortals.find(leaf.get());
        while (it != m_leafPortals.end() && it->first == leaf.get())
        {
            auto& portal = m_portals[it->second];
            portal.m_open = open;

            // for this portal, get the connected room and disable its matching portal segment 
            auto ol = portal.GetOtherLeaf(leaf);
            if (ol->m_collisionSegments)
            {
                for (auto& collseg : *ol->m_collisionSegments)
                {
                    if (!collseg.IsPortal()) continue;
                    if (std::find(portalSegments.begin(), portalSegments.end(), collseg) != portalSegments.end())
                    {
                        collseg.SetDisabled(open);
                        break;
                    }
                }
            }

            ++it;
        }
    }

    // any teleport
    if (m_cameraCurLeaf->m_teleportNdx != -1)
    {
        auto& tp = m_teleports[m_cameraCurLeaf->m_teleportNdx];
        tp.m_open = true;
        auto gameRes = DX::GameResources::instance;
        auto& otherLeaf = tp.GetOtherLeaf(m_cameraCurLeaf);
        gameRes->m_entityMgr.AddEntity(std::make_shared<EntityTeleport>(tp.GetPosition(m_cameraCurLeaf), otherLeaf->m_leafNdx), m_cameraCurLeaf->m_leafNdx);
        gameRes->m_entityMgr.AddEntity(std::make_shared<EntityTeleport>(tp.GetOtherPosition(m_cameraCurLeaf), m_cameraCurLeaf->m_leafNdx), otherLeaf->m_leafNdx);
    }

}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#pragma region LevelMapBSPNode
#pragma warning(disable:4838)
LevelMapBSPNode::PortalDir LevelMapBSPNode::GetPortalDirAt(const LevelMap& lmap, uint32_t x, uint32_t y)
{
    const XMUINT2 xy(x, y);
    XMUINT2 ppos[2];
    auto rang = lmap.m_leafPortals.equal_range(this);
    for (auto it = rang.first; it != rang.second; ++it)
    {
        const auto& portal = lmap.m_portals[it->second];
        ppos[0] = portal.GetPortalPosition(ppos + 1);
        for (int i = 0; i < 2; ++i)
        {
            if (xy == ppos[i])
            {
                const XMUINT2& opp = ppos[(i + 1) % 2];
                if (opp.y == y - 1) return NORTH;
                else if (opp.y == y + 1) return SOUTH;
                else if (opp.x == x - 1) return WEST;
                else if (opp.x == x + 1) return EAST;
                DX::ThrowIfFalse(false);
            }
        }
    }
    return NONE;
}

void LevelMapBSPNode::CreateDeviceDependentResources(const LevelMap& lmap, const std::shared_ptr<DX::DeviceResources>& device)
{
    if (m_dx || !IsLeaf())
        return;
    m_dx = std::make_shared<NodeDXResources>();

    const auto& area = m_area;
    const int pvc = m_pillars ? (int)m_pillars->size() : 0;
    std::vector<VertexPositionNormalColorTextureNdx> vertices;
    vertices.reserve(area.CountTiles() * 4 + pvc*16);
    std::vector<unsigned short> indices; 
    indices.reserve(area.CountTiles() * 6 + pvc*24);
    {
        static const float EP = 1.0f;
        static const float FH = 2.0f;
        static const float FHF = 0.75f;
        static const float OFFSFH = FH*FHF;

        XMFLOAT4 argb(DirectX::Colors::White.f);
        VertexPositionNormalColorTextureNdx quadVerts[4];
        for (int i = 0; i < 4; ++i)
        {
            quadVerts[i].color = argb;
        }
        auto& random = device->GetGameResources()->m_random;
        UINT FLOORTEX = random.Get(5,8);
        UINT CEILINGTEX = random.Get(0, 4);
        UINT WALLTEX = random.Get(3, 7);
        bool ceiling = true;
        if (m_profile == LevelMap::RP_GRAVE || m_profile == LevelMap::RP_WOODS || m_profile == LevelMap::RP_PUMPKINFIELD)
        {
            FLOORTEX = random.Get(0, 4);
            if (m_profile == LevelMap::RP_GRAVE) WALLTEX = 1;
            else if (m_profile == LevelMap::RP_WOODS) WALLTEX = 0;
            else WALLTEX = 2;
            ceiling = false;
        }
        
        unsigned short cvi = 0; // current vertex index
        float x, z;
        for (uint32_t _z = area.m_y0; _z <= area.m_y1; ++_z)
        {
            for (uint32_t _x = area.m_x0; _x <= area.m_x1; ++_x)
            {
                x = float(_x); z = float(_z);
                // floor tile
                {
                    float h0 = 0.0f, h1 = 0.0f;
                    if (m_profile == LevelMap::RP_GRAVE || m_profile == LevelMap::RP_WOODS || m_profile == LevelMap::RP_PUMPKINFIELD)
                    {
                        h0 = std::max(0.0f,sin(x)*0.15f);
                        h1 = std::max(0.0f,sin(x + 1)*0.15f);
                    }
                    quadVerts[0].position = XMFLOAT3(x, h0, z);
                    quadVerts[1].position = XMFLOAT3(x + EP, h1, z);
                    quadVerts[2].position = XMFLOAT3(x + EP, h1, z + EP);
                    quadVerts[3].position = XMFLOAT3(x, h0, z + EP);
                    quadVerts[0].SetTexCoord(0, 0, FLOORTEX, 1);
                    quadVerts[1].SetTexCoord(1, 0, FLOORTEX, 1);
                    quadVerts[2].SetTexCoord(1, 1, FLOORTEX, 1);
                    quadVerts[3].SetTexCoord(0, 1, FLOORTEX, 1);
                    for (auto& v : quadVerts) { v.normal = XMFLOAT3(0, 1, 0); }
                    std::copy(quadVerts, quadVerts + 4, std::back_inserter(vertices));
                    const unsigned short inds[6] = { /*tri0*/cvi, cvi + 1, cvi + 2, /*tri1*/cvi, cvi + 2, cvi + 3 };
                    cvi += 4; // 
                    std::copy(inds, inds + 6, std::back_inserter(indices));
                }

                // ceiling tile
                if ( ceiling )
                {
                    quadVerts[0].position = XMFLOAT3(x, FH, z);
                    quadVerts[3].position = XMFLOAT3(x + EP, FH, z);
                    quadVerts[2].position = XMFLOAT3(x + EP, FH, z + EP);
                    quadVerts[1].position = XMFLOAT3(x, FH, z + EP);
                    quadVerts[0].SetTexCoord(0, 0, CEILINGTEX, 2);
                    quadVerts[1].SetTexCoord(1, 0, CEILINGTEX, 2);
                    quadVerts[2].SetTexCoord(1, 1, CEILINGTEX, 2);
                    quadVerts[3].SetTexCoord(0, 1, CEILINGTEX, 2);
                    for (auto& v : quadVerts) { v.normal = XMFLOAT3(0, -1, 0); }
                    std::copy(quadVerts, quadVerts + 4, std::back_inserter(vertices));
                    const unsigned short inds[6] = { cvi, cvi + 1, cvi + 2, cvi, cvi + 2, cvi + 3 };
                    cvi += 4; // 
                    std::copy(inds, inds + 6, std::back_inserter(indices));
                }

                // walls
                {
                    auto portalDir = GetPortalDirAt(lmap, _x, _z);
                    bool addWallTile = false;
                    if (_z == m_area.m_y0 )              // wall north
                    {
                        const float offsH = portalDir == NORTH ? OFFSFH : 0.0f;
                        const float offsHF = portalDir == NORTH ? 1.0f-FHF : 1.0f;
                        quadVerts[0].position = XMFLOAT3(x, offsH, z);
                        quadVerts[1].position = XMFLOAT3(x + EP, offsH, z);
                        quadVerts[2].position = XMFLOAT3(x + EP, FH, z);
                        quadVerts[3].position = XMFLOAT3(x, FH, z);
                        for (auto& v : quadVerts) { v.normal = XMFLOAT3(0, 0, 1); }
                        quadVerts[0].SetTexCoord(0, 1*offsHF, WALLTEX, 0);
                        quadVerts[1].SetTexCoord(1, 1*offsHF, WALLTEX, 0);
                        quadVerts[2].SetTexCoord(1, 0, WALLTEX, 0);
                        quadVerts[3].SetTexCoord(0, 0, WALLTEX, 0);
                        addWallTile = true;
                    }
                    else if (_z == m_area.m_y1)         // wall south
                    {
                        const float offsH = portalDir == SOUTH ? OFFSFH : 0.0f;
                        const float offsHF = portalDir == SOUTH ? 1.0f - FHF : 1.0f;
                        quadVerts[0].position = XMFLOAT3(x, offsH, z + EP);
                        quadVerts[3].position = XMFLOAT3(x + EP, offsH, z + EP);
                        quadVerts[2].position = XMFLOAT3(x + EP, FH, z + EP);
                        quadVerts[1].position = XMFLOAT3(x, FH, z + EP);
                        for (auto& v : quadVerts) { v.normal = XMFLOAT3(0, 0, -1); }
                        quadVerts[0].SetTexCoord(0, 1*offsHF, WALLTEX, 0);
                        quadVerts[3].SetTexCoord(1, 1*offsHF, WALLTEX, 0);
                        quadVerts[2].SetTexCoord(1, 0, WALLTEX, 0);
                        quadVerts[1].SetTexCoord(0, 0, WALLTEX, 0);
                        addWallTile = true;
                    }

                    if (addWallTile)
                    {
                        std::copy(quadVerts, quadVerts + 4, std::back_inserter(vertices));
                        const unsigned short inds[6] = { /*tri0*/cvi, cvi + 2, cvi + 1, /*tri1*/cvi, cvi + 3, cvi + 2 };
                        cvi += 4; // 
                        std::copy(inds, inds + 6, std::back_inserter(indices));
                    }

                    if (_x == m_area.m_x0 )              // wall west
                    {
                        const float offsH = portalDir == WEST ? OFFSFH : 0.0f;
                        const float offsHF = portalDir == WEST ? 1.0f - FHF : 1.0f;
                        quadVerts[0].position = XMFLOAT3(x, offsH, z + EP);
                        quadVerts[1].position = XMFLOAT3(x, offsH, z);
                        quadVerts[2].position = XMFLOAT3(x, FH, z);
                        quadVerts[3].position = XMFLOAT3(x, FH, z + EP);
                        for (auto& v : quadVerts) { v.normal = XMFLOAT3(1, 0, 0); }
                        quadVerts[0].SetTexCoord(0, 1*offsHF, WALLTEX, 0);
                        quadVerts[1].SetTexCoord(1, 1*offsHF, WALLTEX, 0);
                        quadVerts[2].SetTexCoord(1, 0, WALLTEX, 0);
                        quadVerts[3].SetTexCoord(0, 0, WALLTEX, 0);
                        addWallTile = true;
                    }
                    else if (_x == m_area.m_x1 )         // wall east
                    {
                        const float offsH = portalDir == EAST ? OFFSFH : 0.0f;
                        const float offsHF = portalDir == EAST ? 1.0f - FHF : 1.0f;
                        quadVerts[0].position = XMFLOAT3(x + EP, offsH, z + EP);
                        quadVerts[3].position = XMFLOAT3(x + EP, offsH, z);
                        quadVerts[2].position = XMFLOAT3(x + EP, FH, z);
                        quadVerts[1].position = XMFLOAT3(x + EP, FH, z + EP);
                        for (auto& v : quadVerts) { v.normal = XMFLOAT3(-1, 0, 0); }
                        quadVerts[0].SetTexCoord(0, 1*offsHF, WALLTEX, 0);
                        quadVerts[3].SetTexCoord(1, 1*offsHF, WALLTEX, 0);
                        quadVerts[2].SetTexCoord(1, 0, WALLTEX, 0);
                        quadVerts[1].SetTexCoord(0, 0, WALLTEX, 0);
                        addWallTile = true;
                    }
                    if (addWallTile)
                    {
                        std::copy(quadVerts, quadVerts + 4, std::back_inserter(vertices));
                        const unsigned short inds[6] = { /*tri0*/cvi, cvi + 2, cvi + 1, /*tri1*/cvi, cvi + 3, cvi + 2 };
                        cvi += 4; // 
                        std::copy(inds, inds + 6, std::back_inserter(indices));
                    }
                }
            } // for area x
        } // for area y

        // Pillars
        if (m_pillars)
        {
            for (int i = 0; i < 4; ++i)
                quadVerts[i].textureIndex = XMUINT2(random.Get(3,6), 0);
            float x, z;
            for (const auto& pillar : *m_pillars)
            {
                x = (float)pillar.x; z = (float)pillar.y;
                // north face
                {
                    quadVerts[0].position = XMFLOAT3(x+1, 0.0f, z);
                    quadVerts[1].position = XMFLOAT3(x+1, FH, z);
                    quadVerts[2].position = XMFLOAT3(x, FH, z);
                    quadVerts[3].position = XMFLOAT3(x, 0, z);
                    for (int i = 0; i < 4; ++i) quadVerts[i].normal = XMFLOAT3(0, 0, -1);
                    std::copy(quadVerts, quadVerts + 4, std::back_inserter(vertices));
                    const unsigned short inds[6] = { cvi, cvi + 1, cvi + 2, cvi, cvi + 2, cvi + 3 }; cvi += 4;
                    std::copy(inds, inds + 6, std::back_inserter(indices));
                }

                // east face
                {
                    quadVerts[0].position = quadVerts[3].position;
                    quadVerts[1].position = quadVerts[2].position;
                    quadVerts[2].position = XMFLOAT3(x, FH, z + 1);
                    quadVerts[3].position = XMFLOAT3(x, 0, z + 1);
                    for (int i = 0; i < 4; ++i) quadVerts[i].normal = XMFLOAT3(-1, 0, 0);
                    std::copy(quadVerts, quadVerts + 4, std::back_inserter(vertices));
                    const unsigned short inds[6] = { cvi, cvi + 1, cvi + 2, cvi, cvi + 2, cvi + 3 }; cvi += 4;
                    std::copy(inds, inds + 6, std::back_inserter(indices));
                }

                // south face
                {
                    quadVerts[0].position = quadVerts[3].position;
                    quadVerts[1].position = quadVerts[2].position;
                    quadVerts[2].position = XMFLOAT3(x + 1, FH, z + 1);
                    quadVerts[3].position = XMFLOAT3(x + 1, 0, z + 1);
                    for (int i = 0; i < 4; ++i) quadVerts[i].normal = XMFLOAT3(0, 0, 1);
                    std::copy(quadVerts, quadVerts + 4, std::back_inserter(vertices));
                    const unsigned short inds[6] = { cvi, cvi + 1, cvi + 2, cvi, cvi + 2, cvi + 3 }; cvi += 4;
                    std::copy(inds, inds + 6, std::back_inserter(indices));
                }

                // west face
                {
                    quadVerts[0].position = quadVerts[3].position;
                    quadVerts[1].position = quadVerts[2].position;
                    quadVerts[2].position = XMFLOAT3(x + 1, FH, z );
                    quadVerts[3].position = XMFLOAT3(x + 1, 0, z );
                    for (int i = 0; i < 4; ++i) quadVerts[i].normal = XMFLOAT3(1, 0, 0);
                    std::copy(quadVerts, quadVerts + 4, std::back_inserter(vertices));
                    const unsigned short inds[6] = { cvi, cvi + 1, cvi + 2, cvi, cvi + 2, cvi + 3 }; cvi += 4;
                    std::copy(inds, inds + 6, std::back_inserter(indices));
                }
            }
        }
    }
    m_dx->m_indexCount = indices.size();
    DX::ThrowIfFalse(!vertices.empty() && !indices.empty());

    // VB
    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = vertices.data();
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    const UINT vbsize = UINT(sizeof(VertexPositionNormalColorTextureNdx)*vertices.size());
    CD3D11_BUFFER_DESC vertexBufferDesc(vbsize, D3D11_BIND_VERTEX_BUFFER);
    DX::ThrowIfFailed(
        device->GetD3DDevice()->CreateBuffer(
            &vertexBufferDesc,
            &vertexBufferData,
            &m_dx->m_vertexBuffer
        )
    );

    // IB
    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = indices.data();
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC indexBufferDesc(UINT(sizeof(unsigned short)*indices.size()), D3D11_BIND_INDEX_BUFFER);
    DX::ThrowIfFailed(
        device->GetD3DDevice()->CreateBuffer(
            &indexBufferDesc,
            &indexBufferData,
            &m_dx->m_indexBuffer
        )
    );

}
#pragma warning(default:4838)

void LevelMapBSPNode::ReleaseDeviceDependentResources()
{
    if (!m_dx) return;
    m_dx->m_vertexBuffer.Reset();
    m_dx->m_indexBuffer.Reset();
    m_dx->m_indexCount = 0;
}

void LevelMapBSPNode::GenerateCollisionSegments(const LevelMap& lmap)
{
    if (!IsLeaf()) return;
    m_collisionSegments = std::make_shared<SegmentList>();
    m_collisionSegments->reserve(8); // 
    const float xs[2] = { (float)m_area.m_x0, (float)m_area.m_x1 };
    const float ys[2] = { (float)m_area.m_y0, (float)m_area.m_y1 };

    CollSegment lastSeg;
    lastSeg.flags = CollSegment::WALL;
    
    CollSegment portalSeg;     
    portalSeg.flags = CollSegment::PORTAL;// | CollSegment::DISABLED;

    // north/south
    for (int i = 0; i < 2; ++i)
    {
        lastSeg.start = XMFLOAT2(xs[0], ys[i] + i*1.0f);
        lastSeg.end = lastSeg.start;
        lastSeg.normal = XMFLOAT2(0.0f, 1.0f*(i ? -1.0f : 1.0f));
        for (uint32_t ix = m_area.m_x0; ix <= m_area.m_x1; ++ix)
        {
            auto portalDir = GetPortalDirAt(lmap, ix, (uint32_t)ys[i]);
            if (portalDir == (PortalDir)(NORTH + i))
            {
                if (lastSeg.IsValid())
                    m_collisionSegments->push_back(lastSeg);
                // portal segment
                portalSeg.start = portalSeg.end = lastSeg.end;
                portalSeg.end.x += 1.0f;
                portalSeg.normal = lastSeg.normal;
                m_collisionSegments->push_back(portalSeg);
                lastSeg.start.x = lastSeg.end.x = ix + 1.0f;
            }
            else
            {
                lastSeg.end.x = ix + 1.0f;
            }
        }
        if (lastSeg.IsValid())
            m_collisionSegments->push_back(lastSeg);
    }

    // west/east
    for (int i = 0; i < 2; ++i)
    {
        lastSeg.start = XMFLOAT2(xs[i] + i*1.0f, ys[0]);
        lastSeg.end = lastSeg.start;
        lastSeg.normal = XMFLOAT2(1.0f*(i ? -1.0f : 1.0f), 0.0f);
        for (uint32_t iy = m_area.m_y0; iy <= m_area.m_y1; ++iy)
        {
            auto portalDir = GetPortalDirAt(lmap, (uint32_t)xs[i], iy);
            if (portalDir == (PortalDir)(WEST + i))
            {
                if (lastSeg.IsValid())
                    m_collisionSegments->push_back(lastSeg);
                // portal segment
                portalSeg.start = portalSeg.end = lastSeg.end;
                portalSeg.end.y += 1.0f;
                portalSeg.normal = lastSeg.normal;
                m_collisionSegments->push_back(portalSeg);
                lastSeg.start.y = lastSeg.end.y = iy + 1.0f;
            }
            else
            {
                lastSeg.end.y = iy + 1.0f;
            }
        }
        if (lastSeg.IsValid())
            m_collisionSegments->push_back(lastSeg);
    }


    // pillars
    if (m_pillars)
    {
        CollSegment pillarSeg;
        pillarSeg.flags = CollSegment::PILLAR;
        XMFLOAT2 corners[4];
        float x, y;
        for (auto& pillar : *m_pillars)
        {
            x = (float)pillar.x; y = (float)pillar.y;
            corners[0] = XMFLOAT2(x, y);
            corners[1] = XMFLOAT2(x + 1, y);
            corners[2] = XMFLOAT2(x + 1, y+1);
            corners[3] = XMFLOAT2(x, y+1);

            pillarSeg.start = corners[0]; pillarSeg.end = corners[1]; pillarSeg.normal = XMFLOAT2(0,-1);
            m_collisionSegments->push_back(pillarSeg);
            pillarSeg.start = corners[1]; pillarSeg.end = corners[2]; pillarSeg.normal = XMFLOAT2(1,0);
            m_collisionSegments->push_back(pillarSeg);
            pillarSeg.start = corners[2]; pillarSeg.end = corners[3]; pillarSeg.normal = XMFLOAT2(0,1);
            m_collisionSegments->push_back(pillarSeg);
            pillarSeg.start = corners[3]; pillarSeg.end = corners[0]; pillarSeg.normal = XMFLOAT2(-1,0);
            m_collisionSegments->push_back(pillarSeg);
        }
    }
}

bool LevelMapBSPNode::IsPillar(const XMUINT2& ppos)const
{
    if (!m_pillars) return false;
    return std::find_if(m_pillars->begin(), m_pillars->end(), [ppos](const auto& p)->bool
    {
        return p.x == ppos.x && p.y == ppos.y;
    }) != m_pillars->end();
}

XMFLOAT3 LevelMapBSPNode::GetRandomXZ(const XMFLOAT2& shrink) const
{
    auto& r = DX::GameResources::instance->m_random;
    float a, b;
    XMFLOAT3 xz;
    a = (float)m_area.m_x0 + shrink.x;
    b = (float)m_area.m_x1 - shrink.x;
    xz.x = r.GetF( std::min(a,b), std::max(a,b) );
    a = (float)m_area.m_y0 + shrink.y;
    b = (float)m_area.m_y1 - shrink.y;
    xz.z = r.GetF(std::min(a, b), std::max(a, b));
    xz.y = 0.0f;
    return xz;
}

XMFLOAT3 LevelMapBSPNode::GetRandomXZWithClearance() const
{
    // get free tiles
    std::vector<XMUINT2> freeTiles; freeTiles.reserve(m_area.CountTiles());
    XMUINT2 t;
    for (uint32_t y = m_area.m_y0; y <= m_area.m_y1; ++y)
    {
        for (uint32_t x = m_area.m_x0; x <= m_area.m_x1; ++x)
        {
            t.x = x; t.y = y;
            if (!IsPillar(t))
                freeTiles.push_back(t);
        }
    }
    if (freeTiles.empty())
        throw std::exception("No free tiles in this room");

    t = freeTiles[DX::GameResources::instance->m_random.Get(0, (int)freeTiles.size() - 1)];
    return XMFLOAT3(t.x + 0.5f, 0.0f, t.y + 0.5f);
}


XMUINT2 LevelMapBSPNode::GetRandomTile() const
{
    auto& r = DX::GameResources::instance->m_random;
    XMUINT2 t;
    t.x = r.Get(m_area.m_x0, m_area.m_x1);
    t.y = r.Get(m_area.m_y0, m_area.m_y1);
    return t;
}


#pragma endregion

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#pragma region LevelMapThumbTexture
// TODO: so bad! it destroys/creates the texture and texture view rather than update, change that!
void LevelMapThumbTexture::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& device)
{
    D3D11_TEXTURE2D_DESC desc = { 0 };
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Width = m_dim.x;
    desc.Height = m_dim.y;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA initData = { 0 };
    initData.pSysMem = m_sysMem;
    initData.SysMemSlicePitch = initData.SysMemPitch = sizeof(uint32_t)*m_dim.x;
    initData.SysMemSlicePitch *= m_dim.y;
    DX::ThrowIfFailed(device->GetD3DDevice()->CreateTexture2D(&desc, &initData, m_texture.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(device->GetD3DDevice()->CreateShaderResourceView(m_texture.Get(), nullptr, m_textureView.ReleaseAndGetAddressOf()));
}

void LevelMapThumbTexture::ReleaseDeviceDependentResources()
{
    m_texture.Reset();
    m_textureView.Reset();
}

void LevelMapThumbTexture::Destroy()
{
    if (m_sysMem)
    {
        delete[] m_sysMem;
        m_sysMem = nullptr;
    }
    ReleaseDeviceDependentResources();
}

void LevelMapThumbTexture::SetAt(uint32_t x, uint32_t y, uint32_t value)
{
    m_sysMem[m_dim.y*y + x] = value;
}
#pragma endregion

