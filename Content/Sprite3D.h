#pragma once
#include "ShaderStructures.h"

using namespace DirectX;
namespace DX { class DeviceResources; }

namespace SpookyAdulthood
{
    struct CameraFirstPerson;
    struct Sprite3D
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureSRV;
        std::wstring m_filename;
    };

    struct SpriteRender3D
    {
        size_t m_index;
        XMFLOAT3 m_position;
        XMFLOAT2 m_size;
        float m_distToCameraSq;
    };

    class Sprite3DManager
    {
    public:
        Sprite3DManager(const std::shared_ptr<DX::DeviceResources>& device);

        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Begin(const CameraFirstPerson& camera);
        void End();
        void Render(int spriteIndex, const XMFLOAT3& position, const XMFLOAT2& size);
        int CreateSprite(const std::wstring& pathToTex, int at=-1);
        Sprite3D& GetSprite(int ndx);

    private:        
        std::shared_ptr<DX::DeviceResources>    m_device;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	m_indexBuffer;
        std::vector<Sprite3D> m_sprites;
        XMMATRIX m_camInvYaw;
        ModelViewProjectionConstantBuffer m_cbData;
        std::vector<SpriteRender3D> m_spritesToRender;
        XMFLOAT3 m_camPosition;
        bool m_rendering;
    };

}