﻿#include "pch.h"
#include "SceneRenderer.h"

#include "..\Common\DirectXHelper.h"

using namespace SpookyAdulthood;

using namespace DirectX;
using namespace Windows::Foundation;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
SceneRenderer::SceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_tracking(false),
	m_deviceResources(deviceResources),
    m_camRotation(0,0),
    m_timeUntilNextGen(0.0)
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
    m_camXZ = XMVectorSet(0, 0, 0, 0);
}

// Initializes view parameters when the window size changes.
void SceneRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	// Note that the OrientationTransform3D matrix is post-multiplied here
	// in order to correctly orient the scene to match the display orientation.
	// This post-multiplication step is required for any draw calls that are
	// made to the swap chain render target. For draw calls to other targets,
	// this transform should not be applied.

	// This sample makes use of a right-handed coordinate system using row-major matrices.
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
		fovAngleY,
		aspectRatio,
		0.01f,
		100.0f
		);

	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();

	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(
		&m_constantBufferData.projection,
		XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
		);

	// Eye is at (0,0.7,1.5), looking at point (0,-0.1,0) with the up-vector along the y-axis.
	static const XMVECTORF32 eye = { 0.0f, 0.50f, -4.0f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, 0.50, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

    XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void SceneRenderer::Update(DX::StepTimer const& timer)
{
	if (!m_tracking)
	{
		// Convert degrees to radians, then convert seconds to rotation angle
		float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
		double totalRotation = timer.GetTotalSeconds() * radiansPerSecond;
		float radians = static_cast<float>(fmod(totalRotation, XM_2PI));

		Rotate(radians);
	}

    if (m_loadingComplete)
    {
        auto kb = DirectX::Keyboard::Get().GetState();
        m_timeUntilNextGen -= timer.GetElapsedSeconds();
        if (kb.D1 && m_timeUntilNextGen <= 0.0)
        {
            m_timeUntilNextGen = 0.25;
            m_mapSettings.m_tileCount = XMUINT2(30, 30);
            m_mapSettings.m_minTileCount = XMUINT2(3, 3);
            m_mapSettings.m_maxTileCount = XMUINT2(10, 10);
            m_map.Generate(m_mapSettings);
            
            D3D11_TEXTURE2D_DESC desc = { 0 };
            desc.ArraySize = 1;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = 0;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.Width = m_mapSettings.m_tileCount.x;
            desc.Height = m_mapSettings.m_tileCount.y;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;

            D3D11_SUBRESOURCE_DATA initData = { 0 };
            XMUINT2 mapSize;
            initData.pSysMem = m_map.GetThumbTexPtr(&mapSize);
            initData.SysMemSlicePitch = initData.SysMemPitch = sizeof(uint32_t)*mapSize.x;
            initData.SysMemSlicePitch *= mapSize.y;
            DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture2D(&desc, &initData, m_texture.ReleaseAndGetAddressOf()));
            DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(m_texture.Get(), nullptr, m_textureView.ReleaseAndGetAddressOf()));
        }


        UpdateCamera(kb, timer);        
    }
}

// Rotate the 3D cube model a set amount of radians.
void SceneRenderer::Rotate(float radians)
{
	// Prepare to pass the updated model matrix to the shader
	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(0)));
}

void SceneRenderer::StartTracking()
{
	m_tracking = true;
}

// When tracking, the 3D cube can be rotated around its Y axis by tracking pointer position relative to the output screen width.
void SceneRenderer::TrackingUpdate(float positionX)
{
	if (m_tracking)
	{
		float radians = XM_2PI * 2.0f * positionX / m_deviceResources->GetOutputSize().Width;
		Rotate(radians);
	}
}

void SceneRenderer::StopTracking()
{
	m_tracking = false;
}

// Renders one frame using the vertex and pixel shaders.
void SceneRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// Prepare the constant buffer to send it to the graphics device.
	context->UpdateSubresource1(
		m_constantBuffer.Get(),
		0,
		NULL,
		&m_constantBufferData,
		0,
		0,
		0
		);

	// Each vertex is one instance of the VertexPositionColor struct.
	UINT stride = sizeof(VertexPositionColor);
	UINT offset = 0;
	context->IASetVertexBuffers(
		0,
		1,
		m_vertexBuffer.GetAddressOf(),
		&stride,
		&offset
		);

	context->IASetIndexBuffer(
		m_indexBuffer.Get(),
		DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
		0
		);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->IASetInputLayout(m_inputLayout.Get());

	// Attach our vertex shader.
	context->VSSetShader(
		m_vertexShader.Get(),
		nullptr,
		0
		);

	// Send the constant buffer to the graphics device.
	context->VSSetConstantBuffers1(
		0,
		1,
		m_constantBuffer.GetAddressOf(),
		nullptr,
		nullptr
		);

	// Attach our pixel shader.
	context->PSSetShader(
		m_pixelShader.Get(),
		nullptr,
		0
		);

	// Draw the objects.
	context->DrawIndexed(
		m_indexCount,
		0,
		0
		);

    auto sprites = m_deviceResources->GetSprites();
    sprites->Begin(DirectX::SpriteSortMode_Deferred, nullptr, m_deviceResources->GetCommonStates()->PointClamp());
    sprites->Draw(m_textureView.Get(), XMFLOAT2(10, 10), nullptr, Colors::White, 0, XMFLOAT2(0, 0), XMFLOAT2(16, 16));
    sprites->End();
}

void SceneRenderer::CreateDeviceDependentResources()
{
	// Load shaders asynchronously.
	auto loadVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso");

	// After the vertex shader file is loaded, create the shader and input layout.
	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateVertexShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_vertexShader
				)
			);

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc [] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT,          0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateInputLayout(
				vertexDesc,
				ARRAYSIZE(vertexDesc),
				&fileData[0],
				fileData.size(),
				&m_inputLayout
				)
			);
	});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_pixelShader
				)
			);

		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer) , D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&constantBufferDesc,
				nullptr,
				&m_constantBuffer
				)
			);
	});

	// Once both shaders are loaded, create the mesh.
	auto createCubeTask = (createPSTask && createVSTask).then([this] () {

		// Load mesh vertices. Each vertex has a position and a color.
		static const VertexPositionColor cubeVertices[] = 
		{
            {XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0,0,0)},
			{XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(0,0,0)},
			{XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0,0,0)},
			{XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f), XMFLOAT3(0,0,0)},
			{XMFLOAT3( 0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0,0,0)},
			{XMFLOAT3( 0.5f, -0.5f,  0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT3(0,0,0)},
			{XMFLOAT3( 0.5f,  0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT3(0,0,0)},
			{XMFLOAT3( 0.5f,  0.5f,  0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0,0,0)},
		};

		D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
		vertexBufferData.pSysMem = cubeVertices;
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&vertexBufferDesc,
				&vertexBufferData,
				&m_vertexBuffer
				)
			);

		// Load mesh indices. Each trio of indices represents
		// a triangle to be rendered on the screen.
		// For example: 0,2,1 means that the vertices with indexes
		// 0, 2 and 1 from the vertex buffer compose the 
		// first triangle of this mesh.
		static const unsigned short cubeIndices [] =
		{
			0,2,1, // -x
			1,2,3,

			4,5,6, // +x
			5,7,6,

			0,1,5, // -y
			0,5,4,

			2,6,7, // +y
			2,7,3,

			0,4,6, // -z
			0,6,2,

			1,3,7, // +z
			1,7,5,
		};

		m_indexCount = ARRAYSIZE(cubeIndices);

		D3D11_SUBRESOURCE_DATA indexBufferData = {0};
		indexBufferData.pSysMem = cubeIndices;
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&indexBufferDesc,
				&indexBufferData,
				&m_indexBuffer
				)
			);
	});

    // after mesh, load texture from file
    auto loadTextureTask = (createCubeTask).then([this]() 
    {
        DX::ThrowIfFailed(
            DirectX::CreateDDSTextureFromFile(m_deviceResources->GetD3DDevice(), L"assets\\windowslogo.dds",
                (ID3D11Resource**)m_texture.ReleaseAndGetAddressOf(), m_textureView.ReleaseAndGetAddressOf())
        );        
    });

	// Once the texture is loaded, the object is ready to be rendered.
    loadTextureTask.then([this] () {
		m_loadingComplete = true;
        DirectX::Mouse::Get().SetMode(DirectX::Mouse::MODE_RELATIVE);
	});
}

void SceneRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
    m_texture.Reset();
    m_textureView.Reset();
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_constantBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
}

void SceneRenderer::UpdateCamera(const DirectX::Keyboard::State& kb, DX::StepTimer const & timer)
{
    auto ms = DirectX::Mouse::Get().GetState();

    // update cam
    const float dt = (float)timer.GetElapsedSeconds();
    const float rotDelta = dt*XM_PIDIV2;
    const float movDelta = dt*2.0f;
    
    // rotation input
    m_camRotation.y += rotDelta*ms.x;
    m_camRotation.x += rotDelta*0.5f*ms.y;
    if (m_camRotation.x > XM_PIDIV4) m_camRotation.x = XM_PIDIV4;
    if (m_camRotation.x < -XM_PIDIV4) m_camRotation.x = -XM_PIDIV4;

    // movement input
    float movFw = 0.0f;
    float movSt = 0.0f;
    if (kb.Up||kb.W) movFw = 1.0f;
    else if (kb.Down||kb.S) movFw = -1.0f;
    if (kb.A||kb.Left) movSt = -1.0f;
    else if (kb.D||kb.Right) movSt = 1.0f;

    XMMATRIX ry = XMMatrixRotationY(m_camRotation.y);
    XMMATRIX rx = XMMatrixRotationX(m_camRotation.x);
    if (movFw||movSt)
    {
        // normalize if moving two axes to avoid strafe+fw cheat
        const float il = 1.0f/sqrtf(movFw*movFw + movSt*movSt);
        movFw *= il*movDelta; movSt *= il*movDelta;
        
        // move forward and strafe
        XMVECTOR fw = ry.r[2];
        XMVECTOR md = XMVectorSet(movFw, movFw, movFw, movFw);
        m_camXZ = XMVectorMultiplyAdd(fw, md, m_camXZ);        
        XMVECTOR ri = ry.r[0];
        md = XMVectorSet(movSt, movSt, movSt, movSt);
        m_camXZ = XMVectorMultiplyAdd(ri, md, m_camXZ);
    }

    // rotate camera and translate
    XMMATRIX t = XMMatrixTranslation(-XMVectorGetX(m_camXZ), -0.5f, XMVectorGetZ(m_camXZ));
    XMMATRIX m = XMMatrixMultiply(ry, rx);
    m = XMMatrixMultiply(t, m);

    // udpate for shader
    XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(m));
}