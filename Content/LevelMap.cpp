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

    m_root = std::make_unique<LevelMapBSPNode>();
    LevelMapBSPTileArea area = { 0, settings.m_tileCount.x-1, 0, settings.m_tileCount.y-1 };
    m_random.SetSeed(settings.m_randomSeed);
    RecursiveGenerate(m_root, settings, area, 0);
}

void LevelMap::RecursiveGenerate(LevelMapBSPPtr& node, const LevelMapGenerationSettings& settings, LevelMapBSPTileArea& area, int depth)
{
    // It's a BLOCK, any dimension is not large enough to be a room
    if (area.SizeX() < settings.m_minTileCount.x ||
        area.SizeY() < settings.m_minTileCount.y)
    {
        node->m_type = LevelMapBSPNode::NODE_BLOCK;
        node->m_area = area;
        return;
    }

    // Can be a ROOM?
    if (depth >= settings.m_minRecursiveDepth)
    {

    }
    else
    {
        // WALL node (split)
        const LevelMapBSPNode::NodeType curWall = (LevelMapBSPNode::NodeType)(LevelMapBSPNode::WALL_X + (depth % 2)); // each depth alternate wall dir
        uint32_t at = (curWall == LevelMapBSPNode::WALL_X)
            ? area.m_x0 + m_random.Get(0, area.SizeX() - 1)
            : area.m_y0 + m_random.Get(0, area.SizeY() - 1);

        LevelMapBSPTileArea newAreas[2];
        SplitNode(area, at, curWall, newAreas);
    }

}

void LevelMap::Destroy()
{
    m_root = nullptr;
}