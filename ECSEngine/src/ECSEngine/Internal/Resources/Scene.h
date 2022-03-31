#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "ResourceTypes.h"
#include "../../Rendering/RenderingStructures.h"
#include "../World.h"

namespace ECSEngine {

	struct SceneResources {
		Stream<Stream<wchar_t>> resource_paths;
		Stream<Stream<wchar_t>> modules;
		Stream<Stream<wchar_t>> module_settings;
		Stream<wchar_t> project_settings;
		Stream<wchar_t> entity_data;
	};

	// It will not read the whole file into a buffer. Since this file can get big, it will just read the data directly into the 
	// corresponding managers
	ECSENGINE_API bool LoadScene(Stream<wchar_t> file, World* world);

	ECSENGINE_API void LoadScene(uintptr_t& buffer, World* world);

	ECSENGINE_API bool SaveScene(Stream<wchar_t> file, const SceneResources* resources);

	ECSENGINE_API void SaveScene(uintptr_t& buffer, const SceneResources* resources);

}