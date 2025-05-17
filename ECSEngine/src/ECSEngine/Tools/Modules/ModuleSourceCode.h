// ECS_REFLECT
#pragma once
#include "../../Containers/Stream.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	struct ECS_REFLECT ModuleSourceCode {
		// Deallocates each individual field
		void Deallocate(AllocatorPolymorphic allocator) {
			solution_path.Deallocate(allocator);
			library_name.Deallocate(allocator);
			branch_name.Deallocate(allocator);
			commit_hash.Deallocate(allocator);
			configuration.Deallocate(allocator);
		}

		Stream<wchar_t> solution_path;
		Stream<wchar_t> library_name;
		// These fields are added to help identify the source code for the file
		// In case they match the main repository, these can be left empty.
		Stream<char> branch_name;
		Stream<char> commit_hash;
		// This is the configuration used for the module, i.e. Debug/Release/Distribution
		Stream<char> configuration;
	};

	struct ECS_REFLECT ModulesSourceCode {
		Stream<ModuleSourceCode> values;
	};

}