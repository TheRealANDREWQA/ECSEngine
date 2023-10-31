#include "ecspch.h"
#include "GraphicsHelpers.h"
#include "Graphics.h"
#include "../Utilities/Crash.h"

namespace ECSEngine {

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

	AABBStorage GetMeshBoundingBox(Stream<float3> positions)
	{
		return GetAABBFromPoints(positions);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	AABBStorage GetMeshBoundingBox(Graphics* graphics, VertexBuffer positions_buffer)
	{
		VertexBuffer staging_buffer = BufferToStaging(graphics, positions_buffer);

		float3* positions = (float3*)graphics->MapBuffer(staging_buffer.buffer, ECS_GRAPHICS_MAP_READ);
		AABBStorage bounding_box = GetMeshBoundingBox(Stream<float3>(positions, positions_buffer.size));

		staging_buffer.Release();
		return bounding_box;
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
			graphics->AddInternalResource(new_texture);
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
		buffer_descriptor.MiscFlags = ClearFlag(buffer_descriptor.MiscFlags, ECS_GRAPHICS_MISC_SHARED);

		ID3D11Buffer* _new_buffer = nullptr;
		HRESULT result = device->CreateBuffer(&buffer_descriptor, nullptr, &_new_buffer);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), new_buffer, "Could not create a staging buffer!");

		new_buffer.buffer = _new_buffer;

		CopyGraphicsResource(new_buffer, buffer, graphics->GetContext());
		if (!temporary) {
			graphics->AddInternalResource(new_buffer);
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
			graphics->AddInternalResource(new_texture);
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), new_buffer, "Converting a buffer to immutable with staging buffer failed.");

		UnmapBuffer(staging_buffer.buffer, graphics->GetContext());
		staging_buffer.Release();

		new_buffer = _new_buffer;
		if (!temporary) {
			graphics->AddInternalResource(new_buffer);
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
			graphics->AddInternalResource(new_texture);
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
			graphics->AddInternalResource(new_buffer);
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

	float GetConstantObjectSizeInPerspective(float camera_fov, float distance_to_camera, float object_size)
	{
		return object_size * tan(DegToRad(camera_fov * 0.5f)) * distance_to_camera;
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