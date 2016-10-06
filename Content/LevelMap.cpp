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
    : m_root(nullptr), m_thumbTex(nullptr), m_nLeaves(0)
{

}

void LevelMap::Generate(const LevelMapGenerationSettings& settings)
{
    settings.Validate();

    Destroy();

    m_root = std::make_shared<LevelMapBSPNode>();
    m_root->m_parent = nullptr;
    LevelMapBSPTileArea area = { 0, settings.m_tileCount.x-1, 0, settings.m_tileCount.y-1 };
    m_random.SetSeed(settings.m_randomSeed);
    LevelMapBSPNodePtr lastRoom;
    RecursiveGenerate(m_root, area, settings, lastRoom, 0);
    m_firstLeaf = FindFirstLeaf(m_root);
    if (settings.m_generateThumbTex)
    {
        GenerateThumbTex(settings.m_tileCount);
    }
    GenerateVisibility();
}

void LevelMap::RecursiveGenerate(LevelMapBSPNodePtr& node, LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings
    , LevelMapBSPNodePtr& lastRoom, uint32_t depth)
{
    // It's an EMPTY? any dimension is not large enough to be a room
    if (area.SizeX() < settings.m_minTileCount.x ||
        area.SizeY() < settings.m_minTileCount.y)
    {
        node->m_type = LevelMapBSPNode::NODE_EMPTY;
        node->m_area = area;
        node->m_sibling = nullptr;
        // leaf, no children
        return;
    }

    // Can be a ROOM?
    if ( CanBeRoom(node, area, settings, depth) )
    {
        node->m_type = LevelMapBSPNode::NODE_ROOM;
        node->m_area = area;
        node->m_sibling = nullptr;
        if (lastRoom)
            lastRoom->m_sibling = node;
        lastRoom = node;
        ++m_nLeaves;
        // leaf, no children
    }
    else
    {
        // WALL node (split)
        node->m_type = (LevelMapBSPNode::NodeType)(LevelMapBSPNode::WALL_X + (depth % 2)); // each depth alternate wall dir
        uint32_t at = (node->m_type == LevelMapBSPNode::WALL_X) // random division plane
            ? area.m_x0 + m_random.Get(0, area.SizeX() - 1)
            : area.m_y0 + m_random.Get(0, area.SizeY() - 1);
        //if (at == 0) at = 1;
        // get the two sub-areas and subdivide them recursively
        LevelMapBSPTileArea newAreas[2];
        SplitNode(area, at, node->m_type, newAreas);
        for (int i = 0; i < 2; ++i)
        {
            node->m_children[i] = std::make_shared<LevelMapBSPNode>();
            node->m_children[i]->m_parent = node;
            RecursiveGenerate(node->m_children[i], newAreas[i], settings, lastRoom, depth + 1);            
        }
    }
}

void LevelMap::SplitNode(const LevelMapBSPTileArea& area, uint32_t at, LevelMapBSPNode::NodeType wallDir, LevelMapBSPTileArea* outAreas)
{
    DX::ThrowIfFalse(wallDir == LevelMapBSPNode::WALL_X || wallDir == LevelMapBSPNode::WALL_Y);
    outAreas[0] = outAreas[1] = area;
    switch (wallDir)
    {
    case LevelMapBSPNode::WALL_X:
        DX::ThrowIfFalse(at <= area.m_x1);
        outAreas[0].m_x0 = area.m_x0; outAreas[0].m_x1 = at - 1;
        outAreas[1].m_x0 = at; outAreas[1].m_x1 = area.m_x1;
        break;

    case LevelMapBSPNode::WALL_Y:
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

LevelMapBSPNodePtr LevelMap::FindFirstLeaf(const LevelMapBSPNodePtr node)
{
    if (node)
    {
        if (node->IsLeaf())
            return node;

        if (node->m_children[0])
        {
            LevelMapBSPNodePtr first = FindFirstLeaf(node->m_children[0]);
            return (first != nullptr)
                ? first
                : FindFirstLeaf(node->m_children[1]);
        }
    }
    return nullptr;
}

void LevelMap::Destroy()
{
    m_root = nullptr;
    m_firstLeaf = nullptr;
    m_nLeaves = 0;
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
    for (int i = w*h - 1; i >= 0; --i) m_thumbTex[i] = 0xff000000;
    //ZeroMemory(m_thumbTex, w*h * sizeof(uint32_t));

    // rooms
    LevelMapBSPNodePtr node = m_firstLeaf;
    int count = 0;
    while (node)
    {
        ++count;
        uint32_t argb;
        if (node->m_type == LevelMapBSPNode::NODE_ROOM)
        {
            XMFLOAT4 color = DX::GetColorAt(m_random.Get(0, DX::GetColorCount() - 1));
            argb = DX::VectorColorToARGB(color);
        }

        for (uint32_t y = node->m_area.m_y0; y <= node->m_area.m_y1; ++y)
            for (uint32_t x = node->m_area.m_x0; x <= node->m_area.m_x1; ++x)
                m_thumbTex[y*w + x] = argb;
        node = node->m_sibling;        
    }
    count = count;
}

// I know, I know...
void LevelMap::GenerateVisibility()
{
    DX::ThrowIfFalse(m_nLeaves > 0);
    
    // put leaves in an array
    std::vector<LevelMapBSPNodePtr> leaves;
    leaves.reserve(m_nLeaves);
    LevelMapBSPNodePtr node = m_firstLeaf;
    while (node) { leaves.push_back(node);  node = node->m_sibling; }

    // for each leaf (room) see if any of the processed connects with me
    LevelMapBSPNodePtr curRoom, otherRoom;
    for (int i = 1; i < m_nLeaves; ++i)
    {
        curRoom = leaves[i];
        bool portalGenerated = false;
        for (int j = i-1; j >= 0; --j)
        {
            otherRoom = leaves[j];
            if (VisRoomAreContiguous(otherRoom, curRoom))
            {
                // are contiguous, generate portal
                VisGeneratePortal(curRoom, otherRoom);
                portalGenerated = true;
                break;
            }            
        }

        // we found a room that's not connected/contiguous to any other
        if (!portalGenerated)
        {
            // generate tele-port instead
            VisGenerateTeleport(curRoom, leaves[i - 1]);
        }
    }
}

bool LevelMap::VisRoomAreContiguous(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB)
{
    // early exit, shortcut if are siblings
    if (roomA->m_parent == roomB->m_parent)
        return true;

    int diffX = 0;
    if (roomA->m_area.m_x0 <= roomB->m_area.m_x0)
        diffX = roomB->m_area.m_x0 - roomA->m_area.m_x1;
    else
        diffX = roomA->m_area.m_x0 - roomB->m_area.m_x1;

    int diffY = 0;
    if (roomA->m_area.m_y0 <= roomB->m_area.m_y0)
        diffY = roomB->m_area.m_y0 - roomA->m_area.m_y1;
    else
        diffY = roomA->m_area.m_y0 - roomB->m_area.m_y1;

    return (diffX+diffY) <= 1;
}

void LevelMap::VisGenerateTeleport(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB)
{

}

void LevelMap::VisGeneratePortal(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB)
{

}
