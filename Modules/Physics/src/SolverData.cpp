#include "pch.h"
#include "SolverData.h"

void SolverData::Initialize(MemoryManager* backup_allocator) {
	allocator = MemoryManager(ECS_MB, ECS_KB * 4, ECS_MB * 20, backup_allocator);
	contact_table.Initialize(&allocator, 128);

	iterations = 4;
	baumgarte_factor = 0.05f;
	linear_slop = 0.01f;
	use_warm_starting = true;
	previous_time_step_remainder = 0.0f;
	// Use a default of 60Hz update rate
	SetTimeStepTick(1.0f / 60.0f);

	island_manager.Initialize(backup_allocator);
}