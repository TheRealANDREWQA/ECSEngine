#include "ecspch.h"
#include "Path.h"
#include "Function.h"
#include "FunctionInterfaces.h"

ECS_CONTAINERS;

namespace ECSEngine {

	namespace function {

		// --------------------------------------------------------------------------------------------------

		Path PathParent(Path path, wchar_t separator) {
			while (path.size > 0 && path[path.size - 1] != separator) {
				path.size--;
			}
			return Path(path.buffer, function::Select<size_t>(path.size == 0, 0, path.size - 1));
		}

		ASCIIPath PathParent(ASCIIPath path, char separator) {
			while (path.size > 0 && path[path.size - 1] != separator) {
				path.size--;
			}
			return ASCIIPath(path.buffer, function::Select<size_t>(path.size == 0, 0, path.size - 1));
		}

		Path2 PathParentBoth(Path path)
		{
			return { PathParent(path), PathParent(path, ECS_OS_PATH_SEPARATOR_REL) };
		}

		ASCIIPath2 PathParentBoth(ASCIIPath path)
		{
			return { PathParent(path), PathParent(path, ECS_OS_PATH_SEPARATOR_ASCII_REL) };
		}

		// --------------------------------------------------------------------------------------------------

		size_t PathParentSize(Path path, wchar_t separator) {
			while (path.size > 0 && path[path.size - 1] != separator) {
				path.size--;
			}
			return Select<size_t>(path.size == 0, 0, path.size - 1);
		}

		size_t PathParentSize(ASCIIPath path, char separator) {
			while (path.size > 0 && path[path.size - 1] != separator) {
				path.size--;
			}
			return Select<size_t>(path.size == 0, 0, path.size - 1);
		}

		ulong2 PathParentSizeBoth(Path path)
		{
			return { PathParentSize(path), PathParentSize(path, ECS_OS_PATH_SEPARATOR_REL) };
		}

		ulong2 PathParentSizeBoth(ASCIIPath path)
		{
			return { PathParentSize(path), PathParentSize(path, ECS_OS_PATH_SEPARATOR_ASCII_REL) };
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

		Path2 PathExtensionBoth(Path path)
		{
			return { PathExtension(path), PathExtension(path, ECS_OS_PATH_SEPARATOR_REL) };
		}

		ASCIIPath2 PathExtensionBoth(ASCIIPath path)
		{
			return { PathExtension(path), PathExtension(path, ECS_OS_PATH_SEPARATOR_ASCII_REL) };
		}

		// --------------------------------------------------------------------------------------------------

		size_t PathExtensionSize(Path path, wchar_t separator) {
			return PathExtension(path, separator).size;
		}

		size_t PathExtensionSize(ASCIIPath path, char separator) {
			return PathExtension(path, separator).size;
		}

		ulong2 PathExtensionSizeBoth(Path path)
		{
			return { PathExtensionSize(path),  PathExtensionSize(path, ECS_OS_PATH_SEPARATOR_REL) };
		}

		ulong2 PathExtensionSizeBoth(ASCIIPath path)
		{
			return { PathExtensionSize(path),  PathExtensionSize(path, ECS_OS_PATH_SEPARATOR_ASCII_REL) };
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

		Path2 PathStemBoth(Path path)
		{
			return { PathStem(path), PathStem(path, ECS_OS_PATH_SEPARATOR_REL) };
		}

		ASCIIPath2 PathStemBoth(ASCIIPath path)
		{
			return { PathStem(path), PathStem(path, ECS_OS_PATH_SEPARATOR_ASCII_REL) };
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

		Path2 PathFilenameBoth(Path path)
		{
			return { PathFilename(path), PathFilename(path, ECS_OS_PATH_SEPARATOR_REL) };
		}

		ASCIIPath2 PathFilenameBoth(ASCIIPath path)
		{
			return { PathFilename(path), PathFilename(path, ECS_OS_PATH_SEPARATOR_ASCII_REL) };
		}

		// --------------------------------------------------------------------------------------------------

		Path PathRelativeTo(Path path, Path reference) {
			size_t path_initial_size = path.size;
			path = PathParent(path);
			Path reference_directory = PathFilename(reference, ECS_OS_PATH_SEPARATOR_REL);
			Path path_directory = PathFilename(path);
			while (!CompareStrings(reference_directory, path_directory) && path.size > 0) {
				path = PathParent(path);
				path_directory = PathFilename(path);
			}
			return Path(path.buffer + reference.size + 1, path_initial_size - reference.size - 1);
		}

		// --------------------------------------------------------------------------------------------------

		ASCIIPath PathRelativeTo(ASCIIPath path, ASCIIPath reference) {
			size_t path_initial_size = path.size;
			path = PathParent(path);
			ASCIIPath reference_directory = PathFilename(reference, ECS_OS_PATH_SEPARATOR_ASCII_REL);
			ASCIIPath path_directory = PathFilename(path);
			while (!CompareStrings(reference_directory, path_directory) && path.size > 0) {
				path = PathParent(path);
				path_directory = PathFilename(path);
			}
			return ASCIIPath(path.buffer + reference.size + 1, path_initial_size - reference.size - 1);
		}

		// --------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------
		
		// --------------------------------------------------------------------------------------------------
		
		// --------------------------------------------------------------------------------------------------

	}

}