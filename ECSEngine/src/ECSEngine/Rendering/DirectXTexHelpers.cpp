#include "ecspch.h"
#include "DirectXTexHelpers.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../Utilities/Assert.h"

namespace ECSEngine {

	void DXTexCreateTexture(
		ID3D11Device* device,
		const DirectX::ScratchImage* image,
		ID3D11Texture2D** resource,
		ID3D11ShaderResourceView** view,
		ID3D11DeviceContext* context
	)
	{
		DXTexCreateTextureEx(device, image, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 0, resource, view, context);
	}

	void DXTexCreateTextureEx(
		ID3D11Device* device,
		const DirectX::ScratchImage* image,
		unsigned int bind_flags,
		D3D11_USAGE usage,
		unsigned int misc_flags,
		unsigned int cpu_flags,
		ID3D11Texture2D** resource,
		ID3D11ShaderResourceView** view,
		ID3D11DeviceContext* context
	)
	{
		ECS_ASSERT(resource != nullptr || view != nullptr);

		D3D11_TEXTURE2D_DESC dx_descriptor;
		dx_descriptor.SampleDesc.Count = 1;
		dx_descriptor.SampleDesc.Quality = 0;
		const auto& metadata = image->GetMetadata();

		dx_descriptor.Format = metadata.format;
		dx_descriptor.ArraySize = 1;
		dx_descriptor.BindFlags = (D3D11_BIND_FLAG)bind_flags;
		dx_descriptor.CPUAccessFlags = (D3D11_CPU_ACCESS_FLAG)cpu_flags;

		dx_descriptor.MiscFlags = misc_flags;
		dx_descriptor.Usage = usage;
		dx_descriptor.Width = metadata.width;
		dx_descriptor.Height = metadata.height;

		ID3D11Texture2D* texture_interface;
		ID3D11ShaderResourceView* texture_view = nullptr;

		size_t row_pitch, slice_pitch;
		HRESULT result = DirectX::ComputePitch(metadata.format, metadata.width, metadata.height, row_pitch, slice_pitch);
		ECS_ASSERT(!FAILED(result));

		// No context provided - don't generate mips
		if (context == nullptr) {
			dx_descriptor.MipLevels = 1;

			D3D11_SUBRESOURCE_DATA subresource_data;
			subresource_data.SysMemPitch = row_pitch;
			subresource_data.SysMemSlicePitch = slice_pitch;
			subresource_data.pSysMem = image->GetPixels();

			result = device->CreateTexture2D(&dx_descriptor, &subresource_data, &texture_interface);
			ECS_ASSERT(!FAILED(result), "Creating texture from DXTex failed.");

			if (view != nullptr) {
				// Create a shader view
				result = device->CreateShaderResourceView(texture_interface, nullptr, &texture_view);
				ECS_ASSERT(!FAILED(result), "Creating texture view from DXTex failed.");
			}
		}
		else {
			// Make the initial data nullptr
			dx_descriptor.MipLevels = 0;
			dx_descriptor.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
			// Also make it a render target
			dx_descriptor.BindFlags |= D3D11_BIND_RENDER_TARGET;

			HRESULT result = device->CreateTexture2D(&dx_descriptor, nullptr, &texture_interface);
			ECS_ASSERT(!FAILED(result), "Creating texture from DXTex failed.");

			D3D11_BOX box;
			box.left = 0;
			box.right = dx_descriptor.Width;
			box.top = 0;
			box.bottom = dx_descriptor.Height;
			box.front = 0;
			box.back = 1;
			context->UpdateSubresource(texture_interface, 0, &box, image->GetPixels(), row_pitch, slice_pitch);

			result = device->CreateShaderResourceView(texture_interface, nullptr, &texture_view);
			ECS_ASSERT(!FAILED(result), "Creating texture view from DXTex failed.");

			// Generate mips
			context->GenerateMips(texture_view);
		}

		if (resource != nullptr) {
			*resource = texture_interface;
		}
		if (view != nullptr) {
			*view = texture_view;
		}
	}

}