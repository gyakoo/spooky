#pragma once

#ifndef GLOBALFLAGS_CAN_TWEAK
#define GLOBALFLAGS_CAN_TWEAK 1
#endif
namespace DX { class DeviceResources; }
namespace SpookyAdulthood
{
    struct GlobalFlags
    {
        static bool CollisionsEnabled; // def 0
        
        static int DrawThumbMap; // def 0 {0,1,2}
        static bool DrawDebugLines; // def 0
        static bool DrawLevelGeometry; // def 1
        static bool DrawFlags; // def 1
        static bool DrawWireframe; // def 0

        static bool GenerateNewLevel; // def 0 (auto)
        static bool SpawnPlayer; // def 0 (auto)

        static DirectX::XMFLOAT2 DrawGlobalsPos; // def 10,10

        static void Render(const std::shared_ptr<DX::DeviceResources>& device);
        static void Update(const DX::StepTimer& timer);
        static void OnKeyDown(Windows::System::VirtualKey virtualKey);
    };
}