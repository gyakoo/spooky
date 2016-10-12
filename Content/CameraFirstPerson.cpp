﻿#include "pch.h"
#include "CameraFirstPerson.h"

using namespace SpookyAdulthood;

CameraFirstPerson::CameraFirstPerson(float fovYDeg)
: m_pitchYaw(0,XM_PI), m_height(CAM_DEFAULT_HEIGHT), m_radius(CAM_DEFAULT_RADIUS)
, m_aspectRatio(1.0f)
{
    m_camXZ = XMVectorSet(0, 0, 0, 0);
    XMFLOAT4X4 id;
    XMStoreFloat4x4(&id, XMMatrixIdentity());
    ComputeProjection(fovYDeg*XM_PI / 180.0f, 1.0f, 0.01f, 40.0f, id);
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
    m_aspectRatio = aspectRatio;
}

void CameraFirstPerson::ComputeViewLookAt(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up)
{
    XMVECTOR _eye = XMLoadFloat3(&eye);
    XMVECTOR _at = XMLoadFloat3(&at);
    XMVECTOR _up = XMLoadFloat3(&up);
    XMStoreFloat4x4(&m_view, XMMatrixTranspose(XMMatrixLookAtRH(_eye, _at, _up)));
}

void CameraFirstPerson::SetPosition(const XMFLOAT3& p)
{
    XMFLOAT3 _p(p.x, 0, -p.z);
    m_camXZ = XMLoadFloat3(&_p);
}

XMFLOAT3 CameraFirstPerson::GetForward() const
{
    return XMFLOAT3(0, 0, 0);
}
