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
    m_sprite3D(deviceResources),
    m_test(5,0.45f,6)
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
    m_camera.Update(timer, [this](XMVECTOR curPos, XMVECTOR nextPos, float radius)->XMVECTOR 
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
    });

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


    {
        using namespace SimpleMath;
        Vector3 camPos(m_camera.GetPosition());
        Vector3 srcPos(m_test);
        Vector3 dir = camPos -srcPos; dir.Normalize();
        srcPos += dir*timer.GetElapsedSeconds(); srcPos.y = m_test.y;
        //m_test = srcPos;
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
    m_sprite3D.Begin3D(m_camera);
    m_sprite3D.Draw3D(0, m_test, XMFLOAT2(1, 1));
    m_sprite3D.Draw3D(1, XMFLOAT3(7, 0.5f, 9), XMFLOAT2(1, 1));
    m_sprite3D.Draw3D(2, XMFLOAT3(9, 0.25f, 3), XMFLOAT2(0.5f,0.5f));
    m_sprite3D.End3D();

    auto sprite2D = m_deviceResources->GetGameResources()->GetSprites();
    auto& spr = m_sprite3D.GetSprite(2);
    auto s = m_deviceResources->GetOutputSize();
    //sprite2D->Begin();
    //sprite2D->Draw(spr.m_textureSRV.Get(), XMFLOAT2(s.Width / 2, s.Height-200), nullptr, Colors::White, 0, XMFLOAT2(0, 0), XMFLOAT2(1.0f,1.0f));
    //sprite2D->End();
}

void SceneRenderer::CreateDeviceDependentResources()
{
    auto mapCreateTask = concurrency::create_task([this] 
    {
        m_map.CreateDeviceDependentResources();        
    });

    auto sprTask = concurrency::create_task([this] {
        m_sprite3D.CreateDeviceDependentResources();
        m_sprite3D.CreateSprite(L"assets\\zombie.png");
        m_sprite3D.CreateSprite(L"assets\\zombie2.png");
        m_sprite3D.CreateSprite(L"assets\\baby.png");
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
    m_sprite3D.ReleaseDeviceDependentResources();
}

