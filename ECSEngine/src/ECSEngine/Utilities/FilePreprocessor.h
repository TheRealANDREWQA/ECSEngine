#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

#define ECS_C_FILE_SINGLE_LINE_COMMENT_TOKEN "//"
#define ECS_C_FILE_MULTI_LINE_COMMENT_OPENED_TOKEN "/*"
#define ECS_C_FILE_MULTI_LINE_COMMENT_CLOSED_TOKEN "*/"

	// Returns the "new" string (it will have the same buffer, the size will change only).
	// This is done in place
	ECSENGINE_API Stream<char> RemoveSingleLineComment(Stream<char> characters, Stream<char> token);

	// Returns the "new" string (it will have the same buffer, the size will change only).
	// This is done in place. It can return { nullptr, 0 } if a multi line comment is not properly closed (it will not modify anything)
	ECSENGINE_API Stream<char> RemoveMultiLineComments(Stream<char> characters, Stream<char> open_token, Stream<char> closed_token);

	struct ConditionalPreprocessorDirectives {
		Stream<char> if_directive;
		Stream<char> else_directive;
		Stream<char> elseif_directive;
		Stream<char> end_directive;
	};

	ECSENGINE_API void GetConditionalPreprocessorDirectivesCSource(ConditionalPreprocessorDirectives* directives);

	// Returns the "new" string (it will have the same buffer, the size will change only).
	// This is done in place. It can return { nullptr, 0 } if a directive is not properly closed
	ECSENGINE_API Stream<char> RemoveConditionalPreprocessorBlock(Stream<char> characters, const ConditionalPreprocessorDirectives* directives, Stream<Stream<char>> macros);

	// Removes from the source code of a "C" like file the single line comments, the multi line comments and the conditional blocks
	// Can optionally specify if the defines that are made inside the file should be taken into account
	// Returns the "new" string (it will have the same buffer, the size will change only)
	ECSENGINE_API Stream<char> PreprocessCFile(Stream<char> source_code, Stream<Stream<char>> external_macros = { nullptr, 0 }, bool add_internal_macro_defines = true);

	// If the define_token and the defined_macros are both set, it will retrieve
	// all the macros defined with the given token
	// If the conditional_macros is given, then it will retrieve all the macros
	// which appear in an ifdef_token or elif_token definition.
	// If the allocator is not given, the Stream<char> will reference the characters
	// in the source code. Else they will be allocated individually
	struct SourceCodeMacros {
		Stream<char> define_token = { nullptr, 0 };
		CapacityStream<Stream<char>>* defined_macros = nullptr;

		Stream<char> ifdef_token = { nullptr, 0 };
		Stream<char> elif_token = { nullptr, 0 };
		CapacityStream<Stream<char>>* conditional_macros = nullptr;

		// If this is different from nullptr, it will copy the string instead of simply referencing it
		AllocatorPolymorphic allocator = { nullptr };
	};

	ECS_INLINE void GetSourceCodeMacrosCTokens(SourceCodeMacros* macros) {
		macros->define_token = "#define";
		macros->ifdef_token = "#ifdef";
		macros->elif_token = "#elif";
	}

	ECSENGINE_API void GetSourceCodeMacros(
		Stream<char> characters,
		const SourceCodeMacros* options
	);

}