#pragma once
#include "pch.h"
#include "ShaderStructures.h"

namespace SpookyAdulthood
{
    //--------------------------------------------------------------------------------------
    // Vertex struct holding position, normal vector, color, and texture mapping information.
    const D3D11_INPUT_ELEMENT_DESC VertexPositionNormalColorTexture4::InputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    static_assert(sizeof(VertexPositionNormalColorTexture4) == 56, "Vertex struct/layout mismatch");
}