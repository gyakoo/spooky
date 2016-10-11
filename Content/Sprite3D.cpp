#include "pch.h"
#include "Sprite3D.h"
#include "../Common/DirectXHelper.h"
#include "../Common/DeviceResources.h"
#include "CameraFirstPerson.h"
#include "ShaderStructures.h"

using namespace DirectX;

namespace SpookyAdulthood
{

Sprite3DManager::Sprite3DManager(const std::shared_ptr<DX::DeviceResources>& device)
    : m_device(device)
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
        vt(XMFLOAT3(-0.5f,-0.5f,0), XMFLOAT3(0,0,-1), white, XMFLOAT2(0,1)),
        vt(XMFLOAT3( 0.5f,-0.5f,0), XMFLOAT3(0,0,-1), white, XMFLOAT2(1,1)),
        vt(XMFLOAT3( 0.5f, 0.5f,0), XMFLOAT3(0,0,-1), white, XMFLOAT2(1,0)),
        vt(XMFLOAT3(-0.5f, 0.5f,0), XMFLOAT3(0,0,-1), white, XMFLOAT2(0,0))
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


void Sprite3DManager::Render(int spriteIndex, const CameraFirstPerson& camera, const XMFLOAT3& position, const XMFLOAT2& size)
{
    using namespace DirectX::SimpleMath;
    if (!m_vertexBuffer) 
        return;
    if (spriteIndex < 0 || spriteIndex >= (int)m_sprites.size()) 
        return;
    
    auto dxCommon = m_device->GetGameResources();
    auto context = m_device->GetD3DDeviceContext();
    static float a = 0.0f; a += .001f;
    XMMATRIX mr = XMMatrixRotationY(a);
    XMMATRIX mt = XMMatrixTranslation(position.x, position.y, position.z);
    XMMATRIX mBB = XMMatrixMultiply(mr, mt);    

    ModelViewProjectionConstantBuffer cbData;
    XMStoreFloat4x4(&cbData.model, XMMatrixTranspose(mBB));
    cbData.view = camera.m_view;
    cbData.projection = camera.m_projection;
    context->UpdateSubresource1(dxCommon->m_constantBuffer.Get(),0,NULL,&cbData,0,0,0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(dxCommon->m_inputLayout.Get());
    context->VSSetShader(dxCommon->m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(dxCommon->m_pixelShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers1(0, 1, dxCommon->m_constantBuffer.GetAddressOf(), nullptr, nullptr);
    context->OMSetDepthStencilState(dxCommon->GetCommonStates()->DepthDefault(), 0);
    context->RSSetState(dxCommon->GetCommonStates()->CullNone());

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
    DX::ThrowIfFailed(
        DirectX::CreateDDSTextureFromFile(
            m_device->GetD3DDevice(), pathToTex.c_str(),
            (ID3D11Resource**)spr.m_texture.ReleaseAndGetAddressOf(),
            spr.m_textureSRV.ReleaseAndGetAddressOf() ) );

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


};
