#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "InternalStructures.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
	}

	ECSENGINE_API bool IsReflectionTypeComponent(const Reflection::ReflectionType* type);

	ECSENGINE_API bool IsReflectionTypeSharedComponent(const Reflection::ReflectionType* type);

	ECSENGINE_API bool IsReflectionTypeGlobalComponent(const Reflection::ReflectionType* type);

	ECSENGINE_API bool IsReflectionTypeLinkComponent(const Reflection::ReflectionType* type);

	// Walks through the fields and returns the component buffer and optionally the index of the field that
	// corresponds to that buffer index
	// Example struct { int, Stream<>, Stream<>, int, Stream<> }
	// buffer_index: 0 -> 1; 1 -> 2, 2 -> 4
	ECSENGINE_API ComponentBuffer GetReflectionTypeRuntimeBufferIndex(
		const Reflection::ReflectionType* type, 
		unsigned int buffer_index, 
		unsigned int* field_index = nullptr
	);

	// Determines all the buffers that the ECS runtime can use
	ECSENGINE_API void GetReflectionTypeRuntimeBuffers(const Reflection::ReflectionType* type, CapacityStream<ComponentBuffer>& component_buffers);

	ECSENGINE_API Component GetReflectionTypeComponent(const Reflection::ReflectionType* type);

	// If the hierarchy index is -1 it will search through all types, regardless of folders
	ECSENGINE_API void GetHierarchyComponentTypes(
		const Reflection::ReflectionManager* reflection_manager, 
		unsigned int hierarchy_index, 
		CapacityStream<unsigned int>* unique_indices, 
		CapacityStream<unsigned int>* shared_indices
	);

}