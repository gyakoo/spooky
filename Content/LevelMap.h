#pragma once
#include <utility>
#include <DirectXMath.h>
#include <random>

using namespace DirectX;

namespace SpookyAdulthood
{
    struct LevelMapBSPNode;
    struct VisMatrix;

    typedef std::shared_ptr<LevelMapBSPNode> LevelMapBSPNodePtr;

    struct LevelMapBSPTileArea
    {
        LevelMapBSPTileArea(uint32_t x0=0, uint32_t x1=0, uint32_t y0=0, uint32_t y1=0) 
            : m_x0(x0), m_x1(x1), m_y0(y0), m_y1(y1) 
        {}
        uint32_t SizeX() const { return m_x1 - m_x0 + 1; }
        uint32_t SizeY() const { return m_y1 - m_y0 + 1; }
        bool operator ==(const LevelMapBSPTileArea& rhs) const {
            return m_x0 == rhs.m_x0 && m_x1 == rhs.m_x1 && m_y0 == rhs.m_y0 && m_y1 == rhs.m_y1;
        }

        uint32_t m_x0, m_x1;
        uint32_t m_y0, m_y1;
    };

    struct LevelMapBSPNode 
    {
        LevelMapBSPNode() : m_type(NODE_UNKNOWN), m_teleportNdx(-1), m_leafNdx(-1) {}

        enum NodeType{ NODE_UNKNOWN, NODE_ROOM, NODE_EMPTY, WALL_VERT, WALL_HORIZ };

        bool IsLeaf() const { return m_type == NODE_ROOM; }
        bool IsWall() const { return m_type == WALL_VERT || m_type == WALL_HORIZ;  }

        LevelMapBSPTileArea m_area;
        NodeType m_type;
        LevelMapBSPNodePtr m_parent;
        LevelMapBSPNodePtr m_children[2];
        int m_teleportNdx;
        int m_leafNdx;
    };

    struct LevelMapBSPPortal
    {
        LevelMapBSPNodePtr  m_leaves[2];
        LevelMapBSPNodePtr  m_wallNode; // must be WALL_X or WALL_Y

        XMUINT2 GetPortalPosition(XMUINT2* opposite=nullptr) const;

        int m_index;
    };

    struct LevelMapBSPTeleport
    {
        LevelMapBSPNodePtr m_leaves[2];
        XMUINT2 m_positions[2];
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

	// Well, BSP generation and LevelMap are highly coupled...improve it!
	class LevelMap
	{
	public:
		LevelMap();
        ~LevelMap() { Destroy(); }
		void Generate(const LevelMapGenerationSettings& settings);
        uint32_t* GetThumbTexPtr(XMUINT2* size) const { *size = m_thumbTexSize;  return m_thumbTex; }

	private:
        void Destroy();
        void RecursiveGenerate(LevelMapBSPNodePtr& node, LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, uint32_t depth);
        void GenerateThumbTex(XMUINT2 tcount);
        void GenerateVisibility(const LevelMapGenerationSettings& settings);
        bool VisRoomAreContiguous(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB);
        void VisGeneratePortal(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB);
        void VisGenerateTeleport(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB);
        int VisComputeRandomPortalIndex(const LevelMapBSPTileArea& area1, const LevelMapBSPTileArea& area2, LevelMapBSPNode::NodeType wallDir);
        bool HasNode(const LevelMapBSPNodePtr& node, const LevelMapBSPNodePtr& lookFor);
        void SplitNode(const LevelMapBSPTileArea& area, uint32_t at, LevelMapBSPNode::NodeType wallDir, LevelMapBSPTileArea* outAreas);
        bool CanBeRoom(const LevelMapBSPNodePtr& node, const LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, uint32_t depth);
        void GenerateTeleports(const VisMatrix& visMatrix);
        XMUINT2 GetRandomInArea(const LevelMapBSPTileArea& area, bool checkNotInPortal=true);

        RandomProvider m_random;
        LevelMapBSPNodePtr m_root;
        std::vector<LevelMapBSPNodePtr> m_leaves;
        std::vector<LevelMapBSPTeleport> m_teleports;
        std::vector<LevelMapBSPPortal> m_portals;
        uint32_t* m_thumbTex;
        XMUINT2 m_thumbTexSize;
	};
}

