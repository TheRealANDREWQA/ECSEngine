#pragma once
#include "ContactManifolds.h"

struct ContactConstraintPoint {
	// Initial anchor vectors in world space
	//float3 world_anchor_A;
	//float3 world_anchor_B;

	// Local anchor relative to the center of mass
	float3 local_anchor_A;
	float3 local_anchor_B;
	float separation;
	float normal_impulse;
	// The tangent impulse needs to be broken down
	// In 2 separate components since the velocity
	// Can change directions and we want to precompute
	// The effective mass such that we can simplify the
	// Hot loop
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

struct EntityContact {
	Entity entity_A;
	Entity entity_B;
	ContactManifold manifold;
	float friction;
	float restitution;
};

struct Contact : public EntityContact {
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

// The center of masses need to be in world space
PHYSICS_API void AddContactConstraint(
	World* world,
	const EntityContact* entity_contact,
	float3 center_of_mass_A,
	float3 center_of_mass_B
);