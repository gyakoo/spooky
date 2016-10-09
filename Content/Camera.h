#pragma once
#include <DirectXMath.h>

using namespace DirectX;
namespace DX { class StepTimer;  }

namespace SpookyAdulthood
{
    struct Camera
    {
        Camera(float fovYDeg=60.0f);
        void ComputeProjection(float fovAngleYRad, float aspectRatio, float Near, float Far, const XMFLOAT4X4& orientationMatrix);
        void ComputeViewLookAt(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up = XMFLOAT3(0, 1, 0));
        void Update(DX::StepTimer const& timer);
        XMFLOAT3 GetPosition() const { return m_xyz; }
        void SetPosition(const XMFLOAT3& p);
        void SetPlayingMode(bool playingMode);
        bool IsPlaying()const;

        XMFLOAT4X4 m_projection;
        XMFLOAT4X4 m_view;
        XMFLOAT2 m_pitchYaw;
    
    private:
        XMVECTOR m_camXZ;
        XMFLOAT3 m_xyz;
    };
}

