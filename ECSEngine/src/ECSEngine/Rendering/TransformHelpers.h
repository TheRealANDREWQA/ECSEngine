// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Math/Plane.h"
#include "../ECS/InternalStructures.h"

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

	ECS_INLINE ECS_TRANSFORM_SPACE InvertTransformSpace(ECS_TRANSFORM_SPACE space) {
		return space == ECS_TRANSFORM_LOCAL_SPACE ? ECS_TRANSFORM_WORLD_SPACE : ECS_TRANSFORM_LOCAL_SPACE;
	}

	ECSENGINE_API Component TransformToolComponentID(ECS_TRANSFORM_TOOL tool);

	ECSENGINE_API Stream<char> TransformToolComponentName(ECS_TRANSFORM_TOOL tool);

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

}