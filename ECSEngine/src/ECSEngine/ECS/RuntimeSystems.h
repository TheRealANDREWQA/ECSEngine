#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	struct TaskScheduler;
	struct World;
	namespace Reflection {
		struct ReflectionManager;
	}

	struct ECSENGINE_API RegisterECSRuntimeSystemsOptions {
		RegisterECSRuntimeSystemsOptions Copy(AllocatorPolymorphic allocator) const;

		void Deallocate(AllocatorPolymorphic allocator);

		// All of the fields here are optional. If a system requires a certain field but it was not set
		// In the beginning, then a crash will be generated (to indicate to you that you should set that field)

		// Can be used by certain systems to obtain reflection capabilities for general reflection operations or serialization/UI
		const Reflection::ReflectionManager* reflection_manager = nullptr;
		// The path of the current opened project/current executable path. Can be used to write some files by the systems (temporary or not)
		Stream<wchar_t> project_path = {};
	};

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void RegisterRuntimeMonitoredFloat(const World* world, float value);

	// The largest integer is used, but any type of integer can be set here
	ECSENGINE_API void RegisterRuntimeMonitoredInt(const World* world, int64_t value);

	ECSENGINE_API void RegisterRuntimeMonitoredDouble(const World* world, double value);

	ECSENGINE_API void RegisterRuntimeMonitoredStruct(const World* world, Stream<char> reflection_type_name, const void* struct_data);

	// Basic values include normal floats/doubles/integers and basic types of these with 2, 3 or 4 components
	template<typename BasicType>
	ECS_INLINE void RegisterRuntimeMonitoredBasicValue(const World* world, BasicType value) {
		BasicTypeAction<void>(value, [&](auto value) {
			if constexpr (std::is_same_v<decltype(value), float>) {
				RegisterRuntimeMonitoredFloat(world, value);
			}
			else if constexpr (std::is_same_v<decltype(value), double>) {
				RegisterRuntimeMonitoredDouble(world, value);
			}
			else {
				RegisterRuntimeMonitoredInt(world, (int64_t)value);
			}
		});
	}

	// If you want to have multiple recordings at the same time, you can use a unique file index to distinguish between them
	ECSENGINE_API void SetWriteRuntimeMonitoredStructFileIndex(const World* world, unsigned int file_index);

	// -----------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float GetRuntimeMonitoredFloat(const World* world);

	// The largest integer is used, but you should cast to the appropriate integer type
	ECSENGINE_API int64_t GetRuntimeMonitoredInt(const World* world);

	ECSENGINE_API double GetRuntimeMonitoredDouble(const World* world);

	ECSENGINE_API void GetRuntimeMonitoredStruct(const World* world, Stream<char> reflection_type_name, void* struct_data, AllocatorPolymorphic field_allocator);

	// Basic values include normal floats/doubles/integers/booleans and basic types of these with 2, 3 or 4 components
	template<typename BasicType>
	ECS_INLINE BasicType GetRuntimeMonitoredBasicValue(const World* world) {
		// Use a dummy for the value parameter
		return BasicTypeAction<BasicType, BasicType>(BasicType(), [&](auto value) {
			if constexpr (std::is_same_v<decltype(value), float>) {
				return GetRuntimeMonitoredFloat(world);
			}
			else if constexpr (std::is_same_v<decltype(value), double>) {
				return GetRuntimeMonitoredDouble(world);
			}
			else {
				return (decltype(value))GetRuntimeMonitoredInt(world);
			}
		});
	}

	// If you want to have multiple recordings at the same time, you can use a unique file index to distinguish between them
	ECSENGINE_API void SetReadRuntimeMonitoredStructFileIndex(const World* world, unsigned int file_index);

	// -----------------------------------------------------------------------------------------------------------------------------

	// Adds to the scheduler some default ECS helper runtime systems. These systems provide some default functionality
	// That might be desirable during development or shared across many projects. 
	ECSENGINE_API void RegisterECSRuntimeSystems(TaskScheduler* task_scheduler, const RegisterECSRuntimeSystemsOptions& options = {});

	// Should be called when the simulation for the world has finished to perform some final operations and free any allocations that were made
	ECSENGINE_API void UnregisterECSRuntimeSystems(World* world);
	
	// -----------------------------------------------------------------------------------------------------------------------------

}