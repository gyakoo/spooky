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
        if (m_map.Raycast(m_camera.GetPosition(), m_camera.m_forward, hit))
        {
            hit.y = m_camera.ComputeHeightAtHit(hit);            
            sprite.Draw3D(0,hit, XMFLOAT2(0.3, 0.3), true);
        }
    }

    sprite.End3D();

    // GUN RENDER
    {
        sprite.Begin2D(m_camera);
        float rvel = (m_camera.m_moving && m_camera.m_running) ? 1.0f : 0.5f;
        float t = max(0.0f, m_camera.m_timeShoot);
        float offsx = sin(m_camera.m_runningTime*7.0f)*0.015f*rvel;
        float offsy = sin(m_camera.m_runningTime*5.0f)*0.015f*rvel + m_camera.m_pitchYaw.x*0.1f;        
        sprite.Draw2D(17, XMFLOAT2(offsx*0.8f, -0.6f + offsy), XMFLOAT2(0.9f, 0.9f), 0.0f); // pumpkins
        sprite.Draw2D(18, XMFLOAT2(offsx*0.7f, -0.6f + offsy*1.1f), XMFLOAT2(0.9f, 0.9f), 0.0f); // candies
        sprite.Draw2D(2, XMFLOAT2(offsx, -0.6f + offsy-t*0.08f), XMFLOAT2(0.9f, 0.9f), 0.0f); // cannon
        sprite.Draw2D(16, XMFLOAT2(offsx, -0.6f + offsy - t*0.08f), XMFLOAT2(0.9f, 0.9f), 0.0f); // flashlight
        //sprite.Draw2D(3, XMFLOAT2(-0.9f,0), XMFLOAT2(0.08f, 0.1f), -m_camera.m_pitchYaw.y);        
        sprite.Draw2DAnimation(0, XMFLOAT2(offsx, -0.6f + offsy), XMFLOAT2(0.9f, 0.9f), 0.0f);

        sprite.End2D();
    }
}

void SceneRenderer::CreateDeviceDependentResources()
{
    auto mapCreateTask = concurrency::create_task([this] 
    {
        m_map.CreateDeviceDependentResources();        
    });

    auto sprTask = concurrency::create_task([this] {
        auto& sprite = m_deviceResources->GetGameResources()->m_sprite;
        sprite.CreateDeviceDependentResources();
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
    });

    (mapCreateTask && sprTask).then([this] () 
    {
		m_loadingComplete = true;
	});
}

void SceneRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
    m_map.CreateDeviceDependentResources();
}

