#pragma once
#include "../Core.h"
#include "ColorUtilities.h"
#include "../Math/Matrix.h"
#include "RenderingStructures.h"

namespace ECSEngine {

#define ECS_HIGHLIGHT_OBJECT_THICKNESS_DEFAULT 3

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

#define ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS 3
#define ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS ((1 << ECS_GENERATE_INSTANCE_FRAMEBUFFER_PIXEL_THICKNESS_BITS) - 1)

	// If the instance_index is left at -1, then it will use the index
	// inside the elements stream. Pxel thickness can be at max ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS
	struct GenerateInstanceFramebufferElement {
		RenderingEffectMesh base;
		unsigned char pixel_thickness = 0;
		unsigned int instance_index = -1;
	};

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

	// ------------------------------------------------------------------------------------------------------------
}