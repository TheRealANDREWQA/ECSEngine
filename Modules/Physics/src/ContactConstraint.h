#pragma once
#include "ContactManifolds.h"

struct ContactConstraintPoint {
	// initial anchor vectors in world space
	float3 world_anchor_A;
	float3 world_anchor_B;

	// local anchor relative center of mass
	float3 local_anchor_A;
	float3 local_anchor_B;
	float separation;
	float normal_impulse;
	float tangent_impulse;
	float normal_mass;
	float tangent_mass;
	//float mass_coefficient;
	//float bias_coefficient;
	//float impulse_coefficient;
	//bool frictionValid;
};

struct Contact {
	Entity first_entity;
	Entity second_entity;
	ContactManifold manifold;
	float friction;
	float restitution;
};

struct ContactConstraint {
	const Contact* contact;
	float3 normal;
	ContactConstraintPoint points[4];
};