#pragma once
#include <DirectXMath.h>

using namespace DirectX;
namespace DX { class StepTimer;  class DeviceResources; }

namespace SpookyAdulthood
{
    struct CameraFirstPerson;
    class SpriteManager;

    class Entity
    {
    public:
        enum Flags
        { 
            NONE=0, 
            SPRITE2D=1<<0, 
            SPRITE3D=1<<1, 
            ANIMATION2D=1<<2,

            ACCEPT_RAYCAST=1<<3,
            COLLIDE=1<<4,

            INVALID=1<<5,
            INACTIVE=1<<6
        };

        enum RenderPass { PASS_3D, PASS_2D };
    public:
        Entity(int flags=NONE);        

    protected:
        virtual void Update(float stepTime);
        virtual void Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite);

        bool SupportPass(RenderPass pass) const;        
        bool SupportRaycast() const;
        bool IsActive() const;

        friend class EntityManager;
        XMFLOAT3 m_pos;
        XMFLOAT2 m_size;
        float m_rotation;
        int m_spriteIndex;
        int m_flags;
        float m_timeOut;
        float m_totalTime;
    };

    class EntityManager 
    {
    public:
        EntityManager(const std::shared_ptr<DX::DeviceResources>& device);

        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();

        void AddEntity(const std::shared_ptr<Entity>& entity);
        void Clear();
        void Update(const DX::StepTimer& stepTimer, const CameraFirstPerson& camera);
        void Render3D(const CameraFirstPerson& camera);
        void Render2D(const CameraFirstPerson& camera);
        bool RaycastDir(const XMFLOAT3& origin, const XMFLOAT3& dir, XMFLOAT3& outHit);
        bool RaycastSeg(const XMFLOAT3& origin, const XMFLOAT3& end, XMFLOAT3& outHit);

    private:
        typedef std::vector<std::shared_ptr<Entity>> EntitiesCollection;
        friend class Entity;
        std::shared_ptr<DX::DeviceResources> m_device;
        EntitiesCollection m_entities;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct EntityFluffy : public Entity
    {
        EntityFluffy( const XMFLOAT3& pos);
        ~EntityFluffy();

    protected:
        virtual void Update(float stepTime);

        XMFLOAT3 m_origin;
    };


};