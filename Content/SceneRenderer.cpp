#include "pch.h"
#include "SceneRenderer.h"
#include "..\Common\DirectXHelper.h"
#include "CollisionAndSolving.h"
#include "Sprite.h"
#include "GlobalFlags.h"
#include "CameraFirstPerson.h"

using namespace SpookyAdulthood;

using namespace DirectX;
using namespace Windows::Foundation;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
SceneRenderer::SceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
    
    deviceResources->GetGameResources()->GenerateNewLevel();
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
    
    auto gameRes = m_deviceResources->GetGameResources();
    gameRes->m_camera.ComputeProjection(fovAngleY, aspectRatio, 0.01f, 100.0f, m_deviceResources->GetOrientationTransform3D());
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void SceneRenderer::Update(DX::StepTimer const& timer)
{
    if (!m_loadingComplete)
        return;

    auto gameRes = m_deviceResources->GetGameResources();
    auto& map = gameRes->m_map;

    // Update map and camera (input, collisions and visibility)
    map.Update(timer, gameRes->m_camera);
    gameRes->m_camera.Update(timer, 
    [&](XMVECTOR curPos, XMVECTOR nextPos, float radius)->XMVECTOR  // CALLED FOR COLLISION HANDLING
    { 
        if (GlobalFlags::CollisionsEnabled)
        {
            XMFLOAT2 curPos2D(XMVectorGetX(curPos), XMVectorGetZ(curPos));
            XMFLOAT2 nextPos2D(XMVectorGetX(nextPos), XMVectorGetZ(nextPos));
            XMFLOAT2 solved2D = CollisionAndSolving2D(map.GetCurrentCollisionSegments(), curPos2D, nextPos2D, radius);
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
        switch (ac)
        {
            case CameraFirstPerson::AC_SHOOT:
                gameRes->PlayerShoot();
            break;
        }
    });

    if (GlobalFlags::GenerateNewLevel)
    {
        GlobalFlags::GenerateNewLevel = false;
        gameRes->GenerateNewLevel();
    }

    if (GlobalFlags::SpawnPlayer)
    {
        GlobalFlags::SpawnPlayer = false;
        gameRes->SpawnPlayer();
    }
}

// Renders one frame using the vertex and pixel shaders.
void SceneRenderer::Render()
{
	// Loading is asynchronous.
	if (!m_loadingComplete)
		return;

    // LEVEL rendering
    auto gameRes = m_deviceResources->GetGameResources();
    auto& map = gameRes->m_map;
    auto& cam = gameRes->m_camera;
    map.Render(cam);
    
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
    sprite.Draw3D(11, XMFLOAT3(2, 0.3f, 7), XMFLOAT2(1.5f, 0.6f));
    sprite.Draw3D(11, XMFLOAT3(2.35f, 0.3f, 7.34f), XMFLOAT2(1.5f, 0.6f));
    sprite.Draw3D(12, XMFLOAT3(3.35f, 0.7f, 7.34f), XMFLOAT2(0.6f, 1.0f));

    sprite.End3D();
    */
    // some other input
}

void SceneRenderer::RenderUI()
{
    auto gameRes = m_deviceResources->GetGameResources();
    auto& map = gameRes->m_map;
    auto& cam = gameRes->m_camera;
    map.RenderMinimap(cam);
}

void SceneRenderer::CreateDeviceDependentResources()
{
    auto mapCreateTask = concurrency::create_task([&]
    {
        auto gameRes = m_deviceResources->GetGameResources();
        gameRes->m_map.CreateDeviceDependentResources();
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

