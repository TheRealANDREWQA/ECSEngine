#include "ecspch.h"
#include "FilePreprocessor.h"
#include "BasicTypes.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "StringUtilities.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	Stream<char> RemoveSingleLineComment(Stream<char> characters, Stream<char> token)
	{
		// x - the offset into the characters
		// y - the size of the range
		// z - the amount to shift
		ECS_STACK_CAPACITY_STREAM(uint3, ranges, ECS_KB);
		ranges.Add({ 0, (unsigned int)characters.size, 0 });

		const char* last_character = characters.buffer + characters.size;

		Stream<char> current_parse_range = characters;
		Stream<char> current_token = FindFirstToken(current_parse_range, token);
		while (current_token.size > 0) {
			Stream<char> next_line = FindFirstCharacter(current_token, '\n');
			if (next_line.size == 0) {
				// Make it the last character
				next_line.buffer = (char*)last_character;
			}

			// Split the last block into a new block
			uint3 previous_block = ranges[ranges.size - 1];

			uint3 new_block;
			new_block.x = next_line.buffer - characters.buffer;
			new_block.y = previous_block.x + previous_block.y - new_block.x;
			size_t remove_count = next_line.buffer - current_token.buffer;
			new_block.z = previous_block.z + remove_count;
			characters.size -= remove_count;

			previous_block.y = current_token.buffer - characters.buffer - previous_block.x;

			ranges[ranges.size - 1] = previous_block;
			ranges.AddAssert(new_block);

			current_token.Advance(remove_count);
			current_parse_range = current_token;
			current_token = FindFirstToken(current_parse_range, token);
		}

		for (unsigned int index = 1; index < ranges.size; index++) {
			memmove(characters.buffer + ranges[index].x - ranges[index].z, characters.buffer + ranges[index].x, ranges[index].y * sizeof(char));
		}
		return characters;
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> RemoveMultiLineComments(Stream<char> characters, Stream<char> open_token, Stream<char> closed_token)
	{
		// x - the offset into the characters
		// y - the size of the range
		// z - the amount to shift
		ECS_STACK_CAPACITY_STREAM(uint3, ranges, ECS_KB);
		ranges.Add({ 0, (unsigned int)characters.size, 0 });

		Stream<char> current_parse_range = characters;
		Stream<char> current_token = FindFirstToken(current_parse_range, open_token);
		while (current_token.size > 0) {
			// Find the closing pair
			Stream<char> closing_token = FindFirstToken(current_token, closed_token);
			if (closing_token.size == 0) {
				// Fail
				return { nullptr, 0 };
			}

			// Split the last block into a new block
			uint3 previous_block = ranges[ranges.size - 1];

			uint3 new_block;
			new_block.x = closing_token.buffer - characters.buffer + closed_token.size;
			new_block.y = previous_block.x + previous_block.y - new_block.x;
			size_t remove_count = closing_token.buffer - current_token.buffer + closed_token.size;
			new_block.z = previous_block.z + remove_count;
			characters.size -= remove_count;

			previous_block.y = current_token.buffer - characters.buffer - previous_block.x;

			ranges[ranges.size - 1] = previous_block;
			ranges.AddAssert(new_block);

			closing_token.Advance(closed_token.size);
			current_parse_range = closing_token;
			current_token = FindFirstToken(current_parse_range, open_token);
		}


		for (unsigned int index = 1; index < ranges.size; index++) {
			memmove(characters.buffer + ranges[index].x - ranges[index].z, characters.buffer + ranges[index].x, ranges[index].y * sizeof(char));
		}
		return characters;
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> RemoveConditionalPreprocessorBlock(Stream<char> characters, const ConditionalPreprocessorDirectives* directives, Stream<Stream<char>> macros)
	{
		// x - the offset into the characters
		// y - the size of the range
		// z - the amount to shift
		ECS_STACK_CAPACITY_STREAM(uint3, ranges, ECS_KB);
		ranges.Add({ 0, (unsigned int)characters.size, 0 });

		const char* last_line = characters.buffer + characters.size;

		Stream<char> current_parse_range = characters;
		Stream<char> current_if_directive = FindFirstToken(current_parse_range, directives->if_directive);
		while (current_if_directive.size > 0) {
			// Look for the closing directive
			Stream<char> end_directive = FindFirstToken(current_if_directive, directives->end_directive);
			if (end_directive.size == 0) {
				// Fail
				return { nullptr, 0 };
			}

			uint3* previous_block = &ranges[ranges.size - 1];

			Stream<char> current_macro_definition = current_if_directive;
			current_macro_definition.Advance(directives->if_directive.size);
			current_macro_definition = SkipWhitespace(current_macro_definition);
			Stream<char> current_macro_definition_end = SkipCodeIdentifier(current_macro_definition);
			current_macro_definition.size = current_macro_definition_end.buffer - current_macro_definition.buffer;
			// There must be a new line after the if directive otherwise the end directive wouldn't be found
			Stream<char> if_directive_new_line = FindFirstCharacter(current_macro_definition_end, '\n');
			if (if_directive_new_line.size == 0) {
				return { nullptr, 0 };
			}

			Stream<char> if_directive_line;
			if_directive_line.buffer = current_if_directive.buffer;
			if_directive_line.size = if_directive_new_line.buffer - current_if_directive.buffer;

			Stream<char> end_directive_new_line = FindFirstCharacter(end_directive, '\n');
			// Redirect the new line to the last line in case there is none
			if (end_directive_new_line.size == 0) {
				end_directive_new_line.buffer = (char*)last_line;
			}
			Stream<char> end_directive_line;
			end_directive_line.buffer = end_directive.buffer;
			end_directive_line.size = end_directive_new_line.buffer - end_directive.buffer;

			// Now we need to check for else or else if directives
			// Start with else if directives
			Stream<char> miniblock_parse_range = if_directive_new_line;
			miniblock_parse_range.Advance();
			miniblock_parse_range.size = end_directive.buffer - miniblock_parse_range.buffer;

			uint3 current_block;
			current_block.x = miniblock_parse_range.buffer - characters.buffer;
			current_block.y = miniblock_parse_range.size;
			current_block.z = miniblock_parse_range.buffer - current_if_directive.buffer;

			uint3 elseif_block;
			elseif_block.y = 0;

			unsigned int elseif_index = 0;
			unsigned int matched_elseif_index = -1;
			Stream<char> current_elseif_directive = FindFirstToken(miniblock_parse_range, directives->elseif_directive);
			while (current_elseif_directive.size > 0) {
				Stream<char> next_new_line = FindFirstCharacter(current_elseif_directive, '\n');
				if (next_new_line.size == 0) {
					return { nullptr, 0 };
				}

				Stream<char> macro_definition = current_elseif_directive;
				macro_definition.Advance(directives->elseif_directive.size);
				macro_definition = SkipWhitespace(macro_definition);
				Stream<char> macro_definition_end = SkipCodeIdentifier(macro_definition);
				macro_definition.size = macro_definition_end.buffer - macro_definition.buffer;

				// Check to see if the macro is defined
				if (macros.Find(macro_definition) != -1) {
					// Set the elif block if it has not been matched
					if (matched_elseif_index == -1) {
						elseif_block.x = next_new_line.buffer + 1 - characters.buffer;
						elseif_block.y = current_block.y - elseif_block.x + current_block.x;
						elseif_block.z = elseif_block.x - current_block.x + current_block.z;
						matched_elseif_index = elseif_index;
					}
				}
				else {
					// If this is the first elseif then we need to crop the current_block such that it ends here
					if (elseif_index == 0) {
						current_block.y = current_elseif_directive.buffer - characters.buffer - current_block.x;
					}
					else if (elseif_index == matched_elseif_index + 1) {
						// We need to crop the elseif_block
						elseif_block.y = current_elseif_directive.buffer - characters.buffer - elseif_block.x;
					}
				}

				elseif_index++;

				miniblock_parse_range = next_new_line;
				miniblock_parse_range.Advance();

				current_elseif_directive = FindFirstToken(miniblock_parse_range, directives->elseif_directive);
			}

			// Check the else directive
			Stream<char> current_else_directive = FindFirstToken(miniblock_parse_range, directives->else_directive);
			if (current_else_directive.size > 0) {
				Stream<char> next_line = FindFirstCharacter(current_else_directive, '\n');
				if (next_line.size == 0) {
					return { nullptr, 0 };
				}

				if (matched_elseif_index == -1) {
					// No elseif - change the current block
					current_block.y = current_else_directive.buffer - characters.buffer - current_block.x;
				}
				else {
					// Change the elseif if it is the last one
					if (matched_elseif_index == elseif_index - 1) {
						elseif_block.y = current_else_directive.buffer - characters.buffer - elseif_block.x;
					}
				}

				if (macros.Find(current_macro_definition) != -1) {
					// The if block needs to be added
					ranges.AddAssert(current_block);
				}
				else {
					// The else if block, if there is
					if (matched_elseif_index != -1) {
						ranges.AddAssert(elseif_block);
					}
					else {
						// Add the else block
						current_block.x = next_line.buffer + 1 - characters.buffer;
						current_block.y = end_directive.buffer - characters.buffer - current_block.x;
						current_block.z = current_block.x - (current_if_directive.buffer - characters.buffer);
						ranges.AddAssert(current_block);
					}
				}
			}
			else {
				// Now check the ifdef macro
				if (macros.Find(current_macro_definition) != -1) {
					// Add the current block
					ranges.AddAssert(current_block);
				}
				else {
					if (elseif_block.y > 0) {
						ranges.AddAssert(elseif_block);
					}
				}
			}

			// We also need to add another block after the endif
			uint3 new_block;
			new_block.x = end_directive_new_line.buffer - characters.buffer;
			new_block.y = last_line - characters.buffer - new_block.x;
			new_block.z = previous_block->z + (end_directive_new_line.buffer - current_if_directive.buffer);

			unsigned int previous_block_y = previous_block->y;
			previous_block->y = current_if_directive.buffer - characters.buffer - previous_block->x;

			// If we added a new block
			if (ranges.buffer + ranges.size - 1 != previous_block) {
				// Also for the block added by and if, else if or else we need to increase the z component
				// We also need to account for the new block offset such that it won't overwrite the middle block
				new_block.z -= ranges[ranges.size - 1].y;
				ranges[ranges.size - 1].z += previous_block->z;
				characters.size += ranges[ranges.size - 1].y;
			}

			ranges.AddAssert(new_block);

			characters.size -= previous_block_y - previous_block->y - new_block.y;

			current_parse_range = end_directive_new_line;
			current_if_directive = FindFirstToken(current_parse_range, directives->if_directive);
		}

		for (unsigned int index = 1; index < ranges.size; index++) {
			memmove(characters.buffer + ranges[index].x - ranges[index].z, characters.buffer + ranges[index].x, ranges[index].y * sizeof(char));
		}
		return characters;
	}

	// --------------------------------------------------------------------------------------------------

	void GetSourceCodeMacros(
		Stream<char> characters,
		const SourceCodeMacros* options
	) {
		auto get_macros = [characters](Stream<char> token, CapacityStream<Stream<char>>* macros, AllocatorPolymorphic allocator, auto check_for_existence) {
			Stream<char> current_parse_range = characters;
			Stream<char> current_token = FindFirstToken(current_parse_range, token);

			const char* last_character = characters.buffer + characters.size;
			while (current_token.size > 0) {
				current_token.Advance(token.size);
				Stream<char> next_line = FindFirstCharacter(current_token, '\n');
				if (next_line.size == 0) {
					next_line.buffer = (char*)last_character;
				}
				Stream<char> line;
				line.buffer = current_token.buffer;
				line.size = next_line.buffer - current_token.buffer;

				Stream<char> current_macro_definition = SkipWhitespace(current_token);
				Stream<char> end_macro_definition = SkipCodeIdentifier(current_macro_definition);
				current_macro_definition.size = end_macro_definition.buffer - current_macro_definition.buffer;

				bool add_it = true;
				Stream<char> next_char_after_macro = SkipWhitespace(end_macro_definition);
				if (next_char_after_macro.buffer[0] == '(') {
					// It might be a function declaration - ignore it
					add_it = false;
				}

				if constexpr (check_for_existence) {
					add_it &= macros->Find(current_macro_definition) == -1;
				}

				if (add_it) {
					if (allocator.allocator != nullptr) {
						current_macro_definition = StringCopy(allocator, current_macro_definition);
					}
					macros->AddAssert(current_macro_definition);
				}

				current_parse_range = next_line;
				current_token = FindFirstToken(current_parse_range, token);
			}
		};

		if (options->defined_macros != nullptr && options->define_token.size > 0) {
			get_macros(options->define_token, options->defined_macros, options->allocator, std::false_type{});
		}

		if (options->conditional_macros != nullptr) {
			if (options->ifdef_token.size > 0) {
				get_macros(options->ifdef_token, options->conditional_macros, options->allocator, std::true_type{});
			}

			if (options->elif_token.size > 0) {
				get_macros(options->elif_token, options->conditional_macros, options->allocator, std::true_type{});
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void GetConditionalPreprocessorDirectivesCSource(ConditionalPreprocessorDirectives* directives)
	{
		directives->if_directive = "#ifdef";
		directives->elseif_directive = "#elif";
		directives->else_directive = "#else";
		directives->end_directive = "#endif";
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> PreprocessCFile(Stream<char> source_code, Stream<Stream<char>> external_macros, bool add_internal_macro_defines)
	{
		ECS_STACK_CAPACITY_STREAM(Stream<char>, total_macros, 512);
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temp_allocator, ECS_KB * 64, ECS_MB);

		Stream<Stream<char>> macros = external_macros;

		if (add_internal_macro_defines) {
			total_macros.CopyOther(external_macros);

			// Copy these macros with the stack allocator - if they reference the source code
			// and things get moved around then they will become invalid
			SourceCodeMacros source_macros;
			GetSourceCodeMacrosCTokens(&source_macros);
			source_macros.defined_macros = &total_macros;
			source_macros.allocator = &temp_allocator;
			GetSourceCodeMacros(source_code, &source_macros);
			macros = total_macros;
		}

		source_code = RemoveSingleLineComment(source_code, ECS_C_FILE_SINGLE_LINE_COMMENT_TOKEN);
		source_code = RemoveMultiLineComments(source_code, ECS_C_FILE_MULTI_LINE_COMMENT_OPENED_TOKEN, ECS_C_FILE_MULTI_LINE_COMMENT_CLOSED_TOKEN);

		ConditionalPreprocessorDirectives directives;
		GetConditionalPreprocessorDirectivesCSource(&directives);
		source_code = RemoveConditionalPreprocessorBlock(source_code, &directives, macros);

		return source_code;
	}

	// --------------------------------------------------------------------------------------------------

}