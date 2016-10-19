#include "pch.h"
#include "Sprite.h"
#include "../Common/DirectXHelper.h"
#include "../Common/DeviceResources.h"
#include "CameraFirstPerson.h"
#include "GlobalFlags.h"

using namespace DirectX;

namespace SpookyAdulthood
{
    enum { R3D=0, R2D=1};

    SpriteManager::SpriteManager(const std::shared_ptr<DX::DeviceResources>& device)
        : m_device(device)
    {
        m_rendering[R3D] = m_rendering[R2D] = false;
    }

    void SpriteManager::CreateDeviceDependentResources()
    {
        if (m_vertexBuffer)
            return;
        typedef VertexPositionNormalColorTextureNdx vt;
        XMFLOAT4 white(1, 1, 1, 1);
        vt vertices[4] =
        {
            vt(XMFLOAT3(-0.5f,-0.5f,0), XMFLOAT3(0,0.45f,1), white, XMFLOAT2(0,1)),
            vt(XMFLOAT3(0.5f,-0.5f,0), XMFLOAT3(0,0.45f,1), white, XMFLOAT2(1,1)),
            vt(XMFLOAT3(0.5f, 0.5f,0), XMFLOAT3(0,0.45f,1), white, XMFLOAT2(1,0)),
            vt(XMFLOAT3(-0.5f, 0.5f,0), XMFLOAT3(0,0.45f,1), white, XMFLOAT2(0,0))
        };
        unsigned short indices[6] = { 0, 3, 2, 0, 2, 1 };

        // VB
        D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
        vertexBufferData.pSysMem = vertices;
        vertexBufferData.SysMemPitch = 0;
        vertexBufferData.SysMemSlicePitch = 0;
        const UINT vbsize = UINT(sizeof(VertexPositionNormalColorTextureNdx) * 4);
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
        CD3D11_BUFFER_DESC indexBufferDesc(UINT(sizeof(unsigned short) * 6), D3D11_BIND_INDEX_BUFFER);
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

    void SpriteManager::ReleaseDeviceDependentResources()
    {
        m_vertexBuffer.Reset();
        m_indexBuffer.Reset();

        for (int i = 0; i < (int)m_sprites.size(); ++i)
        {
            CreateSprite(m_sprites[i].m_filename, i);
        }
    }
    
    void SpriteManager::Draw3D(int spriteIndex, const XMFLOAT3& position, const XMFLOAT2& size, bool disableDepth, bool fullBillboard)
    {
        DX::ThrowIfFalse(m_rendering[R3D]); // Begin not called

        using namespace DirectX::SimpleMath;
        if (!m_vertexBuffer)
            return;
        if (spriteIndex < 0 || spriteIndex >= (int)m_sprites.size())
            return;

        const float x = m_camPosition.x - position.x;
        const float y = m_camPosition.z - position.z;
        SpriteRender sprR = { (size_t)spriteIndex, position, size, x*x + y*y, false, disableDepth, fullBillboard };
        m_spritesToRender[R3D].push_back(sprR);
    }

    int SpriteManager::CreateSprite(const std::wstring& pathToTex, int at/*=-1*/)
    {
        Sprite spr;
        spr.m_filename = pathToTex;
        std::transform(spr.m_filename.begin(), spr.m_filename.end(), spr.m_filename.begin(), ::towlower);
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

    void SpriteManager::Begin3D(const CameraFirstPerson& camera)
    {
        DX::ThrowIfFalse(!m_rendering[R3D]);
        m_rendering[R3D] = true;

        auto dxCommon = m_device->GetGameResources();
        if (!dxCommon->m_readyToRender) return;
        auto context = m_device->GetD3DDeviceContext();

        // set state for render
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(dxCommon->m_baseIL.Get());
        context->VSSetShader(dxCommon->m_baseVS.Get(), nullptr, 0);
        context->PSSetShader(dxCommon->m_basePS.Get(), nullptr, 0);        
        float t = std::max(std::min(camera.m_rightDownTime*2.0f, 1.f), std::max(camera.m_timeShoot*0.35f, 0.0f));
        if (GlobalFlags::AllLit) t = 1.0f;
        PixelShaderConstantBuffer pscb = { { 1,1,dxCommon->m_levelTime,camera.m_aspectRatio }, { t,1.0f-int(GlobalFlags::AllLit),0,0} };
        context->OMSetBlendState(dxCommon->m_commonStates->AlphaBlend(), nullptr, 0xffffffff);
        auto rsState = GlobalFlags::DrawWireframe ? dxCommon->m_commonStates->Wireframe() : dxCommon->m_commonStates->CullCounterClockwise();
        context->RSSetState(rsState);
        context->UpdateSubresource1(dxCommon->m_basePSCB.Get(), 0, NULL, &pscb, 0, 0, 0);
        context->PSSetConstantBuffers(0, 1, dxCommon->m_basePSCB.GetAddressOf());
        m_camInvYaw = XMMatrixRotationY(-camera.m_pitchYaw.y); // billboard oriented to cam (Y constrained)
        m_camInvPitch = XMMatrixRotationX(-camera.m_pitchYaw.x);
        m_cbData.view = camera.m_view;
        m_cbData.projection = camera.m_projection;
        m_camPosition = camera.GetPosition();
        m_spritesToRender[R3D].clear();
    }

    void SpriteManager::End3D()
    {
        DX::ThrowIfFalse(m_rendering[R3D]);
        m_rendering[R3D] = false;

        // sort sprites by distance to camera (for alpha blending to work)
        std::sort(m_spritesToRender[R3D].begin(), m_spritesToRender[R3D].end(), [this](const SpriteRender& a, const SpriteRender& b) -> bool
        {
            const auto& sa = m_sprites[a.m_index];
            const auto& sb = m_sprites[b.m_index];
            return a.m_distSqOrRot > b.m_distSqOrRot;
        });

        auto dxCommon = m_device->GetGameResources();
        if (!dxCommon->m_readyToRender) return;
        auto context = m_device->GetD3DDeviceContext();

        for (const auto& sprI : m_spritesToRender[R3D])
        {
            auto& sprite = m_sprites[sprI.m_index];
            const auto& position = sprI.m_position;
            const auto& size = sprI.m_size;

            // billboard constrained to up vector or full billboard?
            XMMATRIX mr = m_camInvYaw;
            if (sprI.m_fullBillboard)
                mr = XMMatrixMultiply(m_camInvPitch, mr);
            mr.r[3] = XMVectorSet(position.x, position.y, position.z, 1.0f);

            XMMATRIX ms = XMMatrixScaling(size.x, size.y, 1.0f);

            // prepare buffer for VS
            XMStoreFloat4x4(&m_cbData.model, XMMatrixMultiplyTranspose(ms, mr));

            // render
            auto depthS = sprI.m_disableDepth ? dxCommon->m_commonStates->DepthNone() : dxCommon->m_commonStates->DepthDefault();
            context->OMSetDepthStencilState(depthS, 0);
            ID3D11SamplerState* sampler = dxCommon->m_commonStates->PointClamp();   
            context->PSSetSamplers(0, 1, &sampler);
            context->UpdateSubresource1(dxCommon->m_baseVSCB.Get(), 0, NULL, &m_cbData, 0, 0, 0);
            context->VSSetConstantBuffers1(0, 1, dxCommon->m_baseVSCB.GetAddressOf(), nullptr, nullptr);
            context->PSSetShaderResources(0, 1, sprite.m_textureSRV.GetAddressOf());
            UINT stride = sizeof(VertexPositionNormalColorTextureNdx);
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
            context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
            context->DrawIndexed(6, 0, 0);
        }
    }

    Sprite& SpriteManager::GetSprite(int ndx)
    {
        return m_sprites[ndx];
    }

    void SpriteManager::Begin2D(const CameraFirstPerson& camera)
    {
        DX::ThrowIfFalse(!m_rendering[R2D]);
        m_rendering[R2D] = true;
        m_aspectRatio = camera.m_aspectRatio;

        auto dxCommon = m_device->GetGameResources();
        if (!dxCommon->m_readyToRender) return;
        auto context = m_device->GetD3DDeviceContext();
        
        //// set state for render
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(dxCommon->m_baseIL.Get());
        context->VSSetShader(dxCommon->m_spriteVS.Get(), nullptr, 0);
        context->PSSetShader(dxCommon->m_spritePS.Get(), nullptr, 0);
        ID3D11SamplerState* sampler = dxCommon->m_commonStates->PointClamp();
        context->PSSetSamplers(0, 1, &sampler);
        context->OMSetDepthStencilState(dxCommon->m_commonStates->DepthNone(), 0);
        context->OMSetBlendState(dxCommon->m_commonStates->AlphaBlend(), nullptr, 0xffffffff);
        context->RSSetState(dxCommon->m_commonStates->CullCounterClockwise());
        m_spritesToRender[R2D].clear();
    }

    void SpriteManager::End2D()
    {
        DX::ThrowIfFalse(m_rendering[R2D]);
        m_rendering[R2D] = false;

        auto dxCommon = m_device->GetGameResources();
        if (!dxCommon->m_readyToRender) return;
        auto context = m_device->GetD3DDeviceContext();

        for (const auto& sprI : m_spritesToRender[R2D])
        {
            int finalIndex = -1;
            if (sprI.m_isAnim)
            {
                const auto& animInst = m_animInstances[sprI.m_index];
                if (animInst.m_active)
                {
                    const auto& anim = m_animations[animInst.m_animIndex];
                    finalIndex = anim.m_indices[ animInst.m_curFrame % anim.m_indices.size() ];
                }
                else
                {
                    continue;
                }
            }            
            finalIndex = std::max(int(sprI.m_index), finalIndex);
            const auto& sprite = m_sprites[finalIndex];
            const auto& position = sprI.m_position;
            const auto& size = sprI.m_size;

            // simple translate rotate
            XMMATRIX mr = XMMatrixRotationZ(sprI.m_distSqOrRot);
            mr.r[3] = XMVectorSet(position.x, position.y, 0.0f, 1.0f);
            XMMATRIX ms = XMMatrixScaling(size.x/m_aspectRatio, size.y, 1.0f);
            XMStoreFloat4x4(&m_cbData.model, XMMatrixMultiplyTranspose(ms, mr));

            context->UpdateSubresource1(dxCommon->m_baseVSCB.Get(), 0, NULL, &m_cbData, 0, 0, 0);
            context->VSSetConstantBuffers1(0, 1, dxCommon->m_baseVSCB.GetAddressOf(), nullptr, nullptr);
            context->PSSetShaderResources(0, 1, sprite.m_textureSRV.GetAddressOf());
            UINT stride = sizeof(VertexPositionNormalColorTextureNdx);
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
            context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
            context->DrawIndexed(6, 0, 0);
        }

    }
    
    void SpriteManager::Draw2D(int spriteIndex, const XMFLOAT2& position, const XMFLOAT2& size, float rot)
    {
        DX::ThrowIfFalse(m_rendering[R2D]); // Begin not called

        using namespace DirectX::SimpleMath;
        if (!m_vertexBuffer)
            return;
        if (spriteIndex < 0 || spriteIndex >= (int)m_sprites.size())
            return;

        XMFLOAT3 pos(position.x, position.y, 0.0f);
        SpriteRender sprR = { (size_t)spriteIndex, pos, size, rot, false };
        m_spritesToRender[R2D].push_back(sprR);
    }

    void SpriteManager::Draw2DAnimation(int instIndex, const XMFLOAT2& position, const XMFLOAT2& size, float rot)
    {
        DX::ThrowIfFalse(m_rendering[R2D]); // Begin not called
        if (instIndex < 0 || instIndex >= (int)m_animInstances.size()) return;
        const auto& ai = m_animInstances[instIndex];
        if (ai.m_animIndex < 0 || ai.m_animIndex >= (int)m_animations.size()) return;

        XMFLOAT3 pos(position.x, position.y, 0.0f);
        SpriteRender sprR = { (size_t)instIndex, pos, size, rot, true };
        m_spritesToRender[R2D].push_back(sprR);
    }

    int SpriteManager::CreateAnimation(const std::vector<int>& spritesIndices, float fps, bool loop)
    {
        SpriteAnimation anim = { spritesIndices, 1.0f / fps, loop };
        m_animations.push_back(anim);
        return (int)m_animations.size() - 1;
    }

    int SpriteManager::CreateAnimationInstance(int animationIndex, int _at)
    {
        // look for a free usable slot (m_active==false)
        int at = -1;
        if (_at == -1 || _at <0 || _at >= (int)m_animInstances.size())
        {
            for (size_t i = 0; i < m_animInstances.size(); ++i)
            {
                if (!m_animInstances[i].m_active)
                {
                    at = (int)i;
                    break;
                }
            }
        }
        else
        {
            at = _at;
        }
        SpriteAnimationInstance sprAnimInst = { animationIndex, 0,  0.0f, true };
        if (at == -1)
        {
            at = (int)m_animInstances.size();
            m_animInstances.push_back(sprAnimInst);
        }
        else
        {
            m_animInstances[at] = sprAnimInst;
        }

        return at;
    }


    void SpriteManager::DrawScreenQuad(ID3D11ShaderResourceView* srv, const XMFLOAT4& params0, const XMFLOAT4& params1)
    {
        auto dxCommon = m_device->GetGameResources();
        if (!dxCommon->m_readyToRender) return;
        auto context = m_device->GetD3DDeviceContext();

        //// set state for render
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(dxCommon->m_baseIL.Get());
        context->VSSetShader(dxCommon->m_spriteVS.Get(), nullptr, 0);
        context->PSSetShader(dxCommon->m_postPS.Get(), nullptr, 0);
        ID3D11SamplerState* sampler = dxCommon->m_commonStates->PointClamp();
        context->PSSetSamplers(0, 1, &sampler);
        PixelShaderConstantBuffer pscb = { params0, params1 };
        context->UpdateSubresource1(dxCommon->m_basePSCB.Get(), 0, NULL, &pscb, 0, 0, 0);
        context->PSSetConstantBuffers(0, 1, dxCommon->m_basePSCB.GetAddressOf());
        context->OMSetDepthStencilState(dxCommon->m_commonStates->DepthNone(), 0);
        context->OMSetBlendState(dxCommon->m_commonStates->AlphaBlend(), nullptr, 0xffffffff);
        context->RSSetState(dxCommon->m_commonStates->CullCounterClockwise());

        // 
        ModelViewProjectionConstantBuffer cbData;
        XMMATRIX ms = XMMatrixScaling(2.0f, 2.0f, 2.0f);
        XMStoreFloat4x4(&cbData.model, XMMatrixTranspose(ms));

        context->UpdateSubresource1(dxCommon->m_baseVSCB.Get(), 0, NULL, &cbData, 0, 0, 0);
        context->VSSetConstantBuffers1(0, 1, dxCommon->m_baseVSCB.GetAddressOf(), nullptr, nullptr);
        context->PSSetShaderResources(0, 1, &srv);
        UINT stride = sizeof(VertexPositionNormalColorTextureNdx);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->DrawIndexed(6, 0, 0);

        ID3D11ShaderResourceView* nullsrv = nullptr;
        context->PSSetShaderResources(0, 1, &nullsrv);
    }

    void SpriteManager::Update(const DX::StepTimer& timer)
    {
        const float dt = (float)timer.GetElapsedSeconds();

        // update all animations
        for (auto& a : m_animInstances)
        {
            if (!a.m_active) 
                continue;
            const SpriteAnimation& anim = m_animations[a.m_animIndex];            
            a.m_frameTime += dt;
            if (a.m_frameTime >= anim.m_period)
            {
                a.m_frameTime -= anim.m_period;
                a.m_curFrame++;
                if (a.m_curFrame >= (int)anim.m_indices.size())
                {
                    a.m_active = false;
                }
            }
        }
    }


};