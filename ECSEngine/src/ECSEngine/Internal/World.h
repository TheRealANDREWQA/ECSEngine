//ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "EntityManager.h"
#include "Multithreading/TaskManager.h"
#include "../Rendering/Graphics.h"
#include "../Internal/EntityManager.h"
#include "../Internal/InternalStructures.h"
#include "../Internal/Resources/ResourceManager.h"
#include "../Utilities/Mouse.h"
#include "../Utilities/Keyboard.h"
#include "SystemManager.h"

namespace ECSEngine {

	struct ECS_REFLECT WorldDescriptor {
		Graphics* ECS_OMIT_FIELD_REFLECT(8) graphics;
		HID::Mouse* ECS_OMIT_FIELD_REFLECT(8) mouse;
		HID::Keyboard* ECS_OMIT_FIELD_REFLECT(8) keyboard;
		size_t global_memory_size; 
		size_t global_memory_pool_count;
		size_t global_memory_new_allocation_size;
		size_t entity_manager_memory_size;
		size_t entity_manager_memory_pool_count;
		size_t entity_manager_memory_new_allocation_size;
		unsigned int thread_count;
		unsigned int entity_pool_power_of_two;
	};

	struct ECSENGINE_API World
	{
		World();
		World(
			GlobalMemoryManager* _memory, 
			EntityManager* _entity_manager, 
			ResourceManager* _resource_manager, 
			TaskManager* _task_manager,
			HID::Mouse* mouse,
			HID::Keyboard* keyboard
		);
		World(const WorldDescriptor& descriptor);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(World);

		GlobalMemoryManager* memory;
		ResourceManager* resource_manager;
		TaskManager* task_manager;
		EntityManager* entity_manager;
		Graphics* graphics;
		SystemManager* system_manager;
		HID::Mouse* mouse;
		HID::Keyboard* keyboard;
	};

	ECSENGINE_API void DestroyWorld(World* world);

}

