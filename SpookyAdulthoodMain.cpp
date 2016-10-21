#include "pch.h"
#include "SpookyAdulthoodMain.h"
#include "Common\DirectXHelper.h"
#include "Content\GlobalFlags.h"
#include "Content\Entity.h"

using namespace SpookyAdulthood;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;


// Loads and initializes application assets when the application is loaded.
SpookyAdulthoodMain::SpookyAdulthoodMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources)
{
#pragma warning(disable:4316)
	m_deviceResources->RegisterDeviceNotify(this);
	m_sceneRenderer = std::unique_ptr<SceneRenderer>(new SceneRenderer(m_deviceResources));
	m_fpsTextRenderer = std::unique_ptr<UIRenderer>(new UIRenderer(m_deviceResources));    

	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
#pragma warning(default:4316)
}

SpookyAdulthoodMain::~SpookyAdulthoodMain()
{
	// Deregister device notification
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Updates application state when the window size changes (e.g. device orientation change)
void SpookyAdulthoodMain::CreateWindowSizeDependentResources() 
{
    if ( m_sceneRenderer )
	    m_sceneRenderer->CreateWindowSizeDependentResources();
}

// Updates the application state once per frame.
void SpookyAdulthoodMain::Update()
{
	// Update scene objects.
	m_timer.Tick([&]()
	{
		m_sceneRenderer->Update(m_timer);
		m_fpsTextRenderer->Update(m_timer);
        GlobalFlags::Update(m_timer);
        auto gameRes = m_deviceResources->GetGameResources();
        if (gameRes)
            gameRes->Update(m_timer, gameRes->m_camera);
	});
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool SpookyAdulthoodMain::Draw3D() 
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// Reset the viewport to target the whole screen.
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

    auto gameRes = m_deviceResources->GetGameResources();
    auto& cam = gameRes->m_camera;

	// Reset render targets to the screen.
    //ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
    ID3D11RenderTargetView * targets[1] = { m_deviceResources->GetTempRenderTargetView() };
	context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());
	context->ClearRenderTargetView(m_deviceResources->GetTempRenderTargetView(), DirectX::Colors::Black);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // Render the scene objects to RT
	m_sceneRenderer->Render();
    gameRes->m_entityMgr.RenderModel3D(cam);
    gameRes->m_entityMgr.RenderSprites3D(cam);
    gameRes->m_entityMgr.RenderSprites2D(cam); // should it be two passes for 2d? (before and after screen quad?)

    // Render quad on screen
    {
        targets[0] = { m_deviceResources->GetBackBufferRenderTargetView() };
        context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());
        context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        // params (flash)
        float t = std::max(0.0f, 1.0f - 16.0f*(0.5f - gameRes->m_flashScreenTime));
        auto fc = gameRes->m_flashColor;
        XMFLOAT4 params0(fc.x*t*0.2f, fc.y*t*0.2f, fc.z*t*0.2f, 0);
        gameRes->m_sprite.DrawScreenQuad(m_deviceResources->GetTempRenderTargetSRV(), params0);

        // HUD on the final RT
        m_fpsTextRenderer->Render();
        GlobalFlags::Draw3D(m_deviceResources);

    }

	return true;
}

// Notifies renderers that device resources need to be released.
void SpookyAdulthoodMain::OnDeviceLost()
{
	m_sceneRenderer->ReleaseDeviceDependentResources();
	m_fpsTextRenderer->ReleaseDeviceDependentResources();
}

// Notifies renderers that device resources may now be recreated.
void SpookyAdulthoodMain::OnDeviceRestored()
{
	m_sceneRenderer->CreateDeviceDependentResources();
	m_fpsTextRenderer->CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

void SpookyAdulthoodMain::OnKeyDown(Windows::System::VirtualKey virtualKey)
{
    switch (virtualKey)
    {
    case Windows::System::VirtualKey::Escape:
        Windows::ApplicationModel::Core::CoreApplication::Exit();
        return;
    }
    GlobalFlags::OnKeyDown(virtualKey);
}

