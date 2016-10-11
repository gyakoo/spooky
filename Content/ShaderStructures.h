#pragma once

#include <DirectXMath.h>

namespace SpookyAdulthood
{

	// Constant buffer used to send MVP matrices to the vertex shader.
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};


    // Vertex struct holding position, normal vector, color, and texture mapping information.
    struct VertexPositionNormalColorTexture4
    {
        VertexPositionNormalColorTexture4() = default;

        VertexPositionNormalColorTexture4(DirectX::XMFLOAT3 const& position, DirectX::XMFLOAT3 const& normal, 
            DirectX::XMFLOAT4 const& color, DirectX::XMFLOAT4 const& textureCoordinate)
            : position(position),
            normal(normal),
            color(color),
            textureCoordinate(textureCoordinate)
        { }

        VertexPositionNormalColorTexture4(DirectX::FXMVECTOR position, DirectX::FXMVECTOR normal, DirectX::FXMVECTOR color, DirectX::CXMVECTOR textureCoordinate)
        {
            DirectX::XMStoreFloat3(&this->position, position);
            DirectX::XMStoreFloat3(&this->normal, normal);
            DirectX::XMStoreFloat4(&this->color, color);
            DirectX::XMStoreFloat4(&this->textureCoordinate, textureCoordinate);
        }

        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT4 textureCoordinate;

        static const int InputElementCount = 4;
        static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
    };
}