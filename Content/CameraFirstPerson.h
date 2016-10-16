#pragma once
#include <DirectXMath.h>

using namespace DirectX;
namespace DX { class StepTimer;  }

namespace SpookyAdulthood
{
#define CAM_DEFAULT_HEIGHT 0.8f
#define CAM_DEFAULT_FOVY 70.0f
#define CAM_DEFAULT_RADIUS 0.25f
    struct CameraFirstPerson
    {
        enum eAction{ AC_NONE, AC_SHOOT };

        CameraFirstPerson(float fovYDeg = CAM_DEFAULT_FOVY);
        void ComputeProjection(float fovAngleYRad, float aspectRatio, float Near, float Far, const XMFLOAT4X4& orientationMatrix);
        void ComputeViewLookAt(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up = XMFLOAT3(0, 1, 0));
        template<typename T, typename A>
        void Update(DX::StepTimer const& timer, T& collisionFun, A& actionFun);
        XMFLOAT3 GetPosition() const { return m_xyz; }
        void SetPosition(const XMFLOAT3& p);
        XMFLOAT3 GetForward() const;

        XMFLOAT4X4 m_projection;
        XMFLOAT4X4 m_view;
        XMFLOAT2 m_pitchYaw;
        bool m_running;
        bool m_moving;
        float m_height;
        float m_radius;
        float m_aspectRatio;
        float m_fovAngleYRad;
        float m_near, m_far;
        XMFLOAT4X4 m_orientMatrix;
        XMVECTOR m_camXZ;
        XMFLOAT3 m_xyz;
        float m_runningTime;
        float m_timeShoot;
        bool m_leftDown;
        float m_rightDownTime;
    };

    // there are better(faster) ways to do this, anyways
    template<typename T, typename A>
    void CameraFirstPerson::Update(DX::StepTimer const& timer, T& collisionFun, A& actionFun)
    {
        const float dt = (float)timer.GetElapsedSeconds();
        auto ms = DirectX::Mouse::Get().GetState();
        auto kb = DirectX::Keyboard::Get().GetState();
        m_running = kb.LeftShift;
        if (ms.leftButton)
        {
            if (!m_leftDown)
            {
                m_leftDown = true;
                actionFun(AC_SHOOT);
                m_timeShoot = 0.5f;
            }
        }
        else
        {
            if (m_leftDown)
            {
                m_leftDown = false;
            }
        }

        if (ms.rightButton)
        {
            m_rightDownTime += dt;
        }
        else
        {
            m_rightDownTime = 0.0f;
        }

        // update cam
        const float rotDelta = dt*XM_PIDIV4;
        const float movDelta = dt * (m_running ? 3.0f : 1.0f);
        float hvel = 5.0f;

        // rotation input
        m_pitchYaw.y += rotDelta*0.8f*ms.x;
        m_pitchYaw.x += rotDelta*0.5f*ms.y;
        m_pitchYaw.x = Clamp(m_pitchYaw.x, -0.3f, 0.3f);

        // movement input
        float movFw = 0.0f;
        float movSt = 0.0f;
        if (kb.Up || kb.W) movFw = 1.0f;
        else if (kb.Down || kb.S) movFw = -1.0f;
        if (kb.A || kb.Left) movSt = -1.0f;
        else if (kb.D || kb.Right) movSt = 1.0f;

        if (kb.Q) m_height += movDelta;
        else if (kb.E) m_height -= movDelta;

        m_timeShoot -= dt;
        XMMATRIX ry = XMMatrixRotationY(m_pitchYaw.y + sin(m_runningTime*hvel)*0.02f);
        XMMATRIX rx = XMMatrixRotationX(m_pitchYaw.x - max(m_timeShoot*0.2f,0.0f) + cos(m_runningTime*hvel)*0.01f);
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
            m_camXZ = collisionFun(cp, np, m_radius);
            m_camXZ = XMVectorSetZ(m_camXZ, -XMVectorGetZ(m_camXZ));
            m_runningTime += dt;
            m_moving = true;
            if (m_running)
                hvel = 8.0f;
        }
        else
        {
            m_moving = false;
        }

        // walking pseudo effect
        const float offsX = cos(m_runningTime*hvel)*0.04f;
        const float offsY = sin(m_runningTime*hvel)*0.05f;
        
        // rotate camera and translate
        XMMATRIX t = XMMatrixTranslation(-XMVectorGetX(m_camXZ)-offsX, -m_height-offsY, XMVectorGetZ(m_camXZ));
        XMStoreFloat3(&m_xyz, m_camXZ);
        m_xyz.y = m_height;
        m_xyz.z = -m_xyz.z;
        XMMATRIX m = XMMatrixMultiply(ry, rx);
        m = XMMatrixMultiply(t, m);

        // udpate view mat
        XMStoreFloat4x4(&m_view, XMMatrixTranspose(m));

        if (m_near >= 0.0f)
        {
            const float varA = 8.0f + sinf(m_runningTime)*8.0f;
            const float finA = CAM_DEFAULT_FOVY + varA;
            ComputeProjection(finA*XM_PI / 180.0f, m_aspectRatio, m_near, m_far, m_orientMatrix);
        }
    }
}