#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"
#include "Content\LevelMap.h"
#include "Content\Sprite.h"

namespace SpookyAdulthood
{
	// This sample renderer instantiates a basic rendering pipeline.
	class SceneRenderer
	{
	public:
		SceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void Render();
        void RenderUI();

	private:
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
        float	m_degreesPerSecond;
        bool	m_loadingComplete;        
	};
}

