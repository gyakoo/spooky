#pragma once
#include "ShaderStructures.h"

using namespace DirectX;
namespace DX { class DeviceResources; }

namespace SpookyAdulthood
{
    struct CameraFirstPerson;
    struct Sprite
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureSRV;
        std::wstring m_filename;
    };

    struct SpriteRender
    {
        size_t m_index;
        XMFLOAT3 m_position;
        XMFLOAT2 m_size;
        float m_distToCameraSq;
    };

    class SpriteManager
    {
    public:
        SpriteManager(const std::shared_ptr<DX::DeviceResources>& device);

        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        
        void Begin3D(const CameraFirstPerson& camera);
        void End3D();
        void Draw3D(int spriteIndex, const XMFLOAT3& position, const XMFLOAT2& size);
        
        void Begin2D(const CameraFirstPerson& camera);
        void End2D();
        void Draw2D(int spriteIndex, const XMFLOAT2& position, const XMFLOAT2& size, float rot);

        int CreateSprite(const std::wstring& pathToTex, int at = -1);
        Sprite& GetSprite(int ndx);

    private:
        std::shared_ptr<DX::DeviceResources>    m_device;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	m_indexBuffer;
        std::vector<Sprite> m_sprites;
        XMMATRIX m_camInvYaw;
        ModelViewProjectionConstantBuffer m_cbData;
        std::vector<SpriteRender> m_spritesToRender[2];
        float m_aspectRatio;
        XMFLOAT3 m_camPosition;
        bool m_rendering[2];
    };

}