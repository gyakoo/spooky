#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "Content/ShaderStructures.h"
#include "Content/GlobalFlags.h"
#include "Content/CameraFirstPerson.h"

using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;
using namespace SpookyAdulthood;

namespace DisplayMetrics
{
	// High resolution displays can require a lot of GPU and battery power to render.
	// High resolution phones, for example, may suffer from poor battery life if
	// games attempt to render at 60 frames per second at full fidelity.
	// The decision to render at full fidelity across all platforms and form factors
	// should be deliberate.
	static const bool SupportHighResolutions = false;

	// The default thresholds that define a "high resolution" display. If the thresholds
	// are exceeded and SupportHighResolutions is false, the dimensions will be scaled
	// by 50%.
	static const float DpiThreshold = 192.0f;		// 200% of standard desktop display.
	static const float WidthThreshold = 1920.0f;	// 1080p width.
	static const float HeightThreshold = 1080.0f;	// 1080p height.
};

// Constants used to calculate screen rotations
namespace ScreenRotation
{
	// 0-degree Z-rotation
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 90-degree Z-rotation
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 180-degree Z-rotation
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 270-degree Z-rotation
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// Constructor for DeviceResources.
DX::DeviceResources::DeviceResources() :
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_deviceNotify(nullptr)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Configures resources that don't depend on the Direct3D device.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Initialize Direct2D resources.
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// If the project is in a debug build, enable Direct2D debugging via SDK Layers.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Initialize the Direct2D Factory.
	DX::ThrowIfFailed(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&options,
			&m_d2dFactory
			)
		);

	// Initialize the DirectWrite Factory.
	DX::ThrowIfFailed(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			&m_dwriteFactory
			)
		);

	// Initialize the Windows Imaging Component (WIC) Factory.
	DX::ThrowIfFailed(
		CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_wicFactory)
			)
		);
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DX::DeviceResources::CreateDeviceResources() 
{
	// This flag adds support for surfaces with a different color channel ordering
	// than the API default. It is required for compatibility with Direct2D.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// If the project is in a debug build, enable debugging via SDK Layers with this flag.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// This array defines the set of DirectX hardware feature levels this app will support.
	// Note the ordering should be preserved.
	// Don't forget to declare your application's minimum required feature level in its
	// description.  All applications are assumed to support 9.1 unless otherwise stated.
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	// Create the Direct3D 11 API device object and a corresponding context.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	HRESULT hr = D3D11CreateDevice(
		nullptr,					// Specify nullptr to use the default adapter.
		D3D_DRIVER_TYPE_HARDWARE,	// Create a device using the hardware graphics driver.
		0,							// Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
		creationFlags,				// Set debug and Direct2D compatibility flags.
		featureLevels,				// List of feature levels this app can support.
		ARRAYSIZE(featureLevels),	// Size of the list above.
		D3D11_SDK_VERSION,			// Always set this to D3D11_SDK_VERSION for Windows Store apps.
		&device,					// Returns the Direct3D device created.
		&m_d3dFeatureLevel,			// Returns feature level of device created.
		&context					// Returns the device immediate context.
		);

	if (FAILED(hr))
	{
		// If the initialization fails, fall back to the WARP device.
		// For more information on WARP, see: 
		// http://go.microsoft.com/fwlink/?LinkId=286690
		DX::ThrowIfFailed(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&device,
				&m_d3dFeatureLevel,
				&context
				)
			);
	}

	// Store pointers to the Direct3D 11.3 API device and immediate context.
	DX::ThrowIfFailed(
		device.As(&m_d3dDevice)
		);

	DX::ThrowIfFailed(
		context.As(&m_d3dContext)
		);

	// Create the Direct2D device object and a corresponding context.
	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(
		m_d3dDevice.As(&dxgiDevice)
		);

	DX::ThrowIfFailed(
		m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice)
		);

	DX::ThrowIfFailed(
		m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			&m_d2dContext
			)
		);    
    auto dev = std::shared_ptr<DeviceResources>(this);
    m_gameResources = std::make_unique<GameResources>(dev);
}

// These resources need to be recreated every time the window size is changed.
void DX::DeviceResources::CreateWindowSizeDependentResources() 
{
	// Clear the previous window size specific context.
	ID3D11RenderTargetView* nullViews[] = {nullptr};
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d2dContext->SetTarget(nullptr);
	m_d2dTargetBitmap = nullptr;
	m_d3dDepthStencilView = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	// The width and height of the swap chain must be based on the window's
	// natively-oriented width and height. If the window is not in the native
	// orientation, the dimensions must be reversed.
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// If the swap chain already exists, resize it.
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // Double-buffered swap chain.
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// If the device was removed for any reason, a new device and swap chain will need to be created.
			HandleDeviceLost();

			// Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
			// and correctly set up the new device.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// Otherwise, create a new one using the same adapter as the existing Direct3D device.
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Match the size of the window.
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// This is the most common swap chain format.
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// Don't use multi-sampling.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// Use double-buffering to minimize latency.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// All Windows Store apps must use this SwapEffect.
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// This sequence obtains the DXGI factory that was used to create the Direct3D device above.
		ComPtr<IDXGIDevice3> dxgiDevice;
		DX::ThrowIfFailed(
			m_d3dDevice.As(&dxgiDevice)
			);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(&dxgiAdapter)
			);

		ComPtr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
			);

		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForCoreWindow(
				m_d3dDevice.Get(),
				reinterpret_cast<IUnknown*>(m_window.Get()),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);
		DX::ThrowIfFailed(
			swapChain.As(&m_swapChain)
			);
        
        // Ensure that DXGI does not queue more than one frame at a time. This both reduces latency and
		// ensures that the application will only render after each VSync, minimizing power consumption.
		DX::ThrowIfFailed(
			dxgiDevice->SetMaximumFrameLatency(1)
			);
	}

	// Set the proper orientation for the swap chain, and generate 2D and
	// 3D matrix transformations for rendering to the rotated swap chain.
	// Note the rotation angle for the 2D and 3D transforms are different.
	// This is due to the difference in coordinate spaces.  Additionally,
	// the 3D matrix is specified explicitly to avoid rounding errors.

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform2D = Matrix3x2F::Identity();
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(90.0f) *
			Matrix3x2F::Translation(m_logicalSize.Height, 0.0f);
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(180.0f) *
			Matrix3x2F::Translation(m_logicalSize.Width, m_logicalSize.Height);
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(270.0f) *
			Matrix3x2F::Translation(0.0f, m_logicalSize.Width);
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// Create a render target view of the swap chain back buffer.
	ComPtr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
		);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateRenderTargetView1(
			backBuffer.Get(),
			nullptr,
			&m_d3dRenderTargetView
			)
		);

	// Create a depth stencil view for use with 3D rendering if needed.
	CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT, 
		lround(m_d3dRenderTargetSize.Width),
		lround(m_d3dRenderTargetSize.Height),
		1, // This depth stencil view has only one texture.
		1, // Use a single mipmap level.
		D3D11_BIND_DEPTH_STENCIL
		);

	ComPtr<ID3D11Texture2D1> depthStencil;
	DX::ThrowIfFailed(
		m_d3dDevice->CreateTexture2D1(
			&depthStencilDesc,
			nullptr,
			&depthStencil
			)
		);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	DX::ThrowIfFailed(
		m_d3dDevice->CreateDepthStencilView(
			depthStencil.Get(),
			&depthStencilViewDesc,
			&m_d3dDepthStencilView
			)
		);
	
	// Set the 3D rendering viewport to target the entire window.
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
		);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);

	// Create a Direct2D target bitmap associated with the
	// swap chain back buffer and set it as the current target.
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = 
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpi,
			m_dpi
			);

	ComPtr<IDXGISurface2> dxgiBackBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
		);

	DX::ThrowIfFailed(
		m_d2dContext->CreateBitmapFromDxgiSurface(
			dxgiBackBuffer.Get(),
			&bitmapProperties,
			&m_d2dTargetBitmap
			)
		);

	m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
	m_d2dContext->SetDpi(m_effectiveDpi, m_effectiveDpi);

	// Grayscale text anti-aliasing is recommended for all Windows Store apps.
	m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    if ( m_gameResources && m_gameResources->m_readyToRender)
        m_gameResources->m_sprites->SetRotation(ComputeDisplayRotation());
}

// Determine the dimensions of the render target and whether it will be scaled down.
void DX::DeviceResources::UpdateRenderTargetSize()
{
    Windows::Foundation::Size oldSize = m_outputSize;
	m_effectiveDpi = m_dpi;

	// To improve battery life on high resolution devices, render to a smaller render target
	// and allow the GPU to scale the output when it is presented.
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		// When the device is in portrait orientation, height > width. Compare the
		// larger dimension against the width threshold and the smaller dimension
		// against the height threshold.
		if (std::max(width, height) > DisplayMetrics::WidthThreshold && std::min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// To scale the app we change the effective DPI. Logical size does not change.
			m_effectiveDpi /= 2.0f;
		}
	}

	// Calculate the necessary render target size in pixels.
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// Prevent zero size DirectX content from being created.
	m_outputSize.Width = std::max(m_outputSize.Width, 1.0f);
	m_outputSize.Height = std::max(m_outputSize.Height, 1.0f);


    // Temporary RT
    if ( !m_tempRTTexture || oldSize != m_outputSize )
    {
        // Setup the render target texture description.
        D3D11_TEXTURE2D_DESC textureDesc = { 0 };
        textureDesc.Width = (UINT)m_outputSize.Width;
        textureDesc.Height = (UINT)m_outputSize.Height;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = 0;
        DX::ThrowIfFailed(m_d3dDevice->CreateTexture2D(&textureDesc, nullptr, m_tempRTTexture.ReleaseAndGetAddressOf()));

        // to use texture as render target
        D3D11_RENDER_TARGET_VIEW_DESC rtViewDesc;
        rtViewDesc.Format = textureDesc.Format;
        rtViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtViewDesc.Texture2D.MipSlice = 0;
        DX::ThrowIfFailed(m_d3dDevice->CreateRenderTargetView((ID3D11Resource*)m_tempRTTexture.Get(), &rtViewDesc, m_tempRTView.ReleaseAndGetAddressOf()));

        // to use texture in as shader resource
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderViewDesc;
        shaderViewDesc.Format = textureDesc.Format;
        shaderViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderViewDesc.Texture2D.MostDetailedMip = 0;
        shaderViewDesc.Texture2D.MipLevels = 1;
        DX::ThrowIfFailed(m_d3dDevice->CreateShaderResourceView((ID3D11Resource*)m_tempRTTexture.Get(), &shaderViewDesc, m_tempRTSRV.ReleaseAndGetAddressOf()));
    }

}

// This method is called when the CoreWindow is created (or re-created).
void DX::DeviceResources::SetWindow(CoreWindow^ window)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_window = window;
	m_logicalSize = Windows::Foundation::Size(window->Bounds.Width, window->Bounds.Height);
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_dpi = currentDisplayInformation->LogicalDpi;
	m_d2dContext->SetDpi(m_dpi, m_dpi);

	CreateWindowSizeDependentResources();
}

// This method is called in the event handler for the SizeChanged event.
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// This method is called in the event handler for the DpiChanged event.
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;

		// When the display DPI changes, the logical size of the window (measured in Dips) also changes and needs to be updated.
		m_logicalSize = Windows::Foundation::Size(m_window->Bounds.Width, m_window->Bounds.Height);

		m_d2dContext->SetDpi(m_dpi, m_dpi);
		CreateWindowSizeDependentResources();
	}
}

// This method is called in the event handler for the OrientationChanged event.
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// This method is called in the event handler for the DisplayContentsInvalidated event.
void DX::DeviceResources::ValidateDevice()
{
	// The D3D Device is no longer valid if the default adapter changed since the device
	// was created or if the device has been removed.

	// First, get the information for the default adapter from when the device was created.

	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory4> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// Next, get the information for the current default adapter.

	ComPtr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// If the adapter LUIDs don't match, or if the device reports that it has been removed,
	// a new D3D device must be created.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// Release references to resources related to the old device.
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// Create a new device and swap chain.
		HandleDeviceLost();
	}
}

// Recreate all device resources and set them back to the current state.
void DX::DeviceResources::HandleDeviceLost()
{
	m_swapChain = nullptr;

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	CreateDeviceResources();
	m_d2dContext->SetDpi(m_dpi, m_dpi);
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

// Register our DeviceNotify to be informed on device lost and creation.
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

// Call this method when the app suspends. It provides a hint to the driver that the app 
// is entering an idle state and that temporary buffers can be reclaimed for use by other apps.
void DX::DeviceResources::Trim()
{
	ComPtr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);

	dxgiDevice->Trim();
    if (m_gameResources && m_gameResources->m_audioEngine)
        m_gameResources->m_audioEngine->Suspend();
}

void DX::DeviceResources::Resume()
{
    if (m_gameResources && m_gameResources->m_audioEngine)
        m_gameResources->m_audioEngine->Resume();
}

// Present the contents of the swap chain to the screen.
void DX::DeviceResources::Present() 
{
	// The first argument instructs DXGI to block until VSync, putting the application
	// to sleep until the next VSync. This ensures we don't waste any cycles rendering
	// frames that will never be displayed to the screen.
	DXGI_PRESENT_PARAMETERS parameters = { 0 };
	HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

	// Discard the contents of the render target.
	// This is a valid operation only when the existing contents will be entirely
	// overwritten. If dirty or scroll rects are used, this call should be removed.
	m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

	// Discard the contents of the depth stencil.
	m_d3dContext->DiscardView1(m_d3dDepthStencilView.Get(), nullptr, 0);

	// If the device was removed either by a disconnection or a driver upgrade, we 
	// must recreate all device resources.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

// This method determines the rotation between the display device's native orientation and the
// current display orientation.
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// Note: NativeOrientation can only be Landscape or Portrait even though
	// the DisplayOrientations enum has other values.
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}

// default values for SOUNDS
static const wchar_t* g_sndNames[] = {
    L"assets\\sounds\\walk.wav", L"assets\\sounds\\breathing.wav",
    L"assets\\sounds\\piano.wav", L"assets\\sounds\\shotgun.wav",
    L"assets\\sounds\\heartbeat.wav", L"assets\\sounds\\hit0.wav" };
static const float g_sndVolumes[] = { 1.0f, 0.4f, 0.05f, 0.3f, 0.25f, 1.0f };
static const float g_sndPitches[] = { 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f };

DX::GameResources* DX::GameResources::instance = nullptr;
DX::GameResources::GameResources(const std::shared_ptr<DX::DeviceResources>& device)
    : m_readyToRender(false), m_levelTime(0.0f), m_sprite(device), m_entityMgr(device)
    , m_map(device), m_flashScreenTime(0.0f), m_flashColor(1,1,1,1)
    , m_invincibleTime(-1.0f)
{   
    GameResources::instance = this;
    // vertex shader and input layout
    auto loadVSTask = DX::ReadDataAsync(L"BaseVertexShader.cso");
    auto loadPSTask = DX::ReadDataAsync(L"BasePixelShader.cso");
    auto loadSpriteVS = DX::ReadDataAsync(L"ScreenSpriteVS.cso");
    auto loadSpritePS = DX::ReadDataAsync(L"ScreenSpritePS.cso");
    auto loadPostPS = DX::ReadDataAsync(L"PostPS.cso");

    m_sprites = std::make_unique<SpriteBatch>(device->GetD3DDeviceContext());
    m_fontConsole = std::make_unique<DirectX::SpriteFont>(device->GetD3DDevice(), L"assets\\fonts\\Courier_16.spritefont");
    m_commonStates = std::make_unique<DirectX::CommonStates>(device->GetD3DDevice());
    m_batch = std::make_unique<DirectX::PrimitiveBatch<VertexPositionColor>>(device->GetD3DDeviceContext());
    AUDIO_ENGINE_FLAGS aeflags = AudioEngine_Default;
#ifdef _DEBUG
    aeflags = aeflags | AudioEngine_Debug;
#endif
    m_audioEngine = std::make_unique<DirectX::AudioEngine>(aeflags);

    DX::ThrowIfFailed(
        DirectX::CreateWICTextureFromFile(
            device->GetD3DDevice(), L"assets\\textures\\white.png",
            (ID3D11Resource**)m_textureWhite.ReleaseAndGetAddressOf(),
            m_textureWhiteSRV.ReleaseAndGetAddressOf()));

    // SOUNDS, init values    
    m_soundEffects.resize(SFX_MAX);
    m_sounds.resize(SFX_MAX);
    for (int i = 0; i < SFX_MAX; ++i)
    {
        concurrency::create_task([this,i] {
            m_soundEffects[i] = std::move(std::make_unique<SoundEffect>(m_audioEngine.get(), g_sndNames[i]));
            m_sounds[i] = std::move(m_soundEffects[i]->CreateInstance());
            m_sounds[i]->SetVolume(g_sndVolumes[i]);
            m_sounds[i]->SetPitch(g_sndPitches[i]);
        });
    }

    // BASE VS constant buffer
    {
        CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
        DX::ThrowIfFailed(
            device->GetD3DDevice()->CreateBuffer(
                &constantBufferDesc,
                nullptr,
                m_baseVSCB.GetAddressOf()
            )
        );
    }
    // BASE PS Constant buffer
    {
        CD3D11_BUFFER_DESC constantBufferDesc(sizeof(PixelShaderConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
        DX::ThrowIfFailed(
            device->GetD3DDevice()->CreateBuffer(
                &constantBufferDesc,
                nullptr,
                m_basePSCB.GetAddressOf()
            )
        );
    }

    // BASE SHADERS
    auto createBaseVS = loadVSTask.then([this, device](const std::vector<byte>& fileData) {
        const HRESULT hr = device->GetD3DDevice()->CreateVertexShader(fileData.data(), fileData.size(), nullptr, m_baseVS.GetAddressOf());
        DX::ThrowIfFailed(hr);
        DX::ThrowIfFailed(
            device->GetD3DDevice()->CreateInputLayout(
                VertexPositionNormalColorTextureNdx::InputElements,
                VertexPositionNormalColorTextureNdx::InputElementCount,
                &fileData[0],
                fileData.size(),
                m_baseIL.GetAddressOf()
            )
        );
    });

    auto createBasePS = loadPSTask.then([this, device](const std::vector<byte>& fileData) {
        const HRESULT hr = device->GetD3DDevice()->CreatePixelShader(fileData.data(), fileData.size(), nullptr, m_basePS.GetAddressOf());
        DX::ThrowIfFailed(hr);
    });

    // SCREEN SPRITE SHADERS
    auto createSSVS = loadSpriteVS.then([this, device](const std::vector<byte>& fileData) {
        const HRESULT hr = device->GetD3DDevice()->CreateVertexShader(fileData.data(), fileData.size(), nullptr, m_spriteVS.GetAddressOf());
        DX::ThrowIfFailed(hr);
    });
    auto createSSPS = loadSpritePS.then([this, device](const std::vector<byte>& fileData) {
        const HRESULT hr = device->GetD3DDevice()->CreatePixelShader(fileData.data(), fileData.size(), nullptr, m_spritePS.GetAddressOf());
        DX::ThrowIfFailed(hr);
    });

    auto createPostPS = loadPostPS.then([this, device](const std::vector<byte>& fileData) {
        const HRESULT hr = device->GetD3DDevice()->CreatePixelShader(fileData.data(), fileData.size(), nullptr, m_postPS.GetAddressOf());
        DX::ThrowIfFailed(hr);
    });

    (createBaseVS && createBasePS && createSSVS && createSSPS && createPostPS).then([this]() 
    { 
        m_readyToRender = true; 
    });
}

DX::GameResources::~GameResources()
{
    m_readyToRender = false;
    m_textureWhiteSRV.Reset();
    m_textureWhite.Reset();
    m_baseVS.Reset();
    m_basePS.Reset();
    m_spriteVS.Reset();
    m_spritePS.Reset();
    m_postPS.Reset();
    m_baseVSCB.Reset();
    m_basePSCB.Reset();
    m_baseIL.Reset();
    m_sprites.reset();
    m_fontConsole.reset();
    m_commonStates.reset();
    m_batch.reset();
    m_audioEngine.reset();
    m_sprite.ReleaseDeviceDependentResources();
}

void DX::GameResources::Update(const DX::StepTimer& timer, const CameraFirstPerson& camera)
{
    if (!m_readyToRender)
        return;

    const float stepTime = (float)timer.GetElapsedSeconds();
    m_frameCount = timer.GetFrameCount();
    m_flashScreenTime -= stepTime;
    m_invincibleTime -= stepTime;

    // Update SPRITE MANAGER
    m_sprite.Update(timer);
    m_entityMgr.Update(timer, camera);

    // Update AUDIO
    auto audio = m_audioEngine.get();
    if (audio)
    {
        if (!audio->IsCriticalError())
            audio->Update();

        // Update player audio
        if (m_camera.m_moving)
        {
            SoundResume(DX::GameResources::SFX_WALK);
            SoundPitch(DX::GameResources::SFX_WALK, m_camera.m_running ? 0.5f : 0.0f);
        }
        else
        {
            SoundPause(DX::GameResources::SFX_WALK);
        }
    }

    // thumbnail map update (every N frames)
    if (timer.GetFrameCount() % 30 == 0)
    {
        XMUINT2 ppos = m_map.ConvertToMapPosition(m_camera.GetPosition());
        m_map.GenerateThumbTex(m_mapSettings.m_tileCount, &ppos);
    }
}

void DX::GameResources::FlashScreen(float time, const XMFLOAT4& color)
{
    m_flashScreenTime = time;
    m_flashColor = color;
}

void DX::GameResources::SoundPlay(uint32_t index, bool loop) const
{
    if (index >= m_sounds.size()) return;
    auto s = m_sounds[index].get();
    if (!s) return;
    if (s->GetState() == DirectX::PLAYING)
        s->Stop();
    s->Play(loop);
}

void DX::GameResources::SoundStop(uint32_t index) const
{
    if (index >= m_sounds.size()) return;
    auto s = m_sounds[index].get();
    if (!s) return;
    s->Stop();
}

void DX::GameResources::SoundPause(uint32_t index) const
{
    if (index >= m_sounds.size()) return;
    auto s = m_sounds[index].get();
    if (!s) return;
    s->Pause();
}

void DX::GameResources::SoundResume(uint32_t index) const
{
    if (index >= m_sounds.size()) return;
    auto s = m_sounds[index].get();
    if (!s) return;
    switch (s->GetState())
    {
    case DirectX::STOPPED: s->Play(s->IsLooped()); break;
    case DirectX::PAUSED: s->Resume(); break;
    }
}

SoundEffectInstance* DX::GameResources::SoundGet(uint32_t index) const
{
    if (index >= m_sounds.size()) return nullptr;
    return m_sounds[index].get();
}

void DX::GameResources::SoundPitch(uint32_t index, float p)
{
    if (index >= m_sounds.size()) return;
    auto s = m_sounds[index].get();
    if (!s) return;
    s->SetPitch(p);
}

void DX::GameResources::SoundVolume(uint32_t index, float v)
{
    if (index >= m_sounds.size()) return;
    auto s = m_sounds[index].get();
    if (!s) return;
    if (v < .0f)
        v = g_sndVolumes[index];
    s->SetVolume(v);
}

void DX::GameResources::PlayerShoot()
{
    // sound play, animation, flash screen
    SoundPlay(DX::GameResources::SFX_SHOTGUN, false);
    m_sprite.CreateAnimationInstance(0,0);
    FlashScreen(0.5f, XMFLOAT4(0.5f, 0.5f, 0.4f, 1));

    // raycast bullets
    auto startpos = m_camera.GetPosition();
    // compute all rays for shotgun (this is so ugly and expensive)
    const float A = 0.0f; // aperture angle shoot
#define Rr (m_random.GetF(0.0f,0.3f))
    const XMMATRIX rotations[7] = { // buh, we have memory enough!
        XMMatrixMultiply(XMMatrixRotationX(-A-Rr), XMMatrixRotationY(A+Rr)),
        XMMatrixMultiply(XMMatrixRotationX(-A-Rr),XMMatrixRotationY(-A-Rr)),
        XMMatrixRotationY(A+Rr), XMMatrixIdentity(), XMMatrixRotationY(-A-Rr),
        XMMatrixMultiply(XMMatrixRotationX(A+Rr), XMMatrixRotationY(A+Rr)),
        XMMatrixMultiply(XMMatrixRotationX(A+Rr), XMMatrixRotationY(-A-Rr))
    };
#undef Rr
    const float SHOOT_RANGE = 5.0f;
    const XMVECTOR fw = XMLoadFloat3(&m_camera.m_forward);
    XMFLOAT3 newfw, endpos, hitE, hitM;
    bool wasHitE, wasHitM;
    uint32_t eNdx;
    GlobalFlags::ShootHits = 0;
    for (int i = 0; i < 7; ++i)
    {
        XMStoreFloat3(&newfw, XMVector3TransformNormal(fw, rotations[i]));
        endpos = XM3Mad(startpos, newfw, SHOOT_RANGE);

        // we cast this ray against entities then map
        wasHitE = m_entityMgr.RaycastSeg(startpos, endpos, hitE, -1.0f, &eNdx);
        wasHitM = m_map.RaycastSeg(startpos, endpos, hitM, -1.0f, -0.05f);
        
        if (wasHitE && wasHitM)
        {
            // hit both, get the closest one
            const float distToE = XM3LenSq(XM3Sub(hitE, startpos));
            const float distToM = XM3LenSq(XM3Sub(hitM, startpos));
            if (distToE <= distToM) wasHitM = false;
            else wasHitE = false;
        }

        if (wasHitE)
        {
            // hit only agains entity
            m_entityMgr.AddEntity(std::make_shared<EntityShootHit>(hitE));
            m_entityMgr.DoHitOnEntity(eNdx);
            GlobalFlags::ShootHits++;
        }
        else if (wasHitM)
        {
            // hit only against map
            hitM.y = m_camera.m_height - m_camera.m_pitchYaw.x + m_random.GetF(-0.15f, 0.15f);
            m_entityMgr.AddEntity(std::make_shared<EntityShootHit>(hitM));
        }
    }
}


void DX::GameResources::OpenDoor(uint32_t index)
{

}


void DX::GameResources::OpenRoomDoors()
{
    
}

void DX::GameResources::HitPlayer()
{
    if (m_invincibleTime <= 0.0f)
    {
        FlashScreen(1.0f, XMFLOAT4(1, 0, 0, 1));
        SoundVolume(DX::GameResources::SFX_HIT0, -1.0f);//def.
        SoundPlay(DX::GameResources::SFX_HIT0, false);
        m_invincibleTime = 1.0f;
    }
}

void DX::GameResources::GenerateNewLevel()
{
    m_entityMgr.Clear();
    m_mapSettings.m_tileCount = XMUINT2(35, 35);
    m_mapSettings.m_minTileCount = XMUINT2(4, 4);
    m_mapSettings.m_maxTileCount = XMUINT2(15, 15);
    m_map.Generate(m_mapSettings);
    m_map.GenerateThumbTex(m_mapSettings.m_tileCount);
    SpawnPlayer();

    m_entityMgr.AddEntity(std::make_shared<EntityGun>()); // GUN

}

void DX::GameResources::SpawnPlayer()
{
    XMUINT2 mapPos = m_map.GetRandomPosition();
    XMFLOAT3 p(mapPos.x + 0.5f, 0, mapPos.y + 0.5f);

    m_camera.SetPosition(p);
    if (m_audioEngine)
    {
        SoundPlay(DX::GameResources::SFX_BREATH);
        //gameRes->SoundPlay(DX::GameResources::SFX_PIANO);
        SoundPlay(DX::GameResources::SFX_HEART);
    }
}