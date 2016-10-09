#pragma once
#include <DirectXMath.h>

using namespace DirectX;
namespace DX { class StepTimer;  }

namespace SpookyAdulthood
{
#define CAM_DEFAULT_HEIGHT 0.65f
#define CAM_DEFAULT_FOVY 70.0f
    struct CameraFirstPerson
    {
        CameraFirstPerson(float fovYDeg= CAM_DEFAULT_FOVY);
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
        float m_height;
    private:
        XMVECTOR m_camXZ;
        XMFLOAT3 m_xyz;
    };
}

