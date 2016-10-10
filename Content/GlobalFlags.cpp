#include "pch.h"
#include "GlobalFlags.h"
#include "../Common/DeviceResources.h"

using namespace DirectX;

namespace SpookyAdulthood
{
    bool GlobalFlags::CollisionsEnabled = false;
    int GlobalFlags::DrawThumbMap = 0;
    bool GlobalFlags::DrawDebugLines = false;
    bool GlobalFlags::DrawLevelGeometry = true;
    bool GlobalFlags::GenerateNewLevel = false;
    bool GlobalFlags::SpawnPlayer = false;
    
    bool GlobalFlags::DrawFlags = true;
    XMFLOAT2 GlobalFlags::DrawGlobalsPos(10, 10);

    void GlobalFlags::Render(const std::shared_ptr<DX::DeviceResources>& device)
    {
        static wchar_t buff[256];
        const float padY = 20.0f;
        if (DrawFlags)
        {
            XMFLOAT2 p = DrawGlobalsPos;
            auto s = device->GetSprites();
            auto f = device->GetFontConsole();
            s->Begin();
            {
                swprintf(buff, 256, L"Minimap(Space)=%d", DrawThumbMap);
                f->DrawString(s, buff, p, Colors::Yellow);
                p.y += padY;

                swprintf(buff, 256, L"Collisions(3)=%d", (int)CollisionsEnabled);
                f->DrawString(s, buff, p, Colors::Yellow);
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
        }
    }
};

