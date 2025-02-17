// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineEntities.h"
#include "PhysicsComponents.h"
#include "ContactConstraint.h"
#include "Islands.h"

// Store pointers, since this will reduce the table size, which helps with resizings,
// But also to have referential stability, which can be used to accelerate other structures
ECS_REFLECT typedef HashTable<ContactConstraint*, EntityPair, HashFunctionPowerOfTwo> ContactTable;

struct PHYSICS_API ECS_REFLECT_GLOBAL_COMPONENT_PRIVATE SolverData {
	ECS_INLINE constexpr static short ID() {
		return PHYSICS_GLOBAL_COMPONENT_BASE + 1;
	}

	ECS_INLINE void SetTimeStepTick(float value) {
		time_step_tick = value;
		inverse_time_step_tick = 1.0f / time_step_tick;
	}

	void Initialize(MemoryManager* backup_allocator);

	unsigned int iterations;
	float baumgarte_factor;
	float linear_slop;
	// Expressed in seconds, it tells the simulation
	// At which rate to update itself
	[[ECS_UI_OMIT_FIELD_REFLECT]]
	float time_step_tick;
	[[ECS_UI_OMIT_FIELD_REFLECT]]
	float inverse_time_step_tick;
	[[ECS_UI_OMIT_FIELD_REFLECT]]
	float previous_time_step_remainder = 0.0f;
	bool use_warm_starting = true;

	[[ECS_MAIN_ALLOCATOR]]
	MemoryManager allocator;
	[[ECS_POINTER_KEY_REFERENCE_TARGET(ContactConstraint, ECS_CUSTOM_TYPE_ELEMENT(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE))]]
	ContactTable contact_table;

	IslandManager island_manager;
};