#pragma once
#include <DirectXMath.h>

using namespace DirectX;
namespace DX { class StepTimer;  }

namespace SpookyAdulthood
{
    struct Camera
    {
        Camera();
        void ComputeProjection(float fovAngleYRad, float aspectRatio, float Near, float Far, const XMFLOAT4X4& orientationMatrix);
        void ComputeViewLookAt(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up = XMFLOAT3(0, 1, 0));
        void Update(DX::StepTimer const& timer);

        XMFLOAT4X4 m_projection;
        XMFLOAT4X4 m_view;
        XMFLOAT2 m_camRotation;
        XMVECTOR m_camXZ;
    };
}

