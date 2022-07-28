#include "ecspch.h"
#include "GLTFThumbnail.h"
#include "../Rendering/Graphics.h"
#include "../Rendering/GraphicsHelpers.h"
#include "../Math/Transform.h"
#include "../Utilities/Timer.h"

#define DEFAULT_ROTATION { 0.0f, 0.0f, 0.0f }
#define LIGHT_COLOR { 8.0f, 3.0f, 2.0f, 1.0f }
#define LIGHT_DIRECTION { -0.3f, 0.8f, -0.2f }
#define OBJECT_TINT { 0.5f, 0.5f, 0.5f, 1.0f }

namespace ECSEngine {

	struct PixelConstants {
		float3 camera_position;
		// This will be padded
		float padding;
		float4 tint;
		float4 directional_light;
		float4 directional_color;
	};

	struct VertexConstants {
		Matrix object_matrix;
		Matrix mvp_matrix;
	};

//#define DRAW_MAIN

	// It returns the resized version of this - it will use generate mips in order to generate lower sized textures
	// instead of doing the resizing on the cpu
	void DrawObject(
		RenderTargetView target_view, 
		DepthStencilView depth_view, 
		Graphics* graphics, 
		GLTFThumbnailInfo info,
		const Mesh* mesh,
		uint2 texture_size
	) {
		// Clear the render target view and the depth stencil
		graphics->ClearRenderTarget(target_view, ColorFloat(0.2f, 0.2f, 0.2f, 1.0f));
		graphics->ClearDepthStencil(depth_view);

		Camera camera;
		camera.SetPerspectiveProjectionFOV(60.0f, (float)texture_size.x / (float)texture_size.y, 0.0005f, 100000.0f);
		camera.rotation = { info.object_rotation.x, info.object_rotation.y, 0.0f };

		// The object translation must be modulated by the rotation angles such that it stay framed
		camera.translation = OrbitPoint(info.camera_radius, { info.object_rotation.x, info.object_rotation.y });
			
		VertexConstants vertex_constants;
		Matrix object_matrix = MatrixTranslation(
			info.object_translation.x,
			info.object_translation.y,
			info.object_translation.z
		);
		vertex_constants.object_matrix = object_matrix;

		Matrix camera_matrix = camera.GetViewProjectionMatrix();
		vertex_constants.mvp_matrix = MatrixTranspose(object_matrix * camera_matrix);

		// Now render the object
		// We need 1 vertex shader constant buffer and 1 for the pixel shader
		ConstantBuffer vertex_constant = graphics->CreateConstantBuffer(sizeof(VertexConstants), &vertex_constants, true);

		PixelConstants pixel_constants;
		pixel_constants.camera_position = camera.translation;
		pixel_constants.directional_color = LIGHT_COLOR;
		// This is the direction - pick a random one
		float3 normalized_light_direction = Normalize(LIGHT_DIRECTION);
		pixel_constants.directional_light = float4((const float*)&normalized_light_direction);
		pixel_constants.tint = OBJECT_TINT;

		ConstantBuffer pixel_constant = graphics->CreateConstantBuffer(sizeof(PixelConstants), &pixel_constants, true);

		ECS_MESH_INDEX vertex_buffer_indices[2];
		vertex_buffer_indices[0] = ECS_MESH_POSITION;
		vertex_buffer_indices[1] = ECS_MESH_NORMAL;

		// Get the render state so that it can be restored later on
		GraphicsPipelineState pipeline_state = graphics->GetPipelineState();

		graphics->DisableAlphaBlending();
		graphics->EnableDepth();

		// Now bind the mesh alongside the helper shaders
		graphics->BindMesh(*mesh, { vertex_buffer_indices, std::size(vertex_buffer_indices) });
		graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_GLTF_THUMBNAIL);

		// Now bind the vertex and pixel constant buffers
		graphics->BindVertexConstantBuffer(vertex_constant);
		graphics->BindPixelConstantBuffer(pixel_constant);

		// Bind the render target
		graphics->BindRenderTargetView(target_view, depth_view);

		// Set the viewport
		graphics->BindViewport(0.0f, 0.0f, texture_size.x, texture_size.y, 0.0f, 1.0f);

		// Generate the draw call
		graphics->DrawIndexed(mesh->index_buffer.count);

		// Release all the temporary resources - the constant buffers
		// Restore the previous pipeline state
		graphics->RestorePipelineState(&pipeline_state);
		
		vertex_constant.Release();
		pixel_constant.Release();
	}

	GLTFThumbnailInfo DetermineObjectInitialBounds(Graphics* graphics, const Mesh* mesh) {
		// Get the position buffer to determine the minimum and the maximum bounds
		// Use a staging buffer for this
		VertexBuffer position_buffer = GetMeshVertexBuffer(*mesh, ECS_MESH_POSITION);

		// Assert it succeeded
		ECS_ASSERT(position_buffer.buffer != nullptr);

		Vec4f min_bounds(1000000.0f), max_bounds(-1000000.0f);

		VertexBuffer staging_buffer = BufferToStaging(graphics, position_buffer);
		// Now copy the content of the buffer into the CPU side
		float3* positions = (float3*)graphics->MapBuffer(staging_buffer.buffer, D3D11_MAP_READ);

		// Now get the min and max bounds. Initialize the min with a huge value and the max with a small value

		for (size_t index = 0; index < staging_buffer.size; index++) {
			// Load the data and do the compare
			Vec4f data = Vec4f().load((const float*)(positions + index));
			min_bounds = min(min_bounds, data);
			max_bounds = max(max_bounds, data);
		}

		// The staging buffer can now be released
		graphics->UnmapBuffer(staging_buffer.buffer);
		staging_buffer.Release();

		float4 min_bound;
		float4 max_bound;
		min_bounds.store((float*)&min_bound);
		max_bounds.store((float*)&max_bound);

		float4 difference = max_bound - min_bound;

		// Determine the biggest side
		float maximum_side = difference.x;
		maximum_side = std::max(maximum_side, difference.y);
		maximum_side = std::max(maximum_side, difference.z);

		// Determine x and y offsets - if the object is off centered (i.e. spans more in the negative part than the positive part)
		// Offset the object such that it appears centered
		float x_translation = -difference.x * 0.5f - min_bound.x;
		float y_translation = -difference.y * 0.5f - min_bound.y;
		float z_translation = -difference.z * 0.5f - min_bound.z;

		// Now construct the camera parameters - the radius and the rotation. For rotation select a some x and y values that will generarly
		// work for many meshes
		GLTFThumbnailInfo info;
		info.object_rotation = DEFAULT_ROTATION;
		info.object_translation = { x_translation, y_translation, z_translation };

		// The camera radius is equal to the biggest side - increase it by a bit since some object might not enter the frame correctly
		info.camera_radius = maximum_side * 1.1f;
		info.initial_camera_radius = info.camera_radius;

		// If the biggest side is on the Z axis, rotate the object by 90 degrees on the Y axis such that the object can be better seen
		if (maximum_side == difference.z) {
			info.object_rotation.y = -90.0f;
		}

		return info;
	}

	void GLTFRenderThumbnail(RenderTargetView target, Graphics* graphics, const Mesh* mesh, GLTFThumbnailInfo info, uint2 texture_size) {
		// Create a cleared depth stencil view
		GraphicsTexture2DDescriptor depth_stencil_descriptor;
		depth_stencil_descriptor.size = texture_size;
		depth_stencil_descriptor.bind_flag = D3D11_BIND_DEPTH_STENCIL;
		depth_stencil_descriptor.mip_levels = 1;
		depth_stencil_descriptor.usage = D3D11_USAGE_DEFAULT;
		depth_stencil_descriptor.format = DXGI_FORMAT_D32_FLOAT;
		Texture2D depth_stencil = graphics->CreateTexture(&depth_stencil_descriptor, true);
		DepthStencilView depth_stencil_view = graphics->CreateDepthStencilView(depth_stencil, true);

		DrawObject(target, depth_stencil_view, graphics, info, mesh, texture_size);
		depth_stencil_view.Release();
		depth_stencil.Release();
	}

	// -----------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(GLTFGenerateThumbnailTask)
	{
		GLTFGenerateThumbnailTaskData* data = (GLTFGenerateThumbnailTaskData*)_data;
		
		GLTFThumbnailInfo info = DetermineObjectInitialBounds(data->graphics, data->mesh);

		// Create the texture
		GraphicsTexture2DDescriptor texture_desc;
		texture_desc.bind_flag = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		texture_desc.usage = D3D11_USAGE_DEFAULT;
		texture_desc.mip_levels = 1;
		texture_desc.size = data->texture_size;
		texture_desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;

		Texture2D texture = data->graphics->CreateTexture(&texture_desc);
		ResourceView resource_view = data->graphics->CreateTextureShaderViewResource(texture);
		// Create a temporary render target view
		RenderTargetView render_view = data->graphics->CreateRenderTargetView(texture, 0, true);
		
		data->thumbnail_to_update->info = info;
		data->thumbnail_to_update->texture = resource_view;
		GLTFRenderThumbnail(render_view, data->graphics, data->mesh, info, data->texture_size);
		render_view.Release();
	}

	// -----------------------------------------------------------------------------------------------------------

	ECS_THREAD_TASK(GLTFUpdateThumbnailTask)
	{
		GLTFUpdateThumbnailTaskData* data = (GLTFUpdateThumbnailTaskData*)_data;

		Texture2D initial_texture = data->thumbnail->texture.GetResource();
		uint2 texture_dimensions = GetTextureDimensions(initial_texture);

		// Update the state
		data->thumbnail->info.camera_radius += data->radius_delta * data->thumbnail->info.initial_camera_radius;
		data->thumbnail->info.camera_radius = function::ClampMin(data->thumbnail->info.camera_radius, 0.0005f);

		data->thumbnail->info.object_translation += data->translation_delta;
		data->thumbnail->info.object_rotation += data->rotation_delta;

		// Create a new render target from the texture
		RenderTargetView render_view = data->graphics->CreateRenderTargetView(initial_texture, 0, true);

		GLTFRenderThumbnail(render_view, data->graphics, data->mesh, data->thumbnail->info, texture_dimensions);
		render_view.Release();
	}

	// -----------------------------------------------------------------------------------------------------------

	GLTFThumbnail GLTFGenerateThumbnail(Graphics* graphics, uint2 texture_size, const Mesh* mesh)
	{
		GLTFThumbnail thumbnail;

		GLTFGenerateThumbnailTaskData task_data;
		task_data.graphics = graphics;
		task_data.mesh = mesh;
		task_data.texture_size = texture_size;
		task_data.thumbnail_to_update = &thumbnail;

		GLTFGenerateThumbnailTask(0, nullptr, &task_data);

		return thumbnail;
	}

	// -----------------------------------------------------------------------------------------------------------

	void GLTFUpdateThumbnail(Graphics* graphics, const Mesh* mesh, GLTFThumbnail& thumbnail, float radius_delta, float3 translation_delta, float3 rotation_delta)
	{
		GLTFUpdateThumbnailTaskData task_data;
		task_data.graphics = graphics;
		task_data.mesh = mesh;
		task_data.radius_delta = radius_delta;
		task_data.translation_delta = translation_delta;
		task_data.rotation_delta = rotation_delta;
		task_data.thumbnail = &thumbnail;

		GLTFUpdateThumbnailTask(0, nullptr, &task_data);
	}

	// -----------------------------------------------------------------------------------------------------------

}