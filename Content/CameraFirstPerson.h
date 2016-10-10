#pragma once
#include <DirectXMath.h>

using namespace DirectX;
namespace DX { class StepTimer;  }

namespace SpookyAdulthood
{
#define CAM_DEFAULT_HEIGHT 0.65f
#define CAM_DEFAULT_FOVY 70.0f
#define CAM_DEFAULT_RADIUS 0.5f
    struct CameraFirstPerson
    {
        CameraFirstPerson(float fovYDeg = CAM_DEFAULT_FOVY);
        void ComputeProjection(float fovAngleYRad, float aspectRatio, float Near, float Far, const XMFLOAT4X4& orientationMatrix);
        void ComputeViewLookAt(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up = XMFLOAT3(0, 1, 0));
        template<typename T>
        void Update(DX::StepTimer const& timer, T& collisionFun);
        XMFLOAT3 GetPosition() const { return m_xyz; }
        void SetPosition(const XMFLOAT3& p);

        XMFLOAT4X4 m_projection;
        XMFLOAT4X4 m_view;
        XMFLOAT2 m_pitchYaw;
        float m_height;
        float m_radius;
    private:
        XMVECTOR m_camXZ;
        XMFLOAT3 m_xyz;
    };

    // there are better(faster) ways to do this, anyways
    template<typename T>
    void CameraFirstPerson::Update(DX::StepTimer const& timer, T& collisionFun)
    {
        auto ms = DirectX::Mouse::Get().GetState();
        auto kb = DirectX::Keyboard::Get().GetState();

        // update cam
        const float dt = (float)timer.GetElapsedSeconds();
        const float rotDelta = dt*XM_PIDIV4;
        const float movDelta = dt * (kb.LeftShift ? 6.0f : 1.0f);

        // rotation input
        m_pitchYaw.y += rotDelta*0.8f*ms.x;
        m_pitchYaw.x += rotDelta*0.5f*ms.y;
        if (m_pitchYaw.x > XM_PIDIV4) m_pitchYaw.x = XM_PIDIV4;
        if (m_pitchYaw.x < -XM_PIDIV4) m_pitchYaw.x = -XM_PIDIV4;

        // movement input
        float movFw = 0.0f;
        float movSt = 0.0f;
        if (kb.Up || kb.W) movFw = 1.0f;
        else if (kb.Down || kb.S) movFw = -1.0f;
        if (kb.A || kb.Left) movSt = -1.0f;
        else if (kb.D || kb.Right) movSt = 1.0f;

        if (kb.Q) m_height += movDelta;
        else if (kb.E) m_height -= movDelta;

        XMMATRIX ry = XMMatrixRotationY(m_pitchYaw.y);
        XMMATRIX rx = XMMatrixRotationX(m_pitchYaw.x);
        if (movFw || movSt)
        {
            // normalize if moving two axes to avoid strafe+fw cheat
            const float il = 1.0f / sqrtf(movFw*movFw + movSt*movSt);
            movFw *= il*movDelta; movSt *= il*movDelta;

            // move forward and strafe
            XMVECTOR fw = ry.r[2];
            XMVECTOR md = XMVectorSet(movFw, movFw, movFw, movFw);
            XMVECTOR newPos = XMVectorMultiplyAdd(fw, md, m_camXZ);
            XMVECTOR ri = ry.r[0];
            md = XMVectorSet(movSt, movSt, movSt, movSt);
            newPos = XMVectorMultiplyAdd(ri, md, newPos);
            XMVECTOR movDir = XMVectorSubtract(newPos, m_camXZ);

            // collision func
            XMVECTOR cp = m_camXZ, np = newPos;
            cp = XMVectorSetZ(cp, -XMVectorGetZ(cp));
            np = XMVectorSetZ(np, -XMVectorGetZ(np));
            m_camXZ = collisionFun(cp,np,m_radius);
            m_camXZ = XMVectorSetZ(m_camXZ, -XMVectorGetZ(m_camXZ));
        }

        // rotate camera and translate
        XMMATRIX t = XMMatrixTranslation(-XMVectorGetX(m_camXZ), -m_height, XMVectorGetZ(m_camXZ));
        XMStoreFloat3(&m_xyz, m_camXZ);
        m_xyz.y = m_height;
        m_xyz.z = -m_xyz.z;
        XMMATRIX m = XMMatrixMultiply(ry, rx);
        m = XMMatrixMultiply(t, m);

        // udpate view mat
        XMStoreFloat4x4(&m_view, XMMatrixTranspose(m));
    }
}