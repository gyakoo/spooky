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