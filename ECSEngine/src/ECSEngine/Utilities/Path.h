#pragma once
#include "../Containers/Stream.h"

#define ECS_OS_PATH_SEPARATOR L'\\'
#define ECS_OS_PATH_SEPARATOR_ASCII '\\'

ECS_CONTAINERS;

namespace ECSEngine {

	using Path = Stream<wchar_t>;
	using ASCIIPath = Stream<char>;

	namespace function {

		// -------------------------------------------------------------------------------------------------

		ECSENGINE_API Path PathParent(Path path);

		ECSENGINE_API ASCIIPath PathParent(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// Includes the dot
		ECSENGINE_API size_t PathExtensionSize(Path path);

		// Includes the dot
		ECSENGINE_API size_t PathExtensionSize(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		ECSENGINE_API Path PathExtension(Path path);

		ECSENGINE_API ASCIIPath PathExtension(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		ECSENGINE_API Path PathStem(Path path);

		ECSENGINE_API ASCIIPath PathStem(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		ECSENGINE_API Path PathFilename(Path path);

		ECSENGINE_API ASCIIPath PathFilename(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		ECSENGINE_API Path PathRelativeTo(Path path, Path reference);

		ECSENGINE_API ASCIIPath PathRelativeTo(ASCIIPath path, ASCIIPath reference);

		// -------------------------------------------------------------------------------------------------

		ECSENGINE_API size_t PathParentSize(Path path);

		ECSENGINE_API size_t PathParentSize(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

	}

}