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
    m_map(deviceResources),
    m_sprite(deviceResources)
{
    CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();

    m_mapSettings.m_tileCount = XMUINT2(60, 60);
    m_mapSettings.m_minTileCount = XMUINT2(3, 3);
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
        gameRes->SoundPitch(DX::GameResources::SFX_HEART, -0.5f);
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
    [this](XMVECTOR curPos, XMVECTOR nextPos, float radius)->XMVECTOR 
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
    [this](CameraFirstPerson::eAction ac)
    {
        auto gameRes = m_deviceResources->GetGameResources();
        if (!gameRes) return;
        auto audio = gameRes->m_audioEngine.get();
        if (!audio) return;
        gameRes->SoundPlay(DX::GameResources::SFX_SHOTGUN, false);
    });

    // Audio
    UpdateAudio(timer);
    
    // some other input
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



    // Update AUDIO
    if (audio)
    {
        if (!audio->IsCriticalError())
            audio->Update();
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

    // SPRITEs rendering
    m_sprite.Begin3D(m_camera);
    m_sprite.Draw3D(0, XMFLOAT3(5, 0.45f, 6), XMFLOAT2(0.3, 0.3));
    m_sprite.Draw3D(0, XMFLOAT3(6, 0.65f, 5), XMFLOAT2(0.3, 0.3));
    m_sprite.Draw3D(0, XMFLOAT3(9, 0.75f, 4), XMFLOAT2(0.3, 0.3));
    m_sprite.Draw3D(1, XMFLOAT3(7, 0.5f, 9), XMFLOAT2(1, 1));
    m_sprite.Draw3D(2, XMFLOAT3(9, 0.25f, 3), XMFLOAT2(0.5f,0.5f));
    m_sprite.End3D();
    

    // GUN RENDER
    {
        m_sprite.Begin2D(m_camera);
        float rvel = (m_camera.m_moving && m_camera.m_running) ? 1.0f : 0.5f;
        float offsx = sin(m_camera.m_runningTime*7.0f)*0.015f*rvel;
        float offsy = sin(m_camera.m_runningTime*5.0f)*0.015f*rvel + m_camera.m_pitchYaw.x*0.1f;
        m_sprite.Draw2D(2, XMFLOAT2(offsx, -0.7f+offsy ), XMFLOAT2(0.7f, 0.7f), 0.0f);

        m_sprite.Draw2D(3, XMFLOAT2(-0.9f,0), XMFLOAT2(0.07f, 0.09f), -m_camera.m_pitchYaw.y);

        m_sprite.End2D();
    }
}

void SceneRenderer::CreateDeviceDependentResources()
{
    auto mapCreateTask = concurrency::create_task([this] 
    {
        m_map.CreateDeviceDependentResources();        
    });

    auto sprTask = concurrency::create_task([this] {
        m_sprite.CreateDeviceDependentResources();
        m_sprite.CreateSprite(L"assets\\sprites\\puky.png");
        m_sprite.CreateSprite(L"assets\\sprites\\hand.png");
        m_sprite.CreateSprite(L"assets\\sprites\\gun.png");
        m_sprite.CreateSprite(L"assets\\sprites\\pointinghand.png");
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
    m_sprite.ReleaseDeviceDependentResources();
}

