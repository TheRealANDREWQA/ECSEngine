#include "ecspch.h"
#include "WriteTexture.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../Utilities/StackScope.h"
#include "../Rendering/TextureOperations.h"

namespace ECSEngine {

	bool WriteJPGTexture(Stream<wchar_t> path, const void* texture_data, uint2 dimensions, size_t channel_count, WriteJPGTextureOptions options)
	{
		NULL_TERMINATE_WIDE(path);

		DXGI_FORMAT texture_format = DXGI_FORMAT_R8G8B8A8_UNORM;

		Stream<Stream<void>> converted_texture;
		if (channel_count == 3) {
			Stream<void> texture = { texture_data, dimensions.x * dimensions.y * 3 };
			converted_texture = ConvertRGBTextureToRGBA({ &texture, 1 }, dimensions.x, dimensions.y);
			texture_data = converted_texture[0].buffer;
		}
		else if (channel_count == 1) {
			texture_format = DXGI_FORMAT_R8_UNORM;
		}
		else if (channel_count == 2) {
			texture_format = DXGI_FORMAT_R8G8_UNORM;
		}

		size_t row_pitch, slice_pitch;
		ECS_ASSERT(SUCCEEDED(DirectX::ComputePitch(texture_format, dimensions.x, dimensions.y, row_pitch, slice_pitch)));

		DirectX::WIC_FLAGS wic_flags = options.srgb ? DirectX::WIC_FLAGS_FORCE_SRGB : DirectX::WIC_FLAGS_NONE;

		DirectX::Image dx_image;
		dx_image.pixels = (uint8_t*)texture_data;
		dx_image.format = texture_format;
		dx_image.width = dimensions.x;
		dx_image.height = dimensions.y;
		dx_image.rowPitch = row_pitch;
		dx_image.slicePitch = slice_pitch;

		bool success = SUCCEEDED(DirectX::SaveToWICFile(dx_image, wic_flags, DirectX::GetWICCodec(DirectX::WIC_CODEC_JPEG), path.buffer, nullptr, 
			[&](IPropertyBag2* props) {
				PROPBAG2 dx_options[1] = { NULL };
				dx_options[0].pstrName = const_cast<wchar_t*>(L"ImageQuality");

				VARIANT varValues[1];
				varValues[0].vt = VT_R4;
				varValues[0].fltVal = options.compression_quality;

				props->Write(1, dx_options, varValues);
			}
		));

		if (channel_count == 3) {
			Free(converted_texture.buffer);
		}
		return success;
	}

}