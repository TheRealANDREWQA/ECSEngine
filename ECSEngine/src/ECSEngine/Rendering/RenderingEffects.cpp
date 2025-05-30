#include "ecspch.h"
#include "RenderingEffects.h"
#include "TextureOperations.h"
#include "GraphicsHelpers.h"
#include "Graphics.h"
#include "../Utilities/StreamUtilities.h"

namespace ECSEngine {

	// The functor is called each iteration before the draw call is issued with the index
	// of the current iteration
	template<typename Functor>
	void RenderingEffectElementBaseDraw(
		Graphics* graphics,
		const void* elements,
		size_t count,
		size_t byte_size,
		ConstantBuffer vertex_cbuffer,
		Functor&& functor
	) {
		for (size_t index = 0; index < count; index++) {
			const RenderingEffectMesh* element = (const RenderingEffectMesh*)OffsetPointer(elements, byte_size * index);
			void* cbuffer_data = graphics->MapBuffer(vertex_cbuffer.buffer);
			element->gpu_mvp_matrix.Store(cbuffer_data);
			graphics->UnmapBuffer(vertex_cbuffer.buffer);

			functor(index);

			if (element->is_submesh) {
				graphics->BindMesh(element->coalesced_mesh->mesh);
				Submesh submesh = element->coalesced_mesh->submeshes[element->submesh_index];
				graphics->DrawIndexed(submesh.index_count, submesh.index_buffer_offset, submesh.vertex_buffer_offset);
			}
			else {
				graphics->BindMesh(*element->mesh);
				graphics->DrawIndexed(element->mesh->index_buffer.count);
			}
		}
	}

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
		Texture2D render_texture = pipeline_state.target.target.GetResource();

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

		// Bind the helper stencil output
		graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_HIGHLIGHT_STENCIL);

		ConstantBuffer vertex_cbuffer = graphics->CreateConstantBuffer(sizeof(Matrix), true);
		graphics->BindVertexConstantBuffer(vertex_cbuffer);

		unsigned int mask_value = -1;
		// Write the stencil value as full white
		ConstantBuffer pixel_cbuffer = graphics->CreateConstantBuffer(sizeof(mask_value), &mask_value, true);
		graphics->BindPixelConstantBuffer(pixel_cbuffer);
		graphics->DisableDepth();

		// First pass - output the stencil values
		RenderingEffectElementBaseDraw(
			graphics,
			elements.buffer,
			elements.size,
			elements.MemoryOf(1),
			vertex_cbuffer,
			[](size_t index) {}
		);

		// Second pass
		// Unbind the stencil texture and rebing the initial texture
		graphics->BindRenderTargetView(pipeline_state.target.target, graphics->GetBoundDepthStencil());

		// Enable alpha blending - we'll use this as a way of masking the pixels which should not be highlighted
		graphics->EnableAlphaBlending();

		Texture2DDescriptor render_descriptor = GetTextureDescriptor(render_texture);

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
		
		graphics->RestorePipelineState(&pipeline_state);
	}

	// ------------------------------------------------------------------------------------------------------------

	void GenerateInstanceFramebuffer(
		Graphics* graphics,
		Stream<GenerateInstanceFramebufferElement> elements,
		RenderTargetView render_target,
		DepthStencilView depth_stencil,
		bool do_not_restore_pipeline_state
	) {
		GraphicsPipelineState pipeline_state;
		if (!do_not_restore_pipeline_state) {
			pipeline_state = graphics->GetPipelineState();
		}

		struct VertexCBuffer {
			Matrix mvp_matrix;
		};

		ConstantBuffer vertex_cbuffer = graphics->CreateConstantBuffer(sizeof(VertexCBuffer), true);
		ConstantBuffer pixel_cbuffer = graphics->CreateMousePickShaderPixelCBuffer();

		graphics->BindVertexConstantBuffer(vertex_cbuffer);
		graphics->BindMousePickShaderPixelCBuffer(pixel_cbuffer);

		graphics->ClearDepthStencil(depth_stencil);
		// An instance index of 0 means that the slot is empty
		graphics->ClearRenderTarget(render_target, ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));

		graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_MOUSE_PICK);
		graphics->BindRenderTargetView(render_target, depth_stencil);

		RenderingEffectElementBaseDraw(
			graphics,
			elements.buffer,
			elements.size,
			elements.MemoryOf(1),
			vertex_cbuffer,
			[&](size_t index) {
				unsigned int instance_index = elements[index].instance_index;
				instance_index = instance_index == -1 ? (unsigned int)index : instance_index;
				graphics->UpdateMousePickShaderPixelCBuffer(pixel_cbuffer, GenerateRenderInstanceValue(instance_index, elements[index].pixel_thickness));
			}
		);

		vertex_cbuffer.Release();
		pixel_cbuffer.Release();

		if (!do_not_restore_pipeline_state) {
			graphics->RestorePipelineState(&pipeline_state);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned int GetInstanceFromFramebuffer(Graphics* graphics, RenderTargetView render_target, uint2 pixel_position)
	{
		unsigned int value;
		GetInstancesFromFramebuffer(graphics, render_target, pixel_position, pixel_position + uint2(1, 1), &value);
		return value;
	}

	// ------------------------------------------------------------------------------------------------------------

	// The functor will be called with the pixel data, the clamped top left copy corner
	// and the clamped  bottom right copy corner, the copy size and the mapped texture row byte size
	// The signature is (unsigned int* texture_data, uint2 top_left_copy_corner, 
	// uint2 bottom_right_copy_corner, uint2 copy_size, unsigned int mapped_texture_row_byte_size)
	template<typename Functor>
	void GetInstancesFromFramebufferMain(
		Graphics* graphics,
		RenderTargetView render_target,
		uint2 top_left,
		uint2 bottom_right,
		Functor&& functor
	) {
		Texture2D render_texture = render_target.GetResource();
		Texture2DDescriptor render_texture_descriptor = GetTextureDescriptor(render_texture);

		uint2 top_left_copy_corner = {
			SaturateSub<unsigned int>(top_left.x, ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS),
			SaturateSub<unsigned int>(top_left.y, ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS)
		};

		uint2 bottom_right_copy_corner = {
			ClampMax(bottom_right.x + ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS, render_texture_descriptor.size.x),
			ClampMax(bottom_right.y + ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS, render_texture_descriptor.size.y)
		};

		uint2 copy_size = bottom_right_copy_corner - top_left_copy_corner;

		// Create a temporary staging readback texture
		Texture2DDescriptor temporary_texture_descriptor;
		temporary_texture_descriptor.usage = ECS_GRAPHICS_USAGE_STAGING;
		temporary_texture_descriptor.size = copy_size;
		temporary_texture_descriptor.bind_flag = ECS_GRAPHICS_BIND_NONE;
		temporary_texture_descriptor.cpu_flag = ECS_GRAPHICS_CPU_ACCESS_READ;
		temporary_texture_descriptor.mip_levels = 1;
		temporary_texture_descriptor.format = ECS_GRAPHICS_FORMAT_R32_UINT;

		Texture2D temporary_texture = graphics->CreateTexture(&temporary_texture_descriptor, true);

		CopyTextureSubresource(temporary_texture, { 0, 0 }, 0, render_texture, top_left_copy_corner, copy_size, 0, graphics->GetContext());

		// Now open the texture and read the pixels
		MappedTexture mapped_texture = graphics->MapTexture(temporary_texture, ECS_GRAPHICS_MAP_READ);
		unsigned int* texture_data = (unsigned int*)mapped_texture.data;

		functor(texture_data, top_left_copy_corner, bottom_right_copy_corner, copy_size, mapped_texture.row_byte_size);

		graphics->UnmapTexture(temporary_texture);
		temporary_texture.Release();
	}

	void GetInstancesFromFramebuffer(
		Graphics* graphics, 
		RenderTargetView render_target, 
		uint2 top_left, 
		uint2 bottom_right, 
		unsigned int* values
	)
	{
		GetInstancesFromFramebufferMain(graphics, render_target, top_left, bottom_right, [&](
			unsigned int* texture_data, 
			uint2 top_left_copy_corner,
			uint2 bottom_right_copy_corner,
			uint2 copy_size,
			unsigned int mapped_texture_row_byte_size
		) {
			uint2 dimensions = bottom_right - top_left;
			memset(values, 0, sizeof(unsigned int) * dimensions.x * dimensions.y);

			// Read the values now
			for (unsigned int row = 0; row < copy_size.y; row++) {
				for (unsigned int column = 0; column < copy_size.x; column++) {
					unsigned int instance_index, pixel_thickness;
					RetrieveBlendedBits(
						IndexTextureEx(texture_data, row, column, mapped_texture_row_byte_size),
						32 - ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
						ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
						instance_index,
						pixel_thickness
					);

					uint2 pixel_thickness2 = { pixel_thickness, pixel_thickness };
					uint2 current_position = uint2(row + top_left_copy_corner.x, column + top_left_copy_corner.y);
					uint2 test_top_left = current_position - pixel_thickness2;
					uint2 test_bottom_right = current_position + pixel_thickness2;
					uint4 overlap = RectangleOverlap(test_top_left, test_bottom_right, top_left, bottom_right);

					// Now iterate over the positions where instances need to be retrieved
					unsigned int overlap_width = overlap.z - overlap.x + 1;
					unsigned int overlap_height = overlap.w - overlap.y + 1;
					for (unsigned int overlap_row = 0; overlap_row < overlap_height; overlap_row++) {
						for (unsigned int overlap_column = 0; overlap_column < overlap_width; overlap_column++) {
							uint2 current_instance_position = top_left + uint2(overlap_row, overlap_column);
							uint2 difference = BasicTypeAbsoluteDifference(current_instance_position, current_position);
							if (difference.x <= pixel_thickness && difference.y <= pixel_thickness) {
								values[overlap_row * dimensions.x + overlap_column] = instance_index;
							}
						}
					}
				}
			}

			// Now we need to decrement the the values by one
			// In order to preserve the right order and for pixels that don't have a match
			// to return -1
			size_t texel_count = (size_t)dimensions.x * (size_t)dimensions.y;
			for (size_t index = 0; index < texel_count; index++) {
				values[index]--;
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	static void GetInstancesFromFramebufferFilteredInSelection(
		unsigned int* texture_data,
		uint2 start,
		uint2 dimensions,
		uint2 texture_dimensions,
		unsigned int texture_data_row_byte_size,
		CapacityStream<unsigned int>* filtered_values
	) {
		uint2 bottom_right = start + dimensions;
		if (dimensions.x > 1 || dimensions.y > 1) {
			for (unsigned int row = 0; row < dimensions.y; row++) {
				for (unsigned int column = 0; column < dimensions.x; column++) {
					unsigned int instance_index, pixel_thickness;
					unsigned int current_value = IndexTextureEx(texture_data, start.y + row, start.x + column, texture_data_row_byte_size);
					RetrieveBlendedBits(
						current_value,
						32 - ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
						ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
						instance_index,
						pixel_thickness
					);

					if (instance_index != 0) {
						instance_index--;
						CapacityStream<unsigned int> addition = { &instance_index, 1, 1 };
						// Check to see if it exists and add it if not
						StreamAddUniqueSearchBytes(*filtered_values, addition);
					}
				}
			}
		}
		else if (dimensions.x == 1 && dimensions.y == 1) {
			unsigned int left_start_count = ClampMax<unsigned int>(start.x, ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS);
			unsigned int top_start_count = ClampMax<unsigned int>(start.y, ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS);
			unsigned int right_end_count = ClampMax<unsigned int>(texture_dimensions.x - bottom_right.x, ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS);
			unsigned int bottom_end_count = ClampMax<unsigned int>(texture_dimensions.y - bottom_right.y, ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS);

			uint2 initial_position = start;
			start.x -= left_start_count;
			start.y -= top_start_count;
			bottom_right.x -= right_end_count;
			bottom_right.y -= bottom_end_count;

			// For single point take into consideration the thickness as well
			unsigned int max_pixel_thickness = 0;
			for (unsigned int row = 0; row < top_start_count + bottom_end_count + 1; row++) {
				for (unsigned int column = 0; column < left_start_count + right_end_count + 1; column++) {
					unsigned int instance_index, pixel_thickness;
					unsigned int current_value = IndexTextureEx(texture_data, start.y + row, start.x + column, texture_data_row_byte_size);
					RetrieveBlendedBits(
						current_value,
						32 - ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
						ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
						instance_index,
						pixel_thickness
					);

					if (instance_index != 0) {
						uint2 position_difference = BasicTypeAbsoluteDifference(initial_position, uint2(start.x + column, start.y + row));
						if (position_difference.x <= pixel_thickness && position_difference.y <= pixel_thickness) {
							if (pixel_thickness >= max_pixel_thickness) {
								instance_index--;
								filtered_values->buffer[0] = instance_index;
								filtered_values->size = 1;
								max_pixel_thickness = pixel_thickness;
							}
						}
					}
				}
			}
		}
	}

	void GetInstancesFromFramebufferFiltered(
		Graphics* graphics, 
		RenderTargetView render_target, 
		uint2 top_left, 
		uint2 bottom_right, 
		CapacityStream<unsigned int>* values
	)
	{
		GetInstancesFromFramebufferMain(graphics, render_target, top_left, bottom_right, [&](
			unsigned int* texture_data,
			uint2 top_left_copy_corner,
			uint2 bottom_right_copy_corner,
			uint2 copy_size,
			unsigned int mapped_texture_row_byte_size
		) {
			// Here ignore pixel thickness
			uint2 dimensions = bottom_right - top_left;
			uint2 offsets = top_left - top_left_copy_corner;

			uint2 texture_dimensions = GetTextureDimensions(render_target.AsTexture2D());
			GetInstancesFromFramebufferFilteredInSelection(texture_data, offsets, dimensions, texture_dimensions, mapped_texture_row_byte_size, values);
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	size_t GetInstancesFramebufferAllocateSize(RenderTargetView render_target)
	{
		uint2 texture_dimensions = GetTextureDimensions(render_target.AsTexture2D());
		return texture_dimensions.x * texture_dimensions.y * sizeof(unsigned int);
	}

	// ------------------------------------------------------------------------------------------------------------

	CPUInstanceFramebuffer TransferInstancesFramebufferToCPU(Graphics* graphics, RenderTargetView render_target, unsigned int* values)
	{
		CPUInstanceFramebuffer cpu_framebuffer;
		cpu_framebuffer.values = values;

		// Create a staging texture and then copy from it
		Texture2D current_texture = render_target.AsTexture2D();
		Texture2D staging_texture = TextureToStaging(graphics, current_texture);

		uint2 texture_dimensions = GetTextureDimensions(current_texture);
		cpu_framebuffer.dimensions = texture_dimensions;
		
		MappedTexture mapped_values = graphics->MapTexture(staging_texture, ECS_GRAPHICS_MAP_READ);
		// We need to copy row by row since the texture might have padding in between rows

		// We need to make a copy of these current values, in order to not have cascading updates
		// That propagate more than they should for the post process step. We don't even have to write
		// Into values the mapped data, we can write only to the temporary and in the post processing step
		// The final outputs are written directly.
		unsigned int* copied_values = (unsigned int*)Malloc(sizeof(values[0]) * texture_dimensions.x * texture_dimensions.y);

		unsigned int* write_copied_values_ptr = copied_values;
		for (unsigned int index = 0; index < texture_dimensions.y; index++) {
			memcpy(write_copied_values_ptr, mapped_values.data, sizeof(unsigned int) * texture_dimensions.x);
			write_copied_values_ptr += texture_dimensions.x;
			mapped_values.data = OffsetPointer(mapped_values.data, mapped_values.row_byte_size);
		}

		graphics->UnmapTexture(staging_texture);
		staging_texture.Release();

		// After reading all values, do a post processing pass where for each pixel the actual final
		// Instance IDs are written, such that querying will be much more faster.

		// We need to initialize the the values to -1, such that we override them when a match is found.
		memset(values, 0xFF, sizeof(values[0]) * texture_dimensions.x * texture_dimensions.y);

		__try {
			size_t texture_row_byte_size = sizeof(unsigned int) * (size_t)texture_dimensions.x;
			for (unsigned int row = 0; row < texture_dimensions.y; row++) {
				for (unsigned int column = 0; column < texture_dimensions.x; column++) {
					unsigned int instance_index = 0;
					unsigned int pixel_thickness = 0;

					unsigned int current_value = IndexTextureEx(copied_values, row, column, texture_row_byte_size);
					RetrieveBlendedBits(
						current_value,
						32 - ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
						ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
						instance_index,
						pixel_thickness
					);

					if (instance_index != 0) {
						// Now we need to decrement the value by one
						// In order to preserve the right order and for pixels that don't have a match to return -1.
						// Only write this new value if the texture value is still -1, meaning that no other thickness
						// Value wrote into it.
						unsigned int& texture_value = IndexTextureEx(values, row, column, texture_row_byte_size);
						if (texture_value == -1) {
							texture_value = instance_index - 1;
						}

						for (unsigned int thickness_row = 0; thickness_row < pixel_thickness * 2; thickness_row++) {
							int current_row = (int)row - pixel_thickness + thickness_row;
							if (current_row < 0 || current_row >= texture_dimensions.y) {
								continue;
							}

							for (unsigned int thickness_column = 0; thickness_column < pixel_thickness * 2; thickness_column++) {
								int current_column = (int)column - pixel_thickness + thickness_column;
								if (current_column < 0 || current_column >= texture_dimensions.x) {
									continue;
								}

								unsigned int current_instance_index, current_pixel_thickness;
								unsigned int thickness_set_value = IndexTextureEx(copied_values, row, column, texture_row_byte_size);
								RetrieveBlendedBits(
									thickness_set_value,
									32 - ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
									ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
									current_instance_index,
									current_pixel_thickness
								);

								if (pixel_thickness >= current_pixel_thickness) {
									IndexTextureEx(values, (size_t)current_row, (size_t)current_column, texture_row_byte_size) = instance_index - 1;
								}
							}
						}
					}
				}
			}
		}
		__finally {
			Free(copied_values);
		}

		return cpu_framebuffer;
	}

	// ------------------------------------------------------------------------------------------------------------

	CPUInstanceFramebuffer TransferInstancesFramebufferToCPUAndAllocate(Graphics* graphics, RenderTargetView render_target, AllocatorPolymorphic allocator)
	{
		size_t byte_size = GetInstancesFramebufferAllocateSize(render_target);
		void* allocation = Allocate(allocator, byte_size);
		return TransferInstancesFramebufferToCPU(graphics, render_target, (unsigned int*)allocation);
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetInstancesFromFramebufferFilteredCPU(
		CPUInstanceFramebuffer cpu_values,
		uint2 top_left, 
		uint2 bottom_right, 
		CapacityStream<unsigned int>* filtered_values
	)
	{
		// This is the version that would use the unprocessed values
		/*GetInstancesFromFramebufferFilteredInSelection(
			cpu_values.values, 
			top_left, 
			bottom_right - top_left, 
			cpu_values.dimensions, 
			sizeof(unsigned int) * cpu_values.dimensions.x, 
			filtered_values
		);*/

		// The new version uses the post processed values and can simply use the instances as they are
		uint2 dimensions = bottom_right - top_left;
		size_t texture_row_byte_size = sizeof(cpu_values.values[0]) * cpu_values.dimensions.x;
		for (unsigned int row = 0; row < dimensions.y; row++) {
			for (unsigned int column = 0; column < dimensions.x; column++) {
				unsigned int instance_index = IndexTextureEx(cpu_values.values, top_left.y + row, top_left.x + column, texture_row_byte_size);
				if (instance_index != -1) {
					CapacityStream<unsigned int> addition = { &instance_index, 1, 1 };
					// Check to see if it exists and add it if not
					StreamAddUniqueSearchBytes(*filtered_values, addition);
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void CPUInstanceFramebuffer::Deallocate(AllocatorPolymorphic allocator) const
	{
		ECSEngine::Deallocate(allocator, values);
	}

	// ------------------------------------------------------------------------------------------------------------

}