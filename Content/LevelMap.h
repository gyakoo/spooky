#pragma once
#include <DirectXMath.h>
#include "CollisionAndSolving.h"

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
        inline bool operator ==(const LevelMapBSPTileArea& rhs) const {
            return m_x0 == rhs.m_x0 && m_x1 == rhs.m_x1 && m_y0 == rhs.m_y0 && m_y1 == rhs.m_y1;
        }
        inline bool Contains(const XMUINT2& o) const {
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
        LevelMapBSPNode() : m_type(NODE_UNKNOWN), m_teleportNdx(-1), m_leafNdx(-1), m_tag(0x55555533), m_finished(false){}

        enum NodeType{ NODE_UNKNOWN, NODE_ROOM, NODE_EMPTY, WALL_VERT, WALL_HORIZ };
        enum PortalDir { NONE, NORTH, SOUTH, WEST, EAST };

        inline bool IsLeaf() const { return m_type == NODE_ROOM; }
        inline bool IsWall() const { return m_type == WALL_VERT || m_type == WALL_HORIZ;  }
        void CreateDeviceDependentResources(const LevelMap& lmap, const std::shared_ptr<DX::DeviceResources>& device);
        void ReleaseDeviceDependentResources();
        PortalDir GetPortalDirAt(const LevelMap& lmap, uint32_t x, uint32_t y);
        void GenerateCollisionSegments(const LevelMap& lmap);
        bool IsPillar(const XMUINT2& ppos)const;
        XMFLOAT3 GetRandomXZ(const XMFLOAT2& shrink=XMFLOAT2(0,0)) const;
        inline bool Clearance(const XMUINT2& pos) const { return m_area.Contains(pos) && !IsPillar(pos); };

        LevelMapBSPTileArea m_area;
        NodeType m_type;
        LevelMapBSPNodePtr m_parent;
        LevelMapBSPNodePtr m_children[2];
        NodeDXResourcesPtr m_dx; // only valid when IsLeaf()
        std::shared_ptr<SegmentList> m_collisionSegments;
        std::unique_ptr<std::vector<XMUINT2>> m_pillars;
        int m_teleportNdx;
        int m_leafNdx;
        uint32_t m_tag;
        uint32_t m_profile;
        bool m_finished;
    };

    struct NodeDXResources
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer>		m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
        size_t                                      m_indexCount;
    };

    //* ***************************************************************** *//
    //* LevelMapBSPPortal
    //* ***************************************************************** *//
    struct LevelMapBSPPortal
    {
        LevelMapBSPNodePtr  m_leaves[2];
        LevelMapBSPNodePtr  m_wallNode; // must be WALL_X or WALL_Y
        int m_index;
        bool m_open;

        XMUINT2 GetPortalPosition(XMUINT2* opposite = nullptr) const;
        void GetTransform(XMFLOAT3& pos, float& rotY) const;
        LevelMapBSPNodePtr GetOtherLeaf(LevelMapBSPNodePtr l) const;
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
        XMUINT2 m_minForPillars;    // min dim to consider put pillars
        XMFLOAT2 m_pillarsProbRange; // probabily range for ratio of pillars
        uint32_t m_randomSeed;
        uint32_t m_minRecursiveDepth;
        uint32_t m_maxRecursiveDepth;
        float m_probRoom; // 0..1 probabilities to be a room if other req met
        float m_charRadius;
        bool m_generateThumbTex;
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
        enum RoomProfile
        {
            RP_NORMAL = 0,
            RP_GRAVE,
            RP_WOODS,
            RP_BODYPILES,
            RP_GARGOYLES,
            RP_HANDS,
            RP_SCARYMESSAGES,
            RP_PUMPKINFIELD,
            RP_MAX
        };

		LevelMap(const std::shared_ptr<DX::DeviceResources>& device);
        ~LevelMap() { Destroy(); }
		void Generate(const LevelMapGenerationSettings& settings);
        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Update(const DX::StepTimer& timer, const CameraFirstPerson& camera);
        void Render(const CameraFirstPerson& camera);
        void RenderMinimap(const CameraFirstPerson& camera);
        void GenerateThumbTex(XMUINT2 tcount, const XMUINT2* playerPos=nullptr);
        XMUINT2 GetRandomPosition();
        XMUINT2 ConvertToMapPosition(const XMFLOAT3& xyz) const;
        LevelMapBSPNodePtr GetLeafAt(const XMFLOAT3& pos) const;
        LevelMapBSPNodePtr GetLeafAtIndex(int index) const;
        int GetLeafIndexAt(const XMFLOAT3& pos) const;
        const SegmentList* GetCurrentCollisionSegments(); // return current leaf segments
        const std::vector<LevelMapBSPPortal>& GetPortals()const { return m_portals; }
        const std::vector<LevelMapBSPNodePtr>& GetRooms() const { return m_leaves; }
        void ToggleRoomDoors(int roomIndex=-1, bool open=true);

        bool RaycastDir(const XMFLOAT3& origin, const XMFLOAT3& dir, XMFLOAT3& outHit);
        bool RaycastSeg(const XMFLOAT3& origin, const XMFLOAT3& end, XMFLOAT3& outHit, float optRad=-1.0f, float offsHit=0.0f);

	private:
        friend struct LevelMapBSPNode;
        void Destroy();
        void RecursiveGenerate(LevelMapBSPNodePtr& node, LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, uint32_t depth);
        void GenerateDetailsForRoom(LevelMapBSPNodePtr& node, const LevelMapGenerationSettings& settings);
        void GeneratePillarsForRoom(LevelMapBSPNodePtr& node, const XMUINT2& minForPillars, const XMFLOAT2& probRange);
        void GenerateVisibility(const LevelMapGenerationSettings& settings);
        void GenerateCollisionInfo();
        bool VisRoomAreContiguous(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB);
        void VisGeneratePortal(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB);
        void VisGenerateTeleport(const LevelMapBSPNodePtr& roomA, const LevelMapBSPNodePtr& roomB);
        int VisComputeRandomPortalIndex(const LevelMapBSPTileArea& area1, const LevelMapBSPTileArea& area2, LevelMapBSPNode::NodeType wallDir);
        bool HasNode(const LevelMapBSPNodePtr& node, const LevelMapBSPNodePtr& lookFor);
        void SplitNode(const LevelMapBSPTileArea& area, uint32_t at, LevelMapBSPNode::NodeType wallDir, LevelMapBSPTileArea* outAreas);
        bool CanBeRoom(const LevelMapBSPNodePtr& node, const LevelMapBSPTileArea& area, const LevelMapGenerationSettings& settings, uint32_t depth);
        void GenerateTeleports(const VisMatrix& visMatrix);
        XMUINT2 GetRandomInArea(const LevelMapBSPTileArea& area, bool checkNotInPortal=true);
        bool RenderSetCommonState(const CameraFirstPerson& camera);

    private:
        LevelMapBSPNodePtr m_root;
        std::vector<LevelMapBSPNodePtr> m_leaves;
        std::vector<LevelMapBSPTeleport> m_teleports;
        std::vector<LevelMapBSPPortal> m_portals;
        std::multimap<LevelMapBSPNode*, uint32_t> m_leafPortals; // for a leaf it keeps a list of portal indices
        LevelMapThumbTexture m_thumbTex;        
        XMFLOAT4X4 m_levelTransform;
        LevelMapBSPNodePtr m_cameraCurLeaf;

        // DX resources
        std::shared_ptr<DX::DeviceResources> m_device;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>  m_atlasTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_atlasTextureSRV;
    };
}

