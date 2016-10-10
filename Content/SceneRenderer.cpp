#include "pch.h"
#include "SceneRenderer.h"

#include "..\Common\DirectXHelper.h"
#include <../Common/FPSCDAndSolving.h>
#include "Sprite3D.h"

using namespace SpookyAdulthood;

using namespace DirectX;
using namespace Windows::Foundation;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
SceneRenderer::SceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_degreesPerSecond(45),
	m_deviceResources(deviceResources),
    m_timeUntilNextGen(0.0),
    m_map(deviceResources),
    m_sprite3D(deviceResources)
{
    CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();

    m_mapSettings.m_tileCount = XMUINT2(40, 40);
    m_mapSettings.m_minTileCount = XMUINT2(3, 3);
    m_mapSettings.m_maxTileCount = XMUINT2(10, 10);
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
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 50.0f * XM_PI / 180.0f;

	// This is a simple example of change that can be made when the app is in portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}
    m_camera.ComputeProjection(fovAngleY, aspectRatio, 0.01f, 100.0f, m_deviceResources->GetOrientationTransform3D());
    m_camera.m_radius = 0.8f;
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
        XMFLOAT2 curPos2D(XMVectorGetX(curPos), XMVectorGetZ(curPos));
        XMFLOAT2 nextPos2D(XMVectorGetX(nextPos), XMVectorGetZ(nextPos));
        XMFLOAT2 solved2D = FPSCDAndSolving2D(m_map.GetCurrentCollisionSegments(), curPos2D, nextPos2D, radius);
        XMVECTOR solved3D = XMVectorSet(solved2D.x, XMVectorGetY(nextPos), solved2D.y, 0.0f);
        return solved3D;
    });

    // some other input
    auto kb = DirectX::Keyboard::Get().GetState();
    m_timeUntilNextGen -= timer.GetElapsedSeconds();
    if (kb.D1 && m_timeUntilNextGen <= 0.0)
    {
        m_timeUntilNextGen = 0.25;        
        m_map.Generate(m_mapSettings);
        m_map.GenerateThumbTex(m_mapSettings.m_tileCount);
    }

    if (kb.D2 && m_timeUntilNextGen <= 0.0)
    {
        m_timeUntilNextGen = 0.25;
        SpawnPlayer();
    }

    // thumbnail map update
    if (timer.GetFrameCount() % 30 == 0)
    {
        XMUINT2 ppos = m_map.ConvertToMapPosition(m_camera.GetPosition());
        m_map.GenerateThumbTex(m_mapSettings.m_tileCount,(timer.GetFrameCount() % 60)?&ppos:nullptr);
    }
}

// Renders one frame using the vertex and pixel shaders.
void SceneRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
		return;

    // LEVEL rendering
    m_map.Render(m_camera);

    m_sprite3D.Render(0, m_camera, XMFLOAT3(3, 3, 1), XMFLOAT2(1, 1));

    m_deviceResources->GetSprites()->Begin();
    m_font->DrawString(m_deviceResources->GetSprites(), L"Hello", XMFLOAT2(300, 10), Colors::Yellow);
    m_deviceResources->GetSprites()->End();
}

void SceneRenderer::CreateDeviceDependentResources()
{
    // device resource of map
    auto mapCreateTask = concurrency::create_task([this] 
    {
        m_map.CreateDeviceDependentResources();
        m_sprite3D.CreateDeviceDependentResources();
        m_sprite3D.CreateSprite(L"assets\\windowslogo.dds"); // will return 0
        m_font = std::make_unique<DirectX::SpriteFont>(m_deviceResources->GetD3DDevice(), L"assets\\Courier_16.spritefont");
    });

    // after mesh, load texture from file
/*    auto loadTextureTask = concurrency::create_task([this]() 
    {
        DX::ThrowIfFailed(
            DirectX::CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"assets\\windowslogo.dds",
                (ID3D11Resource**)m_texture.ReleaseAndGetAddressOf(), m_textureView.ReleaseAndGetAddressOf())
        );        
    });
    */

	// Once the texture is loaded, the object is ready to be rendered.
    (/*loadTextureTask &&*/ mapCreateTask).then([this] () 
    {
		m_loadingComplete = true;
	});
}

void SceneRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
    m_map.CreateDeviceDependentResources();
    m_sprite3D.ReleaseDeviceDependentResources();
    m_font.reset();
}


void SceneRenderer::OnKeyDown(Windows::System::VirtualKey virtualKey)
{
    switch (virtualKey)
    {
        case Windows::System::VirtualKey::Space:
            m_map.SetThumbMapRender((LevelMap::ThumbMapRender)(((int)m_map.GetThumbMapRender() + 1) % LevelMap::THUMBMAP_MAX));
        break;
    }
}

