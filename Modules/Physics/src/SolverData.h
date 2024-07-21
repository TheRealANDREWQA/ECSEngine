#pragma once
#include "ECSEngineEntities.h"

#include "ContactConstraint.h"
#include "Islands.h"

#define SOLVER_DATA_STRING "SolverData"

struct ContactPair {
	ECS_INLINE bool operator == (ContactPair other) const {
		return (first == other.first && second == other.second) || (first == other.second && second == other.first);
	}

	ECS_INLINE unsigned int Hash() const {
		// Use CantorPair hashing such that reversed pairs will be considered the same
		return ECSEngine::CantorPair(first.value, second.value);
	}

	ECSEngine::Entity first;
	ECSEngine::Entity second;
};

typedef ECSEngine::HashTable<ContactConstraint, ContactPair, ECSEngine::HashFunctionPowerOfTwo> ContactTable;

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