// ECS_REFLECT
#pragma once
#include "ECSEngineBasics.h"
#include "ECSEngineMath.h"
#include "Export.h"
#include "CollisionDetection/src/ConvexHull.h"
#include "PhysicsComponents.h"

using namespace ECSEngine;

namespace ECSEngine {

	struct ModuleRegisterComponentFunctionsData;

}

struct ECS_REFLECT_COMPONENT Rigidbody {
	constexpr inline static short ID() {
		return PHYSICS_COMPONENT_BASE + 0;
	}

	constexpr inline static bool IsShared() {
		return false;
	}

	float mass_inverse;
	Matrix3x3 inertia_tensor_inverse;
	Matrix3x3 world_space_inertia_tensor_inverse; ECS_SERIALIZATION_OMIT_FIELD
	float3 center_of_mass;
	float3 velocity;
	float3 angular_velocity;
	bool is_static;
	float friction;
};

ECS_INLINE float3 ComputeVelocity(float3 linear_velocity, float3 angular_velocity, float3 local_anchor) {
	return linear_velocity + Cross(angular_velocity, local_anchor);
}

ECS_INLINE float3 ComputeVelocity(const Rigidbody* rigidbody, float3 local_anchor) {
	return ComputeVelocity(rigidbody->velocity, rigidbody->angular_velocity, local_anchor);
}

// Applies the modifications to the given values, instead of the ones from the rigidbody
PHYSICS_API void ApplyImpulse(float3& velocity, float3& angular_velocity, const Rigidbody* rigidbody, float3 local_anchor, float3 impulse);

ECS_INLINE void ApplyImpulse(Rigidbody* rigidbody, float3 local_anchor, float3 impulse) {
	ApplyImpulse(rigidbody->velocity, rigidbody->angular_velocity, rigidbody, local_anchor, impulse);
}


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

void AddRigidbodyBuildEntry(ECSEngine::ModuleRegisterComponentFunctionsData* data);