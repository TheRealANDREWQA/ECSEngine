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

	enum ECS_TRANSFORM_SPACE : unsigned char {
		ECS_TRANSFORM_LOCAL_SPACE,
		ECS_TRANSFORM_WORLD_SPACE,
		ECS_TRANSFORM_SPACE_COUNT
	};

	struct TranslationToolDrag {
		ECS_INLINE void Initialize() {
			last_successful_direction.x = FLT_MAX;
		}

		ECS_TRANSFORM_TOOL_AXIS axis;
		float3 last_successful_direction;
	};

	struct RotationToolDrag {
		ECS_INLINE RotationToolDrag() {}

		ECS_INLINE void InitializeRoller() {
			projected_direction.x = FLT_MAX;
			last_distance = FLT_MAX;
		}

		ECS_INLINE void InitializeCircle() {
			rotation_center_ndc.x = FLT_MAX;
			last_circle_direction.x = FLT_MAX;
		}

		ECS_TRANSFORM_TOOL_AXIS axis;
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
		// We need these such that we know what sign we should apply to the delta
		float2 projected_direction_sign;
	};

	// Returns { 1.0f, 0.0f, 0.0f } for X
	// Returns { 0.0f, 1.0f, 0.0f } for Y
	// Returns { 0.0f, 0.0f, 1.0f } for Z
	ECSENGINE_API float3 AxisDirection(ECS_TRANSFORM_TOOL_AXIS axis);

	// Rotates the AxisDirection by the given rotation
	ECSENGINE_API float3 LocalAxisDirection(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation);

	// Returns one of the two possible planes for that axis
	// Can optionally offer an offset. The functions knows how to extract the correct offset
	// from the given float3
	// The plane is splatted on both the low and high
	ECSENGINE_API Plane AxisPlane(ECS_TRANSFORM_TOOL_AXIS axis, float3 offset = float3::Splat(0.0f));

	// Returns the plane of the axis such that the view direction is not parallel to it
	// Can optionally offer an offset. The functions knows how to extract the correct offset
	// from the given float3. The view direction needs to be normalized
	// The plane is splatted on both the low and high
	ECSENGINE_API Plane AxisPlane(ECS_TRANSFORM_TOOL_AXIS axis, Vector8 view_direction_normalized, float3 offset = float3::Splat(0.0f));

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
	// plane intersection fails
	template<typename CameraType>
	ECSENGINE_API float3 HandleTranslationToolDelta(
		const CameraType* camera,
		float3 plane_point,
		ECS_TRANSFORM_TOOL_AXIS axis,
		uint2 window_size,
		int2 mouse_texel_position,
		int2 mouse_delta
	);

	// Both previous ray direction and current ray direction need to be normalized
	template<typename CameraType>
	ECSENGINE_API float3 HandleTranslationToolDelta(
		const CameraType* camera,
		float3 plane_point,
		ECS_TRANSFORM_TOOL_AXIS axis,
		float3 previous_ray_direction,
		float3 current_ray_direction,
		bool2* success_status = nullptr
	);

	template<typename CameraType>
	ECSENGINE_API float3 HandleTranslationToolDelta(
		const CameraType* camera,
		float3 plane_point,
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
	ECSENGINE_API float4 HandleRotationToolDeltaCircleMapping(
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