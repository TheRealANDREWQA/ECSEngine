#include "ecspch.h"
#include "PBRMaps.h"
#include "Graphics.h"

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCube ConvertEnvironmentMapToDiffuseIBL(ResourceView environment, Graphics* graphics, uint2 dimensions, unsigned int sample_count)
	{
		TextureCube cube;

		// A unit cube is needed because instead of a rectangle because it will rotated by the look at matrices
		VertexBuffer cube_v_buffer;
		IndexBuffer cube_i_buffer;
		CreateCubeVertexBuffer(graphics, 1.0f, cube_v_buffer, cube_i_buffer);

		// The constant buffer for the projection view matrix
		ConstantBuffer vc_buffer = graphics->CreateConstantBuffer(sizeof(Matrix));
		ConstantBuffer pc_buffer = graphics->CreateConstantBuffer(sizeof(float));
		// The buffer expects a float with the delta step - convert to a delta step
		float step = PI * 2 / sample_count;
		UpdateBufferResource(pc_buffer.buffer, &step, sizeof(float), graphics->GetContext());

		// The format of the cube texture is RGBA16F
		GraphicsTextureCubeDescriptor cube_descriptor;
		cube_descriptor.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		cube_descriptor.size = dimensions;
		cube_descriptor.mip_levels = 1;
		cube_descriptor.bind_flag = (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		cube = graphics->CreateTexture(&cube_descriptor);

		RenderTargetView previous_target = graphics->GetBoundRenderTarget();
		DepthStencilView previous_depth = graphics->GetBoundDepthStencil();
		GraphicsViewport previous_viewport = graphics->GetBoundViewport();

		RenderTargetView target_views[6];
		for (size_t index = 0; index < 6; index++) {
			target_views[index] = graphics->CreateRenderTargetView(cube, (TextureCubeFace)index);
		}

		D3D11_SAMPLER_DESC sampler_descriptor = {};
		sampler_descriptor.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_descriptor.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_descriptor.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_descriptor.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_descriptor.MinLOD = 0;
		sampler_descriptor.MaxLOD = D3D11_FLOAT32_MAX;
		SamplerState sampler = graphics->CreateSamplerState(sampler_descriptor);

		graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_CREATE_DIFFUSE_ENVIRONMENT);
		graphics->BindVertexBuffer(cube_v_buffer);
		graphics->BindIndexBuffer(cube_i_buffer);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		graphics->DisableCulling();
		graphics->DisableDepth();
		// Bind a viewport equal to the dimensions
		graphics->BindViewport(0.0f, 0.0f, dimensions.x, dimensions.y, 0.0f, 1.0f);
		graphics->BindVertexConstantBuffer(vc_buffer);
		graphics->BindPixelResourceView(environment);
		graphics->BindPixelConstantBuffer(pc_buffer);
		graphics->BindSamplerState(sampler);

		Matrix projection_matrix = ProjectionMatrixTextureCube();
		for (size_t index = 0; index < 6; index++) {
			Matrix current_matrix = MatrixTranspose(ViewMatrixTextureCube((TextureCubeFace)index) * projection_matrix);
			UpdateBufferResource(vc_buffer.buffer, &current_matrix, sizeof(Matrix), graphics->GetContext());
			graphics->BindRenderTargetView(target_views[index], nullptr);

			graphics->DrawIndexed(cube_i_buffer.count);
		}

		graphics->EnableCulling();
		graphics->EnableDepth();
		graphics->BindRenderTargetView(previous_target, previous_depth);
		graphics->BindViewport(previous_viewport);

		cube_v_buffer.buffer->Release();
		cube_i_buffer.buffer->Release();
		vc_buffer.buffer->Release();
		pc_buffer.buffer->Release();
		sampler.sampler->Release();

		for (size_t index = 0; index < 6; index++) {
			target_views[index].target->Release();
		}

		return cube;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCube ConvertEnvironmentMapToSpecularIBL(ResourceView environment, Graphics* graphics, uint2 dimensions, unsigned int sample_count)
	{
		TextureCube cube;

		// A unit cube is needed because instead of a rectangle because it will rotated by the look at matrices
		VertexBuffer cube_v_buffer;
		IndexBuffer cube_i_buffer;
		CreateCubeVertexBuffer(graphics, 1.0f, cube_v_buffer, cube_i_buffer);

		// The constant buffer for the projection view matrix
		ConstantBuffer vc_buffer = graphics->CreateConstantBuffer(sizeof(Matrix));

		// The roughness buffer
		ConstantBuffer roughness_buffer = graphics->CreateConstantBuffer(sizeof(float));

		Texture2D environment_tex = GetResource(environment);
		D3D11_TEXTURE2D_DESC environment_descriptor;
		environment_tex.tex->GetDesc(&environment_descriptor);

		// The sample buffer
		ConstantBuffer sample_buffer = graphics->CreateConstantBuffer(sizeof(unsigned int), &sample_count);

		// Main face resolution
		float face_resolution = (float)environment_descriptor.Width;
		ConstantBuffer resolution_buffer = graphics->CreateConstantBuffer(sizeof(float), &face_resolution);

		D3D11_SAMPLER_DESC sampler_desc = {};
		sampler_desc.MinLOD = 0;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamplerState sampler_state = graphics->CreateSamplerState(sampler_desc);

		// The format of the cube texture is RGBA16F
		GraphicsTextureCubeDescriptor cube_descriptor;
		cube_descriptor.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		cube_descriptor.size = dimensions;
		cube_descriptor.bind_flag = (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		cube = graphics->CreateTexture(&cube_descriptor);

		RenderTargetView previous_target = graphics->GetBoundRenderTarget();
		DepthStencilView previous_depth = graphics->GetBoundDepthStencil();
		GraphicsViewport previous_viewport = graphics->GetBoundViewport();

		graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_CREATE_SPECULAR_ENVIRONEMNT);
		graphics->BindVertexBuffer(cube_v_buffer);
		graphics->BindIndexBuffer(cube_i_buffer);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		graphics->DisableCulling();
		graphics->DisableDepth();

		graphics->BindVertexConstantBuffer(vc_buffer);
		graphics->BindPixelResourceView(environment);
		graphics->BindPixelConstantBuffer(sample_buffer, 0);
		graphics->BindPixelConstantBuffer(roughness_buffer, 1);
		graphics->BindPixelConstantBuffer(resolution_buffer, 2);
		graphics->BindSamplerState(sampler_state);

		Matrix projection_matrix = ProjectionMatrixTextureCube();

		unsigned int mip_level = 0;

		// Determine the maximum mip_level
		unsigned int max_mip_x = 1;
		unsigned int max_mip_y = 1;
		uint2 dimensions_copy = dimensions;
		while (dimensions_copy.x > 1) {
			max_mip_x++;
			dimensions_copy.x >>= 1;
		}
		while (dimensions_copy.y > 1) {
			max_mip_y++;
			dimensions_copy.y >>= 1;
		}

		float inverse_roughness_step = 1.0f / (float)(std::max(max_mip_x, max_mip_y) - 1);
		while (dimensions.x > 1 || dimensions.y > 1) {
			// Bind a viewport equal to the dimensions
			graphics->BindViewport(0.0f, 0.0f, dimensions.x, dimensions.y, 0.0f, 1.0f);

			float current_roughness = (float)mip_level * inverse_roughness_step;
			// Update the roughness buffer
			UpdateBufferResource(roughness_buffer.buffer, &current_roughness, sizeof(float), graphics->GetContext());

			for (size_t index = 0; index < 6; index++) {
				// Create a render target view of the current face
				RenderTargetView render_target_view = graphics->CreateRenderTargetView(cube, (TextureCubeFace)index, mip_level);

				// Update the vertex buffer
				Matrix current_matrix = MatrixTranspose(ViewMatrixTextureCube((TextureCubeFace)index) * projection_matrix);
				UpdateBufferResource(vc_buffer.buffer, &current_matrix, sizeof(Matrix), graphics->GetContext());

				graphics->BindRenderTargetView(render_target_view, nullptr);

				graphics->DrawIndexed(cube_i_buffer.count);
				render_target_view.target->Release();
			}

			dimensions.x = dimensions.x == 1 ? 1 : dimensions.x >> 1;
			dimensions.y = dimensions.y == 1 ? 1 : dimensions.y >> 1;
			mip_level++;
		}

		graphics->EnableCulling();
		graphics->EnableDepth();
		graphics->BindRenderTargetView(previous_target, previous_depth);
		graphics->BindViewport(previous_viewport);

		cube_v_buffer.buffer->Release();
		cube_i_buffer.buffer->Release();
		vc_buffer.buffer->Release();
		roughness_buffer.buffer->Release();
		sample_buffer.buffer->Release();
		resolution_buffer.buffer->Release();
		sampler_state.sampler->Release();

		return cube;
	}

	// ----------------------------------------------------------------------------------------------------------------

	Texture2D CreateBRDFIntegrationLUT(Graphics* graphics, uint2 dimensions, unsigned int sample_count)
	{
		Texture2D texture;

		GraphicsTexture2DDescriptor descriptor;
		descriptor.size = dimensions;
		descriptor.format = DXGI_FORMAT_R16G16_FLOAT;
		descriptor.bind_flag = (D3D11_BIND_FLAG)(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		descriptor.misc_flag = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		descriptor.mip_levels = 0;
		texture = graphics->CreateTexture(&descriptor);

		// Now render the BRDF integration LUT
#ifdef CUBE
		VertexBuffer cube_v_buffer;
		IndexBuffer cube_i_buffer;
		CreateCubeVertexBuffer(graphics, 1.0f, cube_v_buffer, cube_i_buffer);
#else
		VertexBuffer ndc_quad = CreateRectangleVertexBuffer(graphics, { -1.0f, -1.0f, 0.5f }, { 1.0f, 1.0f, 0.5f });

		float2 uvs[] = {
			{0.0f, 0.0f},
			{1.0f, 0.0f},
			{0.0f, 1.0f},
			{1.0f, 0.0f},
			{1.0f, 1.0f},
			{0.0f, 1.0f}
		};

		VertexBuffer uv_buffer = graphics->CreateVertexBuffer(sizeof(float2), std::size(uvs), uvs);
#endif
		ConstantBuffer sample_buffer = graphics->CreateConstantBuffer(sizeof(unsigned int), &sample_count);

		RenderTargetView previous_target = graphics->GetBoundRenderTarget();
		DepthStencilView previous_depth = graphics->GetBoundDepthStencil();
		GraphicsViewport previous_viewport = graphics->GetBoundViewport();

		graphics->DisableCulling();
		graphics->DisableDepth();

		graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_BRDF_INTEGRATION);
#ifdef CUBE
		graphics->BindVertexBuffer(cube_v_buffer);
		graphics->BindIndexBuffer(cube_i_buffer);
#else
		graphics->BindVertexBuffer(ndc_quad);
		graphics->BindVertexBuffer(uv_buffer, 1);
#endif
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		graphics->BindPixelConstantBuffer(sample_buffer);
		graphics->BindViewport(0.0f, 0.0f, dimensions.x, dimensions.y, 0.0f, 1.0f);

		RenderTargetView render_view = graphics->CreateRenderTargetView(texture);
		graphics->BindRenderTargetView(render_view, nullptr);

#ifdef CUBE
		graphics->DrawIndexed(cube_i_buffer.count);
#else
		graphics->Draw(ndc_quad.size);
#endif

		graphics->EnableCulling();
		graphics->EnableDepth();
		graphics->BindRenderTargetView(previous_target, previous_depth);
		graphics->BindViewport(previous_viewport);

		ResourceView resource_view = graphics->CreateTextureShaderViewResource(texture);
		graphics->GenerateMips(resource_view);
		resource_view.view->Release();

#ifdef CUBE
		cube_v_buffer.buffer->Release();
		cube_i_buffer.buffer->Release();
#else
		ndc_quad.buffer->Release();
		uv_buffer.buffer->Release();
#endif
		sample_buffer.buffer->Release();
		render_view.target->Release();

		return texture;
	}

	// ----------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------

}