#include "ecspch.h"
#include "Path.h"
#include "Function.h"
#include "FunctionInterfaces.h"

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

		Path PathFilenameBoth(Path path)
		{
			return GetValidPath(PathFilename(path), PathFilename(path, ECS_OS_PATH_SEPARATOR_REL));
		}

		ASCIIPath PathFilenameBoth(ASCIIPath path)
		{
			return GetValidPath(PathFilename(path), PathFilename(path, ECS_OS_PATH_SEPARATOR_ASCII_REL));
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
			return Path(path_directory.buffer + reference.size + 1, path_initial_size - (path_directory.buffer - path.buffer) - reference.size - 1);
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

	}

}