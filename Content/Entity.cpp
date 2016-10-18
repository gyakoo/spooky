#include "pch.h"
#include "Entity.h"
#include "../Common/DeviceResources.h"
#include "CameraFirstPerson.h"
#include "Sprite.h"

using namespace SpookyAdulthood;

Entity::Entity(int flags)
    : m_pos(0,0,0), m_size(1,1), m_rotation(0), m_spriteIndex(-1)
    , m_flags(flags), m_timeOut(FLT_MAX)//, m_distToCamSq(FLT_MAX)
    , m_totalTime(0.0f)
{
}

void Entity::Update(float stepTime)
{

}

void Entity::Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite)
{
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
        e->Update(dt);
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

void EntityManager::AddEntity(const std::shared_ptr<Entity>& entity)
{
    m_entities.push_back(entity);
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
    auto fluffy = std::make_shared<EntityFluffy>(XMFLOAT3(5.0f, 1.0f, 5.0f));
    AddEntity(fluffy);
    fluffy->m_timeOut = 10.0f;
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


void EntityFluffy::Update(float stepTime)
{
    m_pos.y = m_origin.y + sin(m_totalTime*0.5f)*stepTime;
}
