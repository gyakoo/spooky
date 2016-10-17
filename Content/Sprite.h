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
        size_t m_index; // if m_isAnim==true then index to m_animInstances
        XMFLOAT3 m_position;
        XMFLOAT2 m_size;
        float m_distSqOrRot;
        bool m_isAnim;
        bool m_disableDepth;
    };

    struct SpriteAnimation
    {
        std::vector<int> m_indices;
        float m_period;
        bool m_looped;
    };

    struct SpriteAnimationInstance
    {
        int m_animIndex;
        size_t m_curFrame;
        float m_frameTime;
        bool m_active;
    };

    class SpriteManager
    {
    public:
        SpriteManager(const std::shared_ptr<DX::DeviceResources>& device);

        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        
        void Update(const DX::StepTimer& timer);

        void Begin3D(const CameraFirstPerson& camera);
        void End3D();
        void Draw3D(int spriteIndex, const XMFLOAT3& position, const XMFLOAT2& size, bool disableDepth=false);
        
        void Begin2D(const CameraFirstPerson& camera);
        void End2D();
        void Draw2D(int spriteIndex, const XMFLOAT2& position, const XMFLOAT2& size, float rot);
        void Draw2DAnimation(int instIndex, const XMFLOAT2& position, const XMFLOAT2& size, float rot);

        int CreateSprite(const std::wstring& pathToTex, int at = -1);
        int CreateAnimation(const std::vector<int>& spritesIndices, float fps, bool loop=false);
        int CreateAnimationInstance(int animationIndex, int at=-1);
        
        Sprite& GetSprite(int ndx);

        void DrawScreenQuad(ID3D11ShaderResourceView* srv, const XMFLOAT4& params0, const XMFLOAT4& params1=XMFLOAT4(0,0,0,0));

    private:
        std::shared_ptr<DX::DeviceResources>    m_device;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>	m_indexBuffer;
        std::vector<Sprite> m_sprites;
        XMMATRIX m_camInvYaw;
        ModelViewProjectionConstantBuffer m_cbData;
        std::vector<SpriteRender> m_spritesToRender[2];
        std::vector<SpriteAnimation> m_animations;
        std::vector<SpriteAnimationInstance> m_animInstances;
        float m_aspectRatio;
        XMFLOAT3 m_camPosition;
        bool m_rendering[2];
    };

}