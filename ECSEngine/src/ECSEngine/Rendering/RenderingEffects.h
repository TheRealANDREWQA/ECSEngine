#pragma once
#include "../Core.h"
#include "ColorUtilities.h"
#include "../Math/Matrix.h"

namespace ECSEngine {

#define ECS_HIGHLIGHT_OBJECT_THICKNESS_DEFAULT 3

	struct Graphics;
	struct Mesh;
	struct CoalescedMesh;

	struct HighlightObjectElement {
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

	ECSENGINE_API void HighlightObject(
		Graphics* graphics,
		ColorFloat color,
		Stream<HighlightObjectElement> elements,
		unsigned int pixel_thickness = ECS_HIGHLIGHT_OBJECT_THICKNESS_DEFAULT
	);

	// ------------------------------------------------------------------------------------------------------------
}