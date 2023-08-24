#include "ecspch.h"
#include "../Utilities/Function.h"
#include "GraphicsHelpers.h"
#include "Graphics.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../Utilities/Path.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int GetTextureDimensions(Texture1D texture, unsigned int mip_level)
	{
		unsigned int result;

		Texture1DDescriptor descriptor = GetTextureDescriptor(texture);
		ECS_ASSERT(mip_level < descriptor.mip_levels);
		result = function::ClampMin<unsigned int>(descriptor.width << mip_level, 1);

		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 GetTextureDimensions(Texture2D texture, unsigned int mip_level) {
		uint2 result;

		Texture2DDescriptor descriptor = GetTextureDescriptor(texture);
		ECS_ASSERT(mip_level < descriptor.mip_levels);
		result.x = function::ClampMin<unsigned int>(descriptor.size.x << mip_level, 1);
		result.y = function::ClampMin<unsigned int>(descriptor.size.y << mip_level, 1);

		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint3 GetTextureDimensions(Texture3D texture, unsigned int mip_level)
	{
		uint3 result;

		Texture3DDescriptor descriptor = GetTextureDescriptor(texture);
		ECS_ASSERT(mip_level < descriptor.mip_levels);
		result.x = function::ClampMin<unsigned int>(descriptor.size.x << mip_level, 1);
		result.y = function::ClampMin<unsigned int>(descriptor.size.y << mip_level, 1);
		result.z = function::ClampMin<unsigned int>(descriptor.size.z << mip_level, 1);

		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	GraphicsViewport GetGraphicsViewportForTexture(Texture2D texture, float min_depth, float max_depth)
	{
		GraphicsViewport viewport;

		D3D11_TEXTURE2D_DESC descriptor;
		texture.Interface()->GetDesc(&descriptor);

		viewport.width = descriptor.Width;
		viewport.height = descriptor.Height;
		viewport.min_depth = min_depth;
		viewport.max_depth = max_depth;

		return viewport;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void CreateCubeVertexBuffer(Graphics* graphics, float positive_span, VertexBuffer& vertex_buffer, IndexBuffer& index_buffer, bool temporary)
	{
		float negative_span = -positive_span;
		float3 vertex_position[] = {
			{negative_span, negative_span, negative_span},
			{positive_span, negative_span, negative_span},
			{negative_span, positive_span, negative_span},
			{positive_span, positive_span, negative_span},
			{negative_span, negative_span, positive_span},
			{positive_span, negative_span, positive_span},
			{negative_span, positive_span, positive_span},
			{positive_span, positive_span, positive_span}
		};

		vertex_buffer = graphics->CreateVertexBuffer(sizeof(float3), std::size(vertex_position), vertex_position, temporary);

		unsigned int indices[] = {
			0, 2, 1,    2, 3, 1,
			1, 3, 5,    3, 7, 5,
			2, 6, 3,    3, 6, 7,
			4, 5, 7,    4, 7, 6,
			0, 4, 2,    2, 4, 6,
			0, 1, 4,    1, 5, 4
		};

		index_buffer = graphics->CreateIndexBuffer(Stream<unsigned int>(indices, std::size(indices)), temporary);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	VertexBuffer CreateRectangleVertexBuffer(Graphics* graphics, float3 top_left, float3 bottom_right, bool temporary)
	{
		// a -- b
		// |    |
		// c -- d
		float3 positions[] = {
			top_left,                                           // a
			{ bottom_right.x, top_left.y, bottom_right.z },     // b
			{ top_left.x, bottom_right.y, top_left.z },         // c
			{bottom_right.x, top_left.y, bottom_right.z},       // b
			bottom_right,                                       // d
			{top_left.x, bottom_right.y, top_left.z}            // c
		};
		return graphics->CreateVertexBuffer(sizeof(float3), std::size(positions), positions, temporary);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void GetMeshBoundingBox(Stream<float3> positions, float3* min_bound, float3* max_bound)
	{
		Vector8 vector_min(FLT_MAX), vector_max(-FLT_MAX);
		size_t simd_count = function::GetSimdCount(positions.size, vector_min.Lanes());
		for (size_t index = 0; index < simd_count; index += vector_min.Lanes()) {
			Vector8 current_positions(positions.buffer + index);
			vector_min = min(vector_min, current_positions);
			vector_max = max(vector_max, current_positions);
		}

		float4 mins[2];
		float4 maxs[2];

		vector_min.Store(mins);
		vector_max.Store(maxs);

		float3 current_min = BasicTypeMin(mins[0].xyz(), mins[1].xyz());
		float3 current_max = BasicTypeMax(maxs[0].xyz(), maxs[1].xyz());

		if (simd_count < positions.size) {
			current_min = BasicTypeMin(current_min, positions[simd_count]);
			current_max = BasicTypeMax(current_max, positions[simd_count]);
		}

		*min_bound = current_min;
		*max_bound = current_max;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void GetMeshBoundingBox(Graphics* graphics, VertexBuffer positions_buffer, float3* min_bound, float3* max_bound)
	{
		VertexBuffer staging_buffer = BufferToStaging(graphics, positions_buffer);

		float3* positions = (float3*)graphics->MapBuffer(staging_buffer.buffer, ECS_GRAPHICS_MAP_READ);
		GetMeshBoundingBox(Stream<float3>(positions, positions_buffer.size), min_bound, max_bound);

		staging_buffer.Release();
	}

	// ----------------------------------------------------------------------------------------------------------------------

#define EXPORT_TEXTURE_TEMPLATE(function_name) ECS_TEMPLATE_FUNCTION(Texture1D, function_name, Graphics*, Texture1D, bool);  \
ECS_TEMPLATE_FUNCTION(Texture2D, function_name, Graphics*, Texture2D, bool); \
ECS_TEMPLATE_FUNCTION(Texture3D, function_name, Graphics*, Texture3D, bool); \

	template<typename Texture>
	Texture TextureToStaging(Graphics* graphics, Texture texture, bool temporary) {
		Texture new_texture;

		GraphicsDevice* device = graphics->GetDevice();
		Texture::RawDescriptor texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		texture_descriptor.BindFlags = 0;
		texture_descriptor.Usage = D3D11_USAGE_STAGING;
		texture_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		texture_descriptor.MiscFlags = 0;

		new_texture = Texture::RawCreate(device, &texture_descriptor);

		CopyGraphicsResource(new_texture, texture, graphics->GetContext());
		if (!temporary) {
			graphics->AddInternalResource(new_texture, ECS_DEBUG_INFO);
		}
		return new_texture;
	}

	EXPORT_TEXTURE_TEMPLATE(TextureToStaging);

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	Buffer BufferToStaging(Graphics* graphics, Buffer buffer, bool temporary) {
		Buffer new_buffer;

		memcpy(&new_buffer, &buffer, sizeof(Buffer));
		GraphicsDevice* device = graphics->GetDevice();
		D3D11_BUFFER_DESC buffer_descriptor;

		buffer.buffer->GetDesc(&buffer_descriptor);

		buffer_descriptor.Usage = D3D11_USAGE_STAGING;
		buffer_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		buffer_descriptor.BindFlags = 0;
		buffer_descriptor.MiscFlags = function::ClearFlag(buffer_descriptor.MiscFlags, ECS_GRAPHICS_MISC_SHARED);

		ID3D11Buffer* _new_buffer = nullptr;
		HRESULT result = device->CreateBuffer(&buffer_descriptor, nullptr, &_new_buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), new_buffer, "Could not create a staging buffer!");

		new_buffer.buffer = _new_buffer;

		CopyGraphicsResource(new_buffer, buffer, graphics->GetContext());
		if (!temporary) {
			graphics->AddInternalResource(new_buffer, ECS_DEBUG_INFO);
		}

		return new_buffer;
	}

#define EXPORT(type) ECS_TEMPLATE_FUNCTION(type, BufferToStaging, Graphics*, type, bool);

	EXPORT(StandardBuffer);
	EXPORT(StructuredBuffer);
	EXPORT(UABuffer);
	EXPORT(IndexBuffer);
	EXPORT(VertexBuffer);

#undef EXPORT

	// ----------------------------------------------------------------------------------------------------------------------

	constexpr size_t MAX_SUBRESOURCES = 32;

	template<typename Texture>
	Texture TextureToImmutableWithStaging(Graphics* graphics, Texture texture, bool temporary) {
		Texture new_texture;

		GraphicsDevice* device = graphics->GetDevice();
		Texture::RawDescriptor texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		texture_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		texture_descriptor.CPUAccessFlags = 0;
		texture_descriptor.BindFlags = 0;

		Texture staging_texture = TextureToStaging(graphics, texture);
		if (staging_texture.tex == nullptr) {
			return staging_texture;
		}

		D3D11_SUBRESOURCE_DATA subresource_data[MAX_SUBRESOURCES];
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			MappedTexture resource = MapTexture(staging_texture, graphics->GetContext(), ECS_GRAPHICS_MAP_READ, index, 0);
			subresource_data[index].pSysMem = resource.data;
			subresource_data[index].SysMemPitch = resource.row_byte_size;
			subresource_data[index].SysMemSlicePitch = resource.slice_byte_size;
		}

		new_texture = Texture::RawCreate(device, &texture_descriptor, subresource_data);

		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			UnmapTexture(staging_texture, graphics->GetContext(), index);
		}
		staging_texture.Release();

		if (!temporary) {
			graphics->AddInternalResource(new_texture, ECS_DEBUG_INFO);
		}
		return new_texture;
	}

	EXPORT_TEXTURE_TEMPLATE(TextureToImmutableWithStaging);

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	Buffer BufferToImmutableWithStaging(Graphics* graphics, Buffer buffer, bool temporary) {
		Buffer new_buffer;
		new_buffer = nullptr;

		GraphicsDevice* device = graphics->GetDevice();
		D3D11_BUFFER_DESC buffer_descriptor;
		buffer.buffer->GetDesc(&buffer_descriptor);

		buffer_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		buffer_descriptor.CPUAccessFlags = 0;

		Buffer staging_buffer = BufferToStaging(graphics, buffer);
		if (staging_buffer.buffer == nullptr) {
			return staging_buffer;
		}

		D3D11_SUBRESOURCE_DATA subresource_data;
		D3D11_MAPPED_SUBRESOURCE resource = MapBufferEx(staging_buffer.buffer, graphics->GetContext(), ECS_GRAPHICS_MAP_READ);
		subresource_data.pSysMem = resource.pData;
		subresource_data.SysMemPitch = resource.RowPitch;
		subresource_data.SysMemSlicePitch = resource.DepthPitch;

		ID3D11Buffer* _new_buffer = nullptr;
		HRESULT result = device->CreateBuffer(&buffer_descriptor, &subresource_data, &_new_buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), new_buffer, "Converting a buffer to immutable with staging buffer failed.");

		UnmapBuffer(staging_buffer.buffer, graphics->GetContext());
		staging_buffer.Release();

		new_buffer = _new_buffer;
		if (!temporary) {
			graphics->AddInternalResource(new_buffer, ECS_DEBUG_INFO);
		}
		return new_buffer;
	}

//#define EXPORT_BUFFER(type) ECS_TEMPLATE_FUNCTION(type, ECSEngine::BufferToImmutableWithStaging, Graphics*, type, bool);

	// CRINGE Visual studio intellisense bug that fills the file with errors even tho no error is present
	// Manual unroll
	ECS_TEMPLATE_FUNCTION(StandardBuffer, BufferToImmutableWithStaging, Graphics*, StandardBuffer, bool);
	ECS_TEMPLATE_FUNCTION(StructuredBuffer, BufferToImmutableWithStaging, Graphics*, StructuredBuffer, bool);
	ECS_TEMPLATE_FUNCTION(UABuffer, BufferToImmutableWithStaging, Graphics*, UABuffer, bool);
	ECS_TEMPLATE_FUNCTION(ConstantBuffer, BufferToImmutableWithStaging, Graphics*, ConstantBuffer, bool);

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	Texture TextureToImmutableWithMap(Graphics* graphics, Texture texture, bool temporary) {
		Texture new_texture;

		GraphicsDevice* device = graphics->GetDevice();
		Texture::RawDescriptor texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		D3D11_SUBRESOURCE_DATA subresource_data[MAX_SUBRESOURCES];
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			MappedTexture resource = MapTexture(texture, graphics->GetContext(), ECS_GRAPHICS_MAP_READ, index, 0);
			subresource_data[index].pSysMem = resource.data;
			subresource_data[index].SysMemPitch = resource.row_byte_size;
			subresource_data[index].SysMemSlicePitch = resource.slice_byte_size;
		}

		new_texture = Texture::RawCreate(device, &texture_descriptor, subresource_data);

		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			UnmapTexture(texture, graphics->GetContext(), index);
		}

		if (!temporary) {
			graphics->AddInternalResource(new_texture, ECS_DEBUG_INFO);
		}
		return new_texture;
	}

	EXPORT_TEXTURE_TEMPLATE(TextureToImmutableWithMap);

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	Buffer BufferToImmutableWithMap(Graphics* graphics, Buffer buffer, bool temporary)
	{
		Buffer new_buffer;

		GraphicsDevice* device = graphics->GetDevice();
		D3D11_BUFFER_DESC buffer_descriptor;
		buffer.buffer->GetDesc(&buffer_descriptor);
	

		D3D11_SUBRESOURCE_DATA subresource_data;
		D3D11_MAPPED_SUBRESOURCE resource = MapBufferEx(buffer.buffer, graphics->GetContext(), ECS_GRAPHICS_MAP_READ);
		subresource_data.pSysMem = resource.pData;
		subresource_data.SysMemPitch = resource.RowPitch;
		subresource_data.SysMemSlicePitch = resource.DepthPitch;

		ID3D11Buffer* _new_buffer = nullptr;
		HRESULT result = device->CreateBuffer(&buffer_descriptor, &subresource_data, &_new_buffer);

		UnmapBuffer(buffer.buffer, graphics->GetContext());

		new_buffer = _new_buffer;
		if (!temporary) {
			graphics->AddInternalResource(new_buffer, ECS_DEBUG_INFO);
		}
		return new_buffer;
	}

	// CRINGE Visual studio intellisense bug that fills the file with errors even tho no error is present
	// Manual unroll
	ECS_TEMPLATE_FUNCTION(StandardBuffer, BufferToImmutableWithMap, Graphics*, StandardBuffer, bool);
	ECS_TEMPLATE_FUNCTION(StructuredBuffer, BufferToImmutableWithMap, Graphics*, StructuredBuffer, bool);
	ECS_TEMPLATE_FUNCTION(UABuffer, BufferToImmutableWithMap, Graphics*, UABuffer, bool);
	ECS_TEMPLATE_FUNCTION(ConstantBuffer, BufferToImmutableWithMap, Graphics*, ConstantBuffer, bool);

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTextureWithStaging(Graphics* graphics, Texture2D texture, size_t new_width, size_t new_height, size_t resize_flag, bool temporary)
	{
		Texture2D staging_texture = TextureToStaging(graphics, texture);
		MappedTexture first_mip = MapTexture(staging_texture, graphics->GetContext(), ECS_GRAPHICS_MAP_READ);
		Texture2D new_texture = ResizeTexture(graphics, first_mip.data, texture, new_width, new_height, { nullptr }, resize_flag, temporary);
		UnmapTexture(staging_texture, graphics->GetContext());
		staging_texture.Release();
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTextureWithMap(Graphics* graphics, Texture2D texture, size_t new_width, size_t new_height, size_t resize_flag, bool temporary)
	{
		Texture2D result;

		MappedTexture first_mip = MapTexture(texture, graphics->GetContext(), ECS_GRAPHICS_MAP_READ);
		result = ResizeTexture(graphics, first_mip.data, texture, new_width, new_height, {nullptr}, resize_flag, temporary);
		UnmapTexture(texture, graphics->GetContext());
		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTexture(
		Graphics* graphics,
		void* data, 
		Texture2D texture, 
		size_t new_width,
		size_t new_height, 
		AllocatorPolymorphic allocator,
		size_t resize_flag,
		bool temporary
	) {
		Texture2D new_texture = (ID3D11Texture2D*)nullptr;

		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		DirectX::Image dx_image;
		dx_image.pixels = (uint8_t*)data;
		dx_image.format = texture_descriptor.Format;
		dx_image.width = texture_descriptor.Width;
		dx_image.height = texture_descriptor.Height;
		HRESULT result = DirectX::ComputePitch(texture_descriptor.Format, texture_descriptor.Width, texture_descriptor.Height, dx_image.rowPitch, dx_image.slicePitch);
		if (FAILED(result)) {
			return Texture2D((ID3D11Texture2D*)nullptr);
		}

		DirectX::TEX_FILTER_FLAGS filter_flag = DirectX::TEX_FILTER_LINEAR;
		size_t filter_flags = function::ClearFlag(resize_flag, ECS_RESIZE_TEXTURE_MIP_MAPS);
		switch (filter_flags) {
		case ECS_RESIZE_TEXTURE_FILTER_BOX:
			filter_flag = DirectX::TEX_FILTER_BOX;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_CUBIC:
			filter_flag = DirectX::TEX_FILTER_CUBIC;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_LINEAR:
			filter_flag = DirectX::TEX_FILTER_LINEAR;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_POINT:
			filter_flag = DirectX::TEX_FILTER_POINT;
			break;
		}

		DirectX::ScratchImage new_image;
		if (allocator.allocator != nullptr) {
			new_image.SetAllocator(allocator.allocator, GetAllocateFunction(allocator), GetDeallocateMutableFunction(allocator));
		}
		result = DirectX::Resize(dx_image, new_width, new_height, filter_flag, new_image);
		if (FAILED(result)) {
			return new_texture;
		}

		const DirectX::Image* resized_image = new_image.GetImage(0, 0, 0);
		D3D11_SUBRESOURCE_DATA subresource_data;
		subresource_data.pSysMem = resized_image->pixels;
		subresource_data.SysMemPitch = resized_image->rowPitch;
		subresource_data.SysMemSlicePitch = resized_image->slicePitch;

		ID3D11Texture2D* _new_texture;
		texture_descriptor.MipLevels = 1;
		texture_descriptor.Width = new_width;
		texture_descriptor.Height = new_height;

		result = graphics->GetDevice()->CreateTexture2D(&texture_descriptor, &subresource_data, &_new_texture);
		if (FAILED(result)) {
			return new_texture;
		}
		if (function::HasFlag(resize_flag, ECS_RESIZE_TEXTURE_MIP_MAPS)) {
			ID3D11Texture2D* first_mip = _new_texture;
			texture_descriptor.MipLevels = 0;
			texture_descriptor.BindFlags |= D3D11_BIND_RENDER_TARGET;
			texture_descriptor.Usage = D3D11_USAGE_DEFAULT;
			texture_descriptor.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
			
			result = graphics->GetDevice()->CreateTexture2D(&texture_descriptor, nullptr, &_new_texture);
			if (FAILED(result)) {
				return new_texture;
			}

			CopyTextureSubresource(_new_texture, { 0, 0 }, 0, first_mip, { 0, 0 }, { (unsigned int)resized_image->width, (unsigned int)resized_image->height }, 0, graphics->GetContext());
			first_mip->Release();

			ID3D11ShaderResourceView* resource_view;
			result = graphics->GetDevice()->CreateShaderResourceView(_new_texture, nullptr, &resource_view);
			if (FAILED(result)) {
				_new_texture->Release();
				return new_texture;
			}
			graphics->GenerateMips(resource_view);
			resource_view->Release();
		}

		new_texture = _new_texture;
		if (!temporary){
			graphics->AddInternalResource(new_texture, ECS_DEBUG_INFO);
		}
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Stream<void> ResizeTexture(void* texture_data, size_t current_width, size_t current_height, ECS_GRAPHICS_FORMAT format, size_t new_width, size_t new_height, AllocatorPolymorphic allocator, size_t resize_flags)
	{
		Stream<void> data = { nullptr, 0 };

		DirectX::Image dx_image;
		dx_image.pixels = (uint8_t*)texture_data;
		dx_image.format = GetGraphicsNativeFormat(format);
		dx_image.width = current_width;
		dx_image.height = current_height;
		HRESULT result = DirectX::ComputePitch(dx_image.format, dx_image.width, dx_image.height, dx_image.rowPitch, dx_image.slicePitch);
		if (FAILED(result)) {
			return data;
		}

		DirectX::TEX_FILTER_FLAGS filter_flag = DirectX::TEX_FILTER_LINEAR;
		switch (resize_flags) {
		case ECS_RESIZE_TEXTURE_FILTER_BOX:
			filter_flag = DirectX::TEX_FILTER_BOX;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_CUBIC:
			filter_flag = DirectX::TEX_FILTER_CUBIC;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_POINT:
			filter_flag = DirectX::TEX_FILTER_POINT;
			break;
		}

		DirectX::ScratchImage new_image;
		if (allocator.allocator != nullptr) {
			new_image.SetAllocator(allocator.allocator, GetAllocateFunction(allocator), GetDeallocateMutableFunction(allocator));
		}
		result = DirectX::Resize(dx_image, new_width, new_height, filter_flag, new_image);
		if (FAILED(result)) {
			return data;
		}

		data = { new_image.GetPixels(), new_image.GetPixelsSize() };
		new_image.DetachPixels();
		return data;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	DecodedTexture DecodeTexture(Stream<void> data, TextureExtension extension, AllocatorPolymorphic allocator, size_t flags)
	{
		DecodedTexture new_data = { { nullptr, 0 }, 0, 0, ECS_GRAPHICS_FORMAT_UNKNOWN };

		DirectX::ScratchImage image;
		SetInternalImageAllocator(&image, allocator);

		HRESULT result;
		DirectX::TexMetadata metadata;
		if (extension == ECS_TEXTURE_EXTENSION_HDR) {
			bool apply_tonemapping = function::HasFlag(flags, ECS_DECODE_TEXTURE_HDR_TONEMAP);
			result = DirectX::LoadFromHDRMemory(data.buffer, data.size, &metadata, image, apply_tonemapping);
		}
		else if (extension == ECS_TEXTURE_EXTENSION_TGA) {
			result = DirectX::LoadFromTGAMemory(data.buffer, data.size, &metadata, image);
		}
		else {
			DirectX::WIC_FLAGS wic_flags = DirectX::WIC_FLAGS_FORCE_RGB;
			result = DirectX::LoadFromWICMemory(data.buffer, data.size, wic_flags, &metadata, image);
		}

		if (FAILED(result)) {
			return new_data;
		}

		new_data.data = { image.GetPixels(), image.GetPixelsSize() };
		new_data.format = GetGraphicsFormatFromNative(metadata.format);
		new_data.height = metadata.height;
		new_data.width = metadata.width;
		image.DetachPixels();

		if (function::HasFlag(flags, ECS_DECODE_TEXTURE_NO_SRGB)) {
			new_data.format = GetGraphicsFormatNoSRGB(new_data.format);
		}

		if (function::HasFlag(flags, ECS_DECODE_TEXTURE_FORCE_SRGB)) {
			new_data.format = GetGraphicsFormatWithSRGB(new_data.format);
		}

		return new_data;
	}

	// ---------------------------------------------------------------------------------------------------------------------------

	DecodedTexture DecodeTexture(Stream<void> data, Stream<wchar_t> filename, AllocatorPolymorphic allocator, size_t flags)
	{
		Path extension = function::PathExtensionBoth(filename);

		if (function::CompareStrings(extension, L".hdr")) {
			return DecodeTexture(data, ECS_TEXTURE_EXTENSION_HDR, allocator, flags);
		}
		if (function::CompareStrings(extension, L".tga")) {
			return DecodeTexture(data, ECS_TEXTURE_EXTENSION_TGA, allocator, flags);
		}

		// The remaining textures don't matter exactly the type - they will all get mapped to the WIC path
		return DecodeTexture(data, ECS_TEXTURE_EXTENSION_JPG, allocator, flags);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void InvertMeshZAxis(Graphics* graphics, Mesh& mesh)
	{
		// Invert positions, normals, tangents and winding order
		VertexBuffer position_buffer = GetMeshVertexBuffer(mesh, ECS_MESH_POSITION);
		VertexBuffer normal_buffer = GetMeshVertexBuffer(mesh, ECS_MESH_NORMAL);

		// Inverts the z axis of a buffer and returns a new one
		auto invert_z = [](Graphics* graphics, VertexBuffer buffer) {
			// Create a staging one with its data
			VertexBuffer staging = BufferToStaging(graphics, buffer);

			// Map the buffer
			float3* data = (float3*)graphics->MapBuffer(staging.buffer, ECS_GRAPHICS_MAP_READ_WRITE);

			// Modify the data
			for (size_t index = 0; index < staging.size; index++) {
				data[index].z = -data[index].z;
			}

			// Create a copy of the staging data into a GPU resource
			D3D11_BUFFER_DESC buffer_descriptor;
			buffer.buffer->GetDesc(&buffer_descriptor);

			VertexBuffer new_buffer = graphics->CreateVertexBuffer(
				buffer.stride,
				buffer.size,
				data, 
				false, 
				GetGraphicsUsageFromNative(buffer_descriptor.Usage),
				GetGraphicsCPUAccessFromNative(buffer_descriptor.CPUAccessFlags), 
				GetGraphicsMiscFlagsFromNative(buffer_descriptor.MiscFlags)
			);

			// Unmap the buffer
			graphics->UnmapBuffer(staging.buffer);

			// Release the old one and the staging
			graphics->FreeResource(buffer);
			staging.Release();

			return new_buffer;
		};

		if (position_buffer.buffer != nullptr) {
			SetMeshVertexBuffer(mesh, ECS_MESH_POSITION, invert_z(graphics, position_buffer));
		}
		if (normal_buffer.buffer != nullptr) {
			SetMeshVertexBuffer(mesh, ECS_MESH_NORMAL, invert_z(graphics, normal_buffer));
		}

		// Invert the winding order
		IndexBuffer staging_index = BufferToStaging(graphics, mesh.index_buffer);

		void* _indices = graphics->MapBuffer(staging_index.buffer, ECS_GRAPHICS_MAP_READ_WRITE);

		// Create an index buffer with the same specification
		D3D11_BUFFER_DESC index_descriptor;
		mesh.index_buffer.buffer->GetDesc(&index_descriptor);
		if (staging_index.int_size == 1) {
			// U char
			unsigned char* indices = (unsigned char*)_indices;
			// Start from 1 because only the last 2 vertices must be swapped for the winding order
			for (size_t index = 1; index < staging_index.count; index += 3) {
				unsigned char copy = indices[index];
				indices[index] = indices[index + 1];
				indices[index + 1] = copy;
			}
		}
		else if (staging_index.int_size == 2) {
			// U short
			unsigned short* indices = (unsigned short*)_indices;
			// Start from 1 because only the last 2 vertices must be swapped for the winding order
			for (size_t index = 1; index < staging_index.count; index += 3) {
				unsigned short copy = indices[index];
				indices[index] = indices[index + 1];
				indices[index + 1] = copy;
			}
		}
		else if (staging_index.int_size == 4) {
			// U int
			unsigned int* indices = (unsigned int*)_indices;
			// Start from 1 because only the last 2 vertices must be swapped for the winding order
			for (size_t index = 1; index < staging_index.count; index += 3) {
				unsigned int copy = indices[index];
				indices[index] = indices[index + 1];
				indices[index + 1] = copy;
			}
		}

		IndexBuffer new_indices = graphics->CreateIndexBuffer(
			staging_index.int_size, 
			staging_index.count, 
			false, 
			GetGraphicsUsageFromNative(index_descriptor.Usage), 
			GetGraphicsCPUAccessFromNative(index_descriptor.CPUAccessFlags)
		);

		// Release the old buffers
		staging_index.Release();
		graphics->FreeResource(mesh.index_buffer);

		mesh.index_buffer = new_indices;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	//ECS_OPTIMIZE_START;

	template<int index>
	ECS_INLINE Vec32uc ECS_VECTORCALL ConvertSingleChannelTextureToGrayscaleSplat(Vec32uc samples) {
		return permute32<index, index, index, V_DC, index + 1, index + 1, index + 1, V_DC, index + 2, index + 2, index + 2, V_DC, index + 3, index + 3, index + 3, V_DC,
			index + 4, index + 4, index + 4, V_DC, index + 5, index + 5, index + 5, V_DC, index + 6, index + 6, index + 6, V_DC, index + 7, index + 7, index + 7, V_DC>(samples);
	}

	ECS_INLINE Vec32uc ECS_VECTORCALL ConvertSingleChannelTextureToGrayscaleBlend(Vec32uc values, Vec32uc alpha) {
		return blend32<0, 1, 2, 35, 4, 5, 6, 39, 8, 9, 10, 43, 12, 13, 14, 47, 16, 17, 18, 51, 20, 21, 22, 55, 24, 25, 26, 59, 28, 29, 30, 63>(values, alpha);
	}

	Stream<Stream<void>> ConvertSingleChannelTextureToGrayscale(
		Stream<Stream<void>> mip_data,
		size_t width, 
		size_t height, 
		AllocatorPolymorphic allocator
	)
	{
		// First determine the total amount of memory needed to transform the mip maps
		size_t total_data_size = 0;
		for (size_t index = 0; index < mip_data.size; index++) {
			total_data_size += mip_data[index].size * 4;
		}

		// Add the streams necessary to the allocation
		total_data_size += sizeof(Stream<void>) * mip_data.size;

		Stream<void>* streams = (Stream<void>*)AllocateEx(allocator, total_data_size);

		uintptr_t buffer = (uintptr_t)streams;
		buffer += sizeof(Stream<void>) * mip_data.size;

		// Initialize the streams
		for (size_t index = 0; index < mip_data.size; index++) {
			streams[index].InitializeFromBuffer(buffer, mip_data[index].size * 4);
		}

		Vec32uc samples;
		Vec32uc alpha(255);
		// Now copy the data - use SIMD instructions
		for (size_t index = 0; index < mip_data.size; index++) {
			size_t width_times_height = width * height;
			size_t steps = width_times_height / samples.size();
			size_t remainder = width_times_height % samples.size();
			if (remainder == 0) {
				for (size_t step = 0; step < steps; step++) {
					// Load the data into the register
					samples.load(function::OffsetPointer(mip_data[index].buffer, samples.size() * step));

					// Splat the values 8 at a time - but set the alpha to 255
					Vec32uc splat0 = ConvertSingleChannelTextureToGrayscaleSplat<0>(samples);
					Vec32uc splat1 = ConvertSingleChannelTextureToGrayscaleSplat<8>(samples);
					Vec32uc splat2 = ConvertSingleChannelTextureToGrayscaleSplat<16>(samples);
					Vec32uc splat3 = ConvertSingleChannelTextureToGrayscaleSplat<24>(samples);

					// Blend with the alpha
					Vec32uc values0 = ConvertSingleChannelTextureToGrayscaleBlend(splat0, alpha);
					Vec32uc values1 = ConvertSingleChannelTextureToGrayscaleBlend(splat1, alpha);
					Vec32uc values2 = ConvertSingleChannelTextureToGrayscaleBlend(splat2, alpha);
					Vec32uc values3 = ConvertSingleChannelTextureToGrayscaleBlend(splat3, alpha);

					void* base_pointer = function::OffsetPointer(streams[index].buffer, samples.size() * step * 4);
					// Write the values
					values0.store(base_pointer);
					values1.store(function::OffsetPointer(base_pointer, samples.size()));
					values2.store(function::OffsetPointer(base_pointer, samples.size() * 2));
					values3.store(function::OffsetPointer(base_pointer, samples.size() * 3));
				}
			}
			else {
				// Scalar version
				unsigned char* source_data = (unsigned char*)mip_data[index].buffer;
				unsigned char* destination_data = (unsigned char*)streams[index].buffer;
				for (size_t subindex = 0; subindex < mip_data[index].size; subindex++) {
					size_t dest_offset = subindex * 4;
					destination_data[dest_offset] = source_data[subindex];
					destination_data[dest_offset + 1] = source_data[subindex];
					destination_data[dest_offset + 2] = source_data[subindex];
					// Alpha
					destination_data[dest_offset + 3] = 255;
				}
			}
		}

		return { streams, mip_data.size };
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Stream<Stream<void>> ConvertTextureToGrayscale(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		size_t channel_count,
		size_t channel_to_copy, 
		AllocatorPolymorphic allocator
	)
	{
		// First determine the total amount of memory needed to transform the mip maps
		size_t total_data_size = 0;
		for (size_t index = 0; index < mip_data.size; index++) {
			total_data_size += mip_data[index].size / channel_count;
		}

		// Add the streams necessary to the allocation
		total_data_size += sizeof(Stream<void>) * mip_data.size;

		Stream<void>* streams = (Stream<void>*)AllocateEx(allocator, total_data_size);

		uintptr_t buffer = (uintptr_t)streams;
		buffer += sizeof(Stream<void>) * mip_data.size;

		// Initialize the streams
		for (size_t index = 0; index < mip_data.size; index++) {
			streams[index].InitializeFromBuffer(buffer, mip_data[index].size / channel_count);
		}

		// At the moment use just a scalar version. If needed add a SIMD version
		for (size_t index = 0; index < mip_data.size; index++) {
			unsigned char* values = (unsigned char*)streams[index].buffer;
			unsigned char* copy_values = (unsigned char*)mip_data[index].buffer;
			for (size_t pixel_index = 0; pixel_index < streams[index].size; pixel_index++) {
				values[pixel_index] = copy_values[pixel_index * channel_count + channel_to_copy];
			}
		}

		return { streams, mip_data.size };
	}

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCube ConvertTexturesToCube(
		Graphics* graphics,
		Texture2D x_positive,
		Texture2D x_negative, 
		Texture2D y_positive,
		Texture2D y_negative,  
		Texture2D z_positive,
		Texture2D z_negative,
		bool temporary
	)
	{
		D3D11_TEXTURE2D_DESC texture_descriptor;
		x_negative.tex->GetDesc(&texture_descriptor);

		texture_descriptor.ArraySize = 6;
		texture_descriptor.Usage = D3D11_USAGE_DEFAULT;
		texture_descriptor.CPUAccessFlags = 0;
		texture_descriptor.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;
		ID3D11Texture2D* texture = nullptr;
		HRESULT result = graphics->GetDevice()->CreateTexture2D(&texture_descriptor, nullptr, &texture);
		TextureCube cube_texture;
		cube_texture.tex = nullptr;

		ECS_CRASH_RETURN_VALUE(result, cube_texture, "Converting textures to cube textures failed.");
		cube_texture = { texture };

		GraphicsContext* context = graphics->GetContext();
		// For every mip, copy the mip into the corresponding array resource
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			CopyGraphicsResource(cube_texture, x_negative, ECS_TEXTURE_CUBE_X_NEG, context, index);
			CopyGraphicsResource(cube_texture, x_positive, ECS_TEXTURE_CUBE_X_POS, context, index);
			CopyGraphicsResource(cube_texture, y_negative, ECS_TEXTURE_CUBE_Y_NEG, context, index);
			CopyGraphicsResource(cube_texture, y_positive, ECS_TEXTURE_CUBE_Y_POS, context, index);
			CopyGraphicsResource(cube_texture, z_negative, ECS_TEXTURE_CUBE_Z_NEG, context, index);
			CopyGraphicsResource(cube_texture, z_positive, ECS_TEXTURE_CUBE_Z_POS, context, index);
		}

		if (!temporary) {
			graphics->AddInternalResource(cube_texture, ECS_DEBUG_INFO);
		}
		return cube_texture;
	}

	TextureCube ConvertTexturesToCube(Graphics* graphics, const Texture2D* textures, bool temporary)
	{
		return ConvertTexturesToCube(graphics, textures[0], textures[1], textures[2], textures[3], textures[4], textures[5], temporary);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCube ConvertTextureToCube(Graphics* graphics, ResourceView texture_view, ECS_GRAPHICS_FORMAT cube_format, uint2 face_size, bool temporary)
	{
		TextureCube cube;
		
		// Create the 6 faces as render targets
		Texture2DDescriptor texture_descriptor;
		texture_descriptor.format = cube_format;
		texture_descriptor.bind_flag = GetGraphicsBindFromNative((D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
		texture_descriptor.size = face_size;
		texture_descriptor.mip_levels = 0;

		Texture2D cube_textures[6];
		RenderTargetView render_views[6];
		for (size_t index = 0; index < 6; index++) {
			cube_textures[index] = graphics->CreateTexture(&texture_descriptor, true);
			render_views[index] = graphics->CreateRenderTargetView(cube_textures[index], 0, ECS_GRAPHICS_FORMAT_UNKNOWN, true);
		}

		// Generate a unit cube vertex buffer - a cube is needed instead of a rectangle because it will be rotated
		// by the look at matrix
		VertexBuffer vertex_buffer;
		IndexBuffer index_buffer;
		CreateCubeVertexBuffer(graphics, 0.5f, vertex_buffer, index_buffer, true);

		// Bind a nullptr depth stencil view - remove depth
		RenderTargetView current_render_view = graphics->GetBoundRenderTarget();
		DepthStencilView current_depth_view = graphics->GetBoundDepthStencil();
		GraphicsViewport current_viewport = graphics->GetBoundViewport();

		graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_CREATE_TEXTURE_CUBE);
		graphics->BindVertexBuffer(vertex_buffer);
		graphics->BindIndexBuffer(index_buffer);
		graphics->BindTopology(Topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
		graphics->BindPixelResourceView(texture_view);

		Matrix projection_matrix = ProjectionMatrixTextureCube();
		
		ConstantBuffer vertex_constants = graphics->CreateConstantBuffer(sizeof(Matrix), true);
		graphics->BindVertexConstantBuffer(vertex_constants);
		GraphicsViewport cube_viewport = { 0.0f, 0.0f, face_size.x, face_size.y, 0.0f, 1.0f };
		graphics->BindViewport(cube_viewport);
		graphics->DisableDepth();
		graphics->DisableCulling();

		for (size_t index = 0; index < 6; index++) {
			Matrix current_matrix = MatrixGPU(ViewMatrixTextureCube((TextureCubeFace)index) * projection_matrix);
			UpdateBufferResource(vertex_constants.buffer, &current_matrix, sizeof(Matrix), graphics->GetContext());
			graphics->BindRenderTargetView(render_views[index], nullptr);

			graphics->DrawIndexed(index_buffer.count);
		}
		graphics->EnableDepth();
		graphics->EnableCulling();

		graphics->BindRenderTargetView(current_render_view, current_depth_view);
		graphics->BindViewport(current_viewport);

		cube = ConvertTexturesToCube(graphics, cube_textures, temporary);
		ResourceView cube_view = graphics->CreateTextureShaderViewResource(cube, true);
		graphics->GenerateMips(cube_view);
		cube_view.Release();

		vertex_buffer.Release();
		index_buffer.Release();
		vertex_constants.Release();
		for (size_t index = 0; index < 6; index++) {
			render_views[index].Release();
			cube_textures[index].Release();
		}

		return cube;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 GetTextureDimensions(Stream<wchar_t> filename)
	{
		NULL_TERMINATE_WIDE(filename);

		uint2 dimensions = { 0,0 };

		Path extension = function::PathExtensionBoth(filename);

		if (extension.size == 0) {
			return dimensions;
		}

		bool is_tga = false;
		bool is_hdr = false;

		is_tga = function::CompareStrings(extension, L".tga");
		is_hdr = function::CompareStrings(extension, L".hdr");

		DirectX::TexMetadata metadata;
		HRESULT result;

		if (is_tga) {
			result = DirectX::GetMetadataFromTGAFile(filename.buffer, metadata);
		}
		else if (is_hdr) {
			result = DirectX::GetMetadataFromHDRFile(filename.buffer, metadata);
		}
		else {
			result = DirectX::GetMetadataFromWICFile(filename.buffer, DirectX::WIC_FLAGS_NONE, metadata);
		}

		if (FAILED(result)) {
			return dimensions;
		}

		dimensions = { (unsigned int)metadata.width, (unsigned int)metadata.height };
		return dimensions;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 GetTextureDimensions(Stream<void> data, TextureExtension extension)
	{
		using function = uint2(*)(Stream<void> data);
		function functions[] = {
			GetTextureDimensionsJPG,
			GetTextureDimensionsPNG,
			GetTextureDimensionsTIFF,
			GetTextureDimensionsBMP,
			GetTextureDimensionsTGA,
			GetTextureDimensionsHDR
		};

		return functions[extension](data);
	}

	template<typename Handler>
	uint2 GetTextureDimensionsExtension(Stream<void> data, Handler&& handler) {
		uint2 dimensions = { 0,0 };

		DirectX::TexMetadata metadata;
		HRESULT result;

		result = handler(metadata);

		if (FAILED(result)) {
			return dimensions;
		}

		dimensions = { (unsigned int)metadata.width, (unsigned int)metadata.height };
		return dimensions;
	}

	uint2 GetTextureDimensionsPNG(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
		});
	}

	uint2 GetTextureDimensionsJPG(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsBMP(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsTIFF(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsTGA(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromTGAMemory(data.buffer, data.size, metadata);
			});
	}

	uint2 GetTextureDimensionsHDR(Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromHDRMemory(data.buffer, data.size, metadata);
			});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	float GetConstantObjectSizeInPerspective(float camera_fov, float distance_to_camera, float object_size)
	{
		return object_size * tan(DegToRad(camera_fov * 0.5f)) * distance_to_camera;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture1DDescriptor GetTextureDescriptor(Texture1D texture)
	{
		Texture1DDescriptor descriptor;

		D3D11_TEXTURE1D_DESC desc;
		texture.tex->GetDesc(&desc);

		descriptor.array_size = desc.ArraySize;
		descriptor.bind_flag = GetGraphicsBindFromNative(desc.BindFlags);
		descriptor.cpu_flag = GetGraphicsCPUAccessFromNative(desc.CPUAccessFlags);
		descriptor.format = GetGraphicsFormatFromNative(desc.Format);
		descriptor.mip_data = { nullptr, 0 };
		descriptor.mip_levels = desc.MipLevels;
		descriptor.misc_flag = GetGraphicsMiscFlagsFromNative(desc.MiscFlags);
		descriptor.usage = GetGraphicsUsageFromNative(desc.Usage);
		descriptor.width = desc.Width;

		return descriptor;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2DDescriptor GetTextureDescriptor(Texture2D texture)
	{
		Texture2DDescriptor descriptor;

		D3D11_TEXTURE2D_DESC desc;
		texture.tex->GetDesc(&desc);

		descriptor.array_size = desc.ArraySize;
		descriptor.bind_flag = GetGraphicsBindFromNative(desc.BindFlags);
		descriptor.cpu_flag = GetGraphicsCPUAccessFromNative(desc.CPUAccessFlags);
		descriptor.format = GetGraphicsFormatFromNative(desc.Format);
		descriptor.mip_data = { nullptr, 0 };
		descriptor.mip_levels = desc.MipLevels;
		descriptor.misc_flag = GetGraphicsMiscFlagsFromNative(desc.MiscFlags);
		descriptor.usage = GetGraphicsUsageFromNative(desc.Usage);
		descriptor.size = { desc.Width, desc.Height };
		descriptor.sampler_quality = desc.SampleDesc.Quality;
		descriptor.sample_count = desc.SampleDesc.Count;

		return descriptor;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture3DDescriptor GetTextureDescriptor(Texture3D texture)
	{
		Texture3DDescriptor descriptor;

		D3D11_TEXTURE3D_DESC desc;
		texture.tex->GetDesc(&desc);

		descriptor.bind_flag = GetGraphicsBindFromNative(desc.BindFlags);
		descriptor.cpu_flag = GetGraphicsCPUAccessFromNative(desc.CPUAccessFlags);
		descriptor.format = GetGraphicsFormatFromNative(desc.Format);
		descriptor.mip_data = { nullptr, 0 };
		descriptor.mip_levels = desc.MipLevels;
		descriptor.misc_flag = GetGraphicsMiscFlagsFromNative(desc.MiscFlags);
		descriptor.usage = GetGraphicsUsageFromNative(desc.Usage);
		descriptor.size = { desc.Width, desc.Height, desc.Depth };

		return descriptor;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCubeDescriptor GetTextureDescriptor(TextureCube texture)
	{
		TextureCubeDescriptor descriptor;

		D3D11_TEXTURE2D_DESC desc;
		texture.tex->GetDesc(&desc);

		descriptor.bind_flag = GetGraphicsBindFromNative(desc.BindFlags);
		descriptor.cpu_flag = GetGraphicsCPUAccessFromNative(desc.CPUAccessFlags);
		descriptor.format = GetGraphicsFormatFromNative(desc.Format);
		descriptor.mip_data = { nullptr, 0 };
		descriptor.mip_levels = desc.MipLevels;
		descriptor.misc_flag = GetGraphicsMiscFlagsFromNative(desc.MiscFlags);
		descriptor.usage = GetGraphicsUsageFromNative(desc.Usage);
		descriptor.size = { desc.Width, desc.Height };

		return descriptor;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int ClampToTextureBounds(Texture1D texture, unsigned int position, unsigned int mip_level)
	{
		unsigned int texture_bound = GetTextureDimensions(texture, mip_level);
		return function::ClampMax(position, texture_bound);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 ClampToTextureBounds(Texture2D texture, uint2 position, unsigned int mip_level)
	{
		uint2 texture_bounds = GetTextureDimensions(texture, mip_level);
		return BasicTypeClampMax(position, texture_bounds);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint3 ClampToTextureBounds(Texture3D texture, uint3 position, unsigned int mip_level)
	{
		uint3 texture_bounds = GetTextureDimensions(texture, mip_level);
		return BasicTypeClampMax(position, texture_bounds);
	}

	// ----------------------------------------------------------------------------------------------------------------------
	
	void DrawWholeViewportQuad(Graphics* graphics, GraphicsContext* context)
	{
		BindVertexBuffer(graphics->m_cached_resources.vertex_buffer[ECS_GRAPHICS_CACHED_VERTEX_BUFFER_QUAD], context);
		BindVertexShader(graphics->m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_VIEWPORT_QUAD].vertex, context);
		BindInputLayout(graphics->m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_VIEWPORT_QUAD].input_layout, context);
		BindTopology(Topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST), context);
		Draw(graphics->m_cached_resources.vertex_buffer[ECS_GRAPHICS_CACHED_VERTEX_BUFFER_QUAD].size, context);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ConstantBuffer CreateColorizeConstantBuffer(Graphics* graphics, unsigned int count, bool temporary)
	{
		ConstantBuffer result;
	
		ECS_ASSERT(count <= 256);
		ECS_STACK_CAPACITY_STREAM(ColorFloat, colorize_values, 256);

		// Use an HSV description since it allows us to have different hues
		// which will make things easier to stop difference
		// Start with a given saturation of 128 and value of 255 and go in increments of 64
		// When that is exhausted, start offseting by 16, 32 and then 48 and keep the increment at 64
		// When that is exhausted, increment the saturation by 32 and then reset with offset of 16
		// When that is exhausted, decrease the value by 64 and offset with 16, 32 and 48

		const unsigned int SATURATION_BASE_OFFSET = 128;
		const unsigned int VALUE_BASE_OFFSET = 255;
		for (unsigned int index = 0; index < count; index++) {
			Color hsv_color;
			unsigned int hue_divisor = (index % 4) * 64;
			unsigned int hue_offset = ((index / 4) % 4) * 16;
			unsigned int saturation_divisor = ((index / 16) % 4) * 32;
			unsigned int saturation_offset = ((index / 64) % 2) * 16 + SATURATION_BASE_OFFSET;
			unsigned int value_divisor = ((index / 128) % 4) * 64;
			unsigned int value_offset = VALUE_BASE_OFFSET - ((index / 512) % 4) * 16;
			hsv_color.hue = (unsigned char)(hue_divisor + hue_offset);
			hsv_color.saturation = (unsigned char)(saturation_divisor + saturation_offset);
			hsv_color.value = (unsigned char)(value_offset - value_divisor);

			colorize_values[index] = HSVToRGB(hsv_color);
		}

		result = graphics->CreateConstantBuffer(count * sizeof(ColorFloat), colorize_values.buffer, temporary);
		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

}