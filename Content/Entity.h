﻿#pragma once
#include <DirectXMath.h>

using namespace DirectX;
namespace DX { class StepTimer;  class DeviceResources; }

namespace SpookyAdulthood
{
    struct CameraFirstPerson;
    class SpriteManager;

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

        enum RenderPass { PASS_SPRITE3D, PASS_SPRITE2D, PASS_3D };
    public:
        Entity(int flags=NONE);        

    protected:
        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite);
        
        float GetBoundingRadius() const;
        bool SupportPass(RenderPass pass) const;        
        bool SupportRaycast() const;
        bool IsActive() const;
        void Invalidate();

        friend class EntityManager;
        XMFLOAT3 m_pos;
        XMFLOAT2 m_size;
        float m_rotation;
        int m_spriteIndex;
        int m_flags;
        float m_timeOut;
        float m_totalTime;
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
        bool RaycastDir(const XMFLOAT3& origin, const XMFLOAT3& dir, XMFLOAT3& outHit);
        bool RaycastSeg(const XMFLOAT3& origin, const XMFLOAT3& end, XMFLOAT3& outHit, float optRad=-1.0f);

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
    struct EntityFluffy : public Entity
    {
        EntityFluffy( const XMFLOAT3& pos);
        ~EntityFluffy();

    protected:
        virtual void Update(float stepTime, const CameraFirstPerson& camera);

        XMFLOAT3 m_origin;
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
    struct EntityTreeBlack : public Entity
    {
        EntityTreeBlack(const XMFLOAT3& pos, float shootEverySecs=20.5f);
        virtual void Update(float stepTime, const CameraFirstPerson& camera);
        virtual void Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite);
        void Shoot(const CameraFirstPerson& camera);

        float m_shootEvery;
        float m_timeToNextShoot;
    };


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EntityGirl : public Entity
    {

    };


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EntityProjectile : public Entity
    {
        enum Behavior{ STRAIGHT, FOLLOWER };
        enum Target { PLAYER, FREE };
        EntityProjectile(const XMFLOAT3& pos, int spriteNdx, Behavior behavior, Target target, float speed, const XMFLOAT3& dir=XMFLOAT3(0,0,0));

        virtual void Update(float stepTime, const CameraFirstPerson& camera);

        Behavior m_behavior;
        Target m_target;
        XMFLOAT3 m_dir;
        float m_speed;
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
    struct EntityDoor : public Entity
    {
        EntityDoor();
    };

};