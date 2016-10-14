#include "pch.h"
#include "GlobalFlags.h"
#include "../Common/DeviceResources.h"

using namespace DirectX;

namespace SpookyAdulthood
{
    bool GlobalFlags::CollisionsEnabled = false;
    int GlobalFlags::DrawThumbMap = 1;
    bool GlobalFlags::DrawDebugLines = false;
    bool GlobalFlags::DrawLevelGeometry = true;
    bool GlobalFlags::DrawWireframe = false;
    bool GlobalFlags::GenerateNewLevel = false;
    bool GlobalFlags::SpawnPlayer = false;
    
    bool GlobalFlags::DrawFlags = true;
    XMFLOAT2 GlobalFlags::DrawGlobalsPos(10, 10);

    template<typename T>
    static inline const XMVECTORF32 CEnbl(T v)
    {
        return !v ? Colors::Red : Colors::Yellow;
    }

    void GlobalFlags::Draw3D(const std::shared_ptr<DX::DeviceResources>& device)
    {
        static wchar_t buff[256];
        if (DrawFlags)
        {
            auto dxCommon = device->GetGameResources();
            XMFLOAT2 p = DrawGlobalsPos;
            auto s = dxCommon->m_sprites.get();
            auto f = dxCommon->m_fontConsole.get();
            const float padY = XMVectorGetY(f->MeasureString(L"Test"));
            s->Begin();
            {
                swprintf(buff, 256, L"Draw this(0)=%d", (int)DrawFlags);
                f->DrawString(s, buff, p, CEnbl(DrawFlags));
                p.y += padY;

                swprintf(buff, 256, L"Minimap(Space)=%d", DrawThumbMap);
                f->DrawString(s, buff, p, CEnbl(DrawThumbMap));
                p.y += padY;

                swprintf(buff, 256, L"Collisions(3)=%d", (int)CollisionsEnabled);
                f->DrawString(s, buff, p, CEnbl(CollisionsEnabled));
                p.y += padY;

                swprintf(buff, 256, L"Wireframe(4)=%d", (int)DrawWireframe);
                f->DrawString(s, buff, p, CEnbl(DrawWireframe));
                p.y += padY;
            }
            s->End();
        }
    }

    void GlobalFlags::Update(const DX::StepTimer& timer)
    {
    }

    void GlobalFlags::OnKeyDown(Windows::System::VirtualKey virtualKey)
    {
#if GLOBALFLAGS_CAN_TWEAK == 1
        using namespace Windows::System;
        switch (virtualKey)
        {
            case VirtualKey::Space: 
                DrawThumbMap = (DrawThumbMap + 1) % 3;
            break;
            case VirtualKey::Number0:
                DrawFlags = !DrawFlags;
            break;
            case VirtualKey::Number1:
                GenerateNewLevel = true;
            break;
            case VirtualKey::Number2:
                SpawnPlayer = true;
            break;
            case VirtualKey::Number3:
                CollisionsEnabled = !CollisionsEnabled;
            break;
            case VirtualKey::Number4:
                DrawWireframe = !DrawWireframe;
            break;
        }
    }
#endif
};

