#pragma once
#include "ecspch.h"
#include "../Core.h"

namespace DirectX {
	class ScratchImage;
}

namespace ECSEngine {

    // Select resource and/or view to get as pointers back. Sending a nullptr means ignore that pointer
    // Uses the metadata from the image to set the flags accordingly. If mip-maps want to be generated from the first mip then give the context
    // It will use the immediate context in order to generate those mips
    // It will not add the resource into the graphics monitoring array
    // Returns true if it succeeded, else false. In case the texture was created but the view not,
    // it will free the texture and return false
    ECSENGINE_API bool DXTexCreateTexture(
        ID3D11Device* device,
        const DirectX::ScratchImage* image,
        ID3D11Texture2D** resource = nullptr,
        ID3D11ShaderResourceView** view = nullptr,
        ID3D11DeviceContext* context = nullptr
    );

    // Select resource and/or view to get as pointers back. Sending a nullptr means ignore that pointer
    // If mip-maps want to be generated from the first mip then give the context
    // It will use the immediate context in order to generate those mips
    // It will not add the resource into the graphics monitoring array
    // Returns true if it succeeded, else false. In case the texture was created but the view not,
    // it will free the texture and return false
    ECSENGINE_API bool DXTexCreateTextureEx(
        ID3D11Device* device,
        const DirectX::ScratchImage* image,
        unsigned int bind_flags,
        D3D11_USAGE usage,
        unsigned int misc_flags,
        unsigned int cpu_flags,
        ID3D11Texture2D** resource = nullptr,
        ID3D11ShaderResourceView** view = nullptr,
        ID3D11DeviceContext* context = nullptr
    );

}