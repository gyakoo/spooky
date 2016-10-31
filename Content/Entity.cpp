#include "pch.h"
#include "Entity.h"
#include "../Common/DeviceResources.h"
#include "CameraFirstPerson.h"
#include "Sprite.h"
#include "GlobalFlags.h"

using namespace SpookyAdulthood;

static const XMFLOAT4 XM4RED(1, 0, 0, 1);
#define RND (DX::GameResources::instance->m_random)
#define CAM (DX::GameResources::instance->m_camera)
const float FBIGVAL = 1e10;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////// BASE ENTITY
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Entity::Entity(int flags)
    : m_pos(0,0,0), m_size(1,1), m_rotation(0), m_spriteIndex(-1)
    , m_flags(flags), m_timeOut(FLT_MAX)//, m_distToCamSq(FLT_MAX)
    , m_totalTime(0.0f), m_modulate(1,1,1,1), m_invalidateReason(NONE_i)
    , m_life(-1.0f), m_lastFrameHit(0)
{
}

void Entity::Update(float stepTime, const CameraFirstPerson& camera)
{

}

void Entity::PerformHit()
{
    const uint32_t curFrame = DX::GameResources::instance->m_frameCount;
    if (m_lastFrameHit == curFrame)
        return;

    m_lastFrameHit = curFrame;
    DoHit();
}


void Entity::Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite)
{
    if ((m_flags & SPRITE3D) != 0)
    {
        if (pass == PASS_SPRITE3D)
        {
            if ( m_spriteIndex != -1 )
                sprite.Draw3D(m_spriteIndex, m_pos, m_size, m_modulate);
            if (m_life >= 0.0f)
            {
                const XMFLOAT3 barp(m_pos.x, m_pos.y + m_size.y*0.5f, m_pos.z);
                const XMFLOAT2 bars(m_size.x*m_life, 0.015f);
                sprite.Draw3D(29, barp, bars, XM4RED);
            }
        }
    }
    else if ((m_flags & SPRITE2D) != 0 && m_spriteIndex != -1)
    {
        XMFLOAT2 p(m_pos.x, m_pos.y);
        sprite.Draw2D(m_spriteIndex, p, m_size, m_rotation);
    }
    else if ((m_flags & ANIMATION2D) != 0 && m_spriteIndex != -1)
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

bool Entity::IsValid() const
{
    const bool isInvalid = (m_flags & INVALID) != 0;
    return !isInvalid;
}

void Entity::Invalidate(InvReason reason, float after)
{
    m_invalidateReason = reason;
    if (after >= 0.0f)
        m_timeOut = after;
    else
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

float Entity::DistSqToPlayer(XMFLOAT3* dir)
{
    auto plPos = CAM.GetPosition();
    plPos.y = 0.0f;
    const XMFLOAT3 myPos(m_pos.x, 0.0f, m_pos.z);
    const XMFLOAT3 toPl = XM3Sub(plPos, myPos);
    const float lenSq = XM3LenSq(toPl);
    if (dir && lenSq != 0)
        *dir = XM3Mul(toPl, 1.0f / sqrt(lenSq));
    return lenSq;
}

void Entity::PlaySoundDistance(uint32_t sound, float maxdist, bool loop)
{
    // volume depending on distance
    auto gameRes = DX::GameResources::instance;
    gameRes->SoundPlay(sound, loop);
    UpdateSoundDistance(sound, maxdist);
}

void Entity::UpdateSoundDistance(uint32_t sound, float maxdist)
{
    auto gameRes = DX::GameResources::instance;
    const float dtp = sqrt(DistSqToPlayer());
    const float dv = gameRes->SoundGetDefaultVolume(sound);
    gameRes->SoundVolume(sound, dv *(1.0f - std::min(1.0f, dtp / maxdist)));
}

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// /////////////////////////////////////////////////// ENTITY MANAGER
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityManager* EntityManager::s_instance = nullptr;

EntityManager::EntityManager(const std::shared_ptr<DX::DeviceResources>& device)
    : m_device(device), m_duringUpdate(false), m_curRoomIndex(-1), m_paused(true)
{
    EntityManager::s_instance = this;
}

void EntityManager::CreateEntities_Puky(LevelMapBSPNode* r, int n, uint32_t prob)
{
    auto& rnd = m_device->GetGameResources()->instance->m_random;
    XMFLOAT3 p;
    for (int i = 0; i < n; ++i)
    {
        if (rnd.Get(0, 99) < prob)
        {
            p = r->GetRandomXZ(XMFLOAT2(0.15f, 0.15f));
            p.y = rnd.GetF(0.2f, 0.8f);
            AddEntity(std::make_shared<EnemyPuky>(p), r->m_leafNdx);
        }
    }
}

void EntityManager::CreateEntities_Pumpkin(LevelMapBSPNode* r, int n, uint32_t prob)
{
    auto& rnd = m_device->GetGameResources()->instance->m_random;
    XMFLOAT3 p;
    for (int i = 0; i < n; ++i)
    {
        if (rnd.Get(0, 99) < prob)
        {
            p = r->GetRandomXZ(XMFLOAT2(0.15f, 0.15f));
            if ( r->Clearance(p))
                AddEntity(std::make_shared<EnemyPumpkin>(p), r->m_leafNdx);
        }
    }
}

void EntityManager::CreateEntities_Girl(LevelMapBSPNode* r, int n, uint32_t prob)
{
    auto& rnd = m_device->GetGameResources()->instance->m_random;
    XMFLOAT3 p;
    for (int i = 0; i < n; ++i)
    {
        if (rnd.Get(0, 99) < prob)
        {
            p = r->GetRandomXZ(XMFLOAT2(0.5f, 0.5f));
            AddEntity(std::make_shared<EnemyGirl>(p), r->m_leafNdx);
        }
    }
}

void EntityManager::CreateEntities_Gargoyle(LevelMapBSPNode* r, int n, uint32_t prob)
{
    auto& rnd = m_device->GetGameResources()->instance->m_random;
    XMFLOAT3 p;
    for (int i = 0; i < n; ++i)
    {
        if (rnd.Get(0, 99) < prob)
        {
            p = r->GetRandomXZ(XMFLOAT2(0.75f, 0.75f));
            if (r->Clearance(p))
                AddEntity(std::make_shared<EnemyGargoyle>(p, rnd.GetF(3.0f,6.0f)), r->m_leafNdx);
        }
    }
}

void EntityManager::CreateEntities_BlackHands(LevelMapBSPNode* r, int n, uint32_t prob)
{
    auto& rnd = m_device->GetGameResources()->instance->m_random;
    XMUINT2 p;
    for (int i = 0; i < n; ++i)
    {
        if (rnd.Get(0, 99) < prob)
        {
            p = r->GetRandomTile();
            if (r->Clearance(p))
                AddEntity(std::make_shared<EnemyBlackHands>(p, rnd.Get(6,16)), r->m_leafNdx);
        }
    }
}

void EntityManager::ReserveAndCreateEntities(int roomCount)
{
    if (roomCount <= 0)
        throw std::exception("No rooms in entity manager?");
    m_rooms.resize(roomCount);

    // TEST: DELETE
    //{
    //    auto r = DX::GameResources::instance->m_map.GetLeafAtIndex(0);
    //    auto p = r->GetRandomXZWithClearance();
    //    AddEntity(std::make_shared<EnemyBoss>(p), 0);
    //    AddEntity(std::make_shared<EntityRoomChecker_AllDead>(), 0);
    //    return;
    //}


    // will create the entities depending on the room profiles
    auto gameRes = m_device->GetGameResources();
    auto& rnd = gameRes->m_random;
    auto& rooms = gameRes->m_map.GetRooms();
    XMUINT2 decoProbs[DECORMAX];     
    for (auto& r : rooms)
    {
        ZeroMemory(decoProbs, sizeof(XMUINT2)*DECORMAX);
        switch (r->m_profile)
        {
        case LevelMap::RP_NORMAL0:
            decoProbs[BODYPILE] = XMUINT2(rnd.Get(0, 2), 30);
            decoProbs[GREENHAND] = XMUINT2(rnd.Get(0, 4), 50);
            decoProbs[BLACKHAND] = XMUINT2(rnd.Get(0, 4), 50);
            decoProbs[SKULL] = XMUINT2(rnd.Get(2, 10), 80);
            CreateEntities_Pumpkin(r.get(), rnd.Get(0, 5), 70);
            if (r->m_leafNdx)
            {
                CreateEntities_Puky(r.get(), rnd.Get(0, 3), 40);
                CreateEntities_Girl(r.get(), rnd.Get(0, 3), 80);
                CreateEntities_BlackHands(r.get(), rnd.Get(0, 3), 80);
                if (rnd.Get01(0.7f))
                    AddEntity(std::make_shared<EnemyGhost>(r->GetRandomXZWithClearance()), r->m_leafNdx);
            }break;
        case LevelMap::RP_NORMAL1:
            decoProbs[GRAVE] = XMUINT2(rnd.Get(0, 5), 50);
            decoProbs[BLACKHAND] = XMUINT2(rnd.Get(0, 8), 70);
            decoProbs[SKULL] = XMUINT2(rnd.Get(0, 15), 80);
            CreateEntities_Pumpkin(r.get(), rnd.Get(0, 5), 70);
            if (r->m_leafNdx)
            {
                CreateEntities_Girl(r.get(), rnd.Get(0, 2), 70);
                CreateEntities_Gargoyle(r.get(), rnd.Get(0, 2), 60);
                AddEntity(std::make_shared<EntityRandomSound>(DX::GameResources::SFX_CAT, 12.0f, 30.0f), r->m_leafNdx);
                if (rnd.Get01(0.4f))
                    AddEntity(std::make_shared<EnemyGhost>(r->GetRandomXZWithClearance()), r->m_leafNdx);
            }break;
        case LevelMap::RP_GRAVE:
            decoProbs[BODYPILE] = XMUINT2(rnd.Get(0, 3), 20);
            decoProbs[GRAVE] = XMUINT2(rnd.Get(3, 20), 90);
            decoProbs[TREEBLACK] = XMUINT2(rnd.Get(1, 4), 70);
            decoProbs[GREENHAND] = XMUINT2(rnd.Get(0, 4), 50);
            decoProbs[BLACKHAND] = XMUINT2(rnd.Get(0, 4), 50);
            decoProbs[SKULL] = XMUINT2(rnd.Get(2, 10), 80);
            CreateEntities_Pumpkin(r.get(), rnd.Get(0, 5), 80);
            if (r->m_leafNdx)
            {
                CreateEntities_Girl(r.get(), rnd.Get(0, 3), 80);
                CreateEntities_BlackHands(r.get(), rnd.Get(0, 2), 60);
                CreateEntities_Gargoyle(r.get(), rnd.Get(0, 2), 60);
                AddEntity(std::make_shared<EntityRandomSound>(DX::GameResources::SFX_OWL, 12.0f, 30.0f), r->m_leafNdx);
                for (int i = rnd.Get(0, 3); i > 0; --i)
                    if (rnd.Get01())
                        AddEntity(std::make_shared<EnemyGhost>(r->GetRandomXZWithClearance()), r->m_leafNdx);
            }break;
        case LevelMap::RP_WOODS: 
            decoProbs[BODYPILE] = XMUINT2(rnd.Get(0, 2), 20);
            decoProbs[GRAVE] = XMUINT2(rnd.Get(0, 5), 60);
            decoProbs[TREEBLACK] = XMUINT2(rnd.Get(5, 20), 100);
            decoProbs[GREENHAND] = XMUINT2(rnd.Get(0, 4), 50);
            decoProbs[BLACKHAND] = XMUINT2(rnd.Get(0, 4), 50);
            decoProbs[SKULL] = XMUINT2(rnd.Get(2, 8), 80);
            if (r->m_leafNdx)
            {
                CreateEntities_Pumpkin(r.get(), rnd.Get(0, 5), 80);
                CreateEntities_Girl(r.get(), rnd.Get(0, 5), 70);
                CreateEntities_Puky(r.get(), rnd.Get(0, 5), 60);
                AddEntity(std::make_shared<EntityRandomSound>(DX::GameResources::SFX_OWL, 12.0f, 30.0f), r->m_leafNdx);
                for (int i = rnd.Get(0, 3); i > 0; --i)
                    if (rnd.Get01())
                        AddEntity(std::make_shared<EnemyGhost>(r->GetRandomXZWithClearance()), r->m_leafNdx);
            }break;
        case LevelMap::RP_BODYPILES: 
            decoProbs[BODYPILE] = XMUINT2(rnd.Get(4, 10), 60);
            decoProbs[GREENHAND] = XMUINT2(rnd.Get(4, 10), 40);
            decoProbs[BLACKHAND] = XMUINT2(rnd.Get(4, 10), 40);
            decoProbs[SKULL] = XMUINT2(rnd.Get(4, 15), 80);
            CreateEntities_Pumpkin(r.get(), rnd.Get(0, 5), 80);
            if (r->m_leafNdx)
            {
                CreateEntities_BlackHands(r.get(), rnd.Get(0, 2), 60);
                CreateEntities_Puky(r.get(), rnd.Get(0, 3), 40);
                AddEntity(std::make_shared<EntityRandomSound>(DX::GameResources::SFX_CAT, 12.0f, 30.0f), r->m_leafNdx);
                if (rnd.Get01(0.3f))
                    AddEntity(std::make_shared<EnemyGhost>(r->GetRandomXZWithClearance()), r->m_leafNdx);
            }break;
        case LevelMap::RP_GARGOYLES: 
            decoProbs[BODYPILE] = XMUINT2(rnd.Get(0, 2), 20);
            decoProbs[GRAVE] = XMUINT2(rnd.Get(0, 5), 60);
            decoProbs[SKULL] = XMUINT2(rnd.Get(2, 8), 80);
            CreateEntities_Pumpkin(r.get(), rnd.Get(0, 5), 70);
            if (r->m_leafNdx)
            {
                CreateEntities_Gargoyle(r.get(), rnd.Get(1, 10), 80);
                CreateEntities_Girl(r.get(), rnd.Get(0, 3), 70);
                AddEntity(std::make_shared<EntityRandomSound>(DX::GameResources::SFX_CAT, 12.0f, 30.0f), r->m_leafNdx);
                for (int i = rnd.Get(0, 3); i > 0; --i)
                    if (rnd.Get01())
                        AddEntity(std::make_shared<EnemyGhost>(r->GetRandomXZWithClearance()), r->m_leafNdx);
            }break;
        case LevelMap::RP_HANDS: 
            decoProbs[BODYPILE] = XMUINT2(rnd.Get(0, 2), 20);
            decoProbs[GREENHAND] = XMUINT2(rnd.Get(0, 10), 50);
            decoProbs[BLACKHAND] = XMUINT2(rnd.Get(0, 10), 50);
            decoProbs[SKULL] = XMUINT2(rnd.Get(2, 8), 80);
            if (r->m_leafNdx)
            {
                CreateEntities_Girl(r.get(), rnd.Get(0, 3), 50);
                CreateEntities_BlackHands(r.get(), rnd.Get(1, 8), 60);
                for (int i = rnd.Get(0, 3); i > 0; --i)
                    if (rnd.Get01())
                        AddEntity(std::make_shared<EnemyGhost>(r->GetRandomXZWithClearance()), r->m_leafNdx);
            }break;
        case LevelMap::RP_SCARYMESSAGES: 
            decoProbs[GRAVE] = XMUINT2(rnd.Get(0, 5), 60);
            decoProbs[SKULL] = XMUINT2(rnd.Get(2, 8), 80);            
            if (r->m_leafNdx)
            {
                CreateEntities_Gargoyle(r.get(), rnd.Get(1, 5), 80);
                for (int i = rnd.Get(0, 3); i > 0; --i)
                    if (rnd.Get01())
                        AddEntity(std::make_shared<EnemyGhost>(r->GetRandomXZWithClearance()), r->m_leafNdx);
            }break;
        case LevelMap::RP_PUMPKINFIELD: 
            decoProbs[GRAVE] = XMUINT2(rnd.Get(0, 3), 70);
            decoProbs[TREEBLACK] = XMUINT2(rnd.Get(5, 10), 80);
            decoProbs[SKULL] = XMUINT2(rnd.Get(2, 8), 80);
            CreateEntities_Pumpkin(r.get(), rnd.Get(5, 20), 80);
            if (r->m_leafNdx)
            {
                CreateEntities_Girl(r.get(), rnd.Get(0, 3), 50);
                CreateEntities_Puky(r.get(), rnd.Get(0, 5), 70);
                AddEntity(std::make_shared<EntityRandomSound>(DX::GameResources::SFX_OWL, 12.0f, 30.0f), r->m_leafNdx);
                if (rnd.Get01())
                    AddEntity(std::make_shared<EnemyGhost>(r->GetRandomXZWithClearance()), r->m_leafNdx);
            }break;
        }

        // decorations
        XMFLOAT2 shrink(0, 0), s;
        for ( int i = 0; i < DECORMAX; ++i )
        { 
            const auto& p = decoProbs[i];
            if (p.x == 0 || p.y == 0) continue;
            for (uint32_t j = 0; j < p.x; ++j)
            {
                if (rnd.Get(0, 99) < p.y)
                {
                    s = EntitySingleDecoration::GetSizeOf((DecorType)i);
                    shrink.x = shrink.y = s.x*0.5f; 
                    auto decopos = r->GetRandomXZ(shrink);
                    if ( r->Clearance( XMUINT2((UINT)decopos.x, (UINT)decopos.z) ) )
                        AddEntity(std::make_shared<EntitySingleDecoration>((DecorType)i, decopos), r->m_leafNdx);
                }
            }
        }

        // checking end of room
        AddEntity(std::make_shared<EntityRoomChecker_AllDead>(), r->m_leafNdx);
    }
}

void EntityManager::SetCurrentRoom(int roomIndex) 
{ 
    if (m_curRoomIndex != roomIndex)
    {
        m_curRoomIndex = roomIndex;
        auto gameRes = DX::GameResources::instance;
        if (m_curRoomIndex != -1)
            gameRes->OnLeaveRoom(m_curRoomIndex);
        
        if ( roomIndex != -1 )
            gameRes->OnEnterRoom(roomIndex);
    }
}

void EntityManager::Update(const DX::StepTimer& stepTimer, const CameraFirstPerson& camera)
{
    if (m_curRoomIndex == -1) return;
    m_duringUpdate = true;
    for (int pass = 0; pass < 2; ++pass)
    {
        auto& entities = !pass ? m_rooms[m_curRoomIndex] : m_omniEntities;
        float dt = (float)stepTimer.GetElapsedSeconds();
        if (m_paused) dt *= 0.1f;
        for (auto it = entities.begin(); it < entities.end(); )
        {
            auto& e = *it;
            bool toDel = false;
            if (e->IsActive() /* && (!m_paused || e->UpdateOnPaused()) */ )
            {
                e->Update(dt, camera);
                e->m_timeOut -= dt;
                if (e->m_timeOut <= 0.0f)
                    e->m_flags |= Entity::INVALID;
                else
                    e->m_totalTime += dt;
                toDel = !(*it)->IsValid();
            }

            if (toDel)
            {
                switch ((*it)->m_invalidateReason)
                {
                case Entity::KILLED:
                    break;
                }

                it = entities.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    // add buffered
    if ( !m_entitiesToAdd.empty() )
    {
        for (auto& e : m_entitiesToAdd)
        {
            auto& whatColl = e->m_roomIndex >= 0 ? m_rooms[e->m_roomIndex] : m_omniEntities;
            whatColl.push_back(e);
        }
        m_entitiesToAdd.clear();
    }

#ifdef _DEBUG
    if (GlobalFlags::KillRoom)
    {
        GlobalFlags::KillRoom = false;
        for (auto& e : m_rooms[m_curRoomIndex])
        {
            e->m_life = -1.0f;
            EntityEnemyBase* eb = dynamic_cast<EntityEnemyBase*>(e.get());
            if (eb)
                eb->DoHit();
        }
    }
#endif
    m_duringUpdate = false;
}

void EntityManager::RenderSprites3D(const CameraFirstPerson& camera)
{
    auto gameRes = m_device->GetGameResources();
    if (!gameRes || !gameRes->m_readyToRender || m_curRoomIndex==-1)
        return;

    auto& sprite = gameRes->m_sprite;
    sprite.Begin3D(camera);
    for (int pass = 0; pass < 2; ++pass)
    {
        auto& entities = !pass ? m_rooms[m_curRoomIndex] : m_omniEntities;
        std::for_each(entities.begin(), entities.end(), [&](auto& e)
        {
            if (e->SupportPass(Entity::PASS_SPRITE3D) /*&& e->IsActive()*/)
                e->Render(Entity::PASS_SPRITE3D, camera, sprite);
        });
    }
    sprite.End3D();
}

void EntityManager::RenderSprites2D(const CameraFirstPerson& camera)
{
    auto gameRes = m_device->GetGameResources();
    if (!gameRes || !gameRes->m_readyToRender || m_curRoomIndex==-1)
        return;

    auto& sprite = gameRes->m_sprite;
    sprite.Begin2D(camera);    
    for (int pass = 0; pass < 2; ++pass)
    {
        auto& entities = !pass ? m_rooms[m_curRoomIndex] : m_omniEntities;
        std::for_each(entities.begin(), entities.end(), [&](auto& e)
        {
            if (e->SupportPass(Entity::PASS_SPRITE2D) /*&& e->IsActive()*/)
                e->Render(Entity::PASS_SPRITE2D, camera, sprite);
        });
    }
    sprite.End2D();
}

void EntityManager::RenderModel3D(const CameraFirstPerson& camera)
{
    auto gameRes = m_device->GetGameResources();
    if (!gameRes || !gameRes->m_readyToRender || m_curRoomIndex == -1)
        return;

    auto& sprite = gameRes->m_sprite;
    for (int pass = 0; pass < 2; ++pass)
    {
        auto& entities = !pass ? m_rooms[m_curRoomIndex] : m_omniEntities;
        std::for_each(entities.begin(), entities.end(), [&](auto& e)
        {
            if (e->SupportPass(Entity::PASS_3D) /*&& e->IsActive()*/)
                e->Render(Entity::PASS_3D, camera, sprite);
        });
    }
}

void EntityManager::AddEntity(const std::shared_ptr<Entity>& entity, float timeout, int roomIndex)
{
    const int ri = roomIndex < 0 ? m_curRoomIndex : roomIndex;
    auto& entities = roomIndex == -2 ? m_omniEntities : m_rooms[ri];
    auto& coll = m_duringUpdate ? m_entitiesToAdd : entities;
    coll.push_back(entity);
    entity->m_roomIndex = ri;
    entity->m_timeOut = timeout;
}

void EntityManager::AddEntity(const std::shared_ptr<Entity>& entity, int roomIndex)
{
    const int ri = roomIndex < 0 ? m_curRoomIndex : roomIndex;
    auto& entities = roomIndex == -2 ? m_omniEntities : m_rooms[ri];
    auto& coll = m_duringUpdate ? m_entitiesToAdd : entities;
    coll.push_back(entity);
    entity->m_roomIndex = ri;
}

void EntityManager::Clear()
{
    for (auto& rc : m_rooms)
        rc.clear();
    m_entitiesToAdd.clear();
    m_omniEntities.clear();
    m_curRoomIndex = -1;
}

bool EntityManager::RaycastDir(const XMFLOAT3& origin, const XMFLOAT3& dir, XMFLOAT3& outHit, uint32_t* sprNdx)
{
    if (m_curRoomIndex == -1)return false;
    // find the closest hit
    int closestNdx = -1;
    float closestFrac = FLT_MAX;
    float frac;
    XMFLOAT3 closestHit(0, 0, 0), hit;

    for (int pass = 0; pass < 2; ++pass)
    {
        auto& entities = !pass ? m_rooms[m_curRoomIndex] : m_omniEntities;
        const int count = (int)entities.size();
        for (int i = 0; i < count; ++i)
        {
            auto& e = entities[i];
            if (!e->SupportRaycast()) continue;
            if (RaycastEntity(*e.get(), origin, dir, hit, frac) && frac < closestFrac)
            {
                closestFrac = frac;
                closestNdx = i;
                closestHit = hit;
                if (sprNdx)
                {
                    outHit = closestHit;
                    int ri = !pass ? m_curRoomIndex : 0xffff;
                    *sprNdx = (ri << 16) | (closestNdx & 0xffff);
                }
            }
        }        
    }
    return closestNdx != -1;
}

bool EntityManager::RaycastSeg(const XMFLOAT3& origin, const XMFLOAT3& end, XMFLOAT3& outHit, float optRad,uint32_t* sprNdx)
{
    XMFLOAT3 dir = XM3Sub(end, origin);
    const float lenSq = XM3LenSq(dir);
    XM3Mul_inplace(dir, 1.0f/sqrtf(lenSq));
    
    if (RaycastDir(origin, dir, outHit, sprNdx))
    {
        const float distToHitSq = XM3LenSq(outHit,origin);
        const float compRad = optRad > 0.0f ? (optRad) : lenSq;
        return (distToHitSq <= compRad);
    }

    return false;
}

void EntityManager::DoHitOnEntity(uint32_t ndx)
{
    const int ri = ndx >> 16;
    const int n = ndx & 0xffff;
    auto& entities = (ri == 0xffff) ? m_omniEntities : m_rooms[ri];
    auto& e = entities[n];
    if (e->IsActive())
        e->PerformHit();
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
    sprite.CreateSprite(L"assets\\textures\\white.png"); // 29
    sprite.CreateSprite(L"assets\\sprites\\pumpkin.png"); // 30
    sprite.CreateSprite(L"assets\\sprites\\door1.png"); // 31
    sprite.CreateSprite(L"assets\\sprites\\skull.png"); // 32
    sprite.CreateSprite(L"assets\\textures\\blue.png"); // 33
    sprite.CreateSprite(L"assets\\textures\\red.png"); // 34
    sprite.CreateSprite(L"assets\\sprites\\itemcandy.png"); // 35
    sprite.CreateSprite(L"assets\\sprites\\heart.png"); // 36
    sprite.CreateSprite(L"assets\\sprites\\splash.png"); // 37
    sprite.CreateSprite(L"assets\\sprites\\splashspooky.png"); // 38
    sprite.CreateSprite(L"assets\\sprites\\micro.png"); // 39
    sprite.CreateSprite(L"assets\\sprites\\scarejam.png"); // 40
    sprite.CreateSprite(L"assets\\sprites\\leftclick.png"); // 41
    sprite.CreateSprite(L"assets\\sprites\\help.png"); // 42

    sprite.CreateAnimation(std::vector<int>{13, 14}, 20.0f); // 0
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

int EntityManager::CountAliveEnemies(int roomIndex)
{
    const int ri = roomIndex < 0 ? m_curRoomIndex : roomIndex;
    if (ri == -1) return 0;
    int count = 0;
    for (int pass = 0; pass < 3; ++pass)
    {
        auto& entities = !pass ? m_rooms[ri] : (pass==1 ? m_omniEntities : m_entitiesToAdd);
        for (auto& e : entities)
        {
            if (e->CanDie())
                ++count;
        }
    }
    return count;
}
void EntityManager::SetPause(bool p)
{
    m_paused = p;
}

void EntityManager::PlayerEntersRoom(int roomIndex)
{
    if (roomIndex < 0 || roomIndex >= (int)m_rooms.size()) return;
    
    for (int pass = 0; pass < 2; ++pass)
    {
        auto& entities = !pass ? m_rooms[roomIndex] : m_omniEntities;
        for (auto& e : entities)
        {
            if (e->IsValid())
                e->PlayerEntersRoom(roomIndex);
        }
    }
}

void EntityManager::PlayerLeavesRoom(int roomIndex)
{
    if (roomIndex < 0 || roomIndex >= (int)m_rooms.size()) return;

    for (int pass = 0; pass < 2; ++pass)
    {
        auto& entities = !pass ? m_rooms[roomIndex] : m_omniEntities;
        for (auto& e : entities)
        {
            if (e->IsValid())
                e->PlayerLeavesRoom(roomIndex);
        }
    }
}

void EntityManager::PlayerFinishesRoom()
{
    for (int pass = 0; pass < 2; ++pass)
    {
        auto& entities = !pass ? m_rooms[m_curRoomIndex] : m_omniEntities;
        for (auto& e : entities)
        {
            if (e->IsValid())
                e->PlayerFinishesRoom();
        }
    }
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
    auto gameRes = DX::GameResources::instance;
    if (!SupportPass(pass) ||gameRes->IsPaused()) 
        return;

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
    sprite.Draw2D(7, XMFLOAT2(0, 0), XMFLOAT2(0.01f, 0.01f), 0);

    // we draw the life here
    {
        bool draw = true;
        const float life = std::max(0.0f, std::min(1.0f,camera.m_life));
        if (life < 0.5f )
            draw = (gameRes->m_frameCount % 50) < 25; // blinking when little left
        if (draw)
        {
            const XMFLOAT2 barSize(0.5f*life, 0.035f);
            const XMFLOAT2 pos(-1.0f + barSize.x*0.5f, -1.0f + barSize.y*0.5f + 0.1f);
            sprite.Draw2D(33, pos, barSize, 0);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// EntityRoomChecker_AllDead
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EntityRoomChecker_AllDead::Update(float stepTime, const CameraFirstPerson& camera)
{
    // check there's no enemy entity
    auto gameRes = DX::GameResources::instance;
    if (gameRes->m_frameCount % 10 == 5 && m_totalTime > 2.0f )
    {
        auto& entityMgr = gameRes->m_entityMgr;
        const int count = entityMgr.CountAliveEnemies();
        if (!count)
        {
            gameRes->OpenCurrentRoom();
            Invalidate();
        }
    }
}

void EntityRoomChecker_AllDead::PlayerEntersRoom(int roomIndex)
{
    m_totalTime = 0.0f;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// PROJECTILE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityProjectile::EntityProjectile(const XMFLOAT3& pos, int spriteNdx, float speed, const XMFLOAT3& dir, bool receiveHit )
    : Entity(SPRITE3D | ANIMATION3D | (receiveHit?ACCEPT_RAYCAST:0)), m_firstTime(true), m_speed(speed)
    , m_waitToCheck(-1.0f), m_killer(false)
{
    m_collidePlayer = true;
    m_pos = pos;
    m_animDir = dir;
    m_timeOut = 10.0f; // max time out for projectiles (just in case)
    m_spriteIndex = spriteNdx;
    m_size = XMFLOAT2(0.5f, 0.5f);
    m_killer = (spriteNdx == 9);
}

void EntityProjectile::Update(float stepTime, const CameraFirstPerson& camera)
{
    m_firstTime = false;

    XMFLOAT3 newPos = XM3Mad(m_pos, m_animDir, m_speed*stepTime);

    // collides?
    auto gameRes = DX::GameResources::instance;
    auto& map = gameRes->m_map;
    XMFLOAT3 hit;
    bool wasHit = false;
    m_waitToCheck -= stepTime;
    if (m_waitToCheck <= 0.0f)
    {
        wasHit = map.RaycastSeg(m_pos, newPos, hit, 0.5f); // against level?
        if (!wasHit && newPos.y <= m_size.y*0.5f)
            wasHit = true;
    }

    if (!wasHit) // no collided map, against player then?
    {
        XMFLOAT3 toPl = XM3Sub(camera.GetPosition(), newPos);
        float distToPl = XM3LenSq(toPl);
        wasHit = distToPl < camera.RadiusCollideSq();
        if (wasHit)
            gameRes->HitPlayer(0.15f, m_killer);

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

void EntityProjectile::DoHit()
{
    if ( m_killer )
        Invalidate(KILLED);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ////////////////////////////// SHOOT HIT
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityShootHit::EntityShootHit(const XMFLOAT3& pos) // 20 21 | 22 23
    : Entity(SPRITE3D), m_lastFrame(false)
{
    m_timeOut = 0.5f;
    m_spriteIndex = RND.Get01();
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
EntityTeleport::EntityTeleport(const XMUINT2& pos, int targetRoom) // 25 26 27
    : Entity(SPRITE3D), m_animDir(1), m_targetRoom(targetRoom)
{
    m_spriteIndex = 25;
    m_size = XMFLOAT2(0.5f, 0.75f);
    m_pos.x = (float)pos.x + 0.5f;
    m_pos.z = (float)pos.y + 0.5f;
    m_pos.y = m_size.y*0.5f + 0.05f;
    PlaySoundDistance(DX::GameResources::SFX_BUZZ, 8.0f, true);
}

void EntityTeleport::Update(float stepTime, const CameraFirstPerson& camera)
{
    auto gameRes = DX::GameResources::instance;

    if (m_totalTime >= 0.1f)
    {
        m_spriteIndex += m_animDir;
        if (m_spriteIndex > 27)
        {
            m_spriteIndex = 26;
            m_animDir = -1;
        }
        else if (m_spriteIndex < 25)
        {
            m_spriteIndex = 26;
            m_animDir = +1;
        }
        m_totalTime = 0.0f;
    }

    if (gameRes->m_frameCount % 4 == 0)
        UpdateSoundDistance(DX::GameResources::SFX_BUZZ, 8.0f);

    const float distToPlSq = DistSqToPlayer();
    if (m_totalTime >= 0.0f && distToPlSq < camera.RadiusCollideSq())
        gameRes->TeleportToRoom(m_targetRoom);
    else if (m_totalTime < 0.0f && distToPlSq > camera.RadiusCollideSq())
        m_totalTime = 0.0f;
}

void EntityTeleport::PlayerEntersRoom(int ri)
{
    PlaySoundDistance(DX::GameResources::SFX_BUZZ, 8.0f, true);
    m_totalTime = -FLT_MAX;
}

void EntityTeleport::PlayerLeavesRoom(int ri)
{
    DX::GameResources::instance->SoundStop(DX::GameResources::SFX_BUZZ);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////////////// ANIMATION
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityAnimation::EntityAnimation(const XMFLOAT3& pos, const XMFLOAT2& size, const std::vector<int>& indices, float fps, 
    bool finishOnEnd, const std::vector<XMFLOAT2>* sizes)
    : Entity(SPRITE3D)
{
    if (fps == 0.0f) throw std::exception("fps must be > 0.0f");
    if (indices.empty()) throw std::exception("indices must contain at least 1 element");
    if (sizes && sizes->size() != indices.size()) throw std::exception("sizes must have == count of elements than indices");

    m_pos = pos;
    m_size = size;
    m_indices = indices;
    m_period = 1.0f / fps;
    m_finishOnEnd = finishOnEnd;
    m_curIndex = 0;
    m_spriteIndex = indices.front();
    if (sizes)
        m_sizes = std::make_unique<std::vector<XMFLOAT2>>(*sizes);
}

void EntityAnimation::Update(float stepTime, const CameraFirstPerson& camera)
{
    m_totalTime += stepTime;
    if (m_totalTime >= m_period)
    {
        ++m_curIndex;
        if (m_curIndex >= (int)m_indices.size() && m_finishOnEnd)
        {
            Invalidate();
            return;
        }
        m_totalTime -= m_period;
    }

    const int boundIndex = m_curIndex % m_indices.size();
    m_spriteIndex = m_indices[boundIndex];
    if ( m_sizes )
        m_size = m_sizes->at(boundIndex);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////////////// RANDOM SOUND
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityRandomSound::EntityRandomSound(uint32_t sound, float time0, float time1, bool endOnPlay, bool playOnFirst)
    : m_sound(sound), m_time0(time0), m_time1(time1), m_end(endOnPlay)
{
    if (playOnFirst)
        DX::GameResources::instance->SoundPlay(m_sound, false);
    else
        m_waitTime = DX::GameResources::instance->m_random.GetF(m_time0, m_time1);
}

void EntityRandomSound::Update(float stepTime, const CameraFirstPerson& camera)
{
    m_waitTime -= stepTime;
    if (m_waitTime <= 0.0f)
    {
        DX::GameResources::instance->SoundPlay(m_sound, false);
        if ( m_end )
            Invalidate();
        m_waitTime = m_waitTime = DX::GameResources::instance->m_random.GetF(m_time0, m_time1);
    }
}

void EntityRandomSound::PlayerLeavesRoom(int ri)
{
    DX::GameResources::instance->SoundStop(m_sound);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////////////// ENEMY BASE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityEnemyBase::EntityEnemyBase()
    : Entity(SPRITE3D | ACCEPT_RAYCAST), m_modulateTime(-1.0f)
    , m_originColor(1, 1, 1, 1)
{
}

void EntityEnemyBase::Update(float stepTime, const CameraFirstPerson& camera)
{
    if (m_modulateTime >= 0.0f)
    {
        const float s = (std::max(m_modulateTime, 0.0f) / m_modulateDuration);
        const float onemins = 1.0f - s;
        m_modulate.x = s*m_modulateTargetColor.x + onemins*m_originColor.x;
        m_modulate.y = s*m_modulateTargetColor.y + onemins*m_originColor.y;
        m_modulate.z = s*m_modulateTargetColor.z + onemins*m_originColor.z;
        m_modulate.w = s*m_modulateTargetColor.w + onemins*m_originColor.w;
        m_modulateTime -= stepTime;
    }
}

void EntityEnemyBase::ModulateToColor(const XMFLOAT4& color, float duration)
{
    m_modulateDuration = m_modulateTime = duration;
    m_modulateTargetColor = color;
}

void EntityEnemyBase::FadeOut(float duration, bool invAfter)
{
    m_originColor = XMFLOAT4(0, 0, 0, 0);
    m_modulateTime = duration;
    if ( invAfter )
        Invalidate(KILLED, duration);
}

EntityProjectile* EntityEnemyBase::ShootToPlayer(int projSprIndex, float speed, const XMFLOAT3& offs, const XMFLOAT2& size, float life, bool predict, float waitTime)
{
    auto gameRes = DX::GameResources::instance;
    auto& rnd = gameRes->m_random;
    auto& cam = gameRes->m_camera;

    XMFLOAT3 origin = XM3Add(m_pos, offs);
    XMFLOAT3 end = cam.GetPosition();
    if (predict)
        XM3Add_inplace(end, XM3Mul(cam.m_movDir, 32.0f));

    XMFLOAT3 toPl = XM3Sub(end, origin);
    XM3Normalize_inplace(toPl);

    auto proj = std::make_shared<EntityProjectile>(origin, projSprIndex, speed, toPl, life>0.0f);
    proj->m_size = size;
    proj->m_life = life;
    proj->m_waitToCheck = waitTime;
    gameRes->m_entityMgr.AddEntity(proj);

    // volume depending on distance
    PlaySoundDistance(DX::GameResources::SFX_SHOOT0, 8.0f);    
    return proj.get();
}

LevelMapBSPNode* EntityEnemyBase::GetCurrentRoom()
{
    auto gameRes = DX::GameResources::instance;
    auto& l = gameRes->m_map.GetLeafAt(m_pos);
    return l ? l.get() : nullptr;
}

bool EntityEnemyBase::CanSeePlayer()
{
    auto gameRes = DX::GameResources::instance;
    XMFLOAT3 hit;
    return !gameRes->m_map.RaycastSeg(m_pos, gameRes->m_camera.GetPosition(), hit);
}

bool EntityEnemyBase::PlayerLookintAtMe(float range, bool checkLoS)
{
    auto& cam = CAM;
    auto plPos = cam.GetPosition(); plPos.y = 0.0f;
    const XMFLOAT3 myPos(m_pos.x, 0.0f, m_pos.z);
    const XMFLOAT3 toPl = XM3Normalize(XM3Sub(plPos, myPos));
    const float dot = XM3Dot(toPl, XM3Neg(cam.m_forward));
    return dot >= range && (checkLoS && CanSeePlayer() || !checkLoS);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////// PUKY
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region PUKY
EnemyPuky::EnemyPuky(const XMFLOAT3& pos)
{
    Init(pos);    
}

void EnemyPuky::Init(const XMFLOAT3& pos)
{
    m_spriteIndex = 0;
    m_pos = pos;
    m_size = XMFLOAT2(0.3f, 0.3f);
    auto room = GetCurrentRoom();
    if (!room) 
        throw std::exception("no room for puky");
    const auto& a = room->m_area;
    const float sxh = m_size.x*0.5f;
    const float syh = m_size.y*0.5f;
    m_bounds[0] = XMFLOAT2(a.m_x0 + sxh, a.m_y0 - sxh);
    m_bounds[1] = XMFLOAT2(a.m_x1 + syh, a.m_y1 - syh);
    GetNextTarget();
    auto& r = RND;
    m_totalTime = r.GetF(0.0f, 10.0f);
    m_speed = r.GetF(1.0f, 4.5f);
    m_amplitude = r.GetF(0.1f, 0.35f); 
}

void EnemyPuky::Update(float stepTime, const CameraFirstPerson& camera)
{
    EntityEnemyBase::Update(stepTime, camera);

    const XMFLOAT3 p(m_pos.x, 0.0f, m_pos.z);
    m_pos = XM3Mad(p, m_targetDir, 1.0f*stepTime);
    m_pos.y = 0.5f + cos(m_totalTime*m_speed)*m_amplitude;
    //
    if (DistSqToPlayer() <= camera.RadiusCollideSq())
    {
        DX::GameResources::instance->HitPlayer(0.15f);
        GetNextTarget();
    }
    else if (XM3LenSq(m_pos, m_nextTarget) < 0.50f*0.50f || OutOfBounds())
        GetNextTarget();
}

void EnemyPuky::DoHit()
{
    Invalidate(KILLED);
}

bool EnemyPuky::OutOfBounds() // shouldn't need this :( anyways!
{
    return (m_pos.x < m_bounds[0].x || m_pos.x > m_bounds[1].x ||
        m_pos.z < m_bounds[0].y || m_pos.z > m_bounds[1].y);
}

void EnemyPuky::GetNextTarget()
{
    auto& r = RND;
    m_nextTarget.x = r.GetF(m_bounds[0].x, m_bounds[1].x);
    m_nextTarget.y = 0.0f;
    m_nextTarget.z = r.GetF(m_bounds[0].y, m_bounds[1].y);
    const XMFLOAT3 p(m_pos.x, 0.0f, m_pos.z);
    m_targetDir = XM3Normalize(XM3Sub(m_nextTarget, p));
}
#pragma endregion

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
        if (curDistSq <= m_minDistSq )
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
#pragma region GIRL
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
    m_timeToNextShoot = RND.GetF(0.0f, 10.0f);
    if (GetNextTargetPoint())
        m_state = GOING;
    else
        m_state = WAITING;
    m_life = 1.0f;
}

void EnemyGirl::Update(float stepTime, const CameraFirstPerson& camera)
{
    EntityEnemyBase::Update(stepTime, camera);
    if (m_timeOut < FBIGVAL) return; // is dying
    m_waitingForNextTarget -= stepTime;
    auto gameRes = DX::GameResources::instance;
    auto& rnd = gameRes->m_random;
    switch (m_state)
    {
    case WAITING: 
        m_waitingForNextTarget -= stepTime;
        if (m_waitingForNextTarget <= 0.0f)
        {
            if (GetNextTargetPoint())
            {
                m_speed = rnd.GetF(2.0f, 4.5f);
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
            if ( (distToPlayerSq <= 3.0f*3.0f || m_hitTime >= 0.0f) && m_life > 0.5f )
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
                        if (gameRes->HitPlayer(0.25f))
                            gameRes->SoundPlay(DX::GameResources::SFX_GIRL, false);
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
                // catches the player
                if (distToPlayerSq <= gameRes->m_camera.RadiusCollideSq())
                {
                    if ( gameRes->HitPlayer(0.25f) )
                        gameRes->SoundPlay(DX::GameResources::SFX_GIRL, false);
                }

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
                m_waitingForNextTarget = rnd.GetF(0.0f, m_life*0.7f);
                m_state = WAITING;
            }
        }break;
    }

    // floating effect :)
    m_pos.y = 0.7f + sinf(m_totalTime*(1.0f-m_life)*8.0f)*0.05f;

    // check if it should shoot player
    if (m_timeToNextShoot <= 0.0f)
    {
        ShootToPlayer(rnd.Get01(0.3f)?9:19, rnd.GetF(3.0f, 5.0f), XMFLOAT3(-0.3f, 0, 0), XMFLOAT2(0.4f,0.2f));
        m_timeToNextShoot = rnd.GetF(1.0f, std::max(m_life*10.0f, 2.5f));
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
    ModulateToColor(XM4RED, 0.5f);
    
    float distToPl = DistSqToPlayer();
    if (distToPl == 0.0f) distToPl = 1.0f;
    distToPl = sqrt(distToPl);
    const float t = std::max(1.0f - distToPl / gameRes->m_camera.m_shotgunRange, 0.0f);
    const float MAXDAMAGE = 0.8f;
    m_life -= t*MAXDAMAGE;
    if (m_life <= 0.0f)
    {
        Die();        
    }
    else
    {
        gameRes->SoundPlay(DX::GameResources::SFX_GIRL, false);
    }
}

void EnemyGirl::Die()
{
    FadeOut(0.8f, true);
    DX::GameResources::instance->SoundStop(DX::GameResources::SFX_GIRL);
    PlaySoundDistance(DX::GameResources::SFX_GIRLDIES, 8.0f);
}

bool EnemyGirl::GetNextTargetPoint()
{
    if (!m_roomNode)
        return false;

    const uint32_t cx = (int)m_pos.x;
    const uint32_t cy = (int)m_pos.z;

    const auto& area = m_roomNode->m_area;
    
    // clearance
    XMUINT2 allowedMoves[8], p;
    static const XMINT2 offsets[8] = {
        XMINT2(-1,-1), XMINT2(0,-1), XMINT2(1,-1),
        XMINT2(-1,0), XMINT2(1,0),
        XMINT2(-1,+1), XMINT2(0,+1), XMINT2(+1,+1) };
    int c = 0;
    for (int i = 0; i < 8; ++i)
    {
        p.x = cx + offsets[i].x;
        p.y = cy + offsets[i].y;        
        if (m_roomNode->Clearance(p))
            allowedMoves[c++] = p;
    }

    if (c == 0)
    {
        Die();
        return false;
    }

    const auto& selected = allowedMoves[RND.Get(0, c - 1)];
    m_nextTargetPoint.x = selected.x + 0.5f;
    m_nextTargetPoint.y = m_pos.y;
    m_nextTargetPoint.z = selected.y + 0.5f;
    return true;
}
#pragma endregion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////// BLACK HANDS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EnemyBlackHands::EnemyBlackHands(const XMUINT2& tile, int n)
    : m_origN(n)
{
    auto& rnd = RND;
    float x = (float)tile.x;
    float z = (float)tile.y;
    m_size = XMFLOAT2(1.0f, 1.0f);
    m_pos = XMFLOAT3(x + 0.5f, 0.50f, z + 0.5f);
    m_hands.reserve(n);
    Hand h; 
    h.distToCamSq = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        h.pos.x = rnd.GetF(x, x + 1.0f);
        h.pos.z = rnd.GetF(z, z + 1.0f);
        h.size = XMFLOAT2(rnd.GetF(0.3f, 0.45f), rnd.GetF(0.3f, 0.45f));
        h.pos.y = h.size.y*0.5f;
        h.t = rnd.GetF(0.0f, 5.0f);
        m_hands.push_back(h);
    }
    m_life = 1.0f;
}

void EnemyBlackHands::Update(float stepTime, const CameraFirstPerson& camera)
{
    if (m_hands.empty()) return;
    EntityEnemyBase::Update(stepTime, camera);
    auto gameRes = DX::GameResources::instance;
    UpdateSort(camera); 
    
    for (auto& h : m_hands)
    {
        h.t += stepTime;
    }

    if (DistSqToPlayer() < 5.0f*5.0f 
        && PlayerLookintAtMe(0.97f) 
        && m_totalTime>3.0f 
        && CanSeePlayer())
    {
        DoHit();
        m_totalTime = 0.0f;
    }
}

void EnemyBlackHands::Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite)
{
    EntityEnemyBase::Render(pass, camera, sprite);
    if (pass == PASS_SPRITE3D)
    {
        XMFLOAT3 p;
        for (const auto& h : m_hands)
        {
            p.x = h.pos.x;
            p.y = h.pos.y + sin(h.t*8.0f)*h.size.y*0.5f;
            p.z = h.pos.z;
            sprite.Draw3D(1, p, h.size, m_modulate );
        }
    }
}

void EnemyBlackHands::DoHit()
{
    ModulateToColor(XM4RED, 0.5f);    
    ShootToPlayer(1, RND.GetF(3.0f,4.5f), XMFLOAT3(0,CAM.m_height,0), m_hands.back().size, 0.1f, true, 0.2f);
    m_hands.pop_back();
    if (m_hands.empty())
    {
        Invalidate(KILLED);
    }
    m_life = float(m_hands.size()) / m_origN;
}

void EnemyBlackHands::UpdateSort(const CameraFirstPerson& camera)
{
    auto gameRes = DX::GameResources::instance;
    const int ndx = (gameRes->m_frameCount % m_hands.size()); // every frame we compute distance to diff hand
    
    if (ndx == 0)// every time we compute dist to all hands, we sort them (we pay the first time)
    {
        std::sort(m_hands.begin(), m_hands.end(), [this](const auto& a, const auto& b) -> bool
        {
            return a.distToCamSq > b.distToCamSq;
        });
    }
    
    auto& h = m_hands[ndx];
    auto cp = camera.GetPosition();
    h.distToCamSq = XM3LenSq(h.pos, cp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////// PUMPKIN
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EnemyPumpkin::EnemyPumpkin(const XMFLOAT3& pos)
    : m_timeInOuter(0.0f)
{
    m_spriteIndex = 30;
    m_size = XMFLOAT2(0.3f, 0.3f);
    m_pos = pos;
    m_pos.y = m_size.y*0.5f;
}

void EnemyPumpkin::Update(float stepTime, const CameraFirstPerson& camera)
{
    EntityEnemyBase::Update(stepTime, camera);

    const float distSqToPl = DistSqToPlayer();
    if (distSqToPl < radiusInnerSq || m_timeInOuter > maxTimeToExplode)
    {
        DoHit();
    }
    else if (distSqToPl < radiusOuterSq)
    {
        if (m_modulateTime <= 0.0f)
        {
            const float t = 0.1f + (1.0f - (distSqToPl - radiusInnerSq) / (radiusOuterSq - radiusInnerSq))*0.4f;
            const float s = 0.1f + (m_timeInOuter / maxTimeToExplode)*0.4f;
            ModulateToColor(XM4RED, std::min(t,s));
            DX::GameResources::instance->SoundPlay(DX::GameResources::SFX_BEEP0, false);
        }
        m_timeInOuter += stepTime;
    }
    else
        m_timeInOuter = 0.0f;
}

void EnemyPumpkin::DoHit()
{
    using namespace DX;
    auto gameRes = GameResources::instance;
    if (gameRes->IsPaused()) return;
    gameRes->SoundPlay(GameResources::SFX_EXPL0,false);
    Invalidate(KILLED);
    if (DistSqToPlayer() <= radiusDamageSq)
        gameRes->HitPlayer(0.35f);
    else
    {
        gameRes->FlashScreen(0.5f, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f));
        gameRes->ConsiderSpawnItem(m_pos,0.4f);
    }    
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////// DECORATION
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntitySingleDecoration::EntitySingleDecoration(DecorType type, const XMFLOAT3& pos)
    : m_type(type)
{
    m_pos = pos;
    auto& rnd = DX::GameResources::instance->m_random;
    // we know the size and sprite index and pos.y of every decoration type
    switch (m_type)
    {
    case BODYPILE:
        m_spriteIndex = 11;
        m_size = XMFLOAT2( rnd.GetF(1.2f,1.7f), rnd.GetF(0.5f,0.7f));
        break;
    case GRAVE:
    {
        m_spriteIndex = 6;
        const float f = rnd.GetF(0.45f, 0.55f);
        m_size = XMFLOAT2(f, f);
    }break;
    case TREEBLACK:
        m_spriteIndex = 8;
        m_size = XMFLOAT2(0.7f, rnd.GetF(1.7f, 2.0f));
        break;
    case GREENHAND:
        m_spriteIndex = 3;
        m_size = XMFLOAT2(0.2f,0.3f);
        break;
    case BLACKHAND:
        m_spriteIndex = 1;
        m_size = XMFLOAT2(0.25f, 0.25f);
        break;
    case SKULL: 
        m_spriteIndex = 32;
        m_size = XMFLOAT2(0.25f, 0.25f);
        break;
    }
    m_pos.y = m_size.y*0.5f;
}

XMFLOAT2 EntitySingleDecoration::GetSizeOf(DecorType type)
{
    XMFLOAT2 s;
    switch (type)
    {
    case BODYPILE: s = XMFLOAT2(1.5f, 0.6f); break;
    case GRAVE: s = XMFLOAT2(0.5f, 0.5f); break;
    case TREEBLACK: s = XMFLOAT2(0.7f, 1.9f); break;
    case GREENHAND: s = XMFLOAT2(0.2f, 0.3f); break;
    case BLACKHAND: s = XMFLOAT2(0.25f, 0.25f); break;
    case SKULL: s = XMFLOAT2(0.25f, 0.25f); break;
    }
    return s;
}


void EntitySingleDecoration::DoHit()
{
    auto gameRes = DX::GameResources::instance;    
    switch (m_type)
    {
    case BODYPILE:
        gameRes->SoundPlay(DX::GameResources::SFX_HIT1, false);
        break;
    case GRAVE:
        {
        gameRes->SoundPlay(DX::GameResources::SFX_HIT1, false);
        }break;
    case TREEBLACK:
        gameRes->SoundPlay(DX::GameResources::SFX_HIT1, false);
        break;
    case GREENHAND:
        gameRes->SoundPlay(DX::GameResources::SFX_BROKEN, false);
        Invalidate(KILLED);
        break;
    case BLACKHAND:
        gameRes->SoundPlay(DX::GameResources::SFX_HIT1, false);
        break;
    case SKULL: 
        gameRes->SoundPlay(DX::GameResources::SFX_BROKEN, false);
        gameRes->ConsiderSpawnItem(m_pos, 0.9f);
        Invalidate(KILLED);
        break;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////// GHOST
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EnemyGhost::EnemyGhost(const XMFLOAT3& pos)
{
    m_spriteIndex = 4;
    m_pos = pos;
    m_size = XMFLOAT2(0.5f, 1.0f);
    m_pos.y = m_size.y*0.5f + 0.1f;
    m_roomNode = DX::GameResources::instance->m_map.GetLeafAt(m_pos).get();
    m_timeToJump = -1.0f;
    m_life = 1.0f;
}

void EnemyGhost::Update(float stepTime, const CameraFirstPerson& camera)
{
    EntityEnemyBase::Update(stepTime, camera);

    if (m_timeOut < FBIGVAL) return;

    m_timeToJump -= stepTime;
    if (m_timeToJump < -5.0f)
        JumpNextTargetPoint();

    m_pos.y = (m_size.y*0.5f + 0.1f)+sin(m_totalTime*2.0f)*0.15f;
    const float distSq = DistSqToPlayer();
    if ( distSq <= camera.RadiusCollideSq())
    {
        DX::GameResources::instance->HitPlayer(0.25f);
        JumpNextTargetPoint();
    }

    if (PlayerLookintAtMe(0.98f) && distSq < 8.0f*8.0f)
    {
        DX::GameResources::instance->m_curDensityMult = 2.0f;
        JumpNextTargetPoint();
    }
}

void EnemyGhost::DoHit()
{
    if (m_timeOut < FBIGVAL) return;
    auto gameRes = DX::GameResources::instance;

    float distToPl = DistSqToPlayer();
    if (distToPl == 0.0f) distToPl = 1.0f;
    distToPl = sqrt(distToPl);
    const float t = std::max(1.0f - distToPl / gameRes->m_camera.m_shotgunRange, 0.0f);
    const float MAXDAMAGE = 0.8f;
    m_life -= t*MAXDAMAGE;
    ModulateToColor(XM4RED, 0.5f);
    if (m_life <= 0.0f)
    {
        Die();
    }
    else
    {
        PlaySoundDistance(DX::GameResources::SFX_OUCH, 6.0f);       
        gameRes->SoundPitch(DX::GameResources::SFX_OUCH, -1.0f);
    }
}

void EnemyGhost::Die()
{
    FadeOut(0.8f, true);
    PlaySoundDistance(DX::GameResources::SFX_DIE0, 8.0f);
}

void EnemyGhost::JumpNextTargetPoint()
{
    if (m_timeToJump >= 0.0f)
        return;
    m_timeToJump = 0.65f;

    const uint32_t cx = (int)m_pos.x;
    const uint32_t cy = (int)m_pos.z;

    const auto& area = m_roomNode->m_area;

    // clearance    
    XMUINT2 allowedMoves[8];
    XMUINT2 p;
    static const XMINT2 offsets[8] = {
        XMINT2(-1,-1), XMINT2(0,-1), XMINT2(1,-1),
        XMINT2(-1,0), XMINT2(1,0),
        XMINT2(-1,+1), XMINT2(0,+1), XMINT2(+1,+1) };
    int c = 0;
    for (int i = 0; i < 8; ++i)
    {
        p.x = cx + offsets[i].x;
        p.y = cy + offsets[i].y;
        if (m_roomNode->Clearance(p))
            allowedMoves[c++] = p;
    }

    if (c == 0)
    {
        Die();
        return;
    }

    const auto& selected = allowedMoves[RND.Get(0, c - 1)];
    m_pos.x = selected.x + 0.5f;
    m_pos.z = selected.y + 0.5f;
    PlaySoundDistance(DX::GameResources::SFX_DASH, 8.0f);
    DX::GameResources::instance->SoundPitch(DX::GameResources::SFX_DASH, RND.GetF(-0.9f, 0.9f));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////// BOSS CHECK READY
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EntityCheckBossReady::Update(float stepTime, const CameraFirstPerson& camera)
{
    auto gameRes = DX::GameResources::instance;
    if (gameRes->m_frameCount % 10 == 3)
    {
        const auto& rooms = gameRes->m_map.GetRooms();
        for (size_t i = 0; i < rooms.size(); ++i)
        {
            if (!rooms[i]->m_finished)
                return;
        }

        // boss is ready
        gameRes->BossIsReady();
        Invalidate();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////// BOSS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EnemyBoss::EnemyBoss(const XMFLOAT3& pos)
    : m_timeUntilShoot(-1.0f), m_state(MOVING), m_timeUntilNextState(-1.0f)
    , m_timeToNextJump(-1.0f), m_movingSpeed(4.0f)
{
    m_flags = SPRITE3D | SPRITE2D | ACCEPT_RAYCAST;
    m_pos = pos;
    m_size = XMFLOAT2(0.7f, 1.9f);
    m_pos.y = m_size.y*0.5f + 0.05f;
    m_spriteIndex = 5;
    m_life = 1.0f;
    auto gameRes = DX::GameResources::instance;
    m_roomNode = gameRes->m_map.GetLeafAt(m_pos).get();
    if (!m_roomNode)
        throw std::exception("No room for boss");
    m_roomNode->m_tag = 0x55000033;
    m_roomNode->m_finished = false;
    auto& rnd = gameRes->m_random;
    const int n = rnd.Get(4, 10);
    for (int i = 0; i < n; ++i)
    {
        auto decopos = m_roomNode->GetRandomXZWithClearance();
        gameRes->m_entityMgr.AddEntity(std::make_shared<EntitySingleDecoration>(SKULL, decopos), m_roomNode->m_leafNdx);
    }
    gameRes->m_entityMgr.CreateEntities_Pumpkin(m_roomNode, rnd.Get(2, 10), 90);
    SelectNextMovingPoint();
}

void EnemyBoss::Update(float stepTime, const CameraFirstPerson& camera)
{
    EntityEnemyBase::Update(stepTime, camera);
    if (!m_roomNode) return;
    auto gameRes = DX::GameResources::instance;
    auto& rnd = RND;

    m_timeUntilShoot -= stepTime;
    m_timeUntilNextState -= stepTime;

    switch (m_state) 
    {
    case JUMPINGBLACK:
    case JUMPING:
        m_timeToNextJump -= stepTime;
        if (m_timeToNextJump <= 0.0f)
        {
            Jump();
        }
        if (m_state == JUMPINGBLACK)
        {
            DX::GameResources::instance->m_curDensityMult = 3.0f;
            if (m_timeUntilNextState <= 0.0f)
                m_state = JUMPING;
        }
        break;
    case MOVING:
        {
            XMFLOAT3 toTarget = XM3Sub(m_nextTargetPoint, m_pos);
            const float toTargetSq = XM3LenSq(toTarget);
            if (toTargetSq < 0.25f*0.25f)
            {
                SelectNextMovingPoint();
            }
            else
            {
                XM3Mul_inplace(toTarget, 1.0f / sqrt(toTargetSq));
                XM3Mad_inplace(m_pos, toTarget, m_movingSpeed*stepTime);
            }
        }break;
    }

    float probDie = 0.1f;
    float projSpeed = 3.0f;
    float rngNS = 3.0f;
    float oscSpeed = 4.0f;
    const float distToPlSq = this->DistSqToPlayer();
    if (m_life <= 0.5f)
    {
        probDie = 0.25f;
        projSpeed = 4.5f;
        rngNS = 2.0f;
        oscSpeed = 8.0f;
        m_movingSpeed = 6.0f;
    }

    if (distToPlSq < camera.RadiusCollideSq())
    {
        gameRes->HitPlayer(0.40f);
    } 
    else if (distToPlSq < 2.5f*2.5f)
    {
        rngNS = 1.0f;
        m_timeUntilShoot -= stepTime*2.0f;
    }
    else if (distToPlSq < 4.0f*4.0f)
    {
        m_timeUntilShoot -= stepTime;
    }
    m_pos.y = m_size.y*0.5f + 0.05f + fabs(sin(m_totalTime*oscSpeed)*0.05f);
    if (m_timeUntilShoot <= 0.0f)
    {
        ShootToPlayer(rnd.Get01(probDie) ? 9 : 19, projSpeed, XMFLOAT3(0, 0.5f, 0), XMFLOAT2(0.5f, 0.5f));
        m_timeUntilShoot = rnd.GetF(0.5f, rngNS);
    }
}

void EnemyBoss::Render(RenderPass pass, const CameraFirstPerson& camera, SpriteManager& sprite)
{
    if (pass == PASS_SPRITE2D)
    {
        bool draw = true;
        const float life = Clamp(m_life, 0.0f, 1.0f);
        const XMFLOAT2 barSize(life, 0.04f);
        const XMFLOAT2 pos(-1.0f + barSize.x*0.5f, 1.0f - barSize.y*0.5f - 0.1f);
        sprite.Draw2D(34, pos, barSize, 0);
    }
    else
        EntityEnemyBase::Render(pass, camera, sprite);
}

void EnemyBoss::Die()
{
    FadeOut(0.8f, true);
}

void EnemyBoss::DoHit()
{
    auto gameRes = DX::GameResources::instance;
    ModulateToColor(XM4RED, 0.5f);

    float distToPl = DistSqToPlayer();
    if (distToPl == 0.0f) distToPl = 1.0f;
    distToPl = sqrt(distToPl);
    const float t = std::max(1.0f - distToPl / gameRes->m_camera.m_shotgunRange, 0.0f);
    const float MAXDAMAGE = 0.4f;
    m_life -= t*MAXDAMAGE;
    if (m_life <= 0.0f)
    {
        Die();
    }
    else
    {
        PlaySoundDistance(DX::GameResources::SFX_LAUGH, 8.0f, false);
        ShootToPlayer(9, 5.0f, XMFLOAT3(0, 0.5f, 0), XMFLOAT2(0.5f, 0.4f));
        if ( gameRes->m_random.Get01() )
            JumpRandom();
    }

    static const uint32_t densfunc[STATEMAX] = { 40,80,100 };
    m_state = (eState)gameRes->m_random.GetWithDensity(densfunc, STATEMAX);
    m_timeUntilNextState = 5.0f;
    SelectNextMovingPoint();
}

void EnemyBoss::JumpRandom()
{
    m_pos = m_roomNode->GetRandomXZWithClearance();
    DX::GameResources::instance->SoundPitch(DX::GameResources::SFX_DASH, -0.5f);
    PlaySoundDistance(DX::GameResources::SFX_DASH, 8.0f);
}

void EnemyBoss::Jump()
{
    auto& rnd = RND;
    m_pos = GetRandomAdjacent();
    DX::GameResources::instance->SoundPitch(DX::GameResources::SFX_DASH, rnd.GetF(-0.9f, 0.9f));
    PlaySoundDistance(DX::GameResources::SFX_DASH, 8.0f);
    m_timeToNextJump = rnd.GetF(0.4f, 1.0f);
}

void EnemyBoss::SelectNextMovingPoint()
{
    m_nextTargetPoint = GetRandomAdjacent();
}

XMFLOAT3 EnemyBoss::GetRandomAdjacent()
{
    const uint32_t cx = (int)m_pos.x;
    const uint32_t cy = (int)m_pos.z;

    const auto& area = m_roomNode->m_area;

    // clearance    
    XMUINT2 allowedMoves[8];
    XMUINT2 p;
    static const XMINT2 offsets[8] = {
        XMINT2(-1,-1), XMINT2(0,-1), XMINT2(1,-1),
        XMINT2(-1,0), XMINT2(1,0),
        XMINT2(-1,+1), XMINT2(0,+1), XMINT2(+1,+1) };
    int c = 0;
    for (int i = 0; i < 8; ++i)
    {
        p.x = cx + offsets[i].x;
        p.y = cy + offsets[i].y;
        if (m_roomNode->Clearance(p))
            allowedMoves[c++] = p;
    }

    if (c == 0)
    {
        Die();
        return m_pos;
    }
    auto& rnd = RND;
    const auto& selected = allowedMoves[rnd.Get(0, c - 1)];
    return XMFLOAT3(selected.x + 0.5f, 0, selected.y + 0.5f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////// ITEM
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EntityItem::EntityItem(ItemType type, const XMFLOAT3& pos, float amount)
    : Entity(SPRITE3D)
{
    switch (type)
    {
    case ITEM_LIFE: 
        m_size = XMFLOAT2(0.25f, 0.3f);
        m_spriteIndex = 36;
        break;
    case ITEM_CANDY: 
        m_size = XMFLOAT2(0.3f, 0.2f); 
        m_spriteIndex = 35;
        break;
    }
    m_pos = pos;
    m_pos.y = m_size.y*0.5f + 0.05f;
    m_type = type;
    m_amount = amount;
}

void EntityItem::Update(float stepTime, const CameraFirstPerson& camera)
{
    m_pos.y = m_size.y*0.5f + 0.1f + sin(m_totalTime*4.0f)*0.02f;
    if ( DistSqToPlayer() < camera.RadiusCollideSq() )
    {
        Pickup();
        Invalidate();
    }
}

void EntityItem::Pickup()
{
    auto gameRes = DX::GameResources::instance;
    if (gameRes->IsPaused()) return;
    gameRes->SoundPlay(DX::GameResources::SFX_ITEMPICK,false);
    gameRes->FlashScreen(0.5f, XMFLOAT4(0.7f, 0.7f, 1, 1));
    gameRes->UpdateHeartVolumeAndPitch();
    switch (m_type)
    {
    case ITEM_LIFE:
        gameRes->m_camera.AddLife(m_amount);
        gameRes->SoundPitch(DX::GameResources::SFX_ITEMPICK, -2.0f);
        break;
    case ITEM_CANDY:
        gameRes->m_camera.AddBullets((int)m_amount);
        gameRes->SoundPitch(DX::GameResources::SFX_ITEMPICK, -1.0f);
        break;
    }
}

#undef CAM
#undef RND