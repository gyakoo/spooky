#include "pch.h"
#include "LevelMap.h"
#include <../Common/DirectXHelper.h>

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

static const uint32_t RANDOM_DEFAULT_SEED = 997;

LevelMapGenerationSettings::LevelMapGenerationSettings()
    : m_randomSeed(RANDOM_DEFAULT_SEED), m_tileSize(1.0f, 1.0f)
    , m_tileCount(20, 20), m_minTileCount(4, 4)
    , m_charRadius(0.25f), m_minRecursiveDepth(3)
    , m_maxRecursiveDepth(5)
    , m_probRoom(0.05f), m_generateThumbTex(true)
    , m_maxTileCount(8,8)
{

}

void RandomProvider::SetSeed(uint32_t seed)
{
    if (!m_gen || m_lastSeed != seed)
        m_gen = std::make_unique<std::mt19937>(seed);
    m_lastSeed = seed;
}

uint32_t RandomProvider::Get(uint32_t minN, uint32_t maxN)
{
    // slow?
    if (!m_gen)
        SetSeed(RANDOM_DEFAULT_SEED);
    return std::uniform_int_distribution<uint32_t>(minN, maxN)(*m_gen);
}

void LevelMapGenerationSettings::Validate() const
{
    DX::ThrowIfFalse(m_charRadius > 0.01f);
    const float charDiam = m_charRadius * 2.0f;
    DX::ThrowIfFalse(m_tileSize.x > charDiam && m_tileSize.y > charDiam);
    DX::ThrowIfFalse(m_tileCount.x >= m_minTileCount.x && m_tileCount.y >= m_minTileCount.y);
    DX::ThrowIfFalse(m_minTileCount.x >= 2 && m_minTileCount.y >= 2);
}

LevelMap::LevelMap()
    : m_root(nullptr), m_thumbTex(nullptr)
{

}

void LevelMap::Generate(const LevelMapGenerationSettings& settings)
{
    settings.Validate();

    Destroy();

    m_root = std::make_shared<LevelMapBSPNode>();
    LevelMapBSPTileArea area(0, settings.m_tileCount.x - 1, 0, settings.m_tileCount.y - 1);
    m_random.SetSeed(settings.m_randomSeed);
    LevelMapBSPNodePtr lastRoom;
    RecursiveGenerate(m_root, area, settings, 0);        
    GenerateVisibility(settings);
    if (settings.m_generateThumbTex)
        GenerateThumbTex(settings.m_tileCount);
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

    // Can be a ROOM?
    if ( CanBeRoom(node, area, settings, depth) )
    {
        node->m_type = LevelMapBSPNode::NODE_ROOM;
        node->m_leafNdx = (int)m_leaves.size();
        m_leaves.push_back(node);
        // leaf, no children
    }
    else
    {
        // WALL node (split)
        node->m_type = (LevelMapBSPNode::NodeType)(LevelMapBSPNode::WALL_VERT + (depth % 2)); // each depth alternate wall dir
        uint32_t at = 0;
        if (node->m_type == LevelMapBSPNode::WALL_VERT) // random division plane
        {
            at = area.m_x0 + m_random.Get(0, area.SizeX() - 1);
            node->m_area.m_x0 = node->m_area.m_x1 = at;
        }
        else
        {
            at = area.m_y0 + m_random.Get(0, area.SizeY() - 1);
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

    
    float dice = m_random.Get(1, 100)*0.01f;
    return dice < settings.m_probRoom;
}

void LevelMap::Destroy()
{
    m_root = nullptr;
    m_leaves.clear();
    m_teleports.clear();
    m_portals.clear();
    if (m_thumbTex)
    {
        delete[] m_thumbTex;
        m_thumbTex = nullptr;
    }
}

void LevelMap::GenerateThumbTex(XMUINT2 tcount)
{
    DX::ThrowIfFalse(m_root != nullptr);
    
    if (m_thumbTex)
    {
        delete[] m_thumbTex;
        m_thumbTex = nullptr;
    }

    uint32_t w = tcount.x;
    uint32_t h = tcount.y;
    m_thumbTexSize = tcount;
    m_thumbTex = new uint32_t[w*h];
    //for (int i = w*h - 1; i >= 0; --i) m_thumbTex[i] = 0xff000000;
    ZeroMemory(m_thumbTex, w*h * sizeof(uint32_t));

    // randomize colors
    std::vector<XMFLOAT4> allColors(DX::GetColorCount());
    for (int i = 0; i < DX::GetColorCount(); ++i) allColors[i] = DX::GetColorAt(i);
    std::random_shuffle(allColors.begin(), allColors.end());
    int curColor = 0;

    // rooms
    for (auto node : m_leaves )
    {
        const XMFLOAT4 color = allColors[(curColor++) % DX::GetColorCount()];
        const uint32_t argb = DX::VectorColorToARGB(color);

        for (uint32_t y = node->m_area.m_y0; y <= node->m_area.m_y1; ++y)
            for (uint32_t x = node->m_area.m_x0; x <= node->m_area.m_x1; ++x)
                m_thumbTex[y*w + x] = argb;
    }

    // teleports
    for (const auto& tp : m_teleports)
    {
        const XMFLOAT4 color = allColors[(curColor++) % DX::GetColorCount()];
        const uint32_t argb = DX::VectorColorToARGB(color);
        for (int i = 0; i < 2; ++i)
        {
            m_thumbTex[tp.m_positions[i].y*w + tp.m_positions[i].x] = argb;
        }
    }

    // portals
    for (const auto& p : m_portals)
    {
        XMUINT2 pos = p.GetPortalPosition();
        for (int i = 0; i < 2; ++i)
        {
            m_thumbTex[pos.y*w + pos.x] = 0xff000000;
            switch (p.m_wallNode->m_type)
            {
            case LevelMapBSPNode::WALL_HORIZ: --pos.y; break;
            case LevelMapBSPNode::WALL_VERT: --pos.x; break;
            }
        }
    }

}

struct SpookyAdulthood::VisMatrix
{
    std::vector<bool> matrix;

    VisMatrix(int nLeaves)
    {
        matrix.resize(nLeaves*nLeaves, false);
    }

    inline bool IsVisible( int n, int i, int j) const
    {
        return matrix[n*j + i];
    }

    inline void SetVisible(int n, int i, int j, bool v)
    {
        matrix [n*j + i] = v;
    }
};



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
                VisComputeRandomPortalIndex(roomA->m_area, roomB->m_area, parent->m_type )
            };

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
    return m_random.Get(a, b);
}

void LevelMap::VisGenerateTeleport(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB)
{
    DX::ThrowIfFailed(roomA->IsLeaf() && roomB->IsLeaf());

    LevelMapBSPTeleport tport =
        {
            {roomA, roomB},
            {GetRandomInArea(roomA->m_area), GetRandomInArea(roomB->m_area) }
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

bool operator ==(const XMUINT2& a, const XMUINT2& b)
{
    return a.x == b.x && a.y == b.y;
}

XMUINT2 LevelMap::GetRandomInArea(const LevelMapBSPTileArea& area, bool checkNotInPortal/*=true*/)
{
    XMUINT2 rndPos(m_random.Get(area.m_x0, area.m_x1), m_random.Get(area.m_y0, area.m_y1));
    
    if (checkNotInPortal)
    {
        XMUINT2 ppos, oppos;
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

// random element in a set
template<typename T>
size_t RandomRoomInSet(const T& roomset, RandomProvider& rnd)
{
    T::const_iterator it = roomset.begin();
    std::advance(it, rnd.Get(0, (uint32_t)roomset.size() - 1));
    return *it;
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
    for (size_t i = 1; i < allRoomSets.size(); ++i)
    {
        const RoomSet& a = allRoomSets[i - 1];
        const RoomSet& b = allRoomSets[i];
        
        // gets a random room in both sets
        const size_t roomANdx = RandomRoomInSet(a, m_random);
        const size_t roomBNdx = RandomRoomInSet(b, m_random);

        // todo: don't allow to put two teleports in the same room!
        this->VisGenerateTeleport(m_leaves[roomANdx], m_leaves[roomBNdx]);
    }
}

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

