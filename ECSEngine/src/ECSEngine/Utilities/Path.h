#pragma once
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"

ECS_CONTAINERS;

namespace ECSEngine {

	using Path = Stream<wchar_t>;
	using ASCIIPath = Stream<char>;

	struct Path2 {
		Path absolute;
		Path relative;
	};

	struct ASCIIPath2 {
		ASCIIPath absolute;
		ASCIIPath relative;
	};

	namespace function {

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API Path PathParent(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API ASCIIPath PathParent(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API Path2 PathParentBoth(Path path);

		// Uses both separators
		ECSENGINE_API ASCIIPath2 PathParentBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// Includes the dot
		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API size_t PathExtensionSize(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API ulong2 PathExtensionSizeBoth(Path path);

		// Includes the dot
		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API size_t PathExtensionSize(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API ulong2 PathExtensionSizeBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API Path PathExtension(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API Path2 PathExtensionBoth(Path path);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API ASCIIPath PathExtension(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API ASCIIPath2 PathExtensionBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API Path PathStem(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API Path2 PathStemBoth(Path path);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API ASCIIPath PathStem(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API ASCIIPath2 PathStemBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API Path PathFilename(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API Path2 PathFilenameBoth(Path path);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API ASCIIPath PathFilename(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API ASCIIPath2 PathFilenameBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// It is assumed that the path is absolute and reference is only a filename
		ECSENGINE_API Path PathRelativeTo(Path path, Path reference);

		// It is assumed that the path is absolute and reference is only a filename
		ECSENGINE_API ASCIIPath PathRelativeTo(ASCIIPath path, ASCIIPath reference);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API size_t PathParentSize(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API ulong2 PathParentSizeBoth(Path path);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API size_t PathParentSize(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API ulong2 PathParentSizeBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// Selects the path that has the size different from 0
		ECSENGINE_API Path GetValidPath(Path2 paths);

		// Selects the path that has the size different from 0
		ECSENGINE_API ASCIIPath GetValidPath(ASCIIPath2 paths);

		// -------------------------------------------------------------------------------------------------

	}

}