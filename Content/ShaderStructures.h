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

    struct PixelShaderConstantBuffer
    {
        DirectX::XMFLOAT4 texAtlasSize;
        DirectX::XMFLOAT4 other;
        DirectX::XMFLOAT4 modulate;
    };


    // Vertex struct holding position, normal vector, color, and texture mapping information.
    struct VertexPositionNormalColorTextureNdx
    {
        VertexPositionNormalColorTextureNdx() = default;

        VertexPositionNormalColorTextureNdx(DirectX::XMFLOAT3 const& position, DirectX::XMFLOAT3 const& normal, 
            DirectX::XMFLOAT4 const& color, DirectX::XMFLOAT2 const& textureCoordinate, DirectX::XMUINT2 const& textureIndex=DirectX::XMUINT2(0,0))
            : position(position),
            normal(normal),
            color(color),
            textureCoordinate(textureCoordinate),
            textureIndex(textureIndex)
        {
        }

        VertexPositionNormalColorTextureNdx(DirectX::FXMVECTOR position, DirectX::FXMVECTOR normal, DirectX::FXMVECTOR color, 
            DirectX::CXMVECTOR textureCoordinate, DirectX::XMUINT2 const& textureIndex)
            : textureIndex(textureIndex)
        {
            DirectX::XMStoreFloat3(&this->position, position);
            DirectX::XMStoreFloat3(&this->normal, normal);
            DirectX::XMStoreFloat4(&this->color, color);
            DirectX::XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
        }

        void SetTexCoord(float u, float v, UINT iX, UINT iY)
        {
            textureCoordinate.x = u; textureCoordinate.y = v;
            textureIndex.x = iX; textureIndex.y = iY;
        }

        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 textureCoordinate;
        DirectX::XMUINT2  textureIndex;

        static const int InputElementCount = 5;
        static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
    };
}