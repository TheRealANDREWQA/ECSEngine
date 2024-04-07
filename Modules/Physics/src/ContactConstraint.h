#pragma once
#include "ContactManifolds.h"

struct ContactConstraintPoint {
	// Initial anchor vectors in world space
	//float3 world_anchor_A;
	//float3 world_anchor_B;

	// Local anchor relative to the center of mass
	// The tangent impulse needs to be broken down
	// In 2 separate components since the velocity
	// Can change directions and we want to precompute
	// The effective mass such that we can simplify the
	// Hot loop
	float3 local_anchor_A;
	float3 local_anchor_B;
	float separation;
	float normal_impulse;
	float tangent_impulse_1;
	float tangent_impulse_2;
	float normal_mass;
	float tangent_mass_1;
	float tangent_mass_2;
	//float mass_coefficient;
	//float bias_coefficient;
	//float impulse_coefficient;
	//bool friction_valid;
};

struct Contact {
	Entity entity_A;
	Entity entity_B;
	ContactManifold manifold;
	float friction;
	float restitution;
	float3 tangent_1;
	float3 tangent_2;
};

struct Rigidbody;

struct ContactConstraint {
	const Contact* contact;
	ContactConstraintPoint points[4];
	// These 2 are in world space
	float3 center_of_mass_A;
	float3 center_of_mass_B;
	// These are cached during a precompute step
	Rigidbody* rigidbody_A;
	Rigidbody* rigidbody_B;
};

ECS_THREAD_TASK(SolveContactConstraints);

void AddSolverTasks(ModuleTaskFunctionData* data);

// TODO: Should we allow such functionality? At the moment
// Disable it such as to not complicate the implementation needlessly

// The center of masses need to be in world space
// This function is mainly intended in case you want to have
// Rigidbodies outside the ECS runtime
//PHYSICS_API void AddContactConstraint(
//	World* world, 
//	const Contact* contact, 
//	Rigidbody* rigidbody_A, 
//	Rigidbody* rigidbody_B,
//	float3 center_of_mass_A,
//	float3 center_of_mass_B
//);

// The center of masses need to be in world space
PHYSICS_API void AddContactConstraint(
	World* world,
	const Contact* contact,
	Entity entity_A,
	Entity entity_B,
	float3 center_of_mass_A,
	float3 center_of_mass_B
);