#pragma once

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

    class Sprite3DManager
    {
    public:
        Sprite3DManager(const std::shared_ptr<DX::DeviceResources>& device);

        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Render(int spriteIndex, const CameraFirstPerson& camera, const XMFLOAT3& position, const XMFLOAT2& size);
        int CreateSprite(const std::wstring& pathToTex, int at=-1);

    private:
        //Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_inputLayout;
        //Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
        //Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShader;
        //Microsoft::WRL::ComPtr<ID3D11Buffer>		m_constantBuffer;
        std::shared_ptr<DX::DeviceResources>    m_device;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	m_indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	m_constantBuffer;
        std::vector<Sprite3D> m_sprites;
    };

}