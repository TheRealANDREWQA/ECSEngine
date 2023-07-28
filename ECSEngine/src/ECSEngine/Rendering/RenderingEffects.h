#pragma once
#include "../Core.h"
#include "ColorUtilities.h"
#include "../Math/Matrix.h"
#include "RenderingStructures.h"

namespace ECSEngine {

#define ECS_HIGHLIGHT_OBJECT_THICKNESS_DEFAULT 3

#define ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS 3
#define ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS ((1 << ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS) - 1)

	struct Graphics;

	struct RenderingEffectMesh {
		bool is_submesh = false;
		// The matrix should be in GPU form
		Matrix gpu_mvp_matrix;
		union {
			struct {
				const Mesh* mesh;
			};
			struct {
				const CoalescedMesh* coalesced_mesh;
				unsigned int submesh_index;
			};
		};
	};

	// ------------------------------------------------------------------------------------------------------------

	struct HighlightObjectElement {
		RenderingEffectMesh base;
	};
	
	ECSENGINE_API void HighlightObject(
		Graphics* graphics,
		ColorFloat color,
		Stream<HighlightObjectElement> elements,
		unsigned int pixel_thickness = ECS_HIGHLIGHT_OBJECT_THICKNESS_DEFAULT
	);

	// ------------------------------------------------------------------------------------------------------------

	// If the instance_index is left at -1, then it will use the index
	// inside the elements stream. Pxel thickness can be at max ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS
	struct GenerateInstanceFramebufferElement {
		RenderingEffectMesh base;
		unsigned char pixel_thickness = 0;
		unsigned int instance_index = -1;
	};

	ECS_INLINE unsigned int GenerateRenderInstanceValue(unsigned int index, unsigned char pixel_thickness) {
		ECS_ASSERT(pixel_thickness < ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS);
		return function::BlendBits<unsigned int>(
			index,
			pixel_thickness,
			32 - ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS,
			ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS
		);
	}

	ECSENGINE_API void GenerateInstanceFramebuffer(
		Graphics* graphics,
		Stream<GenerateInstanceFramebufferElement> elements,
		RenderTargetView render_target,
		DepthStencilView depth_stencil_view
	);

	// Returns -1 if nothing is drawn at that point
	ECSENGINE_API unsigned int GetInstanceFromFramebuffer(
		Graphics* graphics,
		RenderTargetView render_target,
		uint2 pixel_position
	);

	// The values pointer needs to have space for width * height values
	// It does automatic clamping for bottom right. Bottom right is also inclusive,
	// meaning that if bottom_right == top_left, a single value will be filled in
	// at that pixel location
	ECSENGINE_API void GetInstancesFromFramebuffer(
		Graphics* graphics,
		RenderTargetView render_target,
		uint2 top_left,
		uint2 bottom_right,
		unsigned int* values
	);

	// It does the same as the normal call but it will only fill in
	// unique valid values (it will omit -1 values)
	ECSENGINE_API void GetInstancesFromFramebufferFiltered(
		Graphics* graphics,
		RenderTargetView render_target,
		uint2 top_left,
		uint2 bottom_right,
		CapacityStream<unsigned int>* values
	);

	struct CPUInstanceFramebuffer {
		// Can't inline this function since it will cause a crash on malloc
		// if the allocation is made with TransferInstancesFramebufferToCPU
		// that will cause an allocation to be made from the DLL and the deallocation
		// in the exe
		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) const {
			ECSEngine::DeallocateEx(allocator, values);
		}

		unsigned int* values;
		uint2 dimensions;
	};

	// Returns the byte size necessary to get all the values stored in the render target view
	// Such that the entire texture uints can be stored
	ECSENGINE_API size_t GetInstancesFramebufferAllocateSize(
		RenderTargetView render_target
	);

	// Get allocation size of the uints can be determined using the
	// GetInstancesFramebufferAllocateSize function
	ECSENGINE_API CPUInstanceFramebuffer TransferInstancesFramebufferToCPU(
		Graphics* graphics,
		RenderTargetView render_target,
		unsigned int* values
	);

	// It will determine the byte size and make the proper allocation
	ECSENGINE_API CPUInstanceFramebuffer TransferInstancesFramebufferToCPUAndAllocate(
		Graphics* graphics,
		RenderTargetView render_target,
		AllocatorPolymorphic allocator
	);

	ECSENGINE_API void GetInstancesFromFramebufferFilteredCPU(
		CPUInstanceFramebuffer cpu_values,
		uint2 top_left,
		uint2 bottom_right,
		CapacityStream<unsigned int>* filtered_values
	);

	// ------------------------------------------------------------------------------------------------------------
}