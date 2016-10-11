#include "pch.h"
#include "Sprite3D.h"
#include "../Common/DirectXHelper.h"
#include "../Common/DeviceResources.h"
#include "CameraFirstPerson.h"

using namespace DirectX;

namespace SpookyAdulthood
{

Sprite3DManager::Sprite3DManager(const std::shared_ptr<DX::DeviceResources>& device)
    : m_device(device), m_rendering(false)
{

}

void Sprite3DManager::CreateDeviceDependentResources()
{
    if (m_vertexBuffer)
        return;
    typedef VertexPositionNormalColorTexture vt;
    XMFLOAT4 white(1, 1, 1, 1);
    vt vertices[4] = 
    {
        vt(XMFLOAT3(-0.5f,-0.5f,0), XMFLOAT3(0,0.45f,1), white, XMFLOAT2(0,1)),
        vt(XMFLOAT3( 0.5f,-0.5f,0), XMFLOAT3(0,0.45f,1), white, XMFLOAT2(1,1)),
        vt(XMFLOAT3( 0.5f, 0.5f,0), XMFLOAT3(0,0.45f,1), white, XMFLOAT2(1,0)),
        vt(XMFLOAT3(-0.5f, 0.5f,0), XMFLOAT3(0,0.45f,1), white, XMFLOAT2(0,0))
    };
    unsigned short indices[6] = { 0, 1, 2, 0, 2, 3 };

    // VB
    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = vertices;
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    const UINT vbsize = UINT(sizeof(VertexPositionNormalColorTexture)*4);
    CD3D11_BUFFER_DESC vertexBufferDesc(vbsize, D3D11_BIND_VERTEX_BUFFER);
    DX::ThrowIfFailed(
        m_device->GetD3DDevice()->CreateBuffer(
            &vertexBufferDesc,
            &vertexBufferData,
            &m_vertexBuffer
        )
    );

    // IB
    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = indices;
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC indexBufferDesc(UINT(sizeof(unsigned short)*6), D3D11_BIND_INDEX_BUFFER);
    DX::ThrowIfFailed(
        m_device->GetD3DDevice()->CreateBuffer(
            &indexBufferDesc,
            &indexBufferData,
            &m_indexBuffer
        )
    );

    for (auto& sp : m_sprites)
    {
        sp.m_texture.Reset();
        sp.m_textureSRV.Reset();
    }
}

void Sprite3DManager::ReleaseDeviceDependentResources()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    
    for (int i = 0; i < (int)m_sprites.size();++i)
    {
        CreateSprite(m_sprites[i].m_filename, i);
    }
}


void Sprite3DManager::Render(int spriteIndex, const XMFLOAT3& position, const XMFLOAT2& size)
{
    DX::ThrowIfFalse(m_rendering); // Begin not called

    using namespace DirectX::SimpleMath;
    if (!m_vertexBuffer) 
        return;
    if (spriteIndex < 0 || spriteIndex >= (int)m_sprites.size()) 
        return;
    
    auto dxCommon = m_device->GetGameResources();
    auto context = m_device->GetD3DDeviceContext();
    auto& sprite = m_sprites[spriteIndex];    
    
    // billboard constrained to up vector (cheap computation)
    XMMATRIX mr = m_camInvYaw;
    mr.r[3] = XMVectorSet(position.x, position.y, position.z, 1.0f);
    
    // prepare buffer for VS
    XMStoreFloat4x4(&m_cbData.model, XMMatrixTranspose(mr));    

    // render
    context->UpdateSubresource1(dxCommon->m_constantBuffer.Get(),0,NULL,&m_cbData,0,0,0);
    context->VSSetConstantBuffers1(0, 1, dxCommon->m_constantBuffer.GetAddressOf(), nullptr, nullptr);
    context->PSSetShaderResources(0, 1, sprite.m_textureSRV.GetAddressOf());
    UINT stride = sizeof(VertexPositionNormalColorTexture);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->DrawIndexed(6, 0, 0);
}

int Sprite3DManager::CreateSprite(const std::wstring& pathToTex, int at/*=-1*/)
{
    Sprite3D spr;
    spr.m_filename = pathToTex;

    if (pathToTex.substr(pathToTex.find_last_of(L".") + 1) == L"dds")
    {
        DX::ThrowIfFailed(
            DirectX::CreateDDSTextureFromFile(
                m_device->GetD3DDevice(), pathToTex.c_str(),
                (ID3D11Resource**)spr.m_texture.ReleaseAndGetAddressOf(),
                spr.m_textureSRV.ReleaseAndGetAddressOf()));
    }
    else
    {
        DX::ThrowIfFailed(
            DirectX::CreateWICTextureFromFile(
                m_device->GetD3DDevice(), pathToTex.c_str(),
                (ID3D11Resource**)spr.m_texture.ReleaseAndGetAddressOf(),
                spr.m_textureSRV.ReleaseAndGetAddressOf()));
    }

    if (at >= 0 && at < (int)m_sprites.size())
    {
        m_sprites[at] = spr;
    }
    else
    {
        at = (int)m_sprites.size();
        m_sprites.push_back(spr);
    }
    return at;
}

void Sprite3DManager::Begin(const CameraFirstPerson& camera)
{
    DX::ThrowIfFalse(!m_rendering);
    m_rendering = true;

    auto dxCommon = m_device->GetGameResources();
    auto context = m_device->GetD3DDeviceContext();

    // set state for render
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(dxCommon->m_inputLayout.Get());
    context->VSSetShader(dxCommon->m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(dxCommon->m_pixelShader.Get(), nullptr, 0);
    ID3D11SamplerState* sampler = dxCommon->GetCommonStates()->PointClamp();
    context->PSSetSamplers(0, 1, &sampler);
    context->OMSetDepthStencilState(dxCommon->GetCommonStates()->DepthDefault(), 0);
    context->OMSetBlendState(dxCommon->GetCommonStates()->AlphaBlend(), nullptr, 0xffffffff);
    context->RSSetState(dxCommon->GetCommonStates()->CullNone());

    m_camInvYaw = XMMatrixRotationY(-camera.m_pitchYaw.y);
    m_cbData.view = camera.m_view;
    m_cbData.projection = camera.m_projection;
}

void Sprite3DManager::End()
{
    DX::ThrowIfFalse(m_rendering);
    m_rendering = false;
}


};
