#include "pch.h"
#include "Debugging.h"
#include "SolverData.h"
#include "ECSEngineWorld.h"

#define DEBUG_DATA_STRING "PHYSICS_DEBUG_DATA"

struct DebugData {
	bool should_draw;
};

template<bool schedule_element>
ECS_THREAD_TASK(DebugDrawPhysicsIslands) {
	
}

void SetPhysicsIslandDraw(World* world, bool set) {
	DebugData* data = (DebugData*)world->system_manager->TryGetData(DEBUG_DATA_STRING);
	if (data != nullptr) {
		data->should_draw = set;
	}
	else {
		DebugData insert_data;
		insert_data.should_draw = set;
		world->system_manager->BindData(DEBUG_DATA_STRING, &insert_data, sizeof(insert_data));
	}
}