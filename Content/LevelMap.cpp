#include "pch.h"
#include "LevelMap.h"
#include <../Common/DirectXHelper.h>

using namespace SpookyAdulthood;
using namespace DX;

static const uint32_t RANDOM_DEFAULT_SEED = 997;

LevelMapGenerationSettings::LevelMapGenerationSettings()
    : m_randomSeed(RANDOM_DEFAULT_SEED), m_tileSize(1.0f, 1.0f)
    , m_tileCount(20, 20), m_minTileCount(4, 4)
    , m_charRadius(0.25f), m_minRecursiveDepth(3)
    , m_probRoom(0.2f)
{

}

RandomProvider::RandomProvider()
    : m_seed(RANDOM_DEFAULT_SEED)
{
}

void RandomProvider::SetSeed(uint32_t seed)
{
    m_seed = seed;
}

uint32_t RandomProvider::Get(uint32_t minN, uint32_t maxN)
{
    // slow?
    std::mt19937 gen(m_seed);
    return std::uniform_int_distribution<uint32_t>(minN, maxN)(gen);
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
    : m_root(nullptr)
{

}

void LevelMap::Generate(const LevelMapGenerationSettings& settings)
{
    settings.Validate();

    Destroy();

    m_root = std::make_shared<LevelMapBSPNode>();
    LevelMapBSPTileArea area = { 0, settings.m_tileCount.x-1, 0, settings.m_tileCount.y-1 };
    m_random.SetSeed(settings.m_randomSeed);
    LevelMapBSPNodePtr lastRoom;
    RecursiveGenerate(m_root, area, settings, lastRoom, 0);
    m_firstRoom = FindFirstLeaf(m_root);
}

void LevelMap::RecursiveGenerate(LevelMapBSPNodePtr& node, LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, LevelMapBSPNodePtr& lastLeaf, int depth)
{
    // It's a BLOCK? any dimension is not large enough to be a room
    if (area.SizeX() < settings.m_minTileCount.x ||
        area.SizeY() < settings.m_minTileCount.y)
    {
        node->m_type = LevelMapBSPNode::NODE_BLOCK;
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
        if (lastLeaf)
            lastLeaf->m_sibling = node;
        lastLeaf = node;
        // leaf, no children
    }
    else
    {
        // WALL node (split)
        node->m_type = (LevelMapBSPNode::NodeType)(LevelMapBSPNode::WALL_X + (depth % 2)); // each depth alternate wall dir
        uint32_t at = (node->m_type == LevelMapBSPNode::WALL_X) // random division plane
            ? area.m_x0 + m_random.Get(0, area.SizeX() - 1)
            : area.m_y0 + m_random.Get(0, area.SizeY() - 1);

        // get the two sub-areas and subdivide them recursively
        LevelMapBSPTileArea newAreas[2];
        SplitNode(area, at, node->m_type, newAreas);
        for (int i = 0; i < 2; ++i)
        {
            node->m_children[i] = std::make_shared<LevelMapBSPNode>();
            RecursiveGenerate(node->m_children[i], newAreas[i], settings, lastLeaf, depth + 1);            
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

bool LevelMap::CanBeRoom(const LevelMapBSPNodePtr& node, const LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, int depth)
{
    // not there yet
    if (depth < settings.m_minRecursiveDepth) 
        return false;

    // minimum enough to be a room
    if (area.SizeX() == settings.m_minTileCount.x || area.SizeY() == settings.m_minTileCount.y)
        return true;

    return m_random.Get(1, 100)*0.01f <= settings.m_probRoom;
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
    m_firstRoom = nullptr;
}