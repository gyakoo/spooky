#pragma once
#include <utility>
#include <DirectXMath.h>
#include <random>

using namespace DirectX;

namespace SpookyAdulthood
{
    struct LevelMapBSPNode;
    typedef std::shared_ptr<LevelMapBSPNode> LevelMapBSPNodePtr;

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

        bool IsLeaf() const { return m_type == NODE_ROOM; }

        NodeType m_type;
        LevelMapBSPTileArea m_area;
        LevelMapBSPNodePtr m_children[2];
        LevelMapBSPNodePtr m_sibling;
    };

    struct LevelMapGenerationSettings
    {
        LevelMapGenerationSettings();        
        void Validate() const;

        XMFLOAT2 m_tileSize;        // in meters        
        XMUINT2 m_tileCount;        // no of tiles in map
        XMUINT2 m_minTileCount;     // minimum no in tiles for rooms
        XMUINT2 m_maxTileCount;     // max no in tiles for rooms
        uint32_t m_randomSeed;
        uint32_t m_minRecursiveDepth;
        uint32_t m_maxRecursiveDepth;
        float m_probRoom; // 0..1 probabilities to be a room if other req met
        float m_charRadius;
        bool m_generateThumbTex;
    };

    class RandomProvider
    {
    public:
        RandomProvider() :m_lastSeed(0) {}

        void SetSeed(uint32_t seed);
        uint32_t Get(uint32_t minN, uint32_t maxN);

    protected:
        std::unique_ptr<std::mt19937> m_gen;
        uint32_t m_lastSeed, m_seed;
    };

	// This sample renderer instantiates a basic rendering pipeline.
	class LevelMap
	{
	public:
		LevelMap();
        ~LevelMap() { Destroy(); }
		void Generate(const LevelMapGenerationSettings& settings);
        uint32_t* GetThumbTexPtr(XMUINT2* size) const { *size = m_thumbTexSize;  return m_thumbTex; }

	private:
        void Destroy();
        void RecursiveGenerate(LevelMapBSPNodePtr& node, LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, LevelMapBSPNodePtr& lastLeaf, LevelMapBSPNodePtr& lastBlock, uint32_t depth);
        void GenerateThumbTex(XMUINT2 tcount);
        LevelMapBSPNodePtr FindFirstNodeType(LevelMapBSPNode::NodeType ntype, const LevelMapBSPNodePtr node);
        void SplitNode(const LevelMapBSPTileArea& area, uint32_t at, LevelMapBSPNode::NodeType wallDir, LevelMapBSPTileArea* outAreas);
        bool CanBeRoom(const LevelMapBSPNodePtr& node, const LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, uint32_t depth);

        RandomProvider m_random;
        LevelMapBSPNodePtr m_root;		
        LevelMapBSPNodePtr m_firstRoom, m_firstBlock;
        uint32_t* m_thumbTex;
        XMUINT2 m_thumbTexSize;
	};
}

