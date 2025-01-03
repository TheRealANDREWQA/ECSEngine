#include "pch.h"
#include "SolverData.h"

void SolverData::Initialize(MemoryManager* backup_allocator) {
	*this = SolverData();

	allocator = MemoryManager(ECS_MB, ECS_KB * 4, ECS_MB * 20, backup_allocator);
	contact_table.Initialize(&allocator, 128);

	// Use a default of 60Hz update rate
	SetTimeStepTick(1.0f / 60.0f);

	island_manager.Initialize(backup_allocator);
}