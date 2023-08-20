// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "Reflection/ReflectionMacros.h"
#include "BasicTypes.h"
#include "../Math/Plane.h"
#include "../Math/Quaternion.h"

namespace ECSEngine {

	enum ECS_REFLECT ECS_TRANSFORM_TOOL : unsigned char {
		ECS_TRANSFORM_TRANSLATION,
		ECS_TRANSFORM_ROTATION,
		ECS_TRANSFORM_SCALE,
		ECS_TRANSFORM_COUNT
	};

	enum ECS_TRANSFORM_TOOL_AXIS : unsigned char {
		ECS_TRANSFORM_AXIS_X,
		ECS_TRANSFORM_AXIS_Y,
		ECS_TRANSFORM_AXIS_Z,
		ECS_TRANSFORM_AXIS_COUNT
	};

	enum ECS_REFLECT ECS_TRANSFORM_SPACE : unsigned char {
		ECS_TRANSFORM_LOCAL_SPACE,
		ECS_TRANSFORM_WORLD_SPACE,
		ECS_TRANSFORM_SPACE_COUNT
	};

	struct TranslationToolDrag {
		ECS_INLINE void Initialize() {
			last_successful_direction.x = FLT_MAX;
			space = ECS_TRANSFORM_LOCAL_SPACE;
		}

		ECS_TRANSFORM_TOOL_AXIS axis;
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

		ECS_TRANSFORM_TOOL_AXIS axis;
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

		ECS_TRANSFORM_TOOL_AXIS axis;
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

		ECS_INLINE void SetAxis(ECS_TRANSFORM_TOOL_AXIS axis) {
			// All types have the first field the axis - it's fine to reference any
			translation.axis = axis;
		}

		ECS_INLINE void SetSpace(ECS_TRANSFORM_SPACE space) {
			// Both the rotation and the translation have as second field the space
			translation.space = space;
		}

		TranslationToolDrag translation;
		RotationToolDrag rotation;
		ScaleToolDrag scale;
	};

	ECS_INLINE ECS_TRANSFORM_SPACE InvertTransformSpace(ECS_TRANSFORM_SPACE space) {
		return space == ECS_TRANSFORM_LOCAL_SPACE ? ECS_TRANSFORM_WORLD_SPACE : ECS_TRANSFORM_LOCAL_SPACE;
	}

	// Returns { 1.0f, 0.0f, 0.0f } for X
	// Returns { 0.0f, 1.0f, 0.0f } for Y
	// Returns { 0.0f, 0.0f, 1.0f } for Z
	ECSENGINE_API float3 AxisDirection(ECS_TRANSFORM_TOOL_AXIS axis);

	// Rotates the AxisDirection by the given rotation
	ECSENGINE_API float3 ECS_VECTORCALL AxisDirectionLocal(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation);

	// Returns { 1.0f, 0.0f, 0.0f } for X
	// Returns { 0.0f, 1.0f, 0.0f } for Y
	// Returns { 0.0f, 0.0f, 1.0f } for Z
	ECSENGINE_API Vector8 ECS_VECTORCALL AxisDirectionSIMD(ECS_TRANSFORM_TOOL_AXIS axis);

	// If copy_low_to_high is enabled, then the low part of the quaternion is used for the high lane as well
	// Rotates the AxisDirection by the given rotation
	ECSENGINE_API Vector8 ECS_VECTORCALL AxisDirectionLocalSIMD(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation, bool copy_low_to_high = true);

	// Returns either the world or the local space axis according to the space given
	ECSENGINE_API float3 ECS_VECTORCALL AxisDirection(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation, ECS_TRANSFORM_SPACE space, bool copy_low_to_high = true);

	// If copy_low_to_high is enabled, then the low part of the quaternion is used for the high lane as well
	// Returns either the world or the local space axis according to the space given
	ECSENGINE_API Vector8 ECS_VECTORCALL AxisDirectionSIMD(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation, ECS_TRANSFORM_SPACE space, bool copy_low_to_high = true);

	// Returns one of the two possible planes for that axis
	// Can optionally offer an offset. The functions knows how to extract the correct offset
	// from the given float3
	// The plane is splatted on both the low and high
	ECSENGINE_API Plane ECS_VECTORCALL AxisPlane(ECS_TRANSFORM_TOOL_AXIS axis, float3 offset = float3::Splat(0.0f));

	// Returns the plane of the axis such that the view direction is not parallel to it
	// Can optionally offer an offset. The functions knows how to extract the correct offset
	// from the given float3. The view direction needs to be normalized
	// The plane is splatted on both the low and high
	ECSENGINE_API Plane ECS_VECTORCALL AxisPlane(ECS_TRANSFORM_TOOL_AXIS axis, Vector8 view_direction_normalized, float3 offset = float3::Splat(0.0f));

	// If copy_low_to_high is enabled, then the low part of the quaternion is used for the high lane as well
	ECSENGINE_API Plane ECS_VECTORCALL AxisPlaneLocal(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation, float3 offset, bool copy_low_to_high = true);

	// If copy_low_to_high is enabled, then the low part of the quaternion is used for the high lane as well
	ECSENGINE_API Plane ECS_VECTORCALL AxisPlaneLocal(
		ECS_TRANSFORM_TOOL_AXIS axis, 
		Vector8 view_direction_normalized, 
		Quaternion rotation, 
		float3 offset, 
		bool copy_low_to_high = true
	);

	ECSENGINE_API Plane ECS_VECTORCALL AxisPlane(
		ECS_TRANSFORM_TOOL_AXIS axis,
		Quaternion rotation,
		ECS_TRANSFORM_SPACE space,
		float3 offset,
		bool copy_low_to_high = true
	);

	ECSENGINE_API Plane ECS_VECTORCALL AxisPlane(
		ECS_TRANSFORM_TOOL_AXIS axis,
		Vector8 view_direction_normalized,
		Quaternion rotation,
		ECS_TRANSFORM_SPACE space,
		float3 offset,
		bool copy_low_to_high
	);

	struct TransformAxisInfo {
		Vector8 axis_direction;
		Plane plane;
	};

	ECSENGINE_API TransformAxisInfo ECS_VECTORCALL AxisInfo(
		ECS_TRANSFORM_TOOL_AXIS axis,
		Quaternion rotation, 
		ECS_TRANSFORM_SPACE space, 
		float3 plane_offset, 
		bool copy_low_to_high = true
	);

	ECSENGINE_API TransformAxisInfo ECS_VECTORCALL AxisInfo(
		ECS_TRANSFORM_TOOL_AXIS axis,
		Quaternion rotation,
		ECS_TRANSFORM_SPACE space,
		Vector8 plane_view_direction_normalized,
		float3 plane_offset,
		bool copy_low_to_high = true
	);

	// Converts the mouse position into [-1, 1] range if inside, else accordingly
	ECSENGINE_API float2 MouseToNDC(uint2 window_size, int2 mouse_texel_position);

	// Converts the mouse position into [-1, 1] range if inside, else accordingly
	ECSENGINE_API float2 MouseToNDC(float2 window_size, float2 mouse_texel_position);

	// Returns the direction of the ray that passes through the current cursor position
	// Mouse texel positions must be relative to the top left corner
	// At the moment this works only for perspective FOV cameras
	template<typename CameraType>
	ECSENGINE_API float3 MouseRayDirection(const CameraType* camera, uint2 window_size, int2 mouse_texel_position);

	// Returns the translation delta corresponding to the mouse delta
	// We need a point from the plane in order to form the plane
	// This is not a very reliable method, since some translations might be lost when the
	// plane intersection fails.
	// The rotation is taken into consideration only when the drag tool has the local space enabled
	template<typename CameraType>
	ECSENGINE_API float3 HandleTranslationToolDelta(
		const CameraType* camera,
		float3 plane_point,
		Quaternion object_rotation,
		ECS_TRANSFORM_TOOL_AXIS axis,
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
		Quaternion object_rotation,
		ECS_TRANSFORM_TOOL_AXIS axis,
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
		Quaternion object_rotation,
		TranslationToolDrag* drag_tool,
		float3 current_ray_direction
	);

	// Rotation needs to be specified in angles
	template<typename CameraType>
	ECSENGINE_API float3 HandleRotationToolDelta(
		const CameraType* camera,
		float3 rotation_center,
		Quaternion rotation_value,
		RotationToolDrag* drag_tool,
		uint2 window_size,
		int2 mouse_texel_position,
		float rotation_factor
	);

	template<typename CameraType>
	ECSENGINE_API QuaternionStorage HandleRotationToolDeltaCircleMapping(
		const CameraType* camera,
		float3 rotation_center,
		Quaternion rotation_value,
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
		Quaternion rotation_value,
		ScaleToolDrag* scale_tool,
		int2 mouse_texel_delta,
		float scale_factor
	);

}