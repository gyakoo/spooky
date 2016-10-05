#pragma once
#include <utility>
#include <DirectXMath.h>
#include <random>

using namespace DirectX;

namespace SpookyAdulthood
{
    struct LevelMapBSPNode;
    typedef std::unique_ptr<LevelMapBSPNode> LevelMapBSPPtr;

    struct LevelMapBSPTileArea
    {
        uint32_t m_x0, m_x1;
        uint32_t m_y0, m_y1;

        uint32_t SizeX() const { return m_x1 - m_x0 + 1; }
        uint32_t SizeY() const { return m_y1 - m_y0 + 1; }
    };

    struct LevelMapBSPNode 
    {
        enum NodeType{ NODE_ROOM, NODE_BLOCK, WALL_X, WALL_Y };

        NodeType m_type;
        LevelMapBSPTileArea m_area;
        LevelMapBSPPtr m_children[2];
    };

    struct LevelMapGenerationSettings
    {
        LevelMapGenerationSettings();        
        void Validate() const;

        XMFLOAT2 m_tileSize;        // in meters        
        XMUINT2 m_tileCount;        // no of tiles in map
        XMUINT2 m_minTileCount;     // minimum no in tiles for rooms
        uint32_t m_randomSeed;
        uint32_t m_minRecursiveDepth;
        float m_charRadius;
    };

    class RandomProvider
    {
    public:
        RandomProvider();
        void SetSeed(uint32_t seed);
        uint32_t Get(uint32_t minN, uint32_t maxN);

    protected:
        uint32_t m_seed;
    };

	// This sample renderer instantiates a basic rendering pipeline.
	class LevelMap
	{
	public:
		LevelMap();
		void Generate(const LevelMapGenerationSettings& settings);


	private:
        void Destroy();
        void RecursiveGenerate(LevelMapBSPPtr& node, const LevelMapGenerationSettings& settings, LevelMapBSPTileArea& area, int depth);
        
        RandomProvider m_random;
        std::unique_ptr<LevelMapBSPNode> m_root;		
	};
}

