#include "pch.h"
#include "CameraFirstPerson.h"

using namespace SpookyAdulthood;

CameraFirstPerson::CameraFirstPerson(float fovYDeg)
: m_pitchYaw(0,XM_PI), m_height(CAM_DEFAULT_HEIGHT)
{
    m_camXZ = XMVectorSet(0, 0, 0, 0);
    XMFLOAT4X4 id;
    XMStoreFloat4x4(&id, XMMatrixIdentity());
    ComputeProjection(fovYDeg*XM_PI / 180.0f, 1.0f, 0.01f, 100.0f, id);
    m_view = id; //view identity
}

void CameraFirstPerson::ComputeProjection(float fovAngleYRad, float aspectRatio, float Near, float Far, const XMFLOAT4X4& orientationMatrix)
{
    // Note that the OrientationTransform3D matrix is post-multiplied here
    // in order to correctly orient the scene to match the display orientation.
    // This post-multiplication step is required for any draw calls that are
    // made to the swap chain render target. For draw calls to other targets,
    // this transform should not be applied.
    XMMATRIX perspectiveMatrix = 
        XMMatrixPerspectiveFovRH(fovAngleYRad,aspectRatio,Near, Far );

    XMMATRIX xmmOrientation = XMLoadFloat4x4(&orientationMatrix);

    XMStoreFloat4x4(
        &m_projection,
        XMMatrixTranspose(perspectiveMatrix * xmmOrientation)
    );
}

void CameraFirstPerson::ComputeViewLookAt(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up)
{
    XMVECTOR _eye = XMLoadFloat3(&eye);
    XMVECTOR _at = XMLoadFloat3(&at);
    XMVECTOR _up = XMLoadFloat3(&up);
    XMStoreFloat4x4(&m_view, XMMatrixTranspose(XMMatrixLookAtRH(_eye, _at, _up)));
}

// there are better(faster) ways to do this, anyways
void CameraFirstPerson::Update(DX::StepTimer const& timer)
{
    if (!IsPlaying()) 
        return;

    auto ms = DirectX::Mouse::Get().GetState();
    auto kb = DirectX::Keyboard::Get().GetState();

    // update cam
    const float dt = (float)timer.GetElapsedSeconds();
    const float rotDelta = dt*XM_PIDIV2;
    const float movDelta = dt*2.0f * (kb.LeftShift?6.0f:1.0f);

    // rotation input
    m_pitchYaw.y += rotDelta*ms.x;
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
        m_camXZ = XMVectorMultiplyAdd(fw, md, m_camXZ);
        XMVECTOR ri = ry.r[0];
        md = XMVectorSet(movSt, movSt, movSt, movSt);
        m_camXZ = XMVectorMultiplyAdd(ri, md, m_camXZ);
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

void CameraFirstPerson::SetPosition(const XMFLOAT3& p)
{
    XMFLOAT3 _p(p.x, 0, -p.z);
    m_camXZ = XMLoadFloat3(&_p);
}

void CameraFirstPerson::SetPlayingMode(bool playingMode)
{
    //DirectX::Mouse::Get().SetMode( playingMode ? DirectX::Mouse::MODE_RELATIVE: DirectX::Mouse::MODE_ABSOLUTE);
}
bool CameraFirstPerson::IsPlaying()const
{
    return true;
    //auto pm = DirectX::Mouse::Get().GetState().positionMode;
    //return pm == DirectX::Mouse::MODE_RELATIVE;
}