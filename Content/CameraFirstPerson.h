#pragma once
#include <DirectXMath.h>

using namespace DirectX;
namespace DX { class StepTimer;  }

namespace SpookyAdulthood
{
#define CAM_DEFAULT_HEIGHT 0.45f
#define CAM_DEFAULT_FOVY 70.0f
#define CAM_DEFAULT_RADIUS 0.25f
    struct CameraFirstPerson
    {
        enum eAction{ AC_NONE, AC_SHOOT };

        CameraFirstPerson(float fovYDeg = CAM_DEFAULT_FOVY);
        void ComputeProjection(float fovAngleYRad, float aspectRatio, float Near, float Far, const XMFLOAT4X4& orientationMatrix);
        void ComputeViewLookAt(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up = XMFLOAT3(0, 1, 0));
        template<typename T, typename A>
        inline void Update(DX::StepTimer const& timer, T& collisionFun, A& actionFun);
        inline XMFLOAT3 GetPosition() const { return m_xyz; }
        inline XMFLOAT3 GetPositionWithMovement() const { return m_movxyz; }
        void SetPosition(const XMFLOAT3& p);
        float ComputeHeightAtHit(const XMFLOAT3& hit);
        inline float RadiusCollideSq() const { return m_radiusCollide*m_radiusCollide; }

        XMFLOAT4X4 m_projection;
        XMFLOAT4X4 m_orientMatrix;
        XMFLOAT4X4 m_view;
        XMFLOAT2 m_pitchYaw;
        XMFLOAT3 m_movDir;        
        XMVECTOR m_camXZ;
        XMFLOAT3 m_xyz, m_movxyz;
        XMFLOAT3 m_forward;
        float m_height;
        float m_radius;
        float m_radiusCollide;
        float m_aspectRatio;
        float m_fovAngleYRad;
        float m_near, m_far;        
        float m_runningTime;
        float m_timeShoot;
        float m_timeToNextShoot;
        float m_rightDownTime;
        float m_shotgunRange;
        bool m_leftDown;
        bool m_running;
        bool m_moving;
    };

    // there are better(faster) ways to do this, anyways
    template<typename T, typename A>
    void CameraFirstPerson::Update(DX::StepTimer const& timer, T& collisionFun, A& actionFun)
    {
        const float dt = (float)timer.GetElapsedSeconds();
        auto ms = DirectX::Mouse::Get().GetState();
        auto kb = DirectX::Keyboard::Get().GetState();
        m_running = false;// kb.LeftShift;

        if (m_timeToNextShoot <= 0.0f)
        {
            if (ms.leftButton)
            {
                if (!m_leftDown)
                {
                    m_leftDown = true;
                    actionFun(AC_SHOOT);
                    m_timeShoot = 0.5f;
                    m_timeToNextShoot = 0.85f;
                }
            }
            else
            {
                if (m_leftDown)
                {
                    m_leftDown = false;
                }
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
        const float movDelta = dt * (m_running ? 2.0f : 2.0f);
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
        m_timeToNextShoot -= dt;
        XMMATRIX ry = XMMatrixRotationY(m_pitchYaw.y + sin(m_runningTime*hvel)*0.02f);        
        XMMATRIX rx = XMMatrixRotationX(m_pitchYaw.x - std::max(m_timeShoot*0.2f,0.0f) + cos(m_runningTime*hvel)*0.01f);
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
            XMStoreFloat3(&m_movDir, XMVectorSubtract(newPos, m_camXZ));
            m_movDir.z = -m_movDir.z;

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
            m_movDir.x = m_movDir.y = m_movDir.z = 0.0f;
        }

        // walking pseudo effect
        const float offsX = cos(m_runningTime*hvel)*0.04f;
        const float offsY = sin(m_runningTime*hvel)*0.03f;
        
        // rotate camera and translate
        XMMATRIX t = XMMatrixTranslation(-XMVectorGetX(m_camXZ)-offsX, -m_height-offsY, XMVectorGetZ(m_camXZ));
        XMStoreFloat3(&m_xyz, m_camXZ);
        m_movxyz = m_xyz;
        m_movxyz.z = -m_movxyz.z;
        m_movxyz.x += offsX;
        m_xyz.y = m_height;
        m_xyz.z = -m_xyz.z;
        XMMATRIX m = XMMatrixMultiply(ry, rx);
        XMStoreFloat3(&m_forward, XMVectorNegate(XMMatrixTranspose(m).r[2]));        
        m = XMMatrixMultiply(t, m);

        // udpate view mat
        XMStoreFloat4x4(&m_view, XMMatrixTranspose(m));

        //if (m_near >= 0.0f)
        //{
        //    const float varA = 8.0f + sinf(m_runningTime)*8.0f;
        //    const float finA = CAM_DEFAULT_FOVY + varA;
        //    ComputeProjection(finA*XM_PI / 180.0f, m_aspectRatio, m_near, m_far, m_orientMatrix);
        //}
    }
}