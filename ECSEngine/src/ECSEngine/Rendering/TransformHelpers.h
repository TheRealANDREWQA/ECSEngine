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
	ECSENGINE_API float3 AxisDirection(ECS_AXIS axis);

	// Rotates the AxisDirection by the given rotation
	ECSENGINE_API float3 AxisDirectionLocal(ECS_AXIS axis, QuaternionScalar rotation);

	// Returns either the world or the local space axis according to the space given
	ECSENGINE_API float3 AxisDirection(ECS_AXIS axis, QuaternionScalar rotation, ECS_TRANSFORM_SPACE space);

	// Returns one of the two possible planes for that axis
	// Can optionally offer an offset. The functions knows how to extract the correct offset
	// from the given float3
	ECSENGINE_API PlaneScalar AxisPlane(ECS_AXIS axis, float3 offset);

	// Returns the plane of the axis such that the view direction is not parallel to it
	// Can optionally offer an offset. The functions knows how to extract the correct offset
	// from the given float3. The view direction needs to be normalized
	ECSENGINE_API PlaneScalar AxisPlane(ECS_AXIS axis, float3 view_direction_normalized, float3 offset);

	ECSENGINE_API PlaneScalar AxisPlaneLocal(ECS_AXIS axis, QuaternionScalar rotation, float3 offset);

	ECSENGINE_API PlaneScalar AxisPlaneLocal(
		ECS_AXIS axis,
		float3 view_direction_normalized,
		QuaternionScalar rotation,
		float3 offset
	);

	ECSENGINE_API PlaneScalar AxisPlane(
		ECS_AXIS axis,
		QuaternionScalar rotation,
		ECS_TRANSFORM_SPACE space,
		float3 offset
	);

	ECSENGINE_API PlaneScalar AxisPlane(
		ECS_AXIS axis,
		float3 view_direction_normalized,
		QuaternionScalar rotation,
		ECS_TRANSFORM_SPACE space,
		float3 offset
	);

	struct TransformAxisInfo {
		float3 axis_direction;
		PlaneScalar plane;
	};

	ECSENGINE_API TransformAxisInfo AxisInfo(
		ECS_AXIS axis,
		QuaternionScalar rotation,
		ECS_TRANSFORM_SPACE space,
		float3 plane_offset
	);

	ECSENGINE_API TransformAxisInfo AxisInfo(
		ECS_AXIS axis,
		QuaternionScalar rotation,
		ECS_TRANSFORM_SPACE space,
		float3 plane_view_direction_normalized,
		float3 plane_offset
	);

}