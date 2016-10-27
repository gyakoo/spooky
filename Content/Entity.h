﻿#pragma once
#include <DirectXMath.h>

using namespace DirectX;
namespace DX { class StepTimer;  class DeviceResources; }

namespace SpookyAdulthood
{
    struct CameraFirstPerson;
    class SpriteManager;
    struct LevelMapBSPNode;
    struct EntityProjectile;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    class Entity
    {
    public:
        enum Flags
        { 
            NONE=0, 
            SPRITE2D=1<<0, 
            SPRITE3D=1<<1, 
            ANIMATION2D=1<<2,
            ANIMATION3D=1<<3,
            MODEL3D= 1<<4,

            ACCEPT_RAYCAST=1<<5,
            COLLIDE=1<<6,

            INVALID=1<<7,
            INACTIVE=1<<8
        };

        enum InvReason
        {
            NONE_i,
            KILLED
        };

        enum RenderPass { PASS_SPRITE3D, PASS_SPRITE2D, PASS_3D };
    public:
        Entity(int flags=NONE);        

        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite);
        virtual void DoHit() {}

        void PerformHit();
        float GetBoundingRadius() const;
        bool SupportPass(RenderPass pass) const;        
        bool SupportRaycast() const;
        bool IsActive() const;
        bool IsValid() const;
        void Invalidate(InvReason reason=NONE_i);

        friend class EntityManager;
        XMFLOAT3 m_pos;
        XMFLOAT2 m_size;
        XMFLOAT4 m_modulate;
        float m_rotation;
        int m_spriteIndex;
        int m_flags;
        float m_timeOut;
        float m_totalTime;
        float m_life;
        uint32_t m_lastFrameHit;
        InvReason m_invalidateReason;
        bool m_constraintY;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    class EntityManager
    {
    public:
        EntityManager(const std::shared_ptr<DX::DeviceResources>& device);

        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();

        void AddEntity(const std::shared_ptr<Entity>& entity);
        void AddEntity(const std::shared_ptr<Entity>& entity, float timeout);
        void Clear();
        void Update(const DX::StepTimer& stepTimer, const CameraFirstPerson& camera);
        void RenderSprites3D(const CameraFirstPerson& camera);
        void RenderSprites2D(const CameraFirstPerson& camera);
        void RenderModel3D(const CameraFirstPerson& camera);
        bool RaycastDir(const XMFLOAT3& origin, const XMFLOAT3& dir, XMFLOAT3& outHit, uint32_t* sprNdx=nullptr);
        bool RaycastSeg(const XMFLOAT3& origin, const XMFLOAT3& end, XMFLOAT3& outHit, float optRad=-1.0f, uint32_t* sprNdx=nullptr);
        void DoHitOnEntity(uint32_t ndx);
        Entity& GetEntity(uint32_t ndx) { return *m_entities[ndx]; }

        static EntityManager* s_instance;
        std::shared_ptr<DX::DeviceResources> m_device;
    protected:
        bool RaycastEntity(const Entity& e, const XMFLOAT3& raypos, const XMFLOAT3& dir, XMFLOAT3& outhit, float& frac);

        friend class Entity;
        typedef std::vector<std::shared_ptr<Entity>> EntitiesCollection;
        EntitiesCollection m_entities;
        EntitiesCollection m_entitiesToAdd;
        bool m_duringUpdate;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EntityGun : public Entity
    {
        enum { PUMPKIN=0, CANDIES, CANNON, FLASHLIGHT, SPRITES_MAX };
        EntityGun();
        virtual void Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite);        

        int m_spriteIndices[SPRITES_MAX];
        int m_animIndex;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EntityProjectile : public Entity
    {
        EntityProjectile(const XMFLOAT3& pos, int spriteNdx, float speed, const XMFLOAT3& dir=XMFLOAT3(0,0,0), bool receiveHit=false);

        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void DoHit();
        
        XMFLOAT3 m_dir;
        float m_speed;
        float m_waitToCheck;
        bool m_firstTime;
        bool m_collidePlayer;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EntityShootHit : public Entity
    {
        EntityShootHit(const XMFLOAT3& pos);

        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        bool m_lastFrame;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EntityTeleport : public Entity
    {
        EntityTeleport(const XMFLOAT3& pos);
        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        int m_dir;
    };

    struct EntityAnimation : public Entity
    {
        EntityAnimation(const XMFLOAT3& pos, const XMFLOAT2& size, const std::vector<int>& indices, float fps, 
            bool finishOnEnd=true, const std::vector<XMFLOAT2>* sizes=nullptr);
        virtual void Update(float stepTime, const CameraFirstPerson& camera);

        std::vector<int> m_indices;
        std::unique_ptr<std::vector<XMFLOAT2>> m_sizes;
        bool m_finishOnEnd;
        int m_curIndex;
        float m_period;
    };
    
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // ///////// ENEMIES ///////// ENEMIES ///////// ENEMIES ///////// ENEMIES ///////// ENEMIES ///////// ENEMIES ////// ENEMIES
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EntityEnemyBase : public Entity
    {
        EntityEnemyBase();

    protected:
        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        EntityProjectile* ShootToPlayer(int projSprIndex, float speed, const XMFLOAT3& offs, const XMFLOAT2& size, float life=-1.0f, bool predict=false, float waitTime=-1.0f);
        LevelMapBSPNode* GetCurrentRoom();
        float DistSqToPlayer(XMFLOAT3* dir=nullptr);
        bool CanSeePlayer();
        void ModulateToColor(const XMFLOAT4& color, float duration);
        bool PlayerLookintAtMe(float range);

        XMFLOAT4 m_modulateTargetColor;
        float m_modulateTime;
        float m_modulateDuration;
    };


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EnemyPuky : public EntityEnemyBase
    {
        EnemyPuky();
        EnemyPuky(const XMFLOAT3& pos);

        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void DoHit();
        void GetNextTarget();
        void Init(const XMFLOAT3& pos);
        bool OutOfBounds();

        XMFLOAT3 m_nextTarget;
        XMFLOAT3 m_targetDir;
        XMFLOAT2 m_bounds[2];
        float m_speed, m_amplitude;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EnemyPumpkin : public EntityEnemyBase
    {
        const float radiusOuterSq = 1.5f*1.5f;
        const float radiusInnerSq = 0.8f*0.8f;
        const float maxTimeToExplode = 5.0f;

        EnemyPumpkin(const XMFLOAT3& pos);
        

        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void DoHit();
        float m_timeInOuter;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EnemyTreeBlack : public EntityEnemyBase
    {
        EnemyTreeBlack(const XMFLOAT3& pos, float shootEverySecs = 20.5f);
        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite);

        float m_shootEvery;
        float m_timeToNextShoot;
    };


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EnemyGirl : public EntityEnemyBase
    {
        enum eState{ WAITING, GOING };

        EnemyGirl(const XMFLOAT3& pos);
        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void DoHit();

        bool GetNextTargetPoint();

        LevelMapBSPNode* m_roomNode;
        XMFLOAT3 m_nextTargetPoint;
        float m_waitingForNextTarget;
        eState m_state;
        float m_speed;
        float m_hitTime;
        float m_followingPlayer;
        float m_timeToNextShoot;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EnemyGargoyle : public EntityEnemyBase
    {
        EnemyGargoyle(const XMFLOAT3& pos, float minDist=2.5f);
        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void DoHit();

        float m_timeToNextShoot;
        float m_minDistSq;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EnemyBlackHands : public EntityEnemyBase
    {
        struct Hand
        {
            XMFLOAT3 pos;
            XMFLOAT2 size;
            float distToCamSq;
            float t;
        };
        EnemyBlackHands(const XMUINT2& tile, int n=16);

        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite);
        virtual void DoHit();  
        void UpdateSort(const CameraFirstPerson& camera);

        int m_origN;
        std::vector<Hand> m_hands;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*struct EnemyBlackHand : public EntityEnemyBase
    {

    };*/

};