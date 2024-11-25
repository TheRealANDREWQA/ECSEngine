#include "ecspch.h"
#include "Tokenize.h"
#include "StringUtilities.h"
#include "Utilities.h"
#include "../Allocators/ResizableLinearAllocator.h"

namespace ECSEngine {

	void TokenizeString(TokenizedString& string, Stream<char> separators, bool expand_separator_type_value, Stream<char> whitespace_characters) {
		// Early return if it is empty.
		if (string.string.size == 0) {
			return;
		}

		// Use fast SIMD check for whitespace characters in order to avoid having to test each individual character multiple
		ECS_STACK_CAPACITY_STREAM(Vec32uc, separator_simd, 32);
		ECS_STACK_CAPACITY_STREAM(Vec32uc, whitespace_simd, 8);
		ECS_ASSERT(separators.size <= separator_simd.capacity, "Too many separator characters for TokenizeString.");
		ECS_ASSERT(whitespace_characters.size <= whitespace_simd.capacity, "Too many separator characters for TokenizeString.");

		// Splat the separators and whitespace characters
		for (size_t index = 0; index < separators.size; index++) {
			separator_simd[index] = separators[index];
		}
		for (size_t index = 0; index < whitespace_characters.size; index++) {
			whitespace_simd[index] = whitespace_characters[index];
		}

		unsigned int token_start = -1;
		Vec32cb compare_mask_true = true;
		for (size_t index = 0; index < string.string.size; index += separator_simd[0].size()) {
			// Load the values
			Vec32uc current_values = Vec32uc().load(string.string.buffer + index);
			
			// Compare against all SIMD whitespace characters and separators, but keep a mask with the characters that are separators as well
			Vec32cb is_not_separator_or_whitespace = compare_mask_true;
			Vec32cb is_not_separator = compare_mask_true;
			for (size_t simd_index = 0; simd_index < separators.size; simd_index++) {
				is_not_separator &= separator_simd[simd_index] != current_values;
			}
			for (size_t simd_index = 0; simd_index < whitespace_characters.size; simd_index++) {
				is_not_separator_or_whitespace &= whitespace_simd[simd_index] != current_values;
			}
			// And the is_not_separator with the main mask
			is_not_separator_or_whitespace &= is_not_separator;

			// Compress the masks into an integer value
			unsigned int is_not_separator_or_whitespace_int = to_bits(is_not_separator_or_whitespace);
			unsigned int is_not_separator_int = to_bits(is_not_separator);
			unsigned int is_separator_or_whitespace = ~is_not_separator_or_whitespace_int;
			unsigned int is_separator = ~is_not_separator_int;

			// Do an initial retrieval of the token start and separator location, if it is empty
			unsigned int separator_token_bit_offset = 0;
			unsigned int separator_token_lsb = FirstLSB(SafeRightShift(is_separator, separator_token_bit_offset));

			unsigned int normal_token_bit_offset = 0;
			auto token_lsb_handling = [&]() {
				unsigned int first_token_lsb = FirstLSB(SafeRightShift(is_not_separator_or_whitespace_int, normal_token_bit_offset));
				if (first_token_lsb != -1) {
					token_start = index + (size_t)first_token_lsb + (size_t)normal_token_bit_offset;
					normal_token_bit_offset += first_token_lsb + 1;
					// Ensure that the token is in the string range
					if (token_start >= string.string.size) {
						token_start = -1;
					}
					else {
						// If we have separators before the token start, add them
						while (separator_token_lsb != -1) {
							unsigned int separator_token_index = index + (size_t)separator_token_lsb + (size_t)separator_token_bit_offset;
							// If it smaller than token start, than it is automatically in bounds
							if (separator_token_index < token_start) {
								unsigned int separator_type = ECS_TOKEN_TYPE_SEPARATOR;
								if (expand_separator_type_value) {
									for (size_t separator_index = 0; separator_index < separators.size; separator_index++) {
										if (separators[separator_index] == string.string[separator_token_index]) {
											separator_type = (unsigned int)separator_index ;
											break;
										}
									}
								}
								string.tokens.Add({ separator_token_index, 1, separator_type });
							}
							else {
								break;
							}

							separator_token_bit_offset += separator_token_lsb + 1;
							separator_token_lsb = FirstLSB(SafeRightShift(is_separator, separator_token_bit_offset));
						}
					}
				}
			};

			if (token_start == -1) {
				token_lsb_handling();
			}

			while (token_start != -1) {
				// Use the inverted mask and search for the LSB
				unsigned int first_whitespace_lsb = FirstLSB(SafeRightShift(is_separator_or_whitespace, normal_token_bit_offset));
				if (first_whitespace_lsb == -1) {
					// No whitespace character found, continue
					break;
				}

				// The token can be finalized
				size_t final_token_index = index + (size_t)first_whitespace_lsb + (size_t)normal_token_bit_offset;
				// It must be in bounds
				if (final_token_index < string.string.size) {
					string.tokens.Add({ token_start, (unsigned int)final_token_index - token_start, ECS_TOKEN_TYPE_GENERAL });
					// Reset the token start
					token_start = -1;
					normal_token_bit_offset += first_whitespace_lsb + 1;
				}
				else {
					// Can exit, out of bounds
					break;
				}

				// Handle the token lsb now
				token_lsb_handling();
			}

			// Add the remaining separators
			while (separator_token_lsb != -1) {
				unsigned int separator_token_index = index + (size_t)separator_token_lsb + (size_t)separator_token_bit_offset;
				if (separator_token_index >= string.string.size) {
					break;
				}

				unsigned int separator_type = ECS_TOKEN_TYPE_SEPARATOR;
				if (expand_separator_type_value) {
					for (size_t separator_index = 0; separator_index < separators.size; separator_index++) {
						if (separators[separator_index] == string.string[separator_token_index]) {
							separator_type = (unsigned int)separator_index;
							break;
						}
					}
				}
				string.tokens.Add({ separator_token_index, 1, separator_type });
				separator_token_bit_offset += separator_token_lsb + 1;
				separator_token_lsb = FirstLSB(SafeRightShift(is_separator, separator_token_bit_offset));
			}
		}

		// If we still have a set token start, then it means it spans until the end of the string
		if (token_start != -1) {
			string.tokens.Add({ token_start, (unsigned int)string.string.size - token_start });
		}

		// We can't have any remaining separators left, those are automatically found inside the loop
	}

	bool CheckTokenizedString(const TokenizedString& string, Stream<char> separators, bool expand_separator_type_value, Stream<char> whitespace_characters) {
		for (size_t index = 0; index < string.tokens.Size(); index++) {
			Stream<char> token = string[index];
			for (size_t subindex = 0; subindex < token.size; subindex++) {
				if (string.tokens[index].type == ECS_TOKEN_TYPE_GENERAL) {
					for (size_t i = 0; i < separators.size; i++) {
						if (token[subindex] == separators[i]) {
							return false;
						}
					}
				}
				for (size_t i = 0; i < whitespace_characters.size; i++) {
					if (token[subindex] == whitespace_characters[i]) {
						return false;
					}
				}
			}
		}

		for (size_t index = 0; index < string.string.size; index++) {
			for (size_t i = 0; i < separators.size; i++) {
				if (string.string[index] == separators[i]) {
					// Check that it exists in the token stream
					size_t subindex = 0;
					for (; subindex < string.tokens.Size(); subindex++) {
						if (string.tokens[subindex].offset == index) {
							if (expand_separator_type_value) {
								if (string.tokens[subindex].type != i) {
									return false;
								}
							}
							else {
								if (string.tokens[subindex].type != ECS_TOKEN_TYPE_SEPARATOR) {
									return false;
								}
							}
							break;
						}
					}
					if (subindex == string.tokens.Size()) {
						return false;
					}
				}
			}
		}

		return true;
	}

	unsigned int TokenizeFindToken(const TokenizedString& string, Stream<char> token, TokenizedString::Subrange token_subrange) {
		unsigned int token_count = token_subrange.count;
		for (unsigned int index = 0; index < token_count; index++) {
			if (string[token_subrange[index]] == token) {
				return index;
			}
		}
		return -1;
	}

	unsigned int TokenizeFindToken(const TokenizedString& string, Stream<char> token, unsigned int token_type, TokenizedString::Subrange token_subrange) {
		unsigned int token_count = token_subrange.count;
		for (unsigned int index = 0; index < token_count; index++) {
			if (string.tokens[token_subrange[index]].type == token_type && string[token_subrange[index]] == token) {
				return index;
			}
		}
		return -1;
	}

	unsigned int TokenizeFindTokenByType(const TokenizedString& string, unsigned int token_type, TokenizedString::Subrange token_subrange) {
		unsigned int token_count = token_subrange.count;
		for (unsigned int index = 0; index < token_count; index++) {
			if (string.tokens[token_subrange[index]].type == token_type) {
				return index;
			}
		}
		return -1;
	}

	uint2 TokenizeFindMatchingPair(const TokenizedString& string, Stream<char> open_token, Stream<char> closed_token, TokenizedString::Subrange token_subrange) {
		unsigned int token_count = token_subrange.count;
		unsigned int first_open_index = TokenizeFindToken(string, open_token, token_subrange);
		if (first_open_index == -1) {
			return { (unsigned int)-1, (unsigned int)-1 };
		}

		size_t open_count = 1;
		// After finding the first opened, find the matching pair while also taking into account
		// The other open token count
		unsigned int index = first_open_index + 1;
		while (index < token_count && open_count > 0) {
			if (string[token_subrange[index]] == open_token) {
				open_count++;
			}
			else if (string[token_subrange[index]] == closed_token) {
				open_count--;
			}

			index++;
		}

		// No matching token was found
		if (open_count > 0) {
			return { (unsigned int)-1, (unsigned int)-1 };
		}
		
		// The index of the closed token is the last index -1, since the while increments it afterwards
		return { first_open_index, index - 1 };
	}

	uint2 TokenizeFindMatchingPairByType(const TokenizedString& string, unsigned int open_type, unsigned int closed_type, TokenizedString::Subrange token_subrange) {
		unsigned int token_count = token_subrange.count;
		unsigned int first_open_index = TokenizeFindTokenByType(string, open_type, token_subrange);
		if (first_open_index == -1) {
			return { (unsigned int)-1, (unsigned int)-1 };
		}

		size_t open_count = 1;
		// After finding the first opened, find the matching pair while also taking into account
		// The other open token count
		unsigned int index = first_open_index + 1;
		while (index < token_count && open_count > 0) {
			if (string.tokens[token_subrange[index]].type == open_type) {
				open_count++;
			}
			else if (string.tokens[token_subrange[index]].type == closed_type) {
				open_count--;
			}

			index++;
		}

		// No matching token was found
		if (open_count > 0) {
			return { (unsigned int)-1, (unsigned int)-1 };
		}

		// The index of the closed token is the last index -1, since the while increments it afterwards
		return { first_open_index, index - 1 };
	}

	Stream<char> TokenizeMergeEntries(const TokenizedString& string, TokenizedString::Subrange sequence, AllocatorPolymorphic allocator) {
		Stream<char> merged_string;
		
		size_t total_size = 0;
		for (unsigned int index = 0; index < sequence.count; index++) {
			total_size += string[sequence[index]].size;
		}
		merged_string.Initialize(allocator, total_size);
		merged_string.size = 0;

		for (unsigned int index = 0; index < sequence.count; index++) {
			merged_string.AddStream(string[sequence[index]]);
		}

		return merged_string;
	}

	void TokenizeSplitBySeparator(const TokenizedString& string, unsigned int separator_type, AdditionStream<TokenizedString::Subrange> token_ranges) {
		unsigned int range_start = 0;
		TokenizedString::Subrange find_subrange = string.GetSubrangeUntilEnd(0);
		unsigned int separator_index = TokenizeFindTokenByType(string, separator_type, find_subrange);
		while (separator_index != -1) {
			token_ranges.Add({ range_start, separator_index - range_start });
			range_start = separator_index + 1;
			find_subrange = find_subrange.GetSubrangeAfterUntilEnd(separator_index);
			separator_index = TokenizeFindTokenByType(string, separator_type, find_subrange);
		}

		if (range_start < string.tokens.Size()) {
			token_ranges.Add({ range_start, string.tokens.Size() - range_start });
		}
	}

	// A definition with its parsed contents
	struct ParsedDefinition {
		Stream<char> name;
		TokenizeRule rule;
	};

	// It will allocate the final stream from the given allocator
	static Stream<TokenizeRuleEntry> ParseTokenizeRuleSet(Stream<char> string, AllocatorPolymorphic stack_allocator, Stream<ParsedDefinition> definitions, CapacityStream<char>* error_message) {
		Stream<TokenizeRuleEntry> result;

		// When a new expression is encountered, we must add a new entry to a stack of expressions
		ResizableStream<ResizableStream<TokenizeRuleEntry>> expression_blocks(stack_allocator, 1);
		bool last_entry_in_progress = false;
		bool is_current_string_selection = false;
		// This is the string that is building up for a [] block or when a normal string is created, like in typedef. (which means the token typedef appears once)
		ResizableStream<ResizableStream<char>> string_buildup(stack_allocator, 4);
		TokenizeRuleEntry* last_entry = nullptr;
		size_t current_index = 0;

		auto skip_whitespaces = [&]() {
			while (current_index < string.size && (string[current_index] == ' ' || string[current_index] == '\t')) {
				current_index++;
			}
		};

		auto advance_index = [&]() {
			current_index++;
			skip_whitespaces();
		};

		auto reserve_string_buildup = [&]() {
			string_buildup.Reserve();
			string_buildup.size++;
			string_buildup.Last().Initialize(stack_allocator, 4);
		};
		// Reserve an initial buildup
		reserve_string_buildup();

		auto reset_string_buildup = [&]() {
			string_buildup.size = 1;
			string_buildup[0].Reset();
		};

		auto reserve_token = [&]() {
			ResizableStream<TokenizeRuleEntry>& expression_rule = expression_blocks.Last();
			expression_rule.Reserve();
			expression_rule.size++;
			last_entry = &expression_rule.Last();
			last_entry->is_selection_negated = false;
			last_entry_in_progress = false;
			// By default, it is a string selection
			last_entry->selection_type = ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING;
		};

		auto set_char_for_token = [&](char character) {
			string_buildup.Last().Add(character);
		};

		auto set_match_string_for_last_token = [&]() {
			if (string_buildup.size == 1) {
				last_entry->selection_data.is_multiple_strings = false;
				last_entry->selection_data.string = string_buildup[0].Copy(stack_allocator);
			}
			else {
				last_entry->selection_data.is_multiple_strings = true;
				last_entry->selection_data.strings.Initialize(stack_allocator, string_buildup.size);
				for (size_t index = 0; index < string_buildup.size; index++) {
					last_entry->selection_data.strings[index].InitializeAndCopy(stack_allocator, string_buildup[index]);
				}
			}
		};

		// Returns false if an error was encountered, else false. If it fails, it will set an error message.
		// It will advance to the next character as well.
		auto parse_count_type_character = [&]() {
			advance_index();

			if (current_index >= string.size) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Expected a count type character, but it is outside the expression block. Rule set: {#}", string);
				return false;
			}

			switch (string[current_index]) {
			case '.':
				last_entry->count_type = ECS_TOKENIZE_RULE_ONE;
				break;
			case '+':
				last_entry->count_type = ECS_TOKENIZE_RULE_ONE_OR_MORE;
				break;
			case '*':
				last_entry->count_type = ECS_TOKENIZE_RULE_ZERO_OR_MORE;
				break;
			case '?':
				last_entry->count_type = ECS_TOKENIZE_RULE_ZERO_OR_ONE;
				break;
			default:
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Expected a count type character, but a non count type character was found. Rule set: {#}", string);
				return false;
			}
			last_entry_in_progress = false;

			return true;
		};

		// Returns false if an error was encountered, else true
		auto finish_token = [&](bool is_string_selection) {
			if (last_entry_in_progress) {
				// This is an error, an intra token was identified while parsing another token
				return false;
			}

			if (is_string_selection) {
				set_match_string_for_last_token();
			}
			reserve_token();
			reset_string_buildup();
			return true;
		};

		// Returns true if it succeeded, else false. It will write an error message if it fails
		auto enter_expression = [&]() {
			if (is_current_string_selection) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Cannot use ( without escaping it inside a []. Rule set: {#}", string);
				return false;
			}

			if (last_entry_in_progress) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Encountered a ( while the previous token was not finished. Rule set: {#}", string);
				return false;
			}

			expression_blocks.Reserve(1);
			expression_blocks.size++;
			expression_blocks.Last().Initialize(stack_allocator, 1);
			// We must also reserve a new token
			reserve_token();
			return true;
		};

		// Returns true if it succeeded, else false. It will write an error message if it fails
		auto exit_expression = [&]() {
			Stream<TokenizeRuleEntry> rule = expression_blocks.Last();
			expression_blocks.size--;
			// There is always one more entry, because of the reserve
			rule.size--;

			// Add the rule (subexpression) to the previous expression block, or to the result if it is the last expression
			if (expression_blocks.size == 0) {
				result = rule;
			}
			else {
				// We must change the last entry
				last_entry = &expression_blocks.Last().Last();

				// Add the previous rule as a subexpression
				last_entry->selection_type = ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE;
				last_entry->selection_data.subrule.sets.Initialize(stack_allocator, 1);
				last_entry->selection_data.subrule.sets[0].entries = rule;

				if (!parse_count_type_character()) {
					return false;
				}

				finish_token(false);
			}
			return true;
		};

		// Returns true if it succeeded, else false. It will write an error message if it fails
		auto enter_selection_block = [&]() {
			if (is_current_string_selection) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Cannot use [ without escaping it inside a []. Rule set: {#}", string);
				return false;
			}

			if (last_entry_in_progress) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Encountered a [ while the previous token was not finished. Rule set: {#}", string);
				return false;
			}

			is_current_string_selection = true;
			return true;
		};

		// Returns true if it succeeded, else false. It will write an error message if it fails.
		auto exit_selection_block = [&]() {
			if (!is_current_string_selection) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Mismatched [] pair. A [ is missing or there is an additional ]. Rule set: {#}", string);
				return false;
			}

			is_current_string_selection = false;
			if (!parse_count_type_character()) {
				return false;
			}
			if (!finish_token(true)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Failed to finish a [] block. Rule set: {#}", string);
				return false;
			}
			return true;
		};

		enter_expression();

		// This value is used to determine if we have unmatched character pairs
		size_t matched_pair_characters = 0;

		// Skip any initial whitespaces
		skip_whitespaces();
		while (current_index < string.size) {
			// Try to match a previous definition first
			Stream<char> current_string = string.SliceAt(current_index);
			size_t definition_index = 0;
			if (is_current_string_selection) {
				// When inside a [] block, don't match identifiers
				definition_index = definitions.size;
			}
			else {
				for (; definition_index < definitions.size; definition_index++) {
					if (current_string.StartsWith(definitions[definition_index].name)) {
						last_entry->selection_type = ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE;
						last_entry->selection_data.subrule = definitions[definition_index].rule;
						current_index += definitions[definition_index].name.size;
						skip_whitespaces();
						// This entry is in progress now
						last_entry_in_progress = true;
						break;
					}
				}
			}

			if (definition_index == definitions.size) {
				// It didn't match a definition. Check $ characters first, easier to deal with in a separate branch
				if (string[current_index] == '$') {
					advance_index();
					if (current_index == string.size) {
						// This is an erroneous case, exit
						ECS_FORMAT_ERROR_MESSAGE(error_message, "$ at the end of an expression block is not allowed. Rule set: {#}", string);
						return result;
					}

					// It is an escaped character or token selection - check to see which one
					switch (string[current_index]) {
					case 'T':
					{
						if (is_current_string_selection) {
							// Fail, this shouldn't appear inside a []
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Cannot use $T inside a [] block. Rule set: {#}", string);
							return result;
						}

						last_entry->selection_type = ECS_TOKENIZE_RULE_SELECTION_ANY;
						if (!parse_count_type_character()) {
							return result;
						}
						if (!finish_token(false)) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Found a $T sequence inside another token. Rule set: {#}", string);
							return result;
						}
					}
					break;
					case 'G':
					{
						if (is_current_string_selection) {
							// Fail, this shouldn't appear inside a []
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Cannot use $G inside a [] block. Rule set: {#}", string);
							return result;
						}

						last_entry->selection_type = ECS_TOKENIZE_RULE_SELECTION_GENERAL;
						if (!parse_count_type_character()) {
							return result;
						}
						if (!finish_token(false)) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Found a $G sequence inside another token. Rule set: {#}", string);
							return result;
						}
					}
					break;
					case 'S':
					{
						if (is_current_string_selection) {
							// Fail, this shouldn't appear inside a []
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Cannot use $S inside a [] block. Rule set: {#}", string);
							return result;
						}

						last_entry->selection_type = ECS_TOKENIZE_RULE_SELECTION_SEPARATOR;
						if (!parse_count_type_character()) {
							return result;
						}
						if (!finish_token(false)) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Found a $S sequence inside another token. Rule set: {#}", string);
							return result;
						}
					}
					break;
					// Check the escaped characters
					case '(':
					case ')':
					case '[':
					case ']':
					case '.':
					case '+':
					case '*':
					case '?':
						last_entry_in_progress = true;
						set_char_for_token(string[current_index]);
						break;

					default:
						// Default, error
						ECS_FORMAT_ERROR_MESSAGE(error_message, "$ is followed by a character that is not allowed. Rule set: {#}", string);
						return result;
					}
				}
				else {
					// Not an escaped character. Start by checking other special characters
					switch (string[current_index]) {
					case '!':
						last_entry->is_selection_negated = true;
						break;
					case ',':
					{
						// If inside a selection block, then reserve the token buildup
						if (is_current_string_selection) {
							reserve_string_buildup();
						}
						else {
							last_entry_in_progress = true;
							// A normal comma character
							set_char_for_token(string[current_index]);
						}
					}
					break;
					case '(':
					{
						// A new expression should begin
						if (!enter_expression()) {
							return result;
						}
					}
					break;
					case ')':
					{
						if (last_entry_in_progress) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Encountered a ) while the previous token was not finished. Rule set: {#}", string);
							return result;
						}

						// The existing expression should end
						if (expression_blocks.size == 1) {
							// This is invalid, it means that the () are not matched
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Mismatched (). Rule set: {#}", string);
							return result;
						}
						if (!exit_expression()) {
							return result;
						}
					}
					break;
					case '[':
					{
						// A new selection block
						if (!enter_selection_block()) {
							return result;
						}
					}
					break;
					case ']':
					{
						if (!exit_selection_block()) {
							return result;
						}
					}
					break;
					case '\\':
					{
						// Get the next character
						advance_index();
						if (current_index == string.size) {
							// Erroneous case, exit
							ECS_FORMAT_ERROR_MESSAGE(error_message, "A \\ is at the end of an expression block, which is not allowed. Rule set: {#}", string);
							return result;
						}

						last_entry->count_type = ECS_TOKENIZE_RULE_ONE_PAIR_START;
						last_entry->selection_type = ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING;
						matched_pair_characters++;

						set_char_for_token(string[current_index]);
						// The token can be finished
						if (!finish_token(true)) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "\\ sequence inside another token. Rule set: {#}", string);
							return result;
						}
					}
					break;
					case '/':
					{
						// It is a matched pairing.
						if (matched_pair_characters == 0) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Encountered closed pairing for /, but no previous open pairing exists. Rule set: {#}", string);
							return result;
						}

						advance_index();
						if (current_index == string.size) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Encountered / at the end of the expression block. Rule set: {#}", string);
							return result;
						}

						last_entry->count_type = ECS_TOKENIZE_RULE_ONE_PAIR_END;
						last_entry->selection_type = ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING;
						matched_pair_characters--;

						set_char_for_token(string[current_index]);
						// The token can be finished
						if (!finish_token(true)) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Found a / sequence inside another token. Rule set: {#}", string);
							return result;
						}
					}
					break;
					case '.':
					case '+':
					case '*':
					case '?':
					{
						if (!last_entry_in_progress) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Found a count type character, but no token was started. Rule set: {#}", string);
							return result;
						}
						last_entry_in_progress = false;
						switch (string[current_index]) {
						case '.':
							last_entry->count_type = ECS_TOKENIZE_RULE_ONE;
							break;
						case '+':
							last_entry->count_type = ECS_TOKENIZE_RULE_ONE_OR_MORE;
							break;
						case '*':
							last_entry->count_type = ECS_TOKENIZE_RULE_ZERO_OR_MORE;
							break;
						case '?':
							last_entry->count_type = ECS_TOKENIZE_RULE_ZERO_OR_ONE;
							break;
						default:
							ECS_ASSERT(false);
						}
						// Set the string buildup only if this is not a subrule
						if (!finish_token(last_entry->selection_type != ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE)) {
							ECS_FORMAT_ERROR_MESSAGE(error_message, "Could not end token after finding count type character. Rule set: {#}", string);
							return result;
						}
					}
					break;
					default:
						// A normal character
						last_entry_in_progress = true;
						set_char_for_token(string[current_index]);
					}

				}

				advance_index();
			}
		}

		// If the last last entry is still in progress, fail
		if (last_entry_in_progress) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "The final token is still in progress. Rule set: {#}", string);
			return result;
		}

		if (!exit_expression()) {
			return {};
		}
		return result;
	}

	// It will try to optimize the tokenize rule, such that the matching can be done faster.
	// It assumes that the given rule is validated beforehand.
	static void OptimizeTokenizeRule(TokenizeRule& rule, AllocatorPolymorphic allocator, bool deallocate_existing_entries) {
		// Pull out subrules that contain a single set with a single entry, these can be safely pulled out.
		// Another thing that we must perform is to check if we have variable length consecutive entries.
		// In that case, we need to do the following:
		// If we have zero or more followed entries intermingled (optionally with one or more entries) with the same target, i.e. general token,
		// separator or ANY token, then the all but one entry can be removed, because they are engulfed by the remaining entry.
		// We can do this only if the same target token is used, else it can't be applied.
		// If one or more entries are intermingled with the same target, all but one can be transformed into match one, since the variable
		// Length entry will engulf the others. Again, it is important that the same target is used.

		// This is the pass where variable length entries are reduced
		for (size_t set_index = 0; set_index < rule.sets.size; set_index++) {
			for (size_t entry_index = 0; entry_index < rule.sets[set_index].entries.size - 1; entry_index++) {
				TokenizeRuleEntry& current_entry = rule.sets[set_index][entry_index];
				TokenizeRuleEntry& second_entry = rule.sets[set_index][entry_index + 1];
				if (current_entry.count_type == ECS_TOKENIZE_RULE_ONE_OR_MORE) {
					if (second_entry.count_type == ECS_TOKENIZE_RULE_ONE_OR_MORE && current_entry.Compare(second_entry)) {
						// Change the current entry to be one, such that the cascade can continue
						current_entry.count_type = ECS_TOKENIZE_RULE_ONE;
					}
					else if (second_entry.count_type == ECS_TOKENIZE_RULE_ZERO_OR_MORE && current_entry.Compare(second_entry)) {
						// Remove the second entry
						if (deallocate_existing_entries) {
							rule.sets[set_index][entry_index + 1].Deallocate(allocator);
						}
						rule.sets[set_index].entries.Remove(entry_index + 1);
						// Decrement the index such that we come back again to compare the next entry
						entry_index--;
					}
				}
				else if (current_entry.count_type == ECS_TOKENIZE_RULE_ZERO_OR_MORE) {
					if (second_entry.count_type == ECS_TOKENIZE_RULE_ZERO_OR_MORE && current_entry.Compare(second_entry)) {
						// Remove the second entry
						if (deallocate_existing_entries) {
							rule.sets[set_index][entry_index + 1].Deallocate(allocator);
						}
						rule.sets[set_index].entries.Remove(entry_index + 1);
						// Decrement the index such that we come back again to compare the next entry
						entry_index--;
					}
					else if (second_entry.count_type == ECS_TOKENIZE_RULE_ONE_OR_MORE && current_entry.Compare(second_entry)) {
						// Remove this current entry
						if (deallocate_existing_entries) {
							rule.sets[set_index][entry_index].Deallocate(allocator);
						}
						rule.sets[set_index].entries.Remove(entry_index);
						// Decrement the index such that we come back to retest the next entry
						entry_index--;
					}
				}
			}
		}

		// This is the single entry subrule pass
		for (size_t set_index = 0; set_index < rule.sets.size; set_index++) {
			for (size_t entry_index = 0; entry_index < rule.sets[set_index].entries.size; entry_index++) {
				TokenizeRuleEntry& entry = rule.sets[set_index][entry_index];
				if (entry.selection_type == ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE) {
					OptimizeTokenizeRule(entry.selection_data.subrule, allocator, deallocate_existing_entries);
					// After it has been optimized, check to see if it has one entry
					if (entry.selection_data.subrule.sets.size == 1 && entry.selection_data.subrule.sets[0].entries.size == 1
						&& entry.selection_data.subrule.sets[0][0].selection_type != ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE) {
						// Can be pulled out
						TokenizeRuleEntry entry_to_move = entry.selection_data.subrule.sets[0][0];
						if (deallocate_existing_entries) {
							entry.selection_data.subrule.sets.Deallocate(allocator);
						}
						entry = entry_to_move;
					}
				}
			}
		}
	}

	TokenizeRule CreateTokenizeRule(Stream<char> string_rule, AllocatorPolymorphic allocator, bool allocator_is_temporary, CapacityStream<char>* error_message) {
		AllocatorPolymorphic allocator_to_use = allocator;
		ResizableLinearAllocator temporary_allocator;
		if (!allocator_is_temporary) {
			// Stack alloc survives the scope block
			temporary_allocator = ResizableLinearAllocator(ECS_STACK_ALLOC(ECS_KB * 32), ECS_KB * 32, ECS_MB * 32, { nullptr });
			allocator_to_use = &temporary_allocator;
		}
		
		ResizableStream<Stream<char>> lines(allocator_to_use, 8);
		SplitString(string_rule, '\n', &lines);

		// We are using an array for the definitions, even tho they could be put in a hash table, because
		// There will be few entries.
		ResizableStream<ParsedDefinition> parsed_definitions(allocator_to_use, 8);
		// Parse the definitions first
		for (unsigned int index = 0; index < lines.size - 1; index++) {
			// Split the string by the equals - there should be just a single equals, if there are more fail
			ECS_STACK_CAPACITY_STREAM(Stream<char>, split_string, 8);
			SplitString(lines[index], '=', &split_string);
			if (split_string.size != 2) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Definition must have a single equals: {#}", lines[index]);
				return TokenizeRule();
			}

			Stream<char> left_part = split_string[0];
			left_part = SkipWhitespace(left_part);
			left_part = SkipWhitespace(left_part, -1);

			Stream<char> right_part = split_string[1];
			right_part = SkipWhitespace(right_part);
			right_part = SkipWhitespace(right_part, -1);

			ParsedDefinition definition;
			definition.name = left_part;

			// Split the right part by the number of |s
			ResizableStream<Stream<char>> right_part_sets(allocator_to_use, 2);
			SplitString(right_part, '|', &right_part_sets);
			definition.rule.sets.Initialize(allocator_to_use, right_part_sets.size);
			for (unsigned int set_index = 0; set_index < right_part_sets.size; set_index++) {
				definition.rule.sets[set_index].entries = ParseTokenizeRuleSet(right_part_sets[set_index], allocator_to_use, parsed_definitions, error_message);
				if (definition.rule.sets[set_index].entries.size == 0) {
					return TokenizeRule();
				}

			}

			// Validate the definition
			if (!ValidateTokenizeRule(definition.rule)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Definition {#} is not valid", lines[index]);
				return TokenizeRule();
			}

			// Optimize the definition. This is always going to be a temporary allocator
			OptimizeTokenizeRule(definition.rule, allocator_to_use, false);

			parsed_definitions.Add(&definition);
		}

		TokenizeRule rule;
		// Parse the main rule set
		ResizableStream<Stream<char>> main_rule_sets(allocator_to_use, 4);
		SplitString(lines.Last(), '|', &main_rule_sets);
		rule.sets.Initialize(allocator_to_use, main_rule_sets.size);
		for (unsigned int set_index = 0; set_index < main_rule_sets.size; set_index++) {
			rule.sets[set_index].entries = ParseTokenizeRuleSet(main_rule_sets[set_index], allocator_to_use, parsed_definitions, error_message);
			if (rule.sets[set_index].entries.size == 0) {
				return TokenizeRule();
			}
		}

		// Validate the rule
		if (!ValidateTokenizeRule(rule)) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "The definition {#} is not valid", lines.Last());
			return TokenizeRule();
		}

		// Finally, optimize the rule. This is always going to be a temporary allocator
		OptimizeTokenizeRule(rule, allocator_to_use, false);
		
		// Compute the cached rule values
		rule.ComputeMinimumTokenCount();

		// If the allocator is already temporary, we can return it directly, else we need to make a copy of it
		if (allocator_is_temporary) {
			return rule;
		}
		return rule.Copy(allocator);
	}

	bool MatchTokenizeRule(const TokenizedString& string, TokenizedString::Subrange subrange, const TokenizeRule& rule, CapacityStream<unsigned int>* matched_token_counts) {

	}

	bool ValidateTokenizeRule(const TokenizeRule& rule) {
		// At the moment, there is nothing to be validated
		return true;
	}

	void TokenizeRuleMatcher::AddExcludeRule(const TokenizeRule& rule, bool deep_copy) {
		if (deep_copy) {
			exclude_rules.Add(rule.Copy(Allocator()));
		}
		else {
			exclude_rules.Add(&rule);
		}
	}

	void TokenizeRuleMatcher::AddAction(const TokenizeRuleAction& action, bool deep_copy) {
		TokenizeRuleAction copy;
		const TokenizeRuleAction* action_pointer = &action;
		if (deep_copy) {
			action_pointer = &copy;
			copy = action;
			copy.rule = action.rule.Copy(Allocator());
			copy.callback_data = CopyNonZero(Allocator(), action.callback_data);
		}
		actions.Add(action_pointer);
	}

	void TokenizeRuleMatcher::Deallocate(bool deallocate_exclude_rules, bool deallocate_actions) {
		if (deallocate_exclude_rules) {
			StreamDeallocateElements(exclude_rules, Allocator());
		}
		if (deallocate_actions) {
			for (unsigned int index = 0; index < actions.size; index++) {
				actions[index].callback_data.Deallocate(Allocator());
				actions[index].rule.Deallocate(Allocator());
			}
		}

		exclude_rules.FreeBuffer();
		actions.FreeBuffer();
	}

	void TokenizeRuleMatcher::Initialize(AllocatorPolymorphic allocator) {
		exclude_rules.Initialize(allocator, 0);
		actions.Initialize(allocator, 0);
	}

	bool TokenizeRuleMatcher::Match(const TokenizedString& string, TokenizedString::Subrange subrange) {
		while (subrange.count > 0) {
			// Try to match
		}
	}

	bool TokenizeRule::Compare(const TokenizeRule& other) const {
		if (sets.size != other.sets.size) {
			return false;
		}
		
		for (size_t index = 0; index < sets.size; index++) {
			if (sets[index].entries.size != other.sets[index].entries.size) {
				return false;
			}

			for (size_t entry_index = 0; entry_index < sets[index].entries.size; entry_index++) {
				if (!sets[index][entry_index].Compare(other.sets[index][entry_index])) {
					return false;
				}
			}
		}

		return true;
	}

	TokenizeRule TokenizeRule::Copy(AllocatorPolymorphic allocator) const {
		TokenizeRule rule;
		rule.sets = StreamDeepCopy(sets, allocator);
		rule.minimum_token_count = minimum_token_count;
		return rule;
	}

	void TokenizeRule::Deallocate(AllocatorPolymorphic allocator) {
		StreamDeallocateElements(sets, allocator);
		sets.Deallocate(allocator);
	}

	void TokenizeRule::ComputeMinimumTokenCount() {
		unsigned int overall_minimum = UINT_MAX;

		for (size_t set_index = 0; set_index < sets.size; set_index++) {
			unsigned int set_minimum_count = 0;
			for (size_t entry_index = 0; entry_index < sets[set_index].entries.size; entry_index++) {
				TokenizeRuleEntry& entry = sets[set_index][entry_index];
				// Counts that include zero don't contribute
				if (entry.count_type == ECS_TOKENIZE_RULE_ONE || entry.count_type == ECS_TOKENIZE_RULE_ONE_OR_MORE ||
					entry.count_type == ECS_TOKENIZE_RULE_ONE_PAIR_START || entry.count_type == ECS_TOKENIZE_RULE_ONE_PAIR_END) {
					// Subrules are a special case
					if (entry.selection_type == ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE) {
						entry.selection_data.subrule.ComputeMinimumTokenCount();
						set_minimum_count += entry.selection_data.subrule.minimum_token_count;
					}
					else {
						// Normal case, one token required, at the very least
						set_minimum_count++;
					}
				}
			}
			sets[set_index].minimum_token_count = set_minimum_count;
			overall_minimum = min(overall_minimum, set_minimum_count);
		}

		minimum_token_count = overall_minimum;
	}

	bool TokenizeRuleEntry::Compare(const TokenizeRuleEntry& other) const {
		if (count_type != other.count_type) {
			return false;
		}
		return CompareExceptCountType(other);
	}

	bool TokenizeRuleEntry::CompareExceptCountType(const TokenizeRuleEntry& other) const {
		if (is_selection_negated != other.is_selection_negated) {
			return false;
		}
		if (selection_type != other.selection_type) {
			return false;
		}
		
		switch (selection_type) {
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING:
		{
			if (selection_data.is_multiple_strings != other.selection_data.is_multiple_strings) {
				return false;
			}
			if (selection_data.is_multiple_strings) {
				if (selection_data.strings.size != other.selection_data.strings.size) {
					return false;
				}
				for (size_t index = 0; index < selection_data.strings.size; index++) {
					if (selection_data.strings[index] != other.selection_data.strings[index]) {
						return false;
					}
				}
			}
			else {
				if (selection_data.string != other.selection_data.string) {
					return false;
				}
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_TYPE:
		{
			if (selection_data.is_multiple_type_indices != other.selection_data.is_multiple_type_indices) {
				return false;
			}
			if (selection_data.is_multiple_type_indices) {
				if (selection_data.type_indices.size != other.selection_data.type_indices.size) {
					return false;
				}
				if (memcmp(selection_data.type_indices.buffer, other.selection_data.type_indices.buffer, selection_data.type_indices.CopySize()) != 0) {
					return false;
				}
			}
			else {
				if (!selection_data.type_index != other.selection_data.type_index) {
					return false;
				}
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE:
		{
			return selection_data.subrule.Compare(other.selection_data.subrule);
		}
		break;
		default:
			// The defaults are the same
			break;
		}

		return true;
	}

	TokenizeRuleEntry TokenizeRuleEntry::Copy(AllocatorPolymorphic allocator) const {
		TokenizeRuleEntry entry;

		entry.selection_type = selection_type;
		entry.is_selection_negated = is_selection_negated;
		entry.count_type = count_type;

		switch (selection_type) {
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING:
		{
			if (selection_data.is_multiple_strings) {
				entry.selection_data.strings = StreamDeepCopy(selection_data.strings, allocator);
			}
			else {
				entry.selection_data.string = selection_data.string.Copy(allocator);
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_TYPE:
		{
			if (selection_data.is_multiple_type_indices) {
				entry.selection_data.type_indices = selection_data.type_indices.Copy(allocator);
			}
			else {
				entry.selection_data.type_index = selection_data.type_index;
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE:
		{
			entry.selection_data.subrule = selection_data.subrule.Copy(allocator);
		}
		break;
		default:
			// Nothing to be copied
			break;
		}

		return entry;
	}

	void TokenizeRuleEntry::Deallocate(AllocatorPolymorphic allocator) {
		switch (selection_type) {
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING:
		{
			if (selection_data.is_multiple_strings) {
				StreamDeallocateElements(selection_data.strings, allocator);
				selection_data.strings.Deallocate(allocator);
			}
			else {
				selection_data.string.Deallocate(allocator);
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_TYPE:
		{
			if (selection_data.is_multiple_type_indices) {
				selection_data.type_indices.Deallocate(allocator);
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE:
		{
			selection_data.subrule.Deallocate(allocator);
		}
		break;
		default:
			// Nothing to be deallocated
			break;
		}
	}

	void TokenizedString::InitializeResizable(AllocatorPolymorphic allocator) {
		ResizableStream<Token>* allocated_tokens = (ResizableStream<Token>*)Allocate(allocator, sizeof(ResizableStream<Token>));
		// Set an initial small size
		allocated_tokens->Initialize(allocator, 8);
		tokens = allocated_tokens;
	}

	void TokenizedString::DeallocateResizable() {
		AllocatorPolymorphic allocator = tokens.resizable_stream->allocator;
		tokens.resizable_stream->FreeBuffer();
		Deallocate(allocator, tokens.resizable_stream);
	}

}