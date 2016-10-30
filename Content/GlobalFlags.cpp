#include "pch.h"
#include "GlobalFlags.h"
#include "../Common/DeviceResources.h"

using namespace DirectX;

namespace SpookyAdulthood
{
    bool GlobalFlags::CollisionsEnabled = true;
    int GlobalFlags::DrawThumbMap = 1;
    bool GlobalFlags::DrawDebugLines = false;
    bool GlobalFlags::DrawLevelGeometry = true;
    bool GlobalFlags::DrawWireframe = false;
    bool GlobalFlags::GenerateNewLevel = false;
    bool GlobalFlags::SpawnPlayer = false;
    bool GlobalFlags::TestRaycast = false;
    bool GlobalFlags::AllLit = false;
    bool GlobalFlags::SpawnProjectile = false;
    int GlobalFlags::ShootHits = 0;
    bool GlobalFlags::KillRoom = false;
    bool GlobalFlags::DrawFlags = false;
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
            const XMFLOAT3 cp = dxCommon->m_camera.GetPosition();
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

                swprintf(buff, 256, L"TestRaycast(5)=%d", (int)TestRaycast);
                f->DrawString(s, buff, p, CEnbl(TestRaycast));
                p.y += padY;

                swprintf(buff, 256, L"Coll. Lines(6)=%d", (int)DrawDebugLines);
                f->DrawString(s, buff, p, CEnbl(DrawDebugLines));
                p.y += padY;

                swprintf(buff, 256, L"All Lit(7)=%d", (int)AllLit);
                f->DrawString(s, buff, p, CEnbl(AllLit));
                p.y += padY;                

                swprintf(buff, 256, L"Projectile(8)=%d", (int)SpawnProjectile);
                f->DrawString(s, buff, p, CEnbl(SpawnProjectile));
                p.y += padY;

                swprintf(buff, 256, L"Hits=%d", ShootHits);
                f->DrawString(s, buff, p, Colors::White);
                p.y += padY;

                swprintf(buff, 256, L"CamPos=%.2f, %.2f, %.2f", cp.x, cp.y, cp.z);
                f->DrawString(s, buff, p, Colors::White);
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
            case VirtualKey::Number5:
                TestRaycast = !TestRaycast;
            break;
            case VirtualKey::Number6:
                DrawDebugLines = !DrawDebugLines;
                if (DrawDebugLines)
                {
                    AllLit = true;
                    DrawWireframe = true;
                }
            break;
            case VirtualKey::Number7:
                AllLit = !AllLit;
            break;
            case VirtualKey::Number8:
                SpawnProjectile = true;
            break;
            case VirtualKey::Number9:
                KillRoom = true;
            break;
        }
    }
#endif
};

