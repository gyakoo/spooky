#include "pch.h"
#include "Entity.h"
#include "../Common/DeviceResources.h"
#include "CameraFirstPerson.h"
#include "Sprite.h"
#include "GlobalFlags.h"

using namespace SpookyAdulthood;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////// BASE ENTITY
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Entity::Entity(int flags)
    : m_pos(0,0,0), m_size(1,1), m_rotation(0), m_spriteIndex(-1)
    , m_flags(flags), m_timeOut(FLT_MAX)//, m_distToCamSq(FLT_MAX)
    , m_totalTime(0.0f), m_modulate(1,1,1,1)
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
        if (pass == PASS_SPRITE3D)
            sprite.Draw3D(m_spriteIndex, m_pos, m_size, m_modulate);
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
    const bool ismodel3d = (m_flags & MODEL3D) != 0;
    return (pass == PASS_SPRITE2D && is2d) || (pass == PASS_SPRITE3D && is3d) 
        || (pass == PASS_3D && ismodel3d);
}

bool Entity::IsActive() const
{
    const bool isInactive = (m_flags & INACTIVE) != 0;
    return !isInactive;
}

void Entity::Invalidate()
{
    m_flags |= INVALID;
}

float Entity::GetBoundingRadius() const
{
    const XMFLOAT3 xy(m_size.x*0.5f, m_size.y*0.5f, 0);
    return XM3Len(xy);
}

bool Entity::SupportRaycast() const
{
    const bool acceptRaycast = (m_flags & Entity::ACCEPT_RAYCAST) != 0;
    const bool is3D = (m_flags & Entity::SPRITE3D) != 0;
    const bool isInvalid = (m_flags & (Entity::INACTIVE | Entity::INVALID)) != 0;
    return acceptRaycast && is3D && !isInvalid;
}
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// /////////////////////////////////////////////////// ENTITY MANAGER
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityManager* EntityManager::s_instance = nullptr;

EntityManager::EntityManager(const std::shared_ptr<DX::DeviceResources>& device)
    : m_device(device), m_duringUpdate(false)
{
    EntityManager::s_instance = this;
}

void EntityManager::Update(const DX::StepTimer& stepTimer, const CameraFirstPerson& camera)
{
    m_duringUpdate = true;
    const float dt = (float)stepTimer.GetElapsedSeconds();
    for (auto it = m_entities.begin(); it < m_entities.end(); )
    {
        auto& e = *it;
        bool toDel = false;
        if (e->IsActive())
        {
            e->Update(dt, camera);
            e->m_timeOut -= dt;
            if (e->m_timeOut <= 0.0f)
                e->m_flags |= Entity::INVALID;
            else
                e->m_totalTime += dt;
            toDel = ((*it)->m_flags & Entity::INVALID) != 0;
        }

        it = toDel ? m_entities.erase(it) : it + 1;
    }
    m_duringUpdate = false;

    // add buffered
    if ( !m_entitiesToAdd.empty() )
    {
        m_entities.insert(m_entities.end(), m_entitiesToAdd.begin(), m_entitiesToAdd.end());
        m_entitiesToAdd.clear();
    }

    // test
    if (GlobalFlags::SpawnProjectile)
    {
        auto proj = std::make_shared<EntityProjectile>(camera.GetPosition(), 19, EntityProjectile::STRAIGHT, EntityProjectile::FREE, 16.0f, camera.m_forward);
        AddEntity(proj);
        GlobalFlags::SpawnProjectile = false;
    }
}

void EntityManager::RenderSprites3D(const CameraFirstPerson& camera)
{
    auto gameRes = m_device->GetGameResources();
    if (!gameRes || !gameRes->m_readyToRender)
        return;

    auto& sprite = gameRes->m_sprite;
    sprite.Begin3D(camera);
    std::for_each(m_entities.begin(), m_entities.end(), [&](auto& e)
    {
        if ( e->SupportPass(Entity::PASS_SPRITE3D) /*&& e->IsActive()*/ )
            e->Render(Entity::PASS_SPRITE3D, camera, sprite);
    });
    sprite.End3D();
}

void EntityManager::RenderSprites2D(const CameraFirstPerson& camera)
{
    auto gameRes = m_device->GetGameResources();
    if (!gameRes || !gameRes->m_readyToRender)
        return;

    auto& sprite = gameRes->m_sprite;
    
    sprite.Begin2D(camera);
    std::for_each(m_entities.begin(), m_entities.end(), [&](auto& e)
    {
        if (e->SupportPass(Entity::PASS_SPRITE2D) /*&& e->IsActive()*/)
            e->Render(Entity::PASS_SPRITE2D, camera, sprite);
    });
    sprite.End2D();
}

void EntityManager::RenderModel3D(const CameraFirstPerson& camera)
{
    auto gameRes = m_device->GetGameResources();
    if (!gameRes || !gameRes->m_readyToRender)
        return;

    auto& sprite = gameRes->m_sprite;
    std::for_each(m_entities.begin(), m_entities.end(), [&](auto& e)
    {
        if (e->SupportPass(Entity::PASS_3D) /*&& e->IsActive()*/)
            e->Render(Entity::PASS_3D, camera, sprite);
    });
}

void EntityManager::AddEntity(const std::shared_ptr<Entity>& entity, float timeout)
{
    auto& coll = m_duringUpdate ? m_entitiesToAdd : m_entities;
    coll.push_back(entity);
    entity->m_timeOut = timeout;
}

void EntityManager::AddEntity(const std::shared_ptr<Entity>& entity)
{
    auto& coll = m_duringUpdate ? m_entitiesToAdd : m_entities;
    coll.push_back(entity);
}

void EntityManager::Clear()
{
    m_entities.clear();
    m_entitiesToAdd.clear();
}

bool EntityManager::RaycastDir(const XMFLOAT3& origin, const XMFLOAT3& dir, XMFLOAT3& outHit, uint32_t* sprNdx)
{
    // find the closest hit
    int closestNdx = -1;
    float closestFrac = FLT_MAX;
    float frac;
    XMFLOAT3 closestHit(0,0,0), hit;
    const int count = (int)m_entities.size();
    for (int i = 0; i < count; ++i)
    {
        auto& e = m_entities[i];
        if (!e->SupportRaycast()) continue;
        if (RaycastEntity(*e.get(), origin, dir, hit, frac) && frac < closestFrac )
        {
            closestFrac = frac;
            closestNdx = i;
            closestHit = hit;
        }
    }
    outHit = closestHit;
    if (sprNdx) *sprNdx = closestNdx;
    return closestNdx != -1;
}

bool EntityManager::RaycastSeg(const XMFLOAT3& origin, const XMFLOAT3& end, XMFLOAT3& outHit, float optRad,uint32_t* sprNdx)
{
    XMFLOAT3 dir = XM3Sub(end, origin);
    const float lenSq = XM3LenSq(dir);
    XM3Mul_inplace(dir, 1.0f/sqrtf(lenSq));
    
    if (RaycastDir(origin, dir, outHit, sprNdx))
    {
        const float distToHitSq = XM3LenSq(XM3Sub(outHit, origin));
        const float compRad = optRad > 0.0f ? (optRad) : lenSq;
        return (distToHitSq <= compRad);
    }

    return false;
}

void EntityManager::DoHitOnEntity(uint32_t ndx)
{
    auto& e = m_entities[ndx];
    if (e->IsActive())
        e->DoHit();
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
    sprite.CreateSprite(L"assets\\sprites\\proj0.png"); // 19
    sprite.CreateSprite(L"assets\\sprites\\hit00.png"); // 20
    sprite.CreateSprite(L"assets\\sprites\\hit01.png"); // 21
    sprite.CreateSprite(L"assets\\sprites\\hit10.png"); // 22 
    sprite.CreateSprite(L"assets\\sprites\\hit11.png"); // 23
    sprite.CreateSprite(L"assets\\sprites\\door0.png"); // 24
    sprite.CreateSprite(L"assets\\sprites\\teleport0.png"); // 25
    sprite.CreateSprite(L"assets\\sprites\\teleport1.png"); // 26
    sprite.CreateSprite(L"assets\\sprites\\teleport2.png"); // 27
    sprite.CreateSprite(L"assets\\sprites\\garg2.png"); // 28

    sprite.CreateAnimation(std::vector<int>{13, 14}, 20.0f); // 0

    // TEST
    //AddEntity(std::make_shared<EnemyFluffy>(XMFLOAT3(5.0f, 1.0f, 5.0f)), 10.0f);
    //AddEntity(std::make_shared<EnemyTreeBlack>(XMFLOAT3(7.0f, 0, 5.0f)));
    AddEntity(std::make_shared<EnemyGargoyle>(XMFLOAT3(5, 0, 4)));
    AddEntity(std::make_shared<EnemyGirl>(XMFLOAT3(7, 0, 5)));
    AddEntity(std::make_shared<EnemyGirl>(XMFLOAT3(3,0,2)));
}

void EntityManager::ReleaseDeviceDependentResources()
{
    if (!m_device || !m_device->GetGameResources())return;
    auto gameRes = m_device->GetGameResources();
    auto& sprite = gameRes->m_sprite;
    sprite.ReleaseDeviceDependentResources();
}

bool EntityManager::RaycastEntity(const Entity& e, const XMFLOAT3& raypos, const XMFLOAT3& dir, XMFLOAT3& outhit, float& frac)
{
    // first agains the bounding sphere, then against two triangles of quad
    const float rad = e.GetBoundingRadius();
    if (!IntersectRaySphere(raypos, dir, e.m_pos, rad, outhit, frac))
        return false;

    return IntersectRayBillboardQuad(raypos, dir, e.m_pos, e.m_size, outhit, frac);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////// GUN
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
    float offsx = sin(camera.m_runningTime*7.0f)*0.010f*rvel;
    float offsy = sin(camera.m_runningTime*5.0f)*0.010f*rvel + camera.m_pitchYaw.x*0.1f;
    const XMFLOAT2 size(0.8f, 0.8f);
    const float ypos = -0.7f;
    sprite.Draw2D(m_spriteIndices[PUMPKIN], XMFLOAT2(offsx*0.8f, ypos + offsy), size, 0.0f); // pumpkins
    sprite.Draw2D(m_spriteIndices[CANDIES], XMFLOAT2(offsx*0.7f, ypos + offsy*1.1f), size, 0.0f); // candies
    sprite.Draw2D(m_spriteIndices[CANNON], XMFLOAT2(offsx, ypos + offsy - t*0.25f), size, 0.0f); // cannon
    sprite.Draw2D(m_spriteIndices[FLASHLIGHT], XMFLOAT2(offsx, ypos + offsy - t*0.25f), size, 0.0f); // flashlight
    sprite.Draw2DAnimation(m_animIndex, XMFLOAT2(offsx, ypos + offsy-t*0.25f), size, 0.0f);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// PROJECTILE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityProjectile::EntityProjectile(const XMFLOAT3& pos, int spriteNdx, Behavior behavior, Target target, float speed, const XMFLOAT3& dir)
    : Entity(SPRITE3D | ANIMATION3D), m_firstTime(true), m_speed(speed)
{
    if (behavior == FOLLOWER && target == FREE)
        throw std::exception("Follower behavior must have a target");
    m_collidePlayer = true;
    m_pos = pos;
    m_dir = dir;
    m_timeOut = 10.0f; // max time out for projectiles (just in case)
    m_spriteIndex = spriteNdx;
    m_size = XMFLOAT2(0.5f, 0.5f);
    //m_speed = 1.0f; // remove
}

void EntityProjectile::Update(float stepTime, const CameraFirstPerson& camera)
{
    if (m_target == PLAYER)
    {
        if (m_behavior == FOLLOWER || (m_behavior == STRAIGHT && m_firstTime))
        {
            const auto cp = camera.GetPosition();
            m_dir = XM3Normalize(XM3Sub(cp, m_pos));
        }
    }
    m_firstTime = false;

    XMFLOAT3 newPos = XM3Mad(m_pos, m_dir, m_speed*stepTime);

    // collides?
    auto gameRes = DX::GameResources::instance;
    auto& map = gameRes->m_map;
    XMFLOAT3 hit;
    bool wasHit = false;
    wasHit = map.RaycastSeg(m_pos, newPos, hit, 0.5f); // against level?
    if ( !wasHit && newPos.y <= m_size.y*0.5f )
        wasHit = true;

    if (!wasHit) // against player?
    {
        XMFLOAT3 toPl = XM3Sub(camera.GetPosition(), newPos);
        float distToPl = XM3LenSq(toPl);
        const float plRad = camera.m_radiusCollide;
        wasHit = distToPl < (plRad*plRad);
        if (wasHit)
            gameRes->HitPlayer();

    }
    else
    {
        gameRes->SoundVolume(DX::GameResources::SFX_HIT0, 0.2f);
        gameRes->SoundPlay(DX::GameResources::SFX_HIT0, false);
    }

    if (wasHit)
    {
        Invalidate();
        m_pos = hit;
    }
    else
    {
        m_pos = newPos;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ////////////////////////////// SHOOT HIT
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityShootHit::EntityShootHit(const XMFLOAT3& pos) // 20 21 | 22 23
    : Entity(SPRITE3D), m_lastFrame(false)
{
    m_timeOut = 0.5f;
    m_spriteIndex = DX::GameResources::instance->m_random.Get01();
    m_spriteIndex = 20 + m_spriteIndex * 2;
    m_size = XMFLOAT2(0.15f, 0.15f);
    m_pos = pos;
}

void EntityShootHit::Update(float stepTime, const CameraFirstPerson& camera)
{
    if (m_totalTime >= 0.05f)
    {
        if (m_lastFrame)
        {
            Invalidate();
        }
        else
        {
            m_spriteIndex++;
            m_totalTime = 0.0f;
            m_lastFrame = true;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////// TELEPORT
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityTeleport::EntityTeleport(const XMFLOAT3& pos) // 25 26 27
    : Entity(SPRITE3D), m_dir(1)
{
    m_spriteIndex = 25;
    m_size = XMFLOAT2(0.5f, 0.75f);
    m_pos = pos;
}

void EntityTeleport::Update(float stepTime, const CameraFirstPerson& camera)
{
    if (m_totalTime >= 0.4f)
    {
        m_spriteIndex += m_dir;
        if (m_spriteIndex > 27)
        {
            m_spriteIndex = 26;
            m_dir = -1;
        }
        else if (m_spriteIndex < 25)
        {
            m_spriteIndex = 26;
            m_dir = +1;
        }
        m_totalTime = 0.0f;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////////////// ENEMY BASE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityEnemyBase::EntityEnemyBase()
    : Entity(SPRITE3D | ACCEPT_RAYCAST), m_modulateTime(-1.0f)
{
}

void EntityEnemyBase::Update(float stepTime, const CameraFirstPerson& camera)
{
    if (m_modulateTime >= 0.0f)
    {
        const float s = (std::max(m_modulateTime, 0.0f) / m_modulateDuration);
        const float onemins = 1.0f - s;
        m_modulate.x = s*m_modulateTargetColor.x + onemins;
        m_modulate.y = s*m_modulateTargetColor.y + onemins;
        m_modulate.z = s*m_modulateTargetColor.z + onemins;
        m_modulate.w = s*m_modulateTargetColor.w + onemins;
    }
    m_modulateTime -= stepTime;
}

void EntityEnemyBase::ModulateToColor(const XMFLOAT4& color, float duration)
{
    m_modulateDuration = m_modulateTime = duration;
    m_modulateTargetColor = color;
}


void EntityEnemyBase::ShootToPlayer(int projSprIndex, float speed, const XMFLOAT3& offs, const XMFLOAT2& size)
{
    auto gameRes = DX::GameResources::instance;
    auto& rnd = gameRes->m_random;

    XMFLOAT3 origin = XM3Add(m_pos, offs);
    XMFLOAT3 toPl = XM3Sub(gameRes->m_camera.GetPosition(), origin);
    XM3Normalize_inplace(toPl);

    auto proj = std::make_shared<EntityProjectile>(
        origin, projSprIndex, EntityProjectile::STRAIGHT, EntityProjectile::FREE, speed, toPl);
    proj->m_size = size;
    gameRes->m_entityMgr.AddEntity(proj);
}

LevelMapBSPNode* EntityEnemyBase::GetCurrentRoom()
{
    auto gameRes = DX::GameResources::instance;
    auto& l = gameRes->m_map.GetLeafAt(m_pos);
    return l ? l.get() : nullptr;
}

float EntityEnemyBase::DistSqToPlayer(XMFLOAT3* dir)
{
    auto plPos = DX::GameResources::instance->m_camera.GetPosition();
    plPos.y = 0.0f;
    const XMFLOAT3 myPos(m_pos.x, 0.0f, m_pos.z);
    const XMFLOAT3 toPl = XM3Sub(plPos, myPos);
    const float lenSq = XM3LenSq(toPl);
    if (dir && lenSq!=0)
        *dir = XM3Mul(toPl, 1.0f / sqrt(lenSq));
    return lenSq;
}

bool EntityEnemyBase::CanSeePlayer()
{
    auto gameRes = DX::GameResources::instance;
    XMFLOAT3 hit;
    return !gameRes->m_map.RaycastSeg(m_pos, gameRes->m_camera.GetPosition(), hit);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////// PUKY
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EnemyPuky::EnemyPuky(const XMFLOAT3& pos)
    : m_origin(pos)
{
    m_spriteIndex = 0;
    m_pos = pos;
    m_size = XMFLOAT2(0.3f, 0.3f);

}

EnemyPuky::~EnemyPuky()
{
    m_pos.x = 0.0f;
}

void EnemyPuky::Update(float stepTime, const CameraFirstPerson& camera)
{
    EntityEnemyBase::Update(stepTime, camera);
    m_pos.y = m_origin.y + sin(m_totalTime)*stepTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////////////// TREE BLACK
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EnemyTreeBlack::EnemyTreeBlack(const XMFLOAT3& pos, float shootEverySecs)
    : m_shootEvery(shootEverySecs)
{
    m_spriteIndex = 8;
    m_size = XMFLOAT2(0.7f, 1.9f);
    m_pos = pos;
    m_pos.y = 0.95f;
    m_timeToNextShoot = shootEverySecs;
}

void EnemyTreeBlack::Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite)
{
    Entity::Render(pass, camera, sprite);    
}

void EnemyTreeBlack::Update(float stepTime, const CameraFirstPerson& camera)
{
    EntityEnemyBase::Update(stepTime, camera);

    m_timeToNextShoot -= stepTime;
    if (m_timeToNextShoot <= 0.0f)
    {
        auto& rnd = DX::GameResources::instance->m_random;
        const XMFLOAT3 offsets[2] = { XMFLOAT3(0,-.5f,0), XMFLOAT3(0,.45f,0) };
        ShootToPlayer(19, 4.0f, offsets[rnd.Get01()], XMFLOAT2(0.5f, 0.5f));
        m_timeToNextShoot = m_shootEvery - m_timeToNextShoot;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////// GARGOYLE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EnemyGargoyle::EnemyGargoyle(const XMFLOAT3& pos, float minDist)
    : m_timeToNextShoot(-1.0f)
{
    m_spriteIndex = 10;
    m_minDistSq = minDist*minDist;
    m_pos = XMFLOAT3(pos.x, 0.75f, pos.z);
    m_size = XMFLOAT2(1.5f, 1.5f);
}

void EnemyGargoyle::Update(float stepTime, const CameraFirstPerson& camera)
{
    EntityEnemyBase::Update(stepTime, camera);

    if (m_totalTime > 0.40f)
    {
        const float curDistSq = DistSqToPlayer();
        if (curDistSq <= m_minDistSq)
        {
            m_spriteIndex = 28;
            if (m_timeToNextShoot <= 0.0f)
            {
                auto gameRes = DX::GameResources::instance;
                ShootToPlayer(19, 3.0f, XMFLOAT3(0, 0.5f, 0), XMFLOAT2(0.5f,0.5f));
                m_timeToNextShoot = gameRes->m_random.GetF(0.5f, 5.0f);
            }
        }
        else
        {
            m_spriteIndex = 10;
            m_totalTime = 0.0f;
        }
    }
    m_timeToNextShoot -= stepTime;
}

void EnemyGargoyle::DoHit()
{
    // on hit, automatically shoots (if gargoyle is in shooting mode)
    ShootToPlayer(19, 5.0f, XMFLOAT3(0, 0.5f, 0), XMFLOAT2(0.5f,0.5f));
    m_spriteIndex = 28;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////// GIRL
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EnemyGirl::EnemyGirl(const XMFLOAT3& pos)
{
    // check which room am I
    m_roomNode = GetCurrentRoom();
    m_spriteIndex = 12;
    m_size = XMFLOAT2(0.6f,1.0f);
    m_pos = XMFLOAT3(pos.x, 0.7f, pos.z);
    m_waitingForNextTarget = -1.0f;
    m_speed = 1.0f;
    m_hitTime = -1.0f;
    m_followingPlayer = -.1f;
    m_timeToNextShoot = DX::GameResources::instance->m_random.GetF(0.0f, 10.0f);
    if (GetNextTargetPoint())
        m_state = GOING;
    else
        m_state = WAITING;
}

void EnemyGirl::Update(float stepTime, const CameraFirstPerson& camera)
{
    EntityEnemyBase::Update(stepTime, camera);

    m_waitingForNextTarget -= stepTime;
    auto gameRes = DX::GameResources::instance;
    switch (m_state)
    {
    case WAITING: 
        m_waitingForNextTarget -= stepTime;
        if (m_waitingForNextTarget <= 0.0f)
        {
            if (GetNextTargetPoint())
            {
                m_speed = gameRes->m_random.GetF(2.0f, 4.5f);
                m_state = GOING;
            }
            else
            {
                m_waitingForNextTarget = 3.0f;
                m_roomNode = GetCurrentRoom();
            }
        }break;

    case GOING:
        {
            XMFLOAT3 dir;
            const float distToPlayerSq = DistSqToPlayer(&dir);
            bool move = true;
            // close to player?
            if (distToPlayerSq <= 3.0f*3.0f || m_hitTime >= 0.0f)
            {
                m_followingPlayer += stepTime;
                if (m_followingPlayer > 2.5f)
                {
                    // 2.5 seconds after following the player
                    move = false;
                }
                else
                {
                    if (distToPlayerSq <= gameRes->m_camera.RadiusCollideSq())
                    {
                        // catches the player
                        gameRes->HitPlayer();
                        move = false;
                    }
                    else
                    {
                        // following player, speed depending if girl was startled with shotgun or we were too close
                        m_speed = m_hitTime >= 0.0f ? 2.5f : 1.5f;
                        if ((gameRes->m_frameCount % 3) == 0)
                            move = CanSeePlayer(); // every 3 frames checks for a line of sight when following
                    }
                }
            }
            else
            {
                // wandering, going to next target point
                m_followingPlayer = -0.1f;
                dir = XM3Sub(m_nextTargetPoint, m_pos);
                const float distSq = XM3LenSq(dir);
                if (distSq < 0.25f*0.25f)
                {
                    // reached goal
                    move = false;
                }
                else
                {
                    // just linear move, direction
                    XM3Mul_inplace(dir, 1.0f / sqrt(distSq)); 
                }
            }

            // girl has to move
            if (move)
            {
                XM3Mad_inplace(m_pos, dir, m_speed*stepTime);
            }
            else
            {
                // girl won't move, next goal ?
                m_waitingForNextTarget = gameRes->m_random.GetF(0.0f, 0.5f);
                m_state = WAITING;
            }
        }break;
    }

    // floating effect :)
    m_pos.y = 0.7f + sinf(m_totalTime*4.0f)*0.05f;

    // check if it should shoot player
    if (m_timeToNextShoot <= 0.0f)
    {
        ShootToPlayer(9, gameRes->m_random.GetF(3.0f, 5.0f), XMFLOAT3(-0.3f, 0, 0), XMFLOAT2(0.4f,0.2f));
        m_timeToNextShoot = gameRes->m_random.GetF(1.0f, 10.0f);
    }
    m_timeToNextShoot -= stepTime;
    m_hitTime -= stepTime;
}

void EnemyGirl::DoHit()
{
    auto gameRes = DX::GameResources::instance;
    if (m_followingPlayer >= 0.0f)
    {
        if (GetNextTargetPoint())
        {
            m_speed = gameRes->m_random.GetF(2.0f, 4.5f);
            m_state = GOING;
        }
        m_hitTime = 0.0f;
    }
    else
    {
        m_hitTime = gameRes->m_random.GetF(1.0f, 3.5f);
    }
    ModulateToColor(XMFLOAT4(1, 0, 0, 1), 0.5f);
}

bool EnemyGirl::GetNextTargetPoint()
{
    if (!m_roomNode)
        return false;

    const uint32_t cx = (int)m_pos.x;
    const uint32_t cy = (int)m_pos.y;

    const auto& area = m_roomNode->m_area;
    
    // clearance
    XMUINT2 allowedMoves[8], p;
    static const XMUINT2 offsets[8] = {
        XMUINT2(-1,-1), XMUINT2(0,-1), XMUINT2(1,-1),
        XMUINT2(-1,0), XMUINT2(1,0),
        XMUINT2(-1,+1), XMUINT2(0,+1), XMUINT2(+1,+1) };
    int c = 0;
    for (int i = 0; i < 8; ++i)
    {
        p.x = cx + offsets[i].x;
        p.y = cy + offsets[i].y;
        if (area.Contains(p) && !m_roomNode->IsPillar(p))
            allowedMoves[c++] = p;
    }

    if (c == 0)
    {
        Invalidate(); // stuck, she dies, poor little creature
        return false;
    }

    const auto& selected = allowedMoves[DX::GameResources::instance->m_random.Get(0, c - 1)];
    m_nextTargetPoint.x = selected.x + 0.5f;
    m_nextTargetPoint.y = m_pos.y;
    m_nextTargetPoint.z = selected.y + 0.5f;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////// BLACK HANDS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EnemyBlackHands::EnemyBlackHands(const XMUINT2& tile)
{

}
