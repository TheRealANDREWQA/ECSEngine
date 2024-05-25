#include "editorpch.h"

using namespace ECSEngine;

bool AutoDetectCompiler(AdditionStream<wchar_t> path) {
	// Use the default installation path
	// Start with the latest version of MSVC, which is 2022
	// And it is 64 bit only, located at C:\Program Files\Microsoft Visual Studio\2022

	// Then the other versions are in C:\Program Files (x86)\Microsoft Visual Studio\*
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 8);

	ECS_STACK_CAPACITY_STREAM(wchar_t, default_path, 512);
	default_path.CopyOther(L"C:\\Program Files\\Microsoft Visual Studio");

	ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, folders, 512);

	auto search_folder = [&]() {
		// The tier refers to the visual studio plans
		// Community, Professional and Enterprise at the current time
		// Return true if it found a tier and adds it to the path
		// Else false
		auto determine_visual_studio_tier = [](CapacityStream<wchar_t>& path) {
			const Stream<wchar_t> tiers[] = {
				L"Enterprise",
				L"Professional",
				L"Community"
			};

			if (path[path.size - 1] != ECS_OS_PATH_SEPARATOR) {
				path.AddAssert(ECS_OS_PATH_SEPARATOR);
			}

			unsigned int path_size = path.size;
			for (size_t index = 0; index < ECS_COUNTOF(tiers); index++) {
				path.size = path_size;
				path.AddStreamAssert(tiers[index]);
				if (ExistsFileOrFolder(path)) {
					return true;
				}
			}

			path.size = path_size;
			return false;
		};

		if (ExistsFileOrFolder(default_path)) {
			GetDirectoriesOrFilesOptions options;
			options.relative_root = default_path;
			bool success = GetDirectories(default_path, &stack_allocator, &folders, options);
			if (success) {
				unsigned int* folder_years = (unsigned int*)stack_allocator.Allocate(sizeof(unsigned int) * folders.size);
				// Consider only the directories that are numbers
				for (unsigned int index = 0; index < folders.size; index++) {
					bool success = false;
					int64_t year = ConvertCharactersToIntStrict(folders[index], success);
					if (success) {
						// Sanity Clamp
						if (year >= 2010 && year < 2100) {
							folder_years[index] = year;
						}
						else {
							folder_years[index] = 0;
						}
					}
					else {
						folder_years[index] = 0;
					}
				}

				InsertionSort<false>(folder_years, folders.size);

				if (folder_years[0] > 0) {
					// Use this version over 32 bit ones
					// Determine the tier after converting the int to chars
					default_path.Add(ECS_OS_PATH_SEPARATOR);
					unsigned int initial_path_size = default_path.size;
					for (size_t index = 0; index < folders.size; index++) {
						default_path.size = initial_path_size;
						ConvertIntToChars(default_path, folder_years[index]);
						if (determine_visual_studio_tier(default_path)) {
							default_path.AddAssert(L"\\MSBuild\\Current\\Bin");
							path.AddStream(default_path);
							return true;
						}
					}
				}
			}
		}

		return false;
	};
	
	if (search_folder()) {
		// x64 path was found
		return true;
	}

	// Search x96 path
	stack_allocator.Clear();
	folders.size = 0;
	default_path.CopyOther(L"C:\\Program Files (x86)\\Microsoft Visual Studio");
	return search_folder();
}