#pragma once
#include <random>
namespace DX
{
	// Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
	interface IDeviceNotify
	{
		virtual void OnDeviceLost() = 0;
		virtual void OnDeviceRestored() = 0;
	};

    class DeviceResources;


    //* ***************************************************************** *//
    //* RandomProvider
    //* ***************************************************************** *//
    class RandomProvider
    {
    public:
        RandomProvider() :m_lastSeed(0) {}

        void SetSeed(uint32_t seed);
        uint32_t Get(uint32_t minN, uint32_t maxN);

    protected:
        std::unique_ptr<std::mt19937> m_gen;
        uint32_t m_lastSeed, m_seed;
    };


    //* ***************************************************************** *//
    //* Global GAME dx/misc resources
    //* ***************************************************************** *//
    struct GameResources // common
    {
        enum { SFX_WALK=0, SFX_BREATH, SFX_PIANO, SFX_SHOTGUN, SFX_HEART, SFX_MAX};
        GameResources(const DX::DeviceResources* device);
        ~GameResources();

        Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_baseIL;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_baseVS;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_basePS;
        Microsoft::WRL::ComPtr<ID3D11Buffer>		m_baseVSCB;
        Microsoft::WRL::ComPtr<ID3D11Buffer>		m_basePSCB;

        Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_spriteVS;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_spritePS;

        std::unique_ptr<DirectX::SpriteBatch>       m_sprites;
        std::unique_ptr<DirectX::CommonStates>      m_commonStates;
        std::unique_ptr<DirectX::SpriteFont>        m_fontConsole;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>		m_textureWhite;
        std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_batch;// DEBUG LINES

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureWhiteSRV;

        std::unique_ptr<DirectX::AudioEngine>       m_audioEngine;
        std::vector<std::unique_ptr<DirectX::SoundEffect>> m_soundEffects;
        std::vector<std::unique_ptr<DirectX::SoundEffectInstance>> m_sounds;
        RandomProvider m_random;
        float m_levelTime;
        bool m_ready;

        void SoundPlay(uint32_t index, bool loop=true)const;
        void SoundStop(uint32_t index)const;
        void SoundPause(uint32_t index)const;
        void SoundResume(uint32_t index)const ;
        void SoundPitch(uint32_t index, float p);
        void SoundVolume(uint32_t index, float v);
        DirectX::SoundEffectInstance* SoundGet(uint32_t index) const;
    };

	// Controls all the DirectX device resources.
	class DeviceResources
	{
	public:
		DeviceResources();
		void SetWindow(Windows::UI::Core::CoreWindow^ window);
		void SetLogicalSize(Windows::Foundation::Size logicalSize);
		void SetCurrentOrientation(Windows::Graphics::Display::DisplayOrientations currentOrientation);
		void SetDpi(float dpi);
		void ValidateDevice();
		void HandleDeviceLost();
		void RegisterDeviceNotify(IDeviceNotify* deviceNotify);
		void Trim();
        void Resume();
		void Present();

		// The size of the render target, in pixels.
		Windows::Foundation::Size	GetOutputSize() const					{ return m_outputSize; }

		// The size of the render target, in dips.
		Windows::Foundation::Size	GetLogicalSize() const					{ return m_logicalSize; }
		float						GetDpi() const							{ return m_effectiveDpi; }

		// D3D Accessors.
		ID3D11Device3*				GetD3DDevice() const					{ return m_d3dDevice.Get(); }
		ID3D11DeviceContext3*		GetD3DDeviceContext() const				{ return m_d3dContext.Get(); }
		IDXGISwapChain3*			GetSwapChain() const					{ return m_swapChain.Get(); }
		D3D_FEATURE_LEVEL			GetDeviceFeatureLevel() const			{ return m_d3dFeatureLevel; }
		ID3D11RenderTargetView1*	GetBackBufferRenderTargetView() const	{ return m_d3dRenderTargetView.Get(); }
		ID3D11DepthStencilView*		GetDepthStencilView() const				{ return m_d3dDepthStencilView.Get(); }
		D3D11_VIEWPORT				GetScreenViewport() const				{ return m_screenViewport; }
		DirectX::XMFLOAT4X4			GetOrientationTransform3D() const		{ return m_orientationTransform3D; }

		// D2D Accessors.
		ID2D1Factory3*				GetD2DFactory() const					{ return m_d2dFactory.Get(); }
		ID2D1Device2*				GetD2DDevice() const					{ return m_d2dDevice.Get(); }
		ID2D1DeviceContext2*		GetD2DDeviceContext() const				{ return m_d2dContext.Get(); }
		ID2D1Bitmap1*				GetD2DTargetBitmap() const				{ return m_d2dTargetBitmap.Get(); }
		IDWriteFactory3*			GetDWriteFactory() const				{ return m_dwriteFactory.Get(); }
		IWICImagingFactory2*		GetWicImagingFactory() const			{ return m_wicFactory.Get(); }
		D2D1::Matrix3x2F			GetOrientationTransform2D() const		{ return m_orientationTransform2D; }

        // GAME DX Common resources
        GameResources*            GetGameResources() const                { return m_gameResources.get(); }

	private:
		void CreateDeviceIndependentResources();
		void CreateDeviceResources();
		void CreateWindowSizeDependentResources();
		void UpdateRenderTargetSize();
		DXGI_MODE_ROTATION ComputeDisplayRotation();

		// Direct3D objects.
		Microsoft::WRL::ComPtr<ID3D11Device3>			m_d3dDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext3>	m_d3dContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain3>			m_swapChain;

		// Direct3D rendering objects. Required for 3D.
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView1>	m_d3dRenderTargetView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	m_d3dDepthStencilView;
		D3D11_VIEWPORT									m_screenViewport;

		// Direct2D drawing components.
		Microsoft::WRL::ComPtr<ID2D1Factory3>		m_d2dFactory;
		Microsoft::WRL::ComPtr<ID2D1Device2>		m_d2dDevice;
		Microsoft::WRL::ComPtr<ID2D1DeviceContext2>	m_d2dContext;
		Microsoft::WRL::ComPtr<ID2D1Bitmap1>		m_d2dTargetBitmap;

		// DirectWrite drawing components.
		Microsoft::WRL::ComPtr<IDWriteFactory3>		m_dwriteFactory;
		Microsoft::WRL::ComPtr<IWICImagingFactory2>	m_wicFactory;

		// Cached reference to the Window.
		Platform::Agile<Windows::UI::Core::CoreWindow> m_window;

		// Cached device properties.
		D3D_FEATURE_LEVEL								m_d3dFeatureLevel;
		Windows::Foundation::Size						m_d3dRenderTargetSize;
		Windows::Foundation::Size						m_outputSize;
		Windows::Foundation::Size						m_logicalSize;
		Windows::Graphics::Display::DisplayOrientations	m_nativeOrientation;
		Windows::Graphics::Display::DisplayOrientations	m_currentOrientation;
		float											m_dpi;

		// This is the DPI that will be reported back to the app. It takes into account whether the app supports high resolution screens or not.
		float m_effectiveDpi;

		// Transforms used for display orientation.
		D2D1::Matrix3x2F	m_orientationTransform2D;
		DirectX::XMFLOAT4X4	m_orientationTransform3D;

		// The IDeviceNotify can be held directly as it owns the DeviceResources.
		IDeviceNotify* m_deviceNotify;

        // game dx resources        
        std::unique_ptr<GameResources> m_gameResources;

    };
}