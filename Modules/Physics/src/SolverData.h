#pragma once
#include "ECSEngineEntities.h"

#include "ContactConstraint.h"
#include "Islands.h"

#define SOLVER_DATA_STRING "SolverData"

// Store pointers, since this will reduce the table size, which helps with resizings,
// But also to have referential stability, which can be used to accelerate other structures
typedef HashTable<ContactConstraint*, EntityPair, HashFunctionPowerOfTwo> ContactTable;

struct PHYSICS_API SolverData {
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
	float time_step_tick;
	float inverse_time_step_tick;
	float previous_time_step_remainder;
	bool use_warm_starting;

	MemoryManager allocator;
	ContactTable contact_table;

	IslandManager island_manager;
};