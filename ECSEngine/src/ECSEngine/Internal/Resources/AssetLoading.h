#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Containers/HashTable.h"
#include "ResourceTypes.h"

namespace ECSEngine {

	struct Graphics;

	struct ECSENGINE_API AssetLoading {

		void AddTexture(Stream<wchar_t> path, Stream<wchar_t> packed_file = { nullptr, 0 });

		void AddMesh(Stream<wchar_t> path, Stream<wchar_t> packed_file = { nullptr, 0 });

		//Stream<HashTableDefault<void*>> 
	};

}