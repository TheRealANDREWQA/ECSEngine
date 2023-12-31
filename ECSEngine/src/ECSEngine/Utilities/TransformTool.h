#pragma once
#include "../Core.h"
#include "BasicTypes.h"
#include "../Rendering/TransformHelpers.h"

namespace ECSEngine {

	struct TranslationToolDrag {
		ECS_INLINE void Initialize() {
			last_successful_direction.x = FLT_MAX;
			space = ECS_TRANSFORM_LOCAL_SPACE;
		}

		// Returns true if the Init values were set to an initial value, else false
		ECS_INLINE bool WasFirstCalled() const {
			return last_successful_direction.x != FLT_MAX;
		}

		ECS_AXIS axis;
		ECS_TRANSFORM_SPACE space;
		float3 last_successful_direction;
	};

	struct RotationToolDrag {
		ECS_INLINE RotationToolDrag() {}

		ECS_INLINE void InitializeRoller() {
			projected_direction.x = FLT_MAX;
			last_distance = FLT_MAX;
			space = ECS_TRANSFORM_LOCAL_SPACE;
		}

		ECS_INLINE void InitializeCircle() {
			rotation_center_ndc.x = FLT_MAX;
			last_circle_direction.x = FLT_MAX;
			space = ECS_TRANSFORM_LOCAL_SPACE;
		}

		// Returns true if the Init values were set to an initial value, else false
		ECS_INLINE bool WasFirstCalledRoller() const {
			return projected_direction.x != FLT_MAX;
		}

		// Returns true if the Init values were set to an initial value, else false
		ECS_INLINE bool WasFirstCalledCircle() const {
			return last_circle_direction.x != FLT_MAX;
		}

		ECS_AXIS axis;
		ECS_TRANSFORM_SPACE space;
		union {
			// This is used by the roller rotation delta
			struct {
				// The projected line and point are used to calculate the distance
				// from the cursor position. These are in clip space
				float2 projected_direction;
				float2 projected_point;
				float last_distance;
			};
			// This is used by the circle mapping rotation delta
			struct {
				float2 rotation_center_ndc;
				// This is already normalized
				float2 last_circle_direction;
			};
		};
	};

	struct ScaleToolDrag {
		ECS_INLINE void Initialize() {
			projected_direction_sign.x = FLT_MAX;
		}

		// Returns true if the Init values were set to an initial value, else false
		ECS_INLINE bool WasFirstCalled() const {
			return projected_direction_sign.x != FLT_MAX;
		}

		ECS_AXIS axis;
	private:
		// This is placed here in order to have the TransformToolDrag
		// have the same layout such as when setting the space this tool is not affected
		ECS_TRANSFORM_SPACE space;
	public:
		// We need these such that we know what sign we should apply to the delta
		float2 projected_direction_sign;
	};

	union TransformToolDrag  {
		TransformToolDrag() {}

		ECS_INLINE ECS_AXIS GetAxis() const {
			return translation.axis;
		}

		ECS_INLINE ECS_TRANSFORM_SPACE GetSpace() const {
			return translation.space;
		}

		ECS_INLINE void SetAxis(ECS_AXIS axis) {
			// All types have the first field the axis - it's fine to reference any
			translation.axis = axis;
		}

		ECS_INLINE void SetSpace(ECS_TRANSFORM_SPACE space) {
			// Both the rotation and the translation have as second field the space
			translation.space = space;
		}

		ECS_INLINE void Initialize(ECS_TRANSFORM_TOOL tool) {
			switch (tool) {
			case ECS_TRANSFORM_TRANSLATION:
				translation.Initialize();
				break;
			case ECS_TRANSFORM_ROTATION:
				rotation.InitializeCircle();
				break;
			case ECS_TRANSFORM_SCALE:
				scale.Initialize();
				break;
			default:
				ECS_ASSERT(false, "Invalid transform tool when initializing transform drag");
			}
		}

		TranslationToolDrag translation;
		RotationToolDrag rotation;
		ScaleToolDrag scale;
	};

	

	// Returns the translation delta corresponding to the mouse delta
	// We need a point from the plane in order to form the plane
	// This is not a very reliable method, since some translations might be lost when the
	// plane intersection fails.
	// The rotation is taken into consideration only when the drag tool has the local space enabled
	template<typename CameraType>
	ECSENGINE_API float3 HandleTranslationToolDelta(
		const CameraType* camera,
		float3 plane_point,
		QuaternionScalar object_rotation,
		ECS_AXIS axis,
		ECS_TRANSFORM_SPACE space,
		uint2 window_size,
		int2 mouse_texel_position,
		int2 mouse_delta
	);

	// Both previous ray direction and current ray direction need to be normalized
	// The rotation is taken into consideration only when the drag tool has the local space enabled
	template<typename CameraType>
	ECSENGINE_API float3 HandleTranslationToolDelta(
		const CameraType* camera,
		float3 plane_point,
		QuaternionScalar object_rotation,
		ECS_AXIS axis,
		ECS_TRANSFORM_SPACE space,
		float3 previous_ray_direction,
		float3 current_ray_direction,
		bool2* success_status = nullptr
	);

	// The rotation is taken into consideration only when the drag tool has the local space enabled
	template<typename CameraType>
	ECSENGINE_API float3 HandleTranslationToolDelta(
		const CameraType* camera,
		float3 plane_point,
		QuaternionScalar object_rotation,
		TranslationToolDrag* drag_tool,
		float3 current_ray_direction
	);

	// Rotation needs to be specified in angles
	template<typename CameraType>
	ECSENGINE_API float3 HandleRotationToolDelta(
		const CameraType* camera,
		float3 rotation_center,
		QuaternionScalar rotation_value,
		RotationToolDrag* drag_tool,
		uint2 window_size,
		int2 mouse_texel_position,
		float rotation_factor
	);

	template<typename CameraType>
	ECSENGINE_API QuaternionScalar HandleRotationToolDeltaCircleMapping(
		const CameraType* camera,
		float3 rotation_center,
		QuaternionScalar rotation_value,
		RotationToolDrag* drag_tool,
		uint2 window_size,
		int2 mouse_texel_position,
		float rotation_factor
	);

	// Rotation needs to be specified in angles
	template<typename CameraType>
	ECSENGINE_API float3 HandleScaleToolDelta(
		const CameraType* camera,
		float3 scale_center,
		QuaternionScalar rotation_value,
		ScaleToolDrag* scale_tool,
		int2 mouse_texel_delta,
		float scale_factor
	);

}