#include "ecspch.h"
#include "Path.h"
#include "Function.h"
#include "FunctionInterfaces.h"

ECS_CONTAINERS;

namespace ECSEngine {

	namespace function {

		// --------------------------------------------------------------------------------------------------

		Path PathParent(Path path) {
			while (path.size > 0 && path[path.size - 1] != ECS_OS_PATH_SEPARATOR) {
				path.size--;
			}
			return Path(path.buffer, function::PredicateValue<size_t>(path.size == 0, 0, path.size - 1));
		}

		// --------------------------------------------------------------------------------------------------

		ASCIIPath PathParent(ASCIIPath path) {
			while (path.size > 0 && path[path.size - 1] != ECS_OS_PATH_SEPARATOR_ASCII) {
				path.size--;
			}
			return ASCIIPath(path.buffer, function::PredicateValue<size_t>(path.size == 0, 0, path.size - 1));
		}

		// --------------------------------------------------------------------------------------------------

		size_t PathParentSize(Path path) {
			while (path.size > 0 && path[path.size - 1] != ECS_OS_PATH_SEPARATOR) {
				path.size--;
			}
			return PredicateValue<size_t>(path.size == 0, 0, path.size - 1);
		}

		// --------------------------------------------------------------------------------------------------

		size_t PathParentSize(ASCIIPath path) {
			while (path.size > 0 && path[path.size - 1] != ECS_OS_PATH_SEPARATOR_ASCII) {
				path.size--;
			}
			return PredicateValue<size_t>(path.size == 0, 0, path.size - 1);
		}

		// --------------------------------------------------------------------------------------------------

		Path PathExtension(Path string)
		{
			size_t initial_size = string.size;
			while (string.size > 0 && string[string.size - 1] != L'.' && string[string.size - 1] != ECS_OS_PATH_SEPARATOR) {
				string.size--;
			}
			if (string.size > 1 && string[string.size - 1] == L'.') {
				if (string[string.size - 2] == ECS_OS_PATH_SEPARATOR || string[string.size - 2] == L'.') {
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

		// --------------------------------------------------------------------------------------------------

		ASCIIPath PathExtension(ASCIIPath string)
		{
			size_t initial_size = string.size;
			while (string.size > 0 && string[string.size - 1] != '.' && string[string.size - 1] != ECS_OS_PATH_SEPARATOR_ASCII) {
				string.size--;
			}
			if (string.size > 1 && string[string.size - 1] == L'.') {
				if (string[string.size - 2] == ECS_OS_PATH_SEPARATOR_ASCII || string[string.size - 2] == L'.') {
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

		// --------------------------------------------------------------------------------------------------

		size_t PathExtensionSize(Path path) {
			return PathExtension(path).size;
		}

		// --------------------------------------------------------------------------------------------------

		size_t PathExtensionSize(ASCIIPath path) {
			return PathExtension(path).size;
		}

		// --------------------------------------------------------------------------------------------------

		Path PathStem(Path path) {
			Path filename = PathFilename(path);
			filename.size -= PathExtension(path).size;
			return filename;
		}

		// --------------------------------------------------------------------------------------------------

		ASCIIPath PathStem(ASCIIPath path) {
			ASCIIPath filename = PathFilename(path);
			filename.size -= PathExtension(path).size;
			return filename;
		}

		// --------------------------------------------------------------------------------------------------

		Path PathFilename(Path path) {
			size_t parent_size = PathParentSize(path);
			return Path(path.buffer + parent_size + 1, path.size - parent_size - 1);
		}

		// --------------------------------------------------------------------------------------------------

		ASCIIPath PathFilename(ASCIIPath path) {
			size_t parent_size = PathParentSize(path);
			return ASCIIPath(path.buffer + parent_size + 1, path.size - parent_size - 1);
		}

		// --------------------------------------------------------------------------------------------------

		Path PathRelativeTo(Path path, Path reference) {
			size_t path_initial_size = path.size;
			path = PathParent(path);
			Path reference_directory = PathFilename(reference);
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
			ASCIIPath reference_directory = PathFilename(reference);
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