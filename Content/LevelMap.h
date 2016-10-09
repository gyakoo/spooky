#pragma once
#include <DirectXMath.h>
#include <random>
#include <../Common/FPSCDAndSolving.h>

using namespace DirectX;

namespace DX { class DeviceResources; }

namespace SpookyAdulthood
{
    struct LevelMapBSPNode;
    struct NodeDXResources;
    struct VisMatrix;
    struct CameraFirstPerson;
    class LevelMap;
    
    

    typedef std::shared_ptr<LevelMapBSPNode> LevelMapBSPNodePtr;
    typedef std::shared_ptr<NodeDXResources> NodeDXResourcesPtr;

    //* ***************************************************************** *//
    //* LevelMapBSPTileArea
    //* ***************************************************************** *//
    struct LevelMapBSPTileArea
    {
        LevelMapBSPTileArea(uint32_t x0=0, uint32_t x1=0, uint32_t y0=0, uint32_t y1=0) 
            : m_x0(x0), m_x1(x1), m_y0(y0), m_y1(y1) 
        {}
        uint32_t SizeX() const { return m_x1 - m_x0 + 1; }
        uint32_t SizeY() const { return m_y1 - m_y0 + 1; }
        uint32_t CountTiles() const { return SizeX() * SizeY(); }
        bool operator ==(const LevelMapBSPTileArea& rhs) const {
            return m_x0 == rhs.m_x0 && m_x1 == rhs.m_x1 && m_y0 == rhs.m_y0 && m_y1 == rhs.m_y1;
        }
        bool Contains(const XMUINT2& o) {
            return o.x >= m_x0 && o.x <= m_x1 && o.y >= m_y0 && o.y <= m_y1;
        }

        uint32_t m_x0, m_x1;
        uint32_t m_y0, m_y1;
    };

    //* ***************************************************************** *//
    //* LevelMapBSPNode
    //* ***************************************************************** *//
    struct LevelMapBSPNode
    {
        LevelMapBSPNode() : m_type(NODE_UNKNOWN), m_teleportNdx(-1), m_leafNdx(-1), m_tag(0xffffffff){}

        enum NodeType{ NODE_UNKNOWN, NODE_ROOM, NODE_EMPTY, WALL_VERT, WALL_HORIZ };
        enum PortalDir { NONE, NORTH, SOUTH, WEST, EAST };

        bool IsLeaf() const { return m_type == NODE_ROOM; }
        bool IsWall() const { return m_type == WALL_VERT || m_type == WALL_HORIZ;  }
        void CreateDeviceDependentResources(const LevelMap& lmap, const std::shared_ptr<DX::DeviceResources>& device);
        void ReleaseDeviceDependentResources();
        PortalDir GetPortalDirAt(const LevelMap& lmap, uint32_t x, uint32_t y);
        void ComputeCollisionSegments();

        LevelMapBSPTileArea m_area;
        NodeType m_type;
        LevelMapBSPNodePtr m_parent;
        LevelMapBSPNodePtr m_children[2];
        NodeDXResourcesPtr m_dx; // only valid when IsLeaf()
        std::shared_ptr<SegmentList> m_collisionSegments;
        int m_teleportNdx;
        int m_leafNdx;
        uint32_t m_tag;
    };

    struct NodeDXResources
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer>		m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
        size_t                                      m_indexCount;
    };

    //* ***************************************************************** *//
    //* MapDXResources
    //* ***************************************************************** *//
    struct MapDXResources // common
    {
        void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& device);
        void ReleaseDeviceDependentResources();

        Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer>		m_constantBuffer;

    };

    //* ***************************************************************** *//
    //* LevelMapBSPPortal
    //* ***************************************************************** *//
    struct LevelMapBSPPortal
    {
        LevelMapBSPNodePtr  m_leaves[2];
        LevelMapBSPNodePtr  m_wallNode; // must be WALL_X or WALL_Y

        XMUINT2 GetPortalPosition(XMUINT2* opposite=nullptr) const;

        int m_index;
    };

    //* ***************************************************************** *//
    //* LevelMapBSPTeleport
    //* ***************************************************************** *//
    struct LevelMapBSPTeleport
    {
        LevelMapBSPNodePtr m_leaves[2];
        XMUINT2 m_positions[2];
    };

    //* ***************************************************************** *//
    //* LevelMapGenerationSettings
    //* ***************************************************************** *//
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

    //* ***************************************************************** *//
    //* RandomProvider
    //* ***************************************************************** *//
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

    //* ***************************************************************** *//
    //* LevelMapThumbTexture
    //* ***************************************************************** *//
    struct LevelMapThumbTexture
    {
        LevelMapThumbTexture() : m_sysMem(nullptr), m_dim(0,0) {}
        ~LevelMapThumbTexture() { Destroy(); }
        void Destroy();
        void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& device);
        void ReleaseDeviceDependentResources();
        void SetAt(uint32_t x, uint32_t y, uint32_t value);

        uint32_t* m_sysMem;
        XMUINT2 m_dim;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>  m_texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureView;
    };

    //* ***************************************************************** *//
    //* LevelMap
    //* Well, BSP generation and LevelMap are highly coupled :(
    //* ***************************************************************** *//
	class LevelMap
	{
	public:
        enum ThumbMapRender{ THUMBMAP_NONE, THUMBMAP_FIXED, THUMBMAP_ORIENTATED, THUMBMAP_MAX};
		LevelMap(const std::shared_ptr<DX::DeviceResources>& device);
        ~LevelMap() { Destroy(); }
		void Generate(const LevelMapGenerationSettings& settings);
        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Update(const DX::StepTimer& timer, const CameraFirstPerson& camera);
        void Render(const CameraFirstPerson& camera);
        void GenerateThumbTex(XMUINT2 tcount, const XMUINT2* playerPos=nullptr);
        XMUINT2 GetRandomPosition();
        XMUINT2 ConvertToMapPosition(const XMFLOAT3& xyz) const;
        void SetThumbMapRender(ThumbMapRender tmr) { m_thumbTexRender = tmr; }
        ThumbMapRender GetThumbMapRender() { return m_thumbTexRender; }
        LevelMapBSPNodePtr GetLeafAt(const XMFLOAT3& pos);
        const SegmentList* GetCurrentCollisionSegments(); // return current leaf segments

	private:
        friend struct LevelMapBSPNode;
        void Destroy();
        void RecursiveGenerate(LevelMapBSPNodePtr& node, LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, uint32_t depth);
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
        void RenderSetCommonState(const CameraFirstPerson& camera);

    private:
        RandomProvider m_random;
        LevelMapBSPNodePtr m_root;
        std::vector<LevelMapBSPNodePtr> m_leaves;
        std::vector<LevelMapBSPTeleport> m_teleports;
        std::vector<LevelMapBSPPortal> m_portals;
        std::multimap<LevelMapBSPNode*, uint32_t> m_leavePortals;
        LevelMapThumbTexture m_thumbTex;        
        ThumbMapRender m_thumbTexRender;
        XMFLOAT4X4 m_levelTransform;
        LevelMapBSPNodePtr m_cameraCurLeaf;

        // DX resources
        std::shared_ptr<DX::DeviceResources> m_device;
        std::unique_ptr<MapDXResources> m_dxCommon;
	};
}

