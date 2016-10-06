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
    LevelMapBSPTileArea area = { 0, settings.m_tileCount.x-1, 0, settings.m_tileCount.y-1 };
    m_random.SetSeed(settings.m_randomSeed);
    LevelMapBSPNodePtr lastRoom, lastBlock;
    RecursiveGenerate(m_root, area, settings, lastRoom, lastBlock, 0);
    m_firstRoom = FindFirstNodeType(LevelMapBSPNode::NODE_ROOM, m_root);
    m_firstBlock = FindFirstNodeType(LevelMapBSPNode::NODE_BLOCK,m_root);

    if (settings.m_generateThumbTex)
    {
        GenerateThumbTex(settings.m_tileCount);
    }
}

void LevelMap::RecursiveGenerate(LevelMapBSPNodePtr& node, LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings
    , LevelMapBSPNodePtr& lastRoom, LevelMapBSPNodePtr& lastBlock, uint32_t depth)
{
    // It's a BLOCK? any dimension is not large enough to be a room
    if (area.SizeX() < settings.m_minTileCount.x ||
        area.SizeY() < settings.m_minTileCount.y)
    {
        node->m_type = LevelMapBSPNode::NODE_BLOCK;
        node->m_area = area;
        node->m_sibling = nullptr;
        if (lastBlock)
            lastBlock->m_sibling = node;
        lastBlock = node;
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
        // leaf, no children
    }
    else
    {
        // WALL node (split)
        node->m_type = (LevelMapBSPNode::NodeType)(LevelMapBSPNode::WALL_X + (depth % 2)); // each depth alternate wall dir
        uint32_t at = (node->m_type == LevelMapBSPNode::WALL_X) // random division plane
            ? area.m_x0 + m_random.Get(0, area.SizeX() - 1)
            : area.m_y0 + m_random.Get(0, area.SizeY() - 1);
        if (at == 0) at = 1;
        // get the two sub-areas and subdivide them recursively
        LevelMapBSPTileArea newAreas[2];
        SplitNode(area, at, node->m_type, newAreas);
        for (int i = 0; i < 2; ++i)
        {
            node->m_children[i] = std::make_shared<LevelMapBSPNode>();
            RecursiveGenerate(node->m_children[i], newAreas[i], settings, lastRoom, lastBlock, depth + 1);            
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

LevelMapBSPNodePtr LevelMap::FindFirstNodeType(LevelMapBSPNode::NodeType ntype, const LevelMapBSPNodePtr node)
{
    if (node)
    {
        if (node->m_type == ntype)
            return node;

        if (node->m_children[0])
        {
            LevelMapBSPNodePtr first = FindFirstNodeType(ntype, node->m_children[0]);
            return (first != nullptr)
                ? first
                : FindFirstNodeType(ntype, node->m_children[1]);
        }
    }
    return nullptr;
}

void LevelMap::Destroy()
{
    m_root = nullptr;
    m_firstRoom = nullptr;
    m_firstBlock = nullptr;
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
    ZeroMemory(m_thumbTex, w*h * sizeof(uint32_t));

    // rooms
    LevelMapBSPNodePtr node = m_firstRoom;
    int count = 0;
    bool doingBlocks = false;
    while (node)
    {
        ++count;
        uint32_t argb;
        if (!doingBlocks)
        {
            XMFLOAT4 color = DX::GetColorAt(m_random.Get(0, DX::GetColorCount() - 1));
            argb = DX::VectorColorToARGB(color);
        }
        else
        {
            argb = 0xff00ffff;
        }

        for (uint32_t y = node->m_area.m_y0; y <= node->m_area.m_y1; ++y)
            for (uint32_t x = node->m_area.m_x0; x <= node->m_area.m_x1; ++x)
                m_thumbTex[y*w + x] = argb;
        node = node->m_sibling;
        if ( !node && !doingBlocks )
        { 
            doingBlocks = true;
            node = m_firstBlock;
        }
    }
    count = count;

    
}