#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"
#include "Content\LevelMap.h"
#include "Content\CameraFirstPerson.h"
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
        const CameraFirstPerson& GetCamera() { return m_camera; }

    private:
        void SpawnPlayer();
        void UpdateAudio(DX::StepTimer const& timer);

	private:
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
        CameraFirstPerson  m_camera;
        LevelMapGenerationSettings m_mapSettings;
        float	m_degreesPerSecond;
        bool	m_loadingComplete;        
	};
}

