#pragma once

#pragma warning(push)
#pragma warning(disable : 4005)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
//#define NODRAWTEXT
//#define NOGDI
//#define NOBITMAP
//#define NOMCX
//#define NOSERVICE
//#define NOHELP
#pragma warning(pop)

#include <wrl.h>
#include <wrl/client.h>
#include <dxgi1_4.h>
#include <d3d11_3.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <memory>
#include <agile.h>
#include <concrt.h>
#include <algorithm>
#include <queue>
#include <iterator>
#include <utility>
#include <map>
#include <ppl.h>
#include <atomic>
#include <concurrent_vector.h>

// game
#include "Common/StepTimer.h"

// directxtk

#include "DirectXTK/Inc/Audio.h"
#include "DirectXTK/Inc/CommonStates.h"
#include "DirectXTK/Inc/DDSTextureLoader.h"
#include "DirectXTK/Inc/DirectXHelpers.h"
#include "DirectXTK/Inc/Effects.h"
#include "DirectXTK/Inc/GamePad.h"
#include "DirectXTK/Inc/GeometricPrimitive.h"
#include "DirectXTK/Inc/Keyboard.h"
#include "DirectXTK/Inc/Model.h"
#include "DirectXTK/Inc/Mouse.h"
#include "DirectXTK/Inc/PrimitiveBatch.h"
#include "DirectXTK/Inc/SimpleMath.h"
#include "DirectXTK/Inc/SpriteBatch.h"
#include "DirectXTK/Inc/SpriteFont.h"
#include "DirectXTK/Inc/VertexTypes.h"
#include "DirectXTK/Inc/WICTextureLoader.h"

using namespace DirectX;

template<typename T, typename R, typename K>
inline T Clamp(const T& v, const R& _min, const K& _max)
{
    return v < _min ? _min : (v>_max?_max:v);
}

// Calls the provided work function and returns the number of milliseconds 
// that it takes to call that function.
template <class Function>
inline __int64 time_call(Function&& f)
{
    __int64 begin = GetTickCount64();
    f();
    return GetTickCount64() - begin;
}


inline XMFLOAT3 XM3Sub(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline void XM3Sub_inplace(XMFLOAT3& a, const XMFLOAT3& b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
}

inline XMFLOAT3 XM3Mul(const XMFLOAT3& a, float s)
{
    return XMFLOAT3(a.x*s, a.y*s, a.z*s);
}

inline void XM3Mul_inplace(XMFLOAT3& a, float s)
{
    a.x *= s; a.y *= s; a.z *= s;
}

inline float XM3LenSq(const XMFLOAT3& a)
{
    return a.x*a.x + a.y*a.y + a.z*a.z;
}

inline XMFLOAT3 XM3Add(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline float XM3Len(const XMFLOAT3& a)
{
    return sqrt(XM3LenSq(a));
}

inline void XM3Normalize_inplace(XMFLOAT3& a)
{
    const float il = 1.0f / XM3Len(a);
    a.x *= il;
    a.y *= il;
    a.z *= il;
}

inline XMFLOAT3 XM3Normalize(const XMFLOAT3& a)
{
    XMFLOAT3 _a = a;
    XM3Normalize_inplace(_a);
    return _a;
}

// ret a + b*c
inline XMFLOAT3 XM3Mad(const XMFLOAT3& a, const XMFLOAT3& b, float c)
{
    return XMFLOAT3(a.x + b.x*c, a.y + b.y*c, a.z + b.z*c);
}

inline XMFLOAT3 XM3Neg(const XMFLOAT3& a)
{
    return XMFLOAT3(-a.x, -a.y, -a.z);
}

inline XMFLOAT3 XM3Cross(const XMFLOAT3& a, const XMFLOAT3& b)
{
    XMVECTOR v1 = XMLoadFloat3(&a);
    XMVECTOR v2 = XMLoadFloat3(&b);
    XMFLOAT3 r;
    XMStoreFloat3(&r, XMVector3Cross(v1, v2));
    return r;
}

inline float XM3Dot(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline bool operator ==(const DirectX::XMUINT2& a, const DirectX::XMUINT2& b)
{
    return a.x == b.x && a.y == b.y;
}
