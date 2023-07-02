#include "ecspch.h"
#include "RenderingEffects.h"
#include "Graphics.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void HighlightObject(Graphics* graphics, Matrix camera_matrix, Stream<HighlightObjectElement> elements)
	{
		// Read the description of the depth stencil view that is currently bound - if it has stencil continue
		// At the moment don't implement the case when the depth stencil is purely depth
		D3D11_TEXTURE2D_DESC depth_stencil_desc;
		DepthStencilView depth_stencil_view = graphics->GetBoundDepthStencil();
		Texture2D depth_stencil_texture = depth_stencil_view.GetResource();
		depth_stencil_texture.tex->GetDesc(&depth_stencil_desc);

		if (depth_stencil_desc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || depth_stencil_desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT) {
			// Clear the stencil value
			graphics->ClearStencil(depth_stencil_view, 0);

			GraphicsPipelineState pipeline_state = graphics->GetPipelineState();

			// Bind a nullptr pixel shader such that the output is disabled
			graphics->BindPixelShader(nullptr);

			// Bind the default transform vertex shader
			graphics->BindVertexShader(graphics->m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_HIGHLIGHT].vertex);
			graphics->BindInputLayout(graphics->m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_HIGHLIGHT].input_layout);

			ConstantBuffer vertex_cbuffer = graphics->CreateConstantBuffer(sizeof(Matrix), true);
			graphics->BindVertexConstantBuffer(vertex_cbuffer);

			auto pass = [&](auto use_scale_thickness) {
				for (size_t index = 0; index < elements.size; index++) {
					void* cbuffer_data = graphics->MapBuffer(vertex_cbuffer.buffer);
					float scale_multiplier = 1.0f;
					if constexpr (use_scale_thickness) {
						scale_multiplier = elements[index].thickness;
					}
					Matrix mvp_matrix = MatrixScale(elements[index].element_scale * scale_multiplier) * elements[index].translation_rotation_matrix * camera_matrix;
					mvp_matrix = MatrixGPU(mvp_matrix);
					mvp_matrix.Store(cbuffer_data);
					graphics->UnmapBuffer(vertex_cbuffer.buffer);

					if (elements[index].is_coalesced) {
						graphics->BindMesh(*elements[index].mesh);
						graphics->DrawIndexed(elements[index].mesh->index_buffer.count);
					}
					else {
						graphics->BindMesh(elements[index].coalesced_mesh->mesh);
						Submesh submesh = elements[index].coalesced_mesh->submeshes[elements[index].submesh_index];
						graphics->DrawIndexed(submesh.index_count, submesh.index_buffer_offset, submesh.vertex_buffer_offset);
					}
				}
			};

			// First pass - output the stencil values
			// Bind the first draw pass depth stencil state
			graphics->BindDepthStencilState(graphics->m_depth_stencil_helpers[ECS_GRAPHICS_DEPTH_STENCIL_HELPER_HIGHLIGHT_FIRST_PASS], 0xFF);
			// Leave the scale as is
			pass(std::false_type{});
			
			// Second pass - draw the meshes slightly larger and then perform the stencil test such that
			// only the pixels outside the stencil are allowed
			graphics->BindPixelShader(graphics->m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_HIGHLIGHT].pixel);
			graphics->BindDepthStencilState(graphics->m_depth_stencil_helpers[ECS_GRAPHICS_DEPTH_STENCIL_HELPER_HIGHLIGHT_SECOND_PASS], 0xFF);
			pass(std::true_type{});

			vertex_cbuffer.Release();
			graphics->RestorePipelineState(&pipeline_state);
		}
		else {
			ECS_ASSERT(false, "Highlighting objects when the depth buffer doesn't have stencil is not yet implemented");
		}
	}

	// ------------------------------------------------------------------------------------------------------------

}