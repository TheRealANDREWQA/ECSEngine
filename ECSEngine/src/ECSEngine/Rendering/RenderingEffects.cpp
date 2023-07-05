#include "ecspch.h"
#include "RenderingEffects.h"
#include "GraphicsHelpers.h"
#include "Graphics.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void HighlightObject(
		Graphics* graphics,
		ColorFloat color,
		Stream<HighlightObjectElement> elements,
		unsigned int pixel_thickness
	)
	{
		// How this works
		// Create a temporary texture that will be used as a stencil texture
		// Write into it using a default transform vertex shader + solid color pixel shader
		// Then use a compute shader to read the contents of this texture and then
		// perform the highlighting
		GraphicsPipelineState pipeline_state = graphics->GetPipelineState();
		Texture2D render_texture = pipeline_state.views.target.GetResource();

		Texture2DDescriptor stencil_descriptor;
		stencil_descriptor.size = GetTextureDimensions(render_texture);
		stencil_descriptor.bind_flag = ECS_GRAPHICS_BIND_RENDER_TARGET | ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		stencil_descriptor.usage = ECS_GRAPHICS_USAGE_DEFAULT;
		stencil_descriptor.format = ECS_GRAPHICS_FORMAT_R8_UINT;
		stencil_descriptor.mip_levels = 1;
		Texture2D stencil_texture = graphics->CreateTexture(&stencil_descriptor, true);
		RenderTargetView stencil_render_view = graphics->CreateRenderTargetView(stencil_texture, 0, ECS_GRAPHICS_FORMAT_UNKNOWN, true);
		ResourceView stencil_resource_view = graphics->CreateTextureShaderViewResource(stencil_texture, true);
		graphics->ClearRenderTarget(stencil_render_view, ECS_COLOR_BLACK);

		graphics->BindRenderTargetView(stencil_render_view, graphics->GetBoundDepthStencil());

		// Bind the default transform vertex shader and solid color pixel shader
		graphics->BindVertexShader(graphics->m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_HIGHLIGHT_STENCIL].vertex);
		graphics->BindInputLayout(graphics->m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_HIGHLIGHT_STENCIL].input_layout);
		graphics->BindPixelShader(graphics->m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_HIGHLIGHT_STENCIL].pixel);

		ConstantBuffer vertex_cbuffer = graphics->CreateConstantBuffer(sizeof(Matrix), true);
		graphics->BindVertexConstantBuffer(vertex_cbuffer);

		unsigned int mask_value = -1;
		// Write the stencil value as full white
		ConstantBuffer pixel_cbuffer = graphics->CreateConstantBuffer(sizeof(mask_value), &mask_value, true);
		graphics->BindPixelConstantBuffer(pixel_cbuffer);
		graphics->DisableDepth();

		// First pass - output the stencil values
		// Bind the first draw pass depth stencil state
		// Leave the scale as is
		for (size_t index = 0; index < elements.size; index++) {
			void* cbuffer_data = graphics->MapBuffer(vertex_cbuffer.buffer);
			elements[index].gpu_mvp_matrix.Store(cbuffer_data);
			graphics->UnmapBuffer(vertex_cbuffer.buffer);

			if (elements[index].is_submesh) {
				graphics->BindMesh(elements[index].coalesced_mesh->mesh);
				Submesh submesh = elements[index].coalesced_mesh->submeshes[elements[index].submesh_index];
				graphics->DrawIndexed(submesh.index_count, submesh.index_buffer_offset, submesh.vertex_buffer_offset);
			}
			else {
				graphics->BindMesh(*elements[index].mesh);
				graphics->DrawIndexed(elements[index].mesh->index_buffer.count);
			}
		}

		// Second pass
		// Unbind the stencil texture and rebing the initial texture
		graphics->BindRenderTargetView(pipeline_state.views.target, graphics->GetBoundDepthStencil());

		// Enable alpha blending - we'll use this as a way of masking the pixels which should not be highlighted
		graphics->EnableAlphaBlending();

		Texture2DDescriptor render_descriptor;
		GetTextureDescriptor(render_texture, &render_descriptor);

		struct HighlightBlendData {
			ColorFloat color;
			int pixel_count;
		};
		HighlightBlendData blend_pass_data;
		blend_pass_data.color = color;
		blend_pass_data.pixel_count = pixel_thickness;
		ConstantBuffer blend_cbuffer = graphics->CreateConstantBuffer(sizeof(blend_pass_data), &blend_pass_data, true);

		graphics->BindPixelShader(graphics->m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_HIGHLIGHT_BLEND].pixel);
		graphics->BindPixelConstantBuffer(blend_cbuffer);
		graphics->BindPixelResourceView(stencil_resource_view);
		DrawWholeViewportQuad(graphics, graphics->GetContext());

		vertex_cbuffer.Release();
		pixel_cbuffer.Release();
		blend_cbuffer.Release();
		stencil_texture.Release();
		stencil_resource_view.Release();
		stencil_render_view.Release();
		
		//graphics->RestoreBlendState(pipeline_state.render_state.blend_state);
		//graphics->RestoreDepthStencilState(pipeline_state.render_state.depth_stencil_state);
		//graphics->RestoreRasterizerState(pipeline_state.render_state.rasterizer_state);
		//graphics->RestorePipelineShaders(&pipeline_state.render_state.shaders);
		graphics->RestorePipelineState(&pipeline_state);
	}

	// ------------------------------------------------------------------------------------------------------------

}