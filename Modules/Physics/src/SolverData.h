#pragma once
#include "ECSEngineEntities.h"

#include "ContactConstraint.h"
#include "Islands.h"

#define SOLVER_DATA_STRING "SolverData"

typedef ECSEngine::HashTable<ContactConstraint, ECSEngine::EntityPair, ECSEngine::HashFunctionPowerOfTwo> ContactTable;

struct SolverData {
	ECS_INLINE void SetTimeStepTick(float value) {
		time_step_tick = value;
		inverse_time_step_tick = 1.0f / time_step_tick;
	}

	unsigned int iterations;
	float baumgarte_factor;
	float linear_slop;
	// Expressed in seconds, it tells the simulation
	// At which rate to update itself
	float time_step_tick;
	float inverse_time_step_tick;
	float previous_time_step_remainder;
	bool use_warm_starting;

	ECSEngine::MemoryManager allocator;
	ContactTable contact_table;
};