﻿#pragma once

namespace DX { class DeviceResources; }
namespace SpookyAdulthood
{

    static const uint32_t RANDOM_DEFAULT_SEED = 997;

    struct GlobalFlags
    {
        static bool CollisionsEnabled; // def 1
        
        static int DrawThumbMap; // def 0 {0,1,2}
        static bool DrawDebugLines; // def 0
        static bool DrawLevelGeometry; // def 1
        static bool DrawFlags; // def 1
        static bool DrawWireframe; // def 0
        static bool ThumbMapAll; // def 0
        static bool AllLit; // def 0

        static bool GenerateNewLevel; // def 0 (auto)
        static bool SpawnPlayer; // def 0 (auto)
        static bool SpawnProjectile; // def 0 (auto)
        static bool KillRoom; // def 0 (auto)

        static DirectX::XMFLOAT2 DrawGlobalsPos; // def 10,10
        static int ShootHits; // def 0

        static bool TestRaycast; // def 0

        static void Draw3D(const std::shared_ptr<DX::DeviceResources>& device);
        static void Update(const DX::StepTimer& timer);
        static void OnKeyDown(Windows::System::VirtualKey virtualKey);
    };
}