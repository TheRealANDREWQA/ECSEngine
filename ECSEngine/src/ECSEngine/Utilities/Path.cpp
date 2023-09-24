#include "ecspch.h"
#include "Path.h"
#include "Function.h"
#include "FunctionInterfaces.h"
#include "File.h"

namespace ECSEngine {

	namespace function {

		// Selects the path that has the size different from 0
		Path GetValidPath(Path path0, Path path1) {
			return path0.size > 0 ? path0 : path1;
		}

		// Selects the path that has the size different from 0
		ASCIIPath GetValidPath(ASCIIPath path0, ASCIIPath path1) {
			return path0.size > 0 ? path0 : path1;
		}

		// --------------------------------------------------------------------------------------------------

		bool PathIsRelative(Path path)
		{
			return path[1] != L':';
		}

		bool PathIsRelative(ASCIIPath path)
		{
			return path[1] != ':';
		}

		// --------------------------------------------------------------------------------------------------

		bool PathIsAbsolute(Path path)
		{
			return !PathIsRelative(path);
		}

		bool PathIsAbsolute(ASCIIPath path)
		{
			return !PathIsRelative(path);
		}

		// --------------------------------------------------------------------------------------------------

		Path PathParent(Path path, wchar_t separator) {
			while (path.size > 0 && path[path.size - 1] != separator) {
				path.size--;
			}
			return Path(path.buffer, path.size == 0 ? 0 : path.size - 1);
		}

		ASCIIPath PathParent(ASCIIPath path, char separator) {
			while (path.size > 0 && path[path.size - 1] != separator) {
				path.size--;
			}
			return ASCIIPath(path.buffer, path.size == 0 ? 0 : path.size - 1);
		}

		Path PathParentBoth(Path path)
		{
			return GetValidPath(PathParent(path), PathParent(path, ECS_OS_PATH_SEPARATOR_REL));
		}

		ASCIIPath PathParentBoth(ASCIIPath path)
		{
			return GetValidPath(PathParent(path), PathParent(path, ECS_OS_PATH_SEPARATOR_ASCII_REL));
		}

		// --------------------------------------------------------------------------------------------------

		size_t PathParentSize(Path path, wchar_t separator) {
			while (path.size > 0 && path[path.size - 1] != separator) {
				path.size--;
			}
			return path.size == 0 ? 0 : path.size - 1;
		}

		size_t PathParentSize(ASCIIPath path, char separator) {
			while (path.size > 0 && path[path.size - 1] != separator) {
				path.size--;
			}
			return path.size == 0 ? 0 : path.size - 1;
		}

		size_t PathParentSizeBoth(Path path)
		{
			size_t absolute = PathParentSize(path);
			size_t relative = PathParentSize(path, ECS_OS_PATH_SEPARATOR_REL);
			return absolute > 0 ? absolute : relative;
		}

		size_t PathParentSizeBoth(ASCIIPath path)
		{
			size_t absolute = PathParentSize(path);
			size_t relative = PathParentSize(path, ECS_OS_PATH_SEPARATOR_ASCII_REL);
			return absolute > 0 ? absolute : relative;
		}

		// --------------------------------------------------------------------------------------------------

		Path PathExtension(Path string, wchar_t separator)
		{
			size_t initial_size = string.size;
			while (string.size > 0 && string[string.size - 1] != L'.' && string[string.size - 1] != separator) {
				string.size--;
			}
			if (string.size > 1 && string[string.size - 1] == L'.') {
				if (string[string.size - 2] == separator || string[string.size - 2] == L'.') {
					return Path(nullptr, 0);
				}
				else {
					return Path(string.buffer + string.size - 1, initial_size - string.size + 1);
				}
			}
			else {
				return Path(nullptr, 0);
			}
		}

		ASCIIPath PathExtension(ASCIIPath string, char separator)
		{
			size_t initial_size = string.size;
			while (string.size > 0 && string[string.size - 1] != '.' && string[string.size - 1] != separator) {
				string.size--;
			}
			if (string.size > 1 && string[string.size - 1] == L'.') {
				if (string[string.size - 2] == separator || string[string.size - 2] == L'.') {
					return ASCIIPath(nullptr, 0);
				}
				else {
					return ASCIIPath(string.buffer + string.size - 1, initial_size - string.size + 1);
				}
			}
			else {
				return ASCIIPath(nullptr, 0);
			}
		}

		Path PathExtensionBoth(Path path)
		{
			return GetValidPath(PathExtension(path), PathExtension(path, ECS_OS_PATH_SEPARATOR_REL));
		}

		ASCIIPath PathExtensionBoth(ASCIIPath path)
		{
			return GetValidPath(PathExtension(path), PathExtension(path, ECS_OS_PATH_SEPARATOR_ASCII_REL));
		}

		// -------------------------------------------------------------------------------------------------
		
		Path PathNoExtension(Path path, wchar_t separator) {
			Path extension = PathExtension(path, separator);
			return { path.buffer, path.size - extension.size };
		}

		Path PathNoExtensionBoth(Path path) {
			return GetValidPath(PathNoExtension(path), PathNoExtension(path, ECS_OS_PATH_SEPARATOR_REL));
		}
		
		ASCIIPath PathNoExtension(ASCIIPath path, char separator) {
			ASCIIPath extension = PathExtension(path, separator);
			return { path.buffer, path.size - extension.size };
		}
		
		ASCIIPath PathNoExtensionBoth(ASCIIPath path) {
			return GetValidPath(PathNoExtension(path), PathNoExtension(path, ECS_OS_PATH_SEPARATOR_ASCII_REL));
		}

		// --------------------------------------------------------------------------------------------------

		size_t PathExtensionSize(Path path, wchar_t separator) {
			return PathExtension(path, separator).size;
		}

		size_t PathExtensionSize(ASCIIPath path, char separator) {
			return PathExtension(path, separator).size;
		}

		size_t PathExtensionSizeBoth(Path path)
		{
			size_t absolute = PathExtensionSize(path);
			size_t relative = PathExtensionSize(path, ECS_OS_PATH_SEPARATOR_REL);
			return absolute > 0 ? absolute : relative;
		}

		size_t PathExtensionSizeBoth(ASCIIPath path)
		{
			size_t absolute = PathExtensionSize(path);
			size_t relative = PathExtensionSize(path, ECS_OS_PATH_SEPARATOR_ASCII_REL);
			return absolute > 0 ? absolute : relative;
		}

		// --------------------------------------------------------------------------------------------------

		Path PathStem(Path path, wchar_t separator) {
			Path filename = PathFilename(path, separator);
			filename.size -= PathExtension(path, separator).size;
			return filename;
		}

		ASCIIPath PathStem(ASCIIPath path, char separator) {
			ASCIIPath filename = PathFilename(path, separator);
			filename.size -= PathExtension(path, separator).size;
			return filename;
		}

		Path PathStemBoth(Path path)
		{
			return GetValidPath(PathStem(path), PathStem(path, ECS_OS_PATH_SEPARATOR_REL));
		}

		ASCIIPath PathStemBoth(ASCIIPath path)
		{
			return GetValidPath(PathStem(path), PathStem(path, ECS_OS_PATH_SEPARATOR_ASCII_REL));
		}

		// --------------------------------------------------------------------------------------------------

		Path PathFilename(Path path, wchar_t separator) {
			size_t parent_size = PathParentSize(path, separator);
			return Path(path.buffer + parent_size + (parent_size > 0), path.size - parent_size - (parent_size > 0));
		}

		ASCIIPath PathFilename(ASCIIPath path, char separator) {
			size_t parent_size = PathParentSize(path, separator);
			return ASCIIPath(path.buffer + parent_size + (parent_size > 0), path.size - parent_size - (parent_size > 0));
		}

		// --------------------------------------------------------------------------------------------------

		Path PathFilenameBoth(Path path)
		{
			Path absolute_filename = PathFilename(path);
			Path relative_filename = PathFilename(path, ECS_OS_PATH_SEPARATOR_REL);
			return absolute_filename.size < relative_filename.size ? absolute_filename : relative_filename;
		}

		ASCIIPath PathFilenameBoth(ASCIIPath path)
		{
			ASCIIPath absolute_filename = PathFilename(path);
			ASCIIPath relative_filename = PathFilename(path, ECS_OS_PATH_SEPARATOR_REL);
			return absolute_filename.size < relative_filename.size ? absolute_filename : relative_filename;
		}

		// --------------------------------------------------------------------------------------------------

		template<typename PathType, typename CharType>
		PathType PathRelativeToFilenameImpl(PathType path, PathType reference, CharType separator) {
			size_t path_initial_size = path.size;
			path = PathParent(path);
			PathType reference_directory = PathFilename(reference, separator);
			PathType path_directory = PathFilename(path);
			while (!CompareStrings(reference_directory, path_directory) && path.size > 0) {
				path = PathParent(path);
				path_directory = PathFilename(path);
			}
			return PathType(path_directory.buffer + reference.size + 1, path_initial_size - (path_directory.buffer - path.buffer) - reference.size - 1);
		}

		Path PathRelativeToFilename(Path path, Path reference) {
			return PathRelativeToFilenameImpl(path, reference, ECS_OS_PATH_SEPARATOR_REL);
		}

		ASCIIPath PathRelativeToFilename(ASCIIPath path, ASCIIPath reference) {
			return PathRelativeToFilenameImpl(path, reference, ECS_OS_PATH_SEPARATOR_ASCII_REL);
		}

		// --------------------------------------------------------------------------------------------------

		template<typename PathType>
		PathType PathRelativeToAbsoluteImpl(PathType path, PathType absolute_reference) {
			if (memcmp(path.buffer, absolute_reference.buffer, absolute_reference.MemoryOf(absolute_reference.size)) == 0) {
				if (path.size == absolute_reference.size) {
					return { path.buffer + path.size, 0 };
				}
				return { path.buffer + absolute_reference.size + 1, path.size - absolute_reference.size - 1 };
			}
			return { nullptr, 0 };
		}

		Path PathRelativeToAbsolute(Path path, Path absolute_reference)
		{
			return PathRelativeToAbsoluteImpl(path, absolute_reference);
		}

		ASCIIPath PathRelativeToAbsolute(ASCIIPath path, ASCIIPath absolute_reference)
		{
			return PathRelativeToAbsoluteImpl(path, absolute_reference);
		}

		// --------------------------------------------------------------------------------------------------
		
		template<typename PathType, typename CharacterType>
		PathType MountPathImpl(PathType base, PathType mount_point, CapacityStream<CharacterType> storage) {
			if (mount_point.size > 0) {
				storage.CopyOther(mount_point);
				bool is_absolute = PathIsAbsolute(mount_point);
				CharacterType character_to_check;
				CharacterType absolute_separator = Character<CharacterType>(ECS_OS_PATH_SEPARATOR_ASCII);
				CharacterType relative_separator = Character<CharacterType>(ECS_OS_PATH_SEPARATOR_ASCII_REL);

				if (is_absolute) {
					character_to_check = Character<CharacterType>(ECS_OS_PATH_SEPARATOR_ASCII);
				}
				else {
					character_to_check = Character<CharacterType>(ECS_OS_PATH_SEPARATOR_ASCII_REL);
				}

				if (mount_point[mount_point.size - 1] != character_to_check) {
					storage.Add(character_to_check);
				}

				size_t new_path_base_size = storage.size;
				storage.AddStreamSafe(base);
				
				if (is_absolute) {
					// If the path is absolute, replace any relative separators that come from the base into absolute separator
					function::ReplaceCharacter(PathType(storage.buffer + new_path_base_size, base.size), relative_separator, absolute_separator);
				}

				return storage;
			}
			return base;
		}

		Path MountPath(Path base, Path mount_point, CapacityStream<wchar_t> storage)
		{
			return MountPathImpl(base, mount_point, storage);
		}

		ASCIIPath MountPath(ASCIIPath base, ASCIIPath mount_point, CapacityStream<char> storage)
		{
			return MountPathImpl(base, mount_point, storage);
		}

		// --------------------------------------------------------------------------------------------------
		
		template<typename CharacterType, typename PathType>
		PathType MountPathImpl(PathType base, PathType mount_point, AllocatorPolymorphic allocator) {
			if (mount_point.size > 0) {
				void* allocation = Allocate(allocator, sizeof(CharacterType) * (mount_point.size + base.size + 1), alignof(CharacterType));
				PathType new_path;
				new_path.buffer = (CharacterType*)allocation;

				new_path.CopyOther(mount_point);
				bool is_absolute = PathIsAbsolute(mount_point);
				CharacterType absolute_separator = Character<CharacterType>(ECS_OS_PATH_SEPARATOR_ASCII);
				CharacterType relative_separator = Character<CharacterType>(ECS_OS_PATH_SEPARATOR_ASCII_REL);

				CharacterType character_to_check;
				if (is_absolute) {
					character_to_check = absolute_separator;
				}
				else {
					character_to_check = relative_separator;
				}

				if (mount_point[mount_point.size - 1] != character_to_check) {
					new_path.Add(character_to_check);
				}
				size_t new_path_base_size = new_path.size;
				new_path.AddStream(base);

				if (is_absolute) {
					// If the path is absolute, replace any relative separators that come from the base into absolute separator
					function::ReplaceCharacter(PathType(new_path.buffer + new_path_base_size, base.size), relative_separator, absolute_separator);
				}

				return new_path;
			}
			return base;
		}

		Path MountPath(Path base, Path mount_point, AllocatorPolymorphic allocator)
		{
			return MountPathImpl<wchar_t>(base, mount_point, allocator);
		}

		ASCIIPath MountPath(ASCIIPath base, ASCIIPath mount_point, AllocatorPolymorphic allocator)
		{
			return MountPathImpl<char>(base, mount_point, allocator);
		}

		// --------------------------------------------------------------------------------------------------

		template<typename PathType, typename CharacterType>
		PathType MountPathOnlyRelImpl(PathType base, PathType mount_point, CapacityStream<CharacterType> storage) {
			if (PathIsRelative(base)) {
				return MountPath(base, mount_point, storage);
			}
			return base;
		}

		Path MountPathOnlyRel(Path base, Path mount_point, CapacityStream<wchar_t> storage) {
			return MountPathOnlyRelImpl(base, mount_point, storage);
		}

		ASCIIPath MountPathOnlyRel(ASCIIPath base, ASCIIPath mount_point, CapacityStream<char> storage) {
			return MountPathOnlyRelImpl(base, mount_point, storage);
		}

		// -------------------------------------------------------------------------------------------------

		template<typename PathType>
		PathType MountPathOnlyRelImpl(PathType base, PathType mount_point, AllocatorPolymorphic allocator) {
			if (PathIsRelative(base)) {
				return MountPath(base, mount_point, allocator);
			}
			return base;
		}

		Path MountPathOnlyRel(Path base, Path mount_point, AllocatorPolymorphic allocator) {
			return MountPathOnlyRelImpl(base, mount_point, allocator);
		}

		ASCIIPath MountPathOnlyRel(ASCIIPath base, ASCIIPath mount_point, AllocatorPolymorphic allocator) {
			return MountPathOnlyRelImpl(base, mount_point, allocator);
		}

		// -------------------------------------------------------------------------------------------------

		template<typename CharacterType, typename PathType>
		PathType PathFilenameAfterImpl(PathType path, PathType base_path) {
			ECS_ASSERT(path.size > base_path.size);

			CharacterType delimitator = Character<CharacterType>(ECS_OS_PATH_SEPARATOR_ASCII);
			if (PathIsRelative(path)) {
				delimitator = Character<CharacterType>(ECS_OS_PATH_SEPARATOR_ASCII_REL);
			}

			PathType remaining_path = { path.buffer + base_path.size + 1, path.size - base_path.size - 1 };
			PathType after_filename = function::FindFirstCharacter(remaining_path, delimitator);
			if (after_filename.size == 0) {
				return remaining_path;
			}
			return { remaining_path.buffer, remaining_path.size - after_filename.size };
		}

		Path PathFilenameAfter(Path path, Path base_path) {
			return PathFilenameAfterImpl<wchar_t>(path, base_path);
		}

		ASCIIPath PathFilenameAfter(ASCIIPath path, ASCIIPath base_path) {
			return PathFilenameAfterImpl<char>(path, base_path);
		}

		// -------------------------------------------------------------------------------------------------

		bool PathEnsureParents(Path path, Path valid_parent)
		{
			ECS_STACK_CAPACITY_STREAM(wchar_t, new_valid_parent, 512);
			new_valid_parent.CopyOther(valid_parent);

			bool is_relative = PathIsRelative(path);
			wchar_t delimitator = is_relative ? ECS_OS_PATH_SEPARATOR_REL : ECS_OS_PATH_SEPARATOR;

			while (new_valid_parent.size < path.size) {
				Path next_filename = PathFilenameAfter(path, new_valid_parent);
				new_valid_parent.Add(delimitator);
				new_valid_parent.AddStreamAssert(next_filename);
				if (!ExistsFileOrFolder(new_valid_parent)) {
					// Try to create it
					bool success = CreateFolder(new_valid_parent);
					if (!success) {
						return false;
					}
				}
			}

			return true;
		}

		// --------------------------------------------------------------------------------------------------

	}

}