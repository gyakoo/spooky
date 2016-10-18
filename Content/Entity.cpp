#include "pch.h"
#include "Entity.h"
#include "../Common/DeviceResources.h"
#include "CameraFirstPerson.h"
#include "Sprite.h"

using namespace SpookyAdulthood;


inline XMFLOAT3 XM3Sub(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline void XM3Sub_inplace(XMFLOAT3& a, const XMFLOAT3& b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
}

inline float XM3LenSq(const XMFLOAT3& a)
{
    return a.x*a.x + a.y*a.y + a.z*a.z;
}

inline float XM3Len(const XMFLOAT3& a)
{
    return sqrt(XM3LenSq(a));
}

inline void XM3Normalize_inplace(XMFLOAT3& a)
{
    const float il = 1.0f/XM3Len(a);
    a.x *= il;
    a.y *= il;
    a.z *= il;
}

inline XMFLOAT3 XM3Normalize(const XMFLOAT3& a)
{
    XMFLOAT3 _a = a;
    XM3Normalize_inplace(_a);
    return _a;
}

Entity::Entity(int flags)
    : m_pos(0,0,0), m_size(1,1), m_rotation(0), m_spriteIndex(-1)
    , m_flags(flags), m_timeOut(FLT_MAX)//, m_distToCamSq(FLT_MAX)
    , m_totalTime(0.0f)
{
}

void Entity::Update(float stepTime, const CameraFirstPerson& camera)
{

}

void Entity::Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite)
{
    if (m_spriteIndex == -1)
        return;
    if ((m_flags & SPRITE3D) != 0)
    {
        sprite.Draw3D(m_spriteIndex, m_pos, m_size);
    }
    else if ((m_flags & SPRITE2D) != 0)
    {
        XMFLOAT2 p(m_pos.x, m_pos.y);
        sprite.Draw2D(m_spriteIndex, p, m_size, m_rotation);
    }
    else if ((m_flags & ANIMATION2D) != 0)
    {
        XMFLOAT2 p(m_pos.x, m_pos.y);
        sprite.Draw2DAnimation(m_spriteIndex, p, m_size, m_rotation);
    }
}

bool Entity::SupportPass(RenderPass pass) const
{
    const bool is2d = (m_flags & (SPRITE2D | ANIMATION2D)) != 0;
    const bool is3d = (m_flags & SPRITE3D) != 0;
    return (pass == PASS_2D && is2d) || (pass == PASS_3D && is3d);
}

bool Entity::IsActive() const
{
    const bool isInactive = (m_flags & INACTIVE) != 0;
    return !isInactive;
}

bool Entity::SupportRaycast() const
{
    const bool acceptRaycast = (m_flags & Entity::ACCEPT_RAYCAST) != 0;
    const bool is3D = (m_flags & Entity::SPRITE3D) != 0;
    const bool isInvalid = (m_flags & (Entity::INACTIVE | Entity::INACTIVE)) != 0;
    return acceptRaycast && is3D && !isInvalid;
}


EntityManager::EntityManager(const std::shared_ptr<DX::DeviceResources>& device)
    : m_device(device)
{
}

void EntityManager::Update(const DX::StepTimer& stepTimer, const CameraFirstPerson& camera)
{
    const float dt = (float)stepTimer.GetElapsedSeconds();
    for (auto it = m_entities.begin(); it < m_entities.end(); )
    {
        auto& e = *it;
        e->Update(dt, camera);
        e->m_timeOut -= dt;
        if (e->m_timeOut <= 0.0f)
            e->m_flags |= Entity::INVALID;
        else
            e->m_totalTime += dt;
        bool toDel = ((*it)->m_flags & Entity::INVALID)!=0;
        if (toDel)
            it = m_entities.erase(it);
        else
            ++it;
    }
}

void EntityManager::Render3D(const CameraFirstPerson& camera)
{
    auto gameRes = m_device->GetGameResources();
    if (!gameRes || !gameRes->m_readyToRender)
        return;

    auto& sprite = gameRes->m_sprite;
    sprite.Begin3D(camera);
    std::for_each(m_entities.begin(), m_entities.end(), [&](auto& e)
    {
        if ( e->SupportPass(Entity::PASS_3D) && e->IsActive() )
            e->Render(Entity::PASS_3D, camera, sprite);
    });
    sprite.End3D();
}

void EntityManager::Render2D(const CameraFirstPerson& camera)
{
    auto gameRes = m_device->GetGameResources();
    if (!gameRes || !gameRes->m_readyToRender)
        return;

    auto& sprite = gameRes->m_sprite;
    
    sprite.Begin2D(camera);
    std::for_each(m_entities.begin(), m_entities.end(), [&](auto& e)
    {
        if (e->SupportPass(Entity::PASS_2D) && e->IsActive())
            e->Render(Entity::PASS_2D, camera, sprite);
    });
    sprite.End2D();
}

void EntityManager::AddEntity(const std::shared_ptr<Entity>& entity, float timeout)
{
    m_entities.push_back(entity);
    entity->m_timeOut = timeout;
}

void EntityManager::Clear()
{
    m_entities.clear();
}

bool EntityManager::RaycastDir(const XMFLOAT3& origin, const XMFLOAT3& dir, XMFLOAT3& outHit)
{
    // find the closest hit
    int closestNdx = -1;
    float closestFrac = FLT_MAX;
    const int count = (int)m_entities.size();
    for (int i = 0; i < count; ++i)
    {
        auto& e = m_entities[i];
        if (!e->SupportRaycast()) continue;
        throw std::exception("Not implemented");
    }

    return false;
}

bool EntityManager::RaycastSeg(const XMFLOAT3& origin, const XMFLOAT3& end, XMFLOAT3& outHit)
{
    throw std::exception("Not implemented");
    return false;
}

void EntityManager::CreateDeviceDependentResources()
{
    if (!m_device || !m_device->GetGameResources())return;
    auto gameRes = m_device->GetGameResources();
    auto& sprite = gameRes->m_sprite;

    sprite.CreateSprite(L"assets\\sprites\\puky.png"); // 0
    sprite.CreateSprite(L"assets\\sprites\\hand.png"); // 1
    sprite.CreateSprite(L"assets\\sprites\\gun0.png"); // 2
    sprite.CreateSprite(L"assets\\sprites\\pointinghand.png"); // 3
    sprite.CreateSprite(L"assets\\sprites\\anx1.png"); // 4
    sprite.CreateSprite(L"assets\\sprites\\dep1.png"); // 5
    sprite.CreateSprite(L"assets\\sprites\\grave1.png"); // 6
    sprite.CreateSprite(L"assets\\sprites\\crosshair.png"); // 7
    sprite.CreateSprite(L"assets\\sprites\\tree1.png"); // 8
    sprite.CreateSprite(L"assets\\sprites\\msgdie.png"); // 9
    sprite.CreateSprite(L"assets\\sprites\\garg1.png"); // 10
    sprite.CreateSprite(L"assets\\sprites\\bodpile1.png"); // 11
    sprite.CreateSprite(L"assets\\sprites\\girl1.png"); // 12
    sprite.CreateSprite(L"assets\\sprites\\gunshoot0.png"); // 13
    sprite.CreateSprite(L"assets\\sprites\\gunshoot1.png"); // 14
    sprite.CreateSprite(L"assets\\sprites\\gunshoot2.png"); // 15
    sprite.CreateSprite(L"assets\\sprites\\gun1.png"); // 16
    sprite.CreateSprite(L"assets\\sprites\\gun2.png"); // 17
    sprite.CreateSprite(L"assets\\sprites\\gun3.png"); // 18

    sprite.CreateAnimation(std::vector<int>{13, 14, 15}, 20.0f); // 0
    sprite.CreateAnimationInstance(0); // 0


    // TEST
    AddEntity(std::make_shared<EntityFluffy>(XMFLOAT3(5.0f, 1.0f, 5.0f)), 10.0f);
    AddEntity(std::make_shared<EntityGun>()); // GUN
}

void EntityManager::ReleaseDeviceDependentResources()
{
    if (!m_device || !m_device->GetGameResources())return;
    auto gameRes = m_device->GetGameResources();
    auto& sprite = gameRes->m_sprite;
    sprite.ReleaseDeviceDependentResources();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityFluffy::EntityFluffy( const XMFLOAT3& pos)
    : Entity(SPRITE3D | ACCEPT_RAYCAST)
    , m_origin(pos)
{
    m_spriteIndex = 0;
    m_pos = pos;
    m_size = XMFLOAT2(0.3f, 0.3f);

}

EntityFluffy::~EntityFluffy()
{
    m_pos.x = 0.0f;
}

void EntityFluffy::Update(float stepTime, const CameraFirstPerson& camera)
{
    m_pos.y = m_origin.y + sin(m_totalTime)*stepTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityGun::EntityGun()
    : Entity(SPRITE2D | ANIMATION2D) // irrelevant actually as we reimplement Render
{
    m_spriteIndices[PUMPKIN] = 17;
    m_spriteIndices[CANDIES] = 18;
    m_spriteIndices[CANNON] = 2;
    m_spriteIndices[FLASHLIGHT] = 16;
    m_animIndex = 0;
}

void EntityGun::Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite)
{
    if (!SupportPass(pass)) return;
    // don't call super
    float rvel = (camera.m_moving && camera.m_running) ? 1.0f : 0.5f;
    float t = std::max(0.0f, camera.m_timeShoot);
    float offsx = sin(camera.m_runningTime*7.0f)*0.015f*rvel;
    float offsy = sin(camera.m_runningTime*5.0f)*0.015f*rvel + camera.m_pitchYaw.x*0.1f;
    sprite.Draw2D(m_spriteIndices[PUMPKIN], XMFLOAT2(offsx*0.8f, -0.6f + offsy), XMFLOAT2(0.9f, 0.9f), 0.0f); // pumpkins
    sprite.Draw2D(m_spriteIndices[CANDIES], XMFLOAT2(offsx*0.7f, -0.6f + offsy*1.1f), XMFLOAT2(0.9f, 0.9f), 0.0f); // candies
    sprite.Draw2D(m_spriteIndices[CANNON], XMFLOAT2(offsx, -0.6f + offsy - t*0.1f), XMFLOAT2(0.9f, 0.9f), 0.0f); // cannon
    sprite.Draw2D(m_spriteIndices[FLASHLIGHT], XMFLOAT2(offsx, -0.6f + offsy - t*0.1f), XMFLOAT2(0.9f, 0.9f), 0.0f); // flashlight
    sprite.Draw2DAnimation(m_animIndex, XMFLOAT2(offsx, -0.6f + offsy), XMFLOAT2(0.9f, 0.9f), 0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityTreeBlack::EntityTreeBlack()
    : Entity(SPRITE2D)
{

}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityProjectile::EntityProjectile(const XMFLOAT3& pos, int spriteNdx, Behavior behavior, Target target, const XMFLOAT3& dir)
    : Entity(SPRITE3D | ANIMATION3D), m_firstTime(true)
{
    if (behavior == FOLLOWER && target == FREE)
        throw std::exception("Follower behavior must have a target");
    m_pos = pos;
    m_dir = dir;
}

void EntityProjectile::Update(float stepTime, const CameraFirstPerson& camera)
{
    if (m_target == PLAYER)
    {
        if ( m_behavior == FOLLOWER || (m_behavior == STRAIGHT && m_firstTime) )
        {
            const auto cp = camera.GetPosition();
            m_dir = XM3Normalize(XM3Sub(cp, m_pos));
        }
    }
    

    m_firstTime = false;
}