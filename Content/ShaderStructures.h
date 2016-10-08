﻿#pragma once

namespace SpookyAdulthood
{
	// Constant buffer used to send MVP matrices to the vertex shader.
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	// Used to send per-vertex data to the vertex shader.
	struct VertexPositionColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 color;
        DirectX::XMFLOAT3 uvw;

        void Set(float x, float y, float z, float r, float g, float b, float u, float v)
        {
            pos = DirectX::XMFLOAT3(x, y, z);
            color = DirectX::XMFLOAT3(r, g, b);
            uvw = DirectX::XMFLOAT3(u, v, 0.0f);
        }
	};
}