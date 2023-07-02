#pragma once
#include "../Core.h"
#include "ColorUtilities.h"
#include "../Math/Matrix.h"

namespace ECSEngine {

#define ECS_HIGHLIGHT_OBJECT_THICKNESS_DEFAULT 0.2f

	struct Graphics;
	struct Mesh;
	struct CoalescedMesh;

	struct HighlightObjectElement {
		bool is_coalesced = false;
		float thickness = ECS_HIGHLIGHT_OBJECT_THICKNESS_DEFAULT;
		Color highlight_color;
		float3 element_scale;
		Matrix translation_rotation_matrix;
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
		Matrix camera_matrix,
		Stream<HighlightObjectElement> elements
	);

	// ------------------------------------------------------------------------------------------------------------
}