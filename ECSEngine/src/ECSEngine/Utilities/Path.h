#pragma once
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	using Path = Stream<wchar_t>;
	using ASCIIPath = Stream<char>;

	namespace function {

		// -------------------------------------------------------------------------------------------------

		ECSENGINE_API bool PathIsRelative(Path path);

		ECSENGINE_API bool PathIsRelative(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		ECSENGINE_API bool PathIsAbsolute(Path path);

		ECSENGINE_API bool PathIsAbsolute(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API Path PathParent(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API ASCIIPath PathParent(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API Path PathParentBoth(Path path);

		// Uses both separators
		ECSENGINE_API ASCIIPath PathParentBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// Includes the dot
		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API size_t PathExtensionSize(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API size_t PathExtensionSizeBoth(Path path);

		// Includes the dot
		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API size_t PathExtensionSize(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API size_t PathExtensionSizeBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API Path PathExtension(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API Path PathExtensionBoth(Path path);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API ASCIIPath PathExtension(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API ASCIIPath PathExtensionBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API Path PathNoExtension(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);
		
		// Uses both separators
		ECSENGINE_API Path PathNoExtensionBoth(Path path);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API ASCIIPath PathNoExtension(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API ASCIIPath PathNoExtensionBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API Path PathStem(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API Path PathStemBoth(Path path);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API ASCIIPath PathStem(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API ASCIIPath PathStemBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API Path PathFilename(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API Path PathFilenameBoth(Path path);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API ASCIIPath PathFilename(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API ASCIIPath PathFilenameBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// It is assumed that the path is absolute and reference is only a filename
		ECSENGINE_API Path PathRelativeToFilename(Path path, Path reference);

		// It is assumed that the path is absolute and reference is only a filename
		ECSENGINE_API ASCIIPath PathRelativeToFilename(ASCIIPath path, ASCIIPath reference);

		// -------------------------------------------------------------------------------------------------

		// It is assumed that the path is absolute. For example path C:\Users\Name\Project\NewFolder
		// and absolute_reference C:\Users\Name\Project it returns NewFolder (it skips the first backslash)
		// Returns { nullptr, 0 } if the absolute reference doesn't exist in the path
		ECSENGINE_API Path PathRelativeToAbsolute(Path path, Path absolute_reference);

		// It is assumed that the path is absolute. For example path C:\Users\Name\Project\NewFolder
		// and absolute_reference C:\Users\Name\Project it returns NewFolder (it skips the first backslash)
		// Returns { nullptr, 0 } if the absolute reference doesn't exist in the path
		ECSENGINE_API ASCIIPath PathRelativeToAbsolute(ASCIIPath path, ASCIIPath absolute_reference);

		// -------------------------------------------------------------------------------------------------

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API size_t PathParentSize(Path path, wchar_t separator = ECS_OS_PATH_SEPARATOR);

		// Uses both separators
		ECSENGINE_API size_t PathParentSizeBoth(Path path);

		// The separator specifies the character by which the delimitation is made
		ECSENGINE_API size_t PathParentSize(ASCIIPath path, char separator = ECS_OS_PATH_SEPARATOR_ASCII);

		// Uses both separators
		ECSENGINE_API size_t PathParentSizeBoth(ASCIIPath path);

		// -------------------------------------------------------------------------------------------------

		// If mount point has size greater than 0, it will concatenate the mount point with the base
		// into the given storage buffer, else return the base.
		ECSENGINE_API Path MountPath(Path base, Path mount_point, CapacityStream<wchar_t> storage);

		// If mount point has size greater than 0, it will concatenate the mount point with the base
		// into the given storage buffer, else return the base
		ECSENGINE_API ASCIIPath MountPath(ASCIIPath base, ASCIIPath mount_point, CapacityStream<char> storage);

		// -------------------------------------------------------------------------------------------------

		// If mount point has size greater than 0, it will concatenate the mount point with the base
		// into the given storage buffer, else return the base.
		ECSENGINE_API Path MountPath(Path base, Path mount_point, AllocatorPolymorphic allocator);

		// If mount point has size greater than 0, it will concatenate the mount point with the base
		// into the given storage buffer, else return the base.
		ECSENGINE_API ASCIIPath MountPath(ASCIIPath base, ASCIIPath mount_point, AllocatorPolymorphic allocator);

		// -------------------------------------------------------------------------------------------------

		// If mount point has size greater than 0 and the base is a relative path, it will concatenate the mount point with the base
		// into the given storage buffer, else return the base.
		ECSENGINE_API Path MountPathOnlyRel(Path base, Path mount_point, CapacityStream<wchar_t> storage);

		// If mount point has size greater than 0 and the base is a relative path, it will concatenate the mount point with the base
		// into the given storage buffer, else return the base.
		ECSENGINE_API ASCIIPath MountPathOnlyRel(ASCIIPath base, ASCIIPath mount_point, CapacityStream<char> storage);

		// -------------------------------------------------------------------------------------------------

		// If mount point has size greater than 0 and the base is a relative path, it will concatenate the mount point with the base
		// into the given storage buffer, else return the base.
		ECSENGINE_API Path MountPathOnlyRel(Path base, Path mount_point, AllocatorPolymorphic allocator);

		// If mount point has size greater than 0 and the base is a relative path, it will concatenate the mount point with the base
		// into the given storage buffer, else return the base.
		ECSENGINE_API ASCIIPath MountPathOnlyRel(ASCIIPath base, ASCIIPath mount_point, AllocatorPolymorphic allocator);

		// -------------------------------------------------------------------------------------------------
		
		// Returns the filename immediately after the base path in the path
		// For example, path is C:\a\b\c\d\e and base_path is C:\a\b\c
		// It will return d
		ECSENGINE_API Path PathFilenameAfter(Path path, Path base_path);

		// Returns the filename immediately after the base path in the path
		// For example, path is C:\a\b\c\d\e and base_path is C:\a\b\c
		// It will return d
		ECSENGINE_API ASCIIPath PathFilenameAfter(ASCIIPath path, ASCIIPath base_path);

		// -------------------------------------------------------------------------------------------------

		// Ensures that all parents up to that path are valid. It needs a hint as valid parent
		// To make it easier to detect when to stop
		// Works for both relative and absolute paths.
		// Returns true if it succeeded, else false
		ECSENGINE_API bool PathEnsureParents(Path path, Path valid_parent);

		// -------------------------------------------------------------------------------------------------

	}

}