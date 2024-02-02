#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "InternalStructures.h"

namespace ECSEngine {

	namespace Reflection {
		struct ReflectionManager;
		struct ReflectionType;
		struct ReflectionNestedFieldIndex;
	}

	ECSENGINE_API bool IsReflectionTypeComponent(const Reflection::ReflectionType* type);

	ECSENGINE_API bool IsReflectionTypeSharedComponent(const Reflection::ReflectionType* type);

	// Returns true if the type has the component tag but no IsShared function to distinguish between unique and shared
	ECSENGINE_API bool IsReflectionTypeMaybeComponent(const Reflection::ReflectionType* type);

	ECSENGINE_API bool IsReflectionTypeGlobalComponent(const Reflection::ReflectionType* type);

	ECSENGINE_API bool IsReflectionTypeLinkComponent(const Reflection::ReflectionType* type);

	ECSENGINE_API bool IsReflectionTypeComponentType(const Reflection::ReflectionType* type, ECS_COMPONENT_TYPE component_type);

	// Walks through the fields and returns the component buffer and optionally the index of the field that
	// corresponds to that buffer index
	// Example struct { int, Stream<>, Stream<>, int, Stream<> }
	// buffer_index: 0 -> 1; 1 -> 2, 2 -> 4
	// If the reflection_manager is specified, it will search deeply into the user defined types
	// That are referenced by this type
	ECSENGINE_API ComponentBuffer GetReflectionTypeRuntimeBufferIndex(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type, 
		unsigned int buffer_index, 
		Reflection::ReflectionNestedFieldIndex* field_index = nullptr
	);

	// Determines all the buffers that the ECS runtime can use
	// If the reflection_manager is specified, it will search deeply into the user defined types
	// That are referenced by this type
	ECSENGINE_API void GetReflectionTypeRuntimeBuffers(
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* type, 
		CapacityStream<ComponentBuffer>& component_buffers
	);

	// It needs the stack memory to write some data
	// It builds default functions to handle Streams and DataPointers
	// If the reflection_manager is specified, it will search deeply into the user defined types
	// That are referenced by this type
	ECSENGINE_API ComponentFunctions GetReflectionTypeRuntimeComponentFunctions(
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* type, 
		CapacityStream<void>* stack_memory
	);

	// It needs the allocator to write some data for the functions to use
	// It builds default functions to handle Streams and DataPointers
	// If the reflection_manager is specified, it will search deeply into the user defined types
	// That are referenced by this type
	ECSENGINE_API ComponentFunctions GetReflectionTypeRuntimeComponentFunctions(
		const Reflection::ReflectionManager* reflection_manager, 
		const Reflection::ReflectionType* type, 
		AllocatorPolymorphic allocator
	);

	// It needs the stack memory to write some data
	// It builds default functions to handle reflectable types - in case it is
	// Blittable, it won't return a function, it will let the runtime use the default memcmp
	ECSENGINE_API SharedComponentCompareEntry GetReflectionTypeRuntimeCompareEntry(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type, 
		CapacityStream<void>* stack_memory
	);

	// It needs the allocator to write some data for the function to use
	// It builds default functions to handle reflectable types - in case it is
	// Blittable, it won't return a function, it will let the runtime use the default memcmp
	ECSENGINE_API SharedComponentCompareEntry GetReflectionTypeRuntimeCompareEntry(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		AllocatorPolymorphic allocator
	);

	ECSENGINE_API Component GetReflectionTypeComponent(const Reflection::ReflectionType* type);

	// If the hierarchy index is -1 it will search through all types, regardless of folders
	ECSENGINE_API void GetHierarchyComponentTypes(
		const Reflection::ReflectionManager* reflection_manager, 
		unsigned int hierarchy_index, 
		CapacityStream<unsigned int>* unique_indices, 
		CapacityStream<unsigned int>* shared_indices,
		CapacityStream<unsigned int>* global_indices
	);

	// Returns 0 if there is no function specified
	ECSENGINE_API size_t GetReflectionComponentAllocatorSize(const Reflection::ReflectionType* type);

	// Returns ECS_COMPONENT_TYPE_COUNT if it is not a component
	ECSENGINE_API ECS_COMPONENT_TYPE GetReflectionTypeComponentType(const Reflection::ReflectionType* type);

	enum ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT : unsigned char {
		ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_VALID = 0,
		ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_NOT_A_COMPONENT = 1 << 0,
		ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ID_FUNCTION = 1 << 1,
		ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_IS_SHARED_FUNCTION = 1 << 2,
		ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ALLOCATOR_SIZE_FUNCTION = 1 << 3,
		ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_NO_BUFFERS_BUT_ALLOCATOR_SIZE_FUNCTION = 1 << 4
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT);

	ECSENGINE_API bool HasReflectionTypeComponentBuffers(const Reflection::ReflectionType* type);

	// Determines if the reflection type as a component respects all the necessary features
	ECSENGINE_API ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT ValidateReflectionTypeComponent(const Reflection::ReflectionType* type);

}