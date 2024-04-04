// ECS_REFLECT
#pragma once
#include "ECSEngineBasics.h"
#include "ECSEngineMath.h"
#include "Export.h"
#include "CollisionDetection/src/ConvexHull.h"

using namespace ECSEngine;

struct ECS_REFLECT_COMPONENT Rigidbody {
	constexpr inline static short ID() {
		return 400;
	}

	constexpr inline static bool IsShared() {
		return false;
	}

	float mass_inverse;
	Matrix3x3 inertia_tensor_inverse;
	float3 center_of_mass;
	float3 velocity;
	float3 angular_velocity;
};

// The triangles must respect the CCW winding order
// The mass and the center of mass can be computed as well, 
// but these are optional out parameters. The mass is computed
// As if the mesh has a constant density of 1
PHYSICS_API Matrix3x3 ComputeInertiaTensor(
	Stream<float3> points, 
	Stream<uint3> triangle_indices, 
	float density = 1.0f,
	float* output_mass = nullptr, 
	float3* output_center_of_mass = nullptr
);

// For SoA layout
// The triangles must respect the CCW winding order
// The mass and the center of mass can be computed as well, 
// but these are optional out parameters.
PHYSICS_API Matrix3x3 ComputeInertiaTensor(
	const float* points_x, 
	const float* points_y, 
	const float* points_z, 
	size_t count,
	Stream<uint3> triangle_indices,
	float density = 1.0f,
	float* output_mass = nullptr,
	float3* output_center_of_mass = nullptr
);

// VERY IMPORTANT: The convex hull must have only triangle faces!
// The mass and the center of mass can be computed as well, 
// but these are optional out parameters.
PHYSICS_API Matrix3x3 ComputeInertiaTensor(
	const ConvexHull* convex_hull,
	float density = 1.0f,
	float* output_mass = nullptr,
	float3* output_center_of_mass = nullptr
);

// The triangles must respect the CCW winding order
// The mass and the center of mass can be computed as well, 
// but these are optional out parameters.
PHYSICS_API Matrix3x3 ComputeInertiaTensor(
	size_t triangle_count,
	void* functor_data,
	float3 (*GetPointFunction)(void*, unsigned int),
	uint3 (*GetTriangleFunction)(void*, unsigned int),
	float density = 1.0f,
	float* output_mass = nullptr,
	float3* output_center_of_mass = nullptr
);