#include "pch.h"
#include "SolverData.h"
#include "Settings.h"

void SolverData::Initialize(MemoryManager* backup_allocator) {
	*this = SolverData();

	PhysicsSettings default_settings;
	iterations = default_settings.iterations;
	baumgarte_factor = default_settings.baumgarte_factor;
	use_warm_starting = default_settings.use_warm_starting;

	allocator = MemoryManager(ECS_MB * 4, ECS_KB * 4, ECS_MB * 20, backup_allocator);
	contact_table.Initialize(&allocator, 128);

	// Use a default of 60Hz update rate
	SetTimeStepTick(1.0f / 60.0f);

	island_manager.Initialize(&allocator);
}