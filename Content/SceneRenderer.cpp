#include "pch.h"
#include "SceneRenderer.h"
#include "..\Common\DirectXHelper.h"

#include "CollisionAndSolving2D.h"
#include "Sprite.h"
#include "GlobalFlags.h"

using namespace SpookyAdulthood;

using namespace DirectX;
using namespace Windows::Foundation;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
SceneRenderer::SceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_deviceResources(deviceResources),
    m_map(deviceResources)
{
    CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();

    m_mapSettings.m_tileCount = XMUINT2(35, 35);
    m_mapSettings.m_minTileCount = XMUINT2(4, 4);
    m_mapSettings.m_maxTileCount = XMUINT2(15, 15);
    m_map.Generate(m_mapSettings);
    SpawnPlayer();    
}

void SceneRenderer::SpawnPlayer()
{
    XMUINT2 mapPos = m_map.GetRandomPosition();
    XMFLOAT3 p(mapPos.x + 0.5f, 0, mapPos.y + 0.5f);
    m_camera.SetPosition(p);
    auto gameRes = m_deviceResources->GetGameResources();
    if (gameRes && gameRes->m_audioEngine)
    {
        gameRes->SoundPlay(DX::GameResources::SFX_BREATH);
        gameRes->SoundPlay(DX::GameResources::SFX_PIANO);
        gameRes->SoundPlay(DX::GameResources::SFX_HEART);
    }
}

// Initializes view parameters when the window size changes.
void SceneRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	const float aspectRatio = outputSize.Width / outputSize.Height;
    // portrait or snapped view.
    float fovAngleY = CAM_DEFAULT_FOVY * XM_PI / 180.0f;
	if (aspectRatio < 1.0f)
		fovAngleY *= 2.0f;
    m_camera.ComputeProjection(fovAngleY, aspectRatio, 0.01f, 100.0f, m_deviceResources->GetOrientationTransform3D());
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void SceneRenderer::Update(DX::StepTimer const& timer)
{
    if (!m_loadingComplete)
        return;

    // Update map and camera (input, collisions and visibility)
    m_map.Update(timer, m_camera);
    m_camera.Update(timer, 
    [this](XMVECTOR curPos, XMVECTOR nextPos, float radius)->XMVECTOR  // CALLED FOR COLLISION HANDLING
    { 
        if (GlobalFlags::CollisionsEnabled)
        {
            XMFLOAT2 curPos2D(XMVectorGetX(curPos), XMVectorGetZ(curPos));
            XMFLOAT2 nextPos2D(XMVectorGetX(nextPos), XMVectorGetZ(nextPos));
            XMFLOAT2 solved2D = CollisionAndSolving2D(m_map.GetCurrentCollisionSegments(), curPos2D, nextPos2D, radius);
            XMVECTOR solved3D = XMVectorSet(solved2D.x, XMVectorGetY(nextPos), solved2D.y, 0.0f);
            return solved3D;
        }
        else 
            return nextPos;
    },
    [this](CameraFirstPerson::eAction ac) // CALLED WHEN ACTION (shoot)
    {
        auto gameRes = m_deviceResources->GetGameResources();
        if (!gameRes) return;
        auto audio = gameRes->m_audioEngine.get();
        if (!audio) return;
        gameRes->SoundPlay(DX::GameResources::SFX_SHOTGUN, false);
        gameRes->m_sprite.CreateAnimationInstance(0, 0);
    });

    // Audio
    UpdateAudio(timer);
    

    if (GlobalFlags::GenerateNewLevel)
    {
        GlobalFlags::GenerateNewLevel = false;
        m_map.Generate(m_mapSettings);
        m_map.GenerateThumbTex(m_mapSettings.m_tileCount);
    }

    if (GlobalFlags::SpawnPlayer)
    {
        GlobalFlags::SpawnPlayer = false;
        SpawnPlayer();
    }

    // thumbnail map update (every N frames)
    if (timer.GetFrameCount() % 30 == 0)
    {
        XMUINT2 ppos = m_map.ConvertToMapPosition(m_camera.GetPosition());
        m_map.GenerateThumbTex(m_mapSettings.m_tileCount,&ppos);
    }
    
}
void SceneRenderer::UpdateAudio(DX::StepTimer const& timer)
{
    auto gameRes = m_deviceResources->GetGameResources();
    if (!gameRes) return;
    auto audio = gameRes->m_audioEngine.get();
    if (!audio) return;

    if (m_camera.m_moving)
    {
        gameRes->SoundResume(DX::GameResources::SFX_WALK);
        gameRes->SoundPitch(DX::GameResources::SFX_WALK, m_camera.m_running ? 0.5f : 0.0f);
    }
    else
    {
        gameRes->SoundPause(DX::GameResources::SFX_WALK);
    }
}

// Renders one frame using the vertex and pixel shaders.
void SceneRenderer::Render()
{
	// Loading is asynchronous.
	if (!m_loadingComplete)
		return;

    // LEVEL rendering
    m_map.Render(m_camera);
    
    /*
    auto& sprite = m_deviceResources->GetGameResources()->m_sprite;

    // SPRITEs rendering
    sprite.Begin3D(m_camera);
    sprite.Draw3D(0, XMFLOAT3(5, 0.45f, 6), XMFLOAT2(0.3, 0.3));
    sprite.Draw3D(0, XMFLOAT3(6, 0.65f, 5), XMFLOAT2(0.3, 0.3));
    sprite.Draw3D(0, XMFLOAT3(9, 0.75f, 4), XMFLOAT2(0.3, 0.3));
    sprite.Draw3D(4, XMFLOAT3(7, 0.75f, 9), XMFLOAT2(0.7f, 1.5f));
    sprite.Draw3D(5, XMFLOAT3(5, 0.95f, 2), XMFLOAT2(0.7f, 1.9f));
    for ( int i = 0; i < 6; ++i )
        sprite.Draw3D(6, XMFLOAT3(5, 0.25f, i+2), XMFLOAT2(0.5f,0.5f));
    sprite.Draw3D(8, XMFLOAT3(2, 0.95f, 4.5f), XMFLOAT2(0.7f, 1.9f));
    sprite.Draw3D(8, XMFLOAT3(3, 0.95f, 5.5f), XMFLOAT2(0.7f, 1.9f));
    sprite.Draw3D(9, XMFLOAT3(7, 1.0f, 2), XMFLOAT2(0.5f, .35f));
    sprite.Draw3D(10, XMFLOAT3(8, 0.75f, 3), XMFLOAT2(1.5f, 1.5f));
    sprite.Draw3D(11, XMFLOAT3(2, 0.3f, 7), XMFLOAT2(1.5f, 0.6f));
    sprite.Draw3D(11, XMFLOAT3(2.35f, 0.3f, 7.34f), XMFLOAT2(1.5f, 0.6f));
    sprite.Draw3D(12, XMFLOAT3(3.35f, 0.7f, 7.34f), XMFLOAT2(0.6f, 1.0f));


    // some other input
    if (GlobalFlags::TestRaycast)
    {
        XMFLOAT3 hit;
        //if (m_map.RaycastDir(m_camera.GetPosition(), m_camera.m_forward, hit))
        XMFLOAT3 end(m_camera.GetPosition());
        end.x += m_camera.m_forward.x*5.0f;
        end.y += m_camera.m_forward.y*5.0f;
        end.z += m_camera.m_forward.z*5.0f;
        if ( m_map.RaycastSeg(m_camera.GetPosition(), end, hit) )
        {
            hit.y = m_camera.ComputeHeightAtHit(hit);            
            sprite.Draw3D(0,hit, XMFLOAT2(0.3, 0.3), true);
        }
    }

    sprite.End3D();    
    */
}

void SceneRenderer::CreateDeviceDependentResources()
{
    auto mapCreateTask = concurrency::create_task([this] 
    {
        m_map.CreateDeviceDependentResources();        
        auto gameRes = m_deviceResources->GetGameResources();
        gameRes->m_sprite.CreateDeviceDependentResources();
        gameRes->m_entityMgr.CreateDeviceDependentResources();
    });

    

    (mapCreateTask).then([this] () 
    {
		m_loadingComplete = true;
	});
}

void SceneRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
    auto gameRes = m_deviceResources->GetGameResources();
    gameRes->m_sprite.ReleaseDeviceDependentResources();
    gameRes->m_entityMgr.ReleaseDeviceDependentResources();
    m_deviceResources->GetGameResources()->m_entityMgr.ReleaseDeviceDependentResources();
}

