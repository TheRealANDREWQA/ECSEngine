#pragma once
#include "ECSEngineModule.h"
#include "ECSEngineEntities.h"

struct BroadphaseCollisionHandlerFunctionData {
	Entity first_entity;
	Entity second_entity;
	unsigned char first_layer;
	unsigned char second_layer;
};

typedef void (*BroadphaseCollisionHandlerFunction)(unsigned int thread_id, World* world, const BroadphaseCollisionHandlerFunctionData* handler_data, void* user_data);

struct BroadphaseCollisionHandler {
	BroadphaseCollisionHandlerFunction function;
	void* data;
};

// SoA array such that we can lookup fast the handler for a corresponding layer
struct BroadphaseCollisionHandlerLayerMapping {
	unsigned char* layers;
	BroadphaseCollisionHandler* handlers;
	unsigned int size;
	unsigned int capacity;
};

void SetBroadphaseTasks(ECSEngine::ModuleTaskFunctionData* data);

void SetBroadphaseDebugTasks(ECSEngine::ModuleRegisterDebugDrawTaskElementsData* data);