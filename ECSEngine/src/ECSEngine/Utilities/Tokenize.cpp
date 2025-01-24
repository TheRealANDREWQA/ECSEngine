#include "ecspch.h"
#include "Tokenize.h"
#include "StringUtilities.h"
#include "Utilities.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "Algorithms.h"

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
			string.tokens.Add({ token_start, (unsigned int)string.string.size - token_start, ECS_TOKEN_TYPE_GENERAL });
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

	unsigned int TokenizeFindTokenReverse(const TokenizedString& string, Stream<char> token, TokenizedString::Subrange token_subrange) {
		for (int64_t index = (int64_t)token_subrange.count - 1; index >= 0; index--) {
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

	unsigned int TokenizeFindTokenByTypeReverse(const TokenizedString& string, unsigned int token_type, TokenizedString::Subrange token_subrange) {
		for (int64_t index = (int64_t)token_subrange.count - 1; index >= 0; index--) {
			if (string.tokens[token_subrange[index]].type == token_type) {
				return (unsigned int)index;
			}
		}
		return -1;
	}

	uint2 TokenizeFindMatchingPair(const TokenizedString& string, Stream<char> open_token, Stream<char> closed_token, TokenizedString::Subrange token_subrange, bool search_closed_only) {
		unsigned int token_count = token_subrange.count;
		unsigned int first_open_index = -1;
		if (!search_closed_only) {
			first_open_index = TokenizeFindToken(string, open_token, token_subrange);
			if (first_open_index == -1) {
				return { (unsigned int)-1, (unsigned int)-1 };
			}
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

	uint2 TokenizeFindMatchingPairByType(const TokenizedString& string, unsigned int open_type, unsigned int closed_type, TokenizedString::Subrange token_subrange, bool search_closed_only) {
		unsigned int token_count = token_subrange.count;
		unsigned int first_open_index = -1;
		if (!search_closed_only) {
			first_open_index = TokenizeFindTokenByType(string, open_type, token_subrange);
			if (first_open_index == -1) {
				return { (unsigned int)-1, (unsigned int)-1 };
			}
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

	void TokenizeSplitBySeparator(const TokenizedString& string, Stream<char> separator, TokenizedString::Subrange token_subrange, AdditionStream<TokenizedString::Subrange> token_ranges) {
		unsigned int separator_index = TokenizeFindToken(string, separator, token_subrange);
		while (separator_index != -1) {
			token_ranges.Add({ token_subrange.token_start_index, separator_index });
			token_subrange = token_subrange.GetSubrangeAfterUntilEnd(separator_index);
			separator_index = TokenizeFindToken(string, separator, token_subrange);
		}

		if (token_subrange.count > 0 ) {
			token_ranges.Add({ token_subrange.token_start_index, token_subrange.count });
		}
	}

	void TokenizeSplitBySeparatorType(const TokenizedString& string, unsigned int separator_type, TokenizedString::Subrange token_subrange, AdditionStream<TokenizedString::Subrange> token_ranges) {
		unsigned int separator_index = TokenizeFindTokenByType(string, separator_type, token_subrange);
		while (separator_index != -1) {
			token_ranges.Add({ token_subrange.token_start_index, separator_index });
			token_subrange = token_subrange.GetSubrangeAfterUntilEnd(separator_index);
			separator_index = TokenizeFindTokenByType(string, separator_type, token_subrange);
		}

		if (token_subrange.count > 0) {
			token_ranges.Add({ token_subrange.token_start_index, token_subrange.count });
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
					else if (second_entry.count_type == ECS_TOKENIZE_RULE_ONE && current_entry.Compare(second_entry)) {
						// Remove the next entry, and change this one to a one or more
						if (deallocate_existing_entries) {
							rule.sets[set_index][entry_index].Deallocate(allocator);
						}
						current_entry.count_type = ECS_TOKENIZE_RULE_ONE_OR_MORE;
						rule.sets[set_index].entries.Remove(entry_index + 1);
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
			Stream<char> first_equals = FindFirstCharacter(lines[index], '=');
			if (first_equals.size == 0) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Definition must have an equals sign after the definition name: {#}", lines[index]);
				return TokenizeRule();
			}

			Stream<char> left_part = lines[index].StartDifference(first_equals);
			left_part = TrimWhitespace(left_part);

			Stream<char> right_part = first_equals.AdvanceReturn(1);
			right_part = TrimWhitespace(right_part);

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
			ECS_STACK_CAPACITY_STREAM(char, validate_error_message, 512);
			if (!ValidateTokenizeRule(definition.rule, &validate_error_message)) {
				ECS_FORMAT_ERROR_MESSAGE(error_message, "Definition at line {#} is not valid. Reason: {#}", lines[index], validate_error_message);
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
		ECS_STACK_CAPACITY_STREAM(char, validate_error_message, 512);
		if (!ValidateTokenizeRule(rule, &validate_error_message)) {
			ECS_FORMAT_ERROR_MESSAGE(error_message, "The definition at line {#} is not valid. Reason: {#}", lines.Last(), validate_error_message);
			return TokenizeRule();
		}

		// Finally, optimize the rule. This is always going to be a temporary allocator
		OptimizeTokenizeRule(rule, allocator_to_use, false);
		
		// Compute the cached rule values
		rule.ComputeCachedValues();

		// If the allocator is already temporary, we can return it directly, else we need to make a copy of it
		if (allocator_is_temporary) {
			return rule;
		}
		return rule.Copy(allocator);
	}

	struct TokenizeRuleEntryMatchCount {
		// It is -1 if this is variadic
		unsigned int count;
		// It is true if the token can be missing
		bool is_optional;
		// Set to true if the character requires pairing
		bool requires_pairing;
	};

	static TokenizeRuleEntryMatchCount GetTokenizeEntryMatchCount(const TokenizeRuleEntry& entry) {
		TokenizeRuleEntryMatchCount match_count;
		match_count.requires_pairing = false;
		switch (entry.count_type) {
		case ECS_TOKENIZE_RULE_ONE:
		case ECS_TOKENIZE_RULE_ONE_PAIR_START:
		case ECS_TOKENIZE_RULE_ONE_PAIR_END:
		{
			match_count.count = 1;
			match_count.is_optional = false;
			match_count.requires_pairing = entry.count_type == ECS_TOKENIZE_RULE_ONE_PAIR_START || entry.count_type == ECS_TOKENIZE_RULE_ONE_PAIR_END;
		}
		break;
		case ECS_TOKENIZE_RULE_ZERO_OR_ONE:
		{
			match_count.count = 1;
			match_count.is_optional = true;
		}
		break;
		case ECS_TOKENIZE_RULE_ZERO_OR_MORE:
		{
			match_count.count = -1;
			match_count.is_optional = true;
		}
		break;
		case ECS_TOKENIZE_RULE_ONE_OR_MORE:
		{
			match_count.count = -1;
			match_count.is_optional = false;
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid tokenize rule count type");
		}
		return match_count;
	}

	// Returns true if the entry is fixed length, else false
	static bool IsFixedLengthEntry(const TokenizeRuleEntry& entry) {
		if (entry.selection_type == ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE) {
			// Use the already cached value
			return !entry.selection_data.subrule.is_variable_length;
		}
		else {
			switch (entry.count_type) {
			case ECS_TOKENIZE_RULE_ONE:
			case ECS_TOKENIZE_RULE_ONE_PAIR_START:
			case ECS_TOKENIZE_RULE_ONE_PAIR_END:
				return true;
			case ECS_TOKENIZE_RULE_ONE_OR_MORE:
			case ECS_TOKENIZE_RULE_ZERO_OR_MORE:
			case ECS_TOKENIZE_RULE_ZERO_OR_ONE:
				return false;
			default:
				ECS_ASSERT(false, "Invalid count type for IsFixedLengthEntry");
			}
		}
		return false;
	}

	struct FixedLengthEntryInfo {
		// How many tokens a match requires
		unsigned int token_count;
		// How many matches this entry needs
		unsigned int match_count;
		// Set to true if it is a character of type PAIR_START or PAIR_END
		bool requires_pairing;
	};

	// Returns the number of tokens this fixed length entry needs for a match. You must ensure that this entry is fixed length
	static FixedLengthEntryInfo GetFixedLengthEntryInfo(const TokenizeRuleEntry& entry) {
		// At the moment, we can have only a match count of 1
		bool requires_pairing = entry.count_type == ECS_TOKENIZE_RULE_ONE_PAIR_START || 
			entry.count_type == ECS_TOKENIZE_RULE_ONE_PAIR_END;
		if (entry.selection_type == ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE) {
			return { entry.selection_data.subrule.minimum_token_count, 1, requires_pairing };
		}
		else {
			return { 1, 1, requires_pairing };
		}
	}

	// For an entry that is the beginning of pairing, it will return the index of the end pairing, or -1 if there is no such pairing
	static size_t GetEntryPairing(Stream<TokenizeRuleEntry> set, size_t entry_start_index) {
		size_t match_count = 0;
		for (size_t index = entry_start_index + 1; index < set.size; index++) {
			if (set[index].count_type == ECS_TOKENIZE_RULE_ONE_PAIR_START) {
				match_count++;
			}
			else if (set[index].count_type == ECS_TOKENIZE_RULE_ONE_PAIR_END) {
				if (match_count == 0) {
					return index;
				}
				match_count--;
			}
		}
		return -1;
	}

	// The number of selection matches that need to be performed are given by match_count, or you can optionally set this to -1 to indicate that this query is variable length and that there is no set match count.
	// If you want to match this entry but without exhausting the subrange, provide the last argument and it will tell you how much to advance. If you want to exhaust the range, then leave the final argument as nullptr
	static bool MatchTokenizeEntry(const TokenizedString& string, TokenizedString::Subrange subrange, const TokenizeRuleEntry& entry, unsigned int match_count, bool is_optional, unsigned int* total_token_matched_count) {
		// Check this immediately
		if (subrange.count == 0) {
			return is_optional;
		}
		
		if (match_count != -1 && subrange.count < match_count) {
			return false;
		}

		// If the match count is -1, then we need to exhaust the range
		bool exhaustive_search = total_token_matched_count == nullptr || match_count == -1;
		unsigned int adjusted_match_count = exhaustive_search ? subrange.count : match_count;
		if (total_token_matched_count != nullptr) {
			*total_token_matched_count = match_count;
		}

		switch (entry.selection_type) {
		case ECS_TOKENIZE_RULE_SELECTION_ANY:
		{
			// In this case, we accept any tokens, we can advance further
			if (entry.is_selection_negated) {
				// This doesn't make too much sense
				return false;
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_GENERAL:
		{
			for (unsigned int token_index = 0; token_index < adjusted_match_count; token_index++) {
				bool is_different_token = string.tokens[subrange[token_index]].type != ECS_TOKEN_TYPE_GENERAL;
				if (entry.is_selection_negated) {
					is_different_token = !is_different_token;
				}
				if (is_different_token) {
					return false;
				}
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_SEPARATOR:
		{
			for (unsigned int token_index = 0; token_index < adjusted_match_count; token_index++) {
				bool is_different_token = string.tokens[subrange[token_index]].type != ECS_TOKEN_TYPE_SEPARATOR;
				if (entry.is_selection_negated) {
					is_different_token = !is_different_token;
				}
				if (is_different_token) {
					return false;
				}
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING:
		{
			for (unsigned int token_index = 0; token_index < adjusted_match_count; token_index++) {
				Stream<char> current_token = string[subrange[token_index]];
				bool is_different_token = false;
				if (entry.selection_data.is_multiple_strings) {
					is_different_token = FindString(current_token, entry.selection_data.strings) == -1;
				}
				else {
					is_different_token = current_token != entry.selection_data.string;
				}

				if (entry.is_selection_negated) {
					is_different_token = !is_different_token;
				}
				if (is_different_token) {
					return false;
				}
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_TYPE:
		{
			for (unsigned int token_index = 0; token_index < adjusted_match_count; token_index++) {
				unsigned int token_type = string.tokens[subrange[token_index]].type;
				bool is_different_token = false;
				if (entry.selection_data.is_multiple_type_indices) {
					is_different_token = entry.selection_data.type_indices.Find(token_type) == -1;
				}
				else {
					is_different_token = token_type != entry.selection_data.type_index;
				}

				if (entry.is_selection_negated) {
					is_different_token = !is_different_token;
				}

				if (is_different_token) {
					return false;
				}
			}
		}
		break;
		case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE:
		{
			const unsigned int MAX_MATCH_COUNT = 64;

			// Here, we have to handle the variable match count gracefully.
			unsigned int iteration_count = MAX_MATCH_COUNT;
			if (match_count != -1) {
				// It will be incremented once in the loop
				match_count--;
				iteration_count = 1;
			}
			else {
				// It will be incremented once in the loop
				match_count = 0;
			}

			struct CachedCheck {
				unsigned short token_offset;
				unsigned short token_count : 15;
				// Use a single bit to avoid increasing the memory footprint
				unsigned short success : 1;
			};

			// Keep a record of evaluations that were performed for iterations with index larger than 1.
			// For example, for match_count = 3, we can have 6 3 3 and 5 4 3 as options, and we can avoid
			// Checking the last entry in that case if we use a cache.
			ECS_STACK_CAPACITY_STREAM(CachedCheck, cached_checks, 64);

			unsigned int iteration_index = 0;
			for (; iteration_index < iteration_count; iteration_index++) {
				match_count++;
				
				// Early exit if the minimum amount of tokens per total number of matches is not satisfied
				if (match_count * entry.selection_data.subrule.minimum_token_count > subrange.count) {
					iteration_index = iteration_count;
					break;
				}

				// Match the subrule. This is quite involved. But we can try our best to early out as soon as possible.

				// Use unsigned short to reduce the stack consumption
				ECS_STACK_CAPACITY_STREAM(unsigned short, per_iteration_counts, MAX_MATCH_COUNT);
				per_iteration_counts.AssertCapacity(match_count);
				for (unsigned int index = 0; index < match_count; index++) {
					per_iteration_counts[index] = entry.selection_data.subrule.minimum_token_count;
				}
				per_iteration_counts.size = match_count;

				// If the exhaust tokens boolean is true, then it will perform matches only if all the tokens are used
				auto backtrack_permutations = [&](unsigned int subrange_offset, unsigned int remaining_tokens, CapacityStream<unsigned short> current_counts, unsigned int* matched_token_count, bool exhaust_tokens, const auto& backtrack_permutations) {
					// Perform the check for the current counts that we have.
					{
						// Use a stack scope for this to indicate to the compiler that the stack values should be deallocated at the end of the stack scope
						// Such that they don't occupy space and risk a stack overflow
						if (!exhaust_tokens || remaining_tokens == 0 || current_counts.size == 1) {
							if (current_counts.size == 1) {
								current_counts[0] += remaining_tokens;
								remaining_tokens = 0;
							}

							unsigned int current_token_offset = 0;
							bool current_state_success = true;

							// Go through the cache and check to see if we already have a failed state. If we do, we can cancel the search already.
							// Also, keep track of the indices that were successful as per the cache.
							bool* is_index_skipped = (bool*)ECS_STACK_ALLOC(sizeof(bool) * current_counts.size);
							memset(is_index_skipped, 0, sizeof(bool) * current_counts.size);
							for (unsigned int index = 0; index < current_counts.size; index++) {
								unsigned int cache_index = 0;
								for (; cache_index < cached_checks.size; cache_index++) {
									if (cached_checks[cache_index].token_offset == current_token_offset && cached_checks[cache_index].token_count == current_counts[index]) {
										if (!cached_checks[cache_index].success) {
											current_state_success = false;
										}
										is_index_skipped[index] = true;
										break;
									}
								}
								current_token_offset += current_counts[index];
							}

							if (current_state_success) {
								current_token_offset = 0;
								for (unsigned int index = 0; index < current_counts.size; index++) {
									bool success = true;

									// Only entries that are not already in the cache will need to be checked
									if (!is_index_skipped[index]) {
										TokenizedString::Subrange current_subrange = subrange.GetSubrange(subrange_offset + current_token_offset, current_counts[index]);
										success = MatchTokenizeRuleBacktracking(string, current_subrange, entry.selection_data.subrule, nullptr);

										// Add a cached entry, if we have enough space in the cache
										if (cached_checks.size < cached_checks.capacity) {
											CachedCheck cached_value;
											cached_value.success = success;
											cached_value.token_count = current_counts[index];
											cached_value.token_offset = current_token_offset;
											cached_checks.Add(&cached_value);
										}
									}

									if (!success) {
										current_state_success = false;
										break;
									}

									current_token_offset += current_counts[index];
								}
							}

							// We found a match - all subrules matched a portion
							if (current_state_success) {
								*matched_token_count = current_token_offset;
								return true;
							}
						}
					}

					// Continue the backtrace only if we have remaining tokens
					if (remaining_tokens > 0) {
						// Add one more token to each index and backtrace again
						for (unsigned int index = 0; index < current_counts.size; index++) {
							current_counts[index]++;
							if (backtrack_permutations(subrange_offset, remaining_tokens - 1, current_counts, matched_token_count, exhaust_tokens, backtrack_permutations)) {
								// Found a match
								return true;
							}
							current_counts[index]--;
						}
					}

					// None matched
					return false;
				};

				unsigned int token_offset = 0;
				unsigned int match_index = 0;
				for (; match_index < match_count; match_index++) {
					unsigned int remaining_tokens = subrange.count - token_offset - entry.selection_data.subrule.minimum_token_count * match_count;

					// This does not work for one or more matches of variable length subrules perfectly, since
					// The first version that match for an iteration might not work for the next ones, but
					// This is the easiest solution and might be enough for our use case
					unsigned int current_matched_token_count = 0;
					if (!backtrack_permutations(token_offset, remaining_tokens, per_iteration_counts, &current_matched_token_count, exhaustive_search, backtrack_permutations)) {
						// Could not match any combination
						break;
					}
					token_offset += current_matched_token_count;
				}

				if (match_index == match_count) {
					if (total_token_matched_count != nullptr) {
						*total_token_matched_count = token_offset;
					}
					break;
				}
			}

			if (iteration_index == iteration_count) {
				return false;
			}
		}
		break;
		}

		return true;
	}

	bool MatchTokenizeRuleBacktracking(const TokenizedString& string, TokenizedString::Subrange subrange, const TokenizeRule& rule, unsigned int* set_index_that_matched, CapacityStream<unsigned int>* matched_token_counts) {
		// Early exit if the minimum token count is not satisfied
		if (rule.minimum_token_count > subrange.count) {
			return false;
		}

		unsigned int matched_token_counts_initial_size = matched_token_counts != nullptr ? matched_token_counts->size : 0;
		// Matches a set from the rule that is fixed in length - it contains no variable length elements. Returns true if the set
		// Was matched, else false
		auto match_fixed_length_set = [&](size_t set_index) {
			// We can early exit if the count is different
			if (rule.sets[set_index].minimum_token_count != subrange.count) {
				return false;
			}

			// Keep track of the matched token count
			unsigned int matched_token_count = 0;
			for (unsigned int index = 0; index < subrange.count; index++) {
				const TokenizeRuleEntry& entry = rule.sets[set_index][index];

				// At the moment, the count types that are not variable length have just one
				// Token. In the future, if this is extended, it will need to be modified. These entries
				// Are also not optional
				unsigned int current_token_count = 0;
				if (!MatchTokenizeEntry(string, subrange.GetSubrangeUntilEnd(matched_token_count), entry, 1, false, &current_token_count)) {
					return false;
				}

				if (matched_token_counts != nullptr) {
					matched_token_counts->AddAssert(current_token_count);
				}
				matched_token_count += current_token_count;
			}

			return true;
		};

		// If the rule is not variable length, it is easier to match it
		if (!rule.is_variable_length) {
			// Try to match each set
			for (size_t set_index = 0; set_index < rule.sets.size; set_index++) {
				if (rule.sets[set_index].minimum_token_count > subrange.count) {
					// Skip this set
					continue;
				}

				if (matched_token_counts != nullptr) {
					matched_token_counts->size = matched_token_counts_initial_size;
				}

				if (match_fixed_length_set(set_index)) {
					if (set_index_that_matched != nullptr) {
						*set_index_that_matched = set_index;
					}
					return true;
				}
			}

			// No set matched
			return false;
		}
		else {
			for (size_t set_index = 0; set_index < rule.sets.size; set_index++) {
				if (rule.sets[set_index].minimum_token_count > subrange.count) {
					// Skip this set
					continue;
				}

				if (matched_token_counts != nullptr) {
					matched_token_counts->size = matched_token_counts_initial_size;
				}

				if (!rule.sets[set_index].is_variable_length) {
					if (match_fixed_length_set(set_index)) {
						return true;
					}
				}
				else {
					Stream<TokenizeRuleEntry> set = rule.sets[set_index].entries;
					
					auto match_consecutive_entries = [matched_token_counts, &string](Stream<TokenizeRuleEntry> set, TokenizedString::Subrange subrange, size_t set_start_index, size_t set_end_index, const auto& match_variable_length) {
						// Start matching from the start of the set. Keep doing this for fixed length entries, up until the first variable length entry.
						// When that variable length entry is encountered, pivot to the special function that handles it
						unsigned int token_offset = 0;
						for (size_t entry_index = set_start_index; entry_index < set_end_index; entry_index++) {
							if (IsFixedLengthEntry(set[entry_index])) {
								TokenizeRuleEntryMatchCount entry_match_count = GetTokenizeEntryMatchCount(set[entry_index]);
								unsigned int used_token_count = 0;
								if (!MatchTokenizeEntry(string, subrange.GetSubrangeUntilEnd(token_offset), set[entry_index], entry_match_count.count, entry_match_count.is_optional, &used_token_count)) {
									// We can return already
									return false;
								}
								token_offset += used_token_count;
								if (matched_token_counts != nullptr) {
									matched_token_counts->AddAssert(used_token_count);
								}

								if (entry_match_count.requires_pairing) {
									// Find its pairing
									size_t pairing_index = GetEntryPairing(set, entry_index);
									ECS_ASSERT(pairing_index != -1, "Could not find pairing for tokenize pair start entry.");

									// Find if any characters match this token
									uint2 matched_pair_index = TokenizeFindMatchingPair(string, set[entry_index].selection_data.string, set[pairing_index].selection_data.string, subrange.GetSubrangeUntilEnd(token_offset), true);
									if (matched_pair_index.y == -1) {
										// There is no matching end token, we can exit
										return false;
									}
									unsigned int in_between_pairing_token_count = matched_pair_index.y;

									if (pairing_index > entry_index + 1) {
										// Create a "virtual" rule that contains the entries in between the start and the end pairing characters
										TokenizeRule::EntrySet virtual_entry_set;
										virtual_entry_set.entries.buffer = set.buffer + entry_index + 1;
										virtual_entry_set.entries.size = pairing_index - entry_index - 1;
										TokenizeRule virtual_match_rule;
										virtual_match_rule.sets = { &virtual_entry_set, 1 };
										// Compute its cached values, the match function requires it
										virtual_match_rule.ComputeCachedValues();

										// The matched_pair_index is already relative to the subrange that starts from token offset
										// The set that matched should always be nullptr for subrules
										if (!MatchTokenizeRuleBacktracking(string, subrange.GetSubrange(token_offset, in_between_pairing_token_count), virtual_match_rule, nullptr, matched_token_counts)) {
											return false;
										}

										// The inner portion matched the pairing, can continue
										if (matched_token_counts != nullptr) {
											// The pairing end uses another token
											matched_token_counts->AddAssert(1);
										}

										// Jump to this entry, the increment will advance to the next entry
										entry_index = pairing_index;
									}
									else if (in_between_pairing_token_count > 0) {
										// There are no in between entries between the pairing, but there are tokens
										return false;
									}
									token_offset += in_between_pairing_token_count + 1;
								}
							}
							else {
								// It is a variable length entry. Returns its result
								return match_variable_length(entry_index, subrange.GetSubrangeUntilEnd(token_offset), match_variable_length);
							}
						}

						// It means that there were only fixed length entries, return as success
						return true;
					};

					// Returns true if the variable length entry and all the next entries match the sequence, else false.
					// It will perform a basic form of backtracking, with as much early existing as possible.
					auto match_variable_length = [&](size_t entry_index, TokenizedString::Subrange current_subrange, const auto& match_variable_length) {
						TokenizeRuleEntryMatchCount match_count = GetTokenizeEntryMatchCount(set[entry_index]);

						// Determine if we have more entries after us. If not, then we need to match all the remaining tokens
						if (entry_index == set.size - 1) {
							// Don't pass a match token count, since we want to exhaust the subrange
							if (!MatchTokenizeEntry(string, current_subrange, set[entry_index], match_count.count, match_count.is_optional, nullptr)) {
								return false;
							}
						}
						else {
							// PERFORMANCE TODO: Determine if a cache could speed this up, but in practice, there shouldn't
							// Be to many variable length fields one after the other.

							// Use a define such that it can be used inside the match_consecutive_variable_entries_impl lambda
#define MAX_VARIABLE_LENGTH_COUNT 32
							ECS_STACK_CAPACITY_STREAM(bool, variable_lengths_are_optional, MAX_VARIABLE_LENGTH_COUNT);
							ECS_STACK_CAPACITY_STREAM(unsigned int, variable_length_counts, MAX_VARIABLE_LENGTH_COUNT);
							ECS_STACK_CAPACITY_STREAM(unsigned int, variable_lengths_match_count, MAX_VARIABLE_LENGTH_COUNT);

							// Returns true if the combination succeeded, else false
							auto match_consecutive_variable_entries_impl = [&](Stream<unsigned int> variable_counts, unsigned int remaining_tokens, const auto& match_consecutive_variable_entries_impl) {
								size_t variable_index = 0;
								unsigned int current_offset = 0;
								// Check the current configuration, up until the last entry, which needs to handle all the remaining tokens
								for (; variable_index < variable_counts.size - 1; variable_index++) {
									if (!MatchTokenizeEntry(
										string, 
										current_subrange.GetSubrange(current_offset, variable_counts[variable_index]), 
										set[entry_index + variable_index], 
										variable_lengths_match_count[variable_index], 
										variable_lengths_are_optional[variable_index], 
										nullptr
									)) {
										break;
									}
									current_offset += variable_counts[variable_index];
								}

								if (variable_index == variable_counts.size - 1) {
									// All matched, test the last entry now
									if (MatchTokenizeEntry(
										string, 
										current_subrange.GetSubrange(current_offset, variable_counts.Last() + remaining_tokens), 
										set[entry_index + variable_counts.size - 1], 
										variable_lengths_match_count.Last(), 
										variable_lengths_are_optional.Last(), 
										nullptr
									)) {
										// This combination actually matched
										if (matched_token_counts != nullptr) {
											// Add the counts to the user passed argument
											matched_token_counts->AddStreamAssert(variable_counts);
										}
										return true;
									}
								}

								// No more tokens to assign, can exit
								if (remaining_tokens == 0 || variable_counts.size == 1) {
									return false;
								}

								// Recurse - only for the variable indices besides the last, the last one will always be dynamic
								ECS_STACK_CAPACITY_STREAM(unsigned int, current_counts, MAX_VARIABLE_LENGTH_COUNT);
								current_counts.CopyOther(variable_counts);
								for (size_t variable_index = 0; variable_index < variable_counts.size - 1; variable_index++) {
									current_counts[variable_index]++;
									if (match_consecutive_variable_entries_impl(current_counts, remaining_tokens - 1, match_consecutive_variable_entries_impl)) {
										return true;
									}
									current_counts[variable_index]--;
								}

								// No match
								return false;
							};
#undef MAX_VARIABLE_LENGTH_COUNT

							// Returns true if it succeeded, else false. The last_index is exclusive, meaning it will iterate up until it, i.e. the last iterated value is last_index - 1.
							// Token_match_count must be the number of tokens these consecutive variable entries should match
							auto match_consecutive_variable_entries = [&](size_t last_index, unsigned int token_match_count) {
								size_t variable_length_entry_count = last_index - entry_index;
								// Remember the variable lengths optionality
								variable_lengths_are_optional.AssertCapacity(variable_length_entry_count);
								for (unsigned int variable_index = 0; variable_index < variable_length_entry_count; variable_index++) {
									TokenizeRuleEntryMatchCount variable_match_count = GetTokenizeEntryMatchCount(set[entry_index + variable_index]);
									variable_lengths_are_optional[variable_index] = variable_match_count.is_optional;
									variable_lengths_match_count[variable_index] = variable_match_count.count;
									variable_length_counts[variable_index] = 0;
								}
								variable_length_counts.size = variable_length_entry_count;
								variable_lengths_are_optional.size = variable_length_entry_count;
								variable_lengths_match_count.size = variable_length_entry_count;

								return match_consecutive_variable_entries_impl(variable_length_counts, token_match_count, match_consecutive_variable_entries_impl);
							};

							// We have more entries after us. Find the first fixed length entry and find the locations that it matches
							// Then assign to the variable fields up until it the remaining tokens
							for (size_t next_index = entry_index + 1; next_index < set.size; next_index++) {
								if (IsFixedLengthEntry(set[next_index])) {
									FixedLengthEntryInfo fixed_length_info = GetFixedLengthEntryInfo(set[next_index]);
									// Backtrace, find all valid locations for this entry, then partition to the variable length
									// Entries before it the counts
									unsigned int total_token_match_count = fixed_length_info.token_count * fixed_length_info.match_count;
									if (current_subrange.count < total_token_match_count) {
										// Cannot match this subrange, it doesn't have enough tokens left
										return false;
									}

									for (unsigned int index = 0; index < current_subrange.count - total_token_match_count + 1; index++) {
										// Not actually needed as value, but needed for the function
										unsigned int matched_token_count = 0;
										if (MatchTokenizeEntry(string, current_subrange.GetSubrange(index, total_token_match_count), set[next_index], fixed_length_info.match_count, false, &matched_token_count)) {
											// Found an entry. The variable length entries must match the offset we are currently testing at.
											unsigned int matched_tokens_current_count = matched_token_counts != nullptr ? matched_token_counts->size : 0;
											if (match_consecutive_variable_entries(next_index, index)) {
												unsigned int remaining_tokens_to_be_matched_offset = index + total_token_match_count;
												// We don't need to add to the token counts the current matched fixed length entry, that will happen inside the match consecutive entries

												// Call the function that resolves the consecutive entries. We must include the next_index as well such
												// That it can pickup on the fact that it might be a pairing token
												if (match_consecutive_entries(set, current_subrange.GetSubrangeUntilEnd(index), next_index, set.size, match_variable_length)) {
													return true;
												}

												// Walk back the counts that were added
												if (matched_token_counts != nullptr) {
													matched_token_counts->size = matched_tokens_current_count;
												}
											}
										}
									}

									// This entry could not be matched - we can early exit
									return false;
								}
							}

							// If we haven't early exited up until now, then there are only variable length entries.
							// Call the appropriate function to handle this
							if (!match_consecutive_variable_entries(set.size, current_subrange.count)) {
								return false;
							}
						}

						return true;
					};

					// If the set matched, then return now
					if (match_consecutive_entries(set, subrange, 0, set.size, match_variable_length)) {
						if (set_index_that_matched != nullptr) {
							*set_index_that_matched = set_index;
						}
						return true;
					}
				}
			}

			// No set matched
			return false;
		}

		ECS_ASSERT(false, "Shouldn't reach here");
		return false;
	}

	unsigned int FindTokenizeRuleMatchingRange(const TokenizedString& string, const TokenizeRule& rule, TokenizedString::Subrange subrange, unsigned int* set_index_that_matched) {
		if (set_index_that_matched != nullptr) {
			*set_index_that_matched = -1;
		}
		
		if (rule.minimum_token_count > subrange.count) {
			// Early exit if the minimum amount of tokens is not satisfied
			return -1;
		}

		unsigned int maximum_match_count = -1;
		for (size_t set_index = 0; set_index < rule.sets.size; set_index++) {
			unsigned int current_set_match_count = 0;
			
			if (rule.sets[set_index].minimum_token_count > subrange.count) {
				// Don't analyse this set
				current_set_match_count = -1;
				continue;
			}

			Stream<TokenizeRuleEntry> set = rule.sets[set_index].entries;
			for (size_t entry_index = 0; entry_index < set.size; entry_index++) {
				// The pair start is a special case
				const TokenizeRuleEntry& entry = set[entry_index];

				if (entry.count_type == ECS_TOKENIZE_RULE_ONE_PAIR_START) {
					if (string[subrange[current_set_match_count]] != entry.selection_data.string) {
						// This token did not match, quit
						current_set_match_count = -1;
						break;
					}
					
					// Find the end token
					size_t end_token_entry_index = GetEntryPairing(set, entry_index);
					
					// Find the pairing
					uint2 pairing_indices = TokenizeFindMatchingPair(string, entry.selection_data.string, set[end_token_entry_index].selection_data.string, subrange.GetSubrangeUntilEnd(current_set_match_count));
					if (pairing_indices.y == -1) {
						// The pairing could not be established
						current_set_match_count = -1;
						break;
					}

					size_t in_between_pairing_count = end_token_entry_index - entry_index - 1;
					if (in_between_pairing_count > 0) {
						// Create a pseudo tokenize rule that contains the subsection that must match the tokens in between
						TokenizeRule::EntrySet pair_rule_set;
						pair_rule_set.is_variable_length = false;
						pair_rule_set.minimum_token_count = 0;
						pair_rule_set.entries = { set.buffer + entry_index + 1, in_between_pairing_count };
						
						TokenizeRule in_between_pair_rule;
						in_between_pair_rule.sets = { &pair_rule_set, 1 };
						// Compute the cached values, otherwise the call fails
						in_between_pair_rule.ComputeCachedValues();

						TokenizedString::Subrange in_between_subrange = subrange.GetSubrange(current_set_match_count + 1, pairing_indices.y - 1);
						if (FindTokenizeRuleMatchingRange(string, in_between_pair_rule, in_between_subrange) != in_between_subrange.count) {
							// The in between subrange didn't match, fail
							current_set_match_count = -1;
							break;
						}
					}

					// Advance to the end pairing character
					current_set_match_count += pairing_indices.y + 1;
					// Change the entry index, such that we skip over this section
					entry_index = end_token_entry_index;
				}
				else {
					// Returns the number of tokens that it matched, or -1 if the current tokens couldn't be matched
					auto match_entry = [&]() -> unsigned int {
						switch (entry.selection_type) {
						case ECS_TOKENIZE_RULE_SELECTION_ANY:
							return entry.is_selection_negated ? 0 : 1;
						case ECS_TOKENIZE_RULE_SELECTION_GENERAL:
						{
							bool is_different_token = string.tokens[subrange[current_set_match_count]].type != ECS_TOKEN_TYPE_GENERAL;
							if (entry.is_selection_negated) {
								is_different_token = !is_different_token;
							}
							return is_different_token ? -1 : 1;
						}
						case ECS_TOKENIZE_RULE_SELECTION_SEPARATOR:
						{
							bool is_different_token = string.tokens[subrange[current_set_match_count]].type != ECS_TOKEN_TYPE_SEPARATOR;
							if (entry.is_selection_negated) {
								is_different_token = !is_different_token;
							}
							return is_different_token ? -1 : 1;
						}
						case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING:
						{
							Stream<char> current_token = string[subrange[current_set_match_count]];
							bool is_different_token = false;
							if (entry.selection_data.is_multiple_strings) {
								is_different_token = FindString(current_token, entry.selection_data.strings) == -1;
							}
							else {
								is_different_token = current_token != entry.selection_data.string;
							}

							if (entry.is_selection_negated) {
								is_different_token = !is_different_token;
							}
							return is_different_token ? -1 : 1;
						}
						case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_TYPE:
						{
							unsigned int token_type = string.tokens[subrange[current_set_match_count]].type;
							bool is_different_token = false;
							if (entry.selection_data.is_multiple_type_indices) {
								is_different_token = entry.selection_data.type_indices.Find(token_type) == -1;
							}
							else {
								is_different_token = token_type != entry.selection_data.type_index;
							}

							if (entry.is_selection_negated) {
								is_different_token = !is_different_token;
							}

							return is_different_token ? -1 : 1;
						}
						case ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE:
						{
							return FindTokenizeRuleMatchingRange(string, entry.selection_data.subrule, subrange.GetSubrangeUntilEnd(current_set_match_count));
						}
						}

						return 0;
					};

					if (current_set_match_count == subrange.count) {
						// There are no more tokens to be matched, these entries must be optional
						if (!GetTokenizeEntryMatchCount(entry).is_optional) {
							// Exit
							current_set_match_count = -1;
							break;
						}
					}
					else {
						// We cannot exit from the loop using break inside the switch since it will exit from the switch
						// So use a temporary boolean variable
						bool failed = false;
						switch (entry.count_type) {
						case ECS_TOKENIZE_RULE_ONE:
						{
							unsigned int matched_token_count = match_entry();
							if (matched_token_count == -1) {
								// It failed, exit
								failed = true;
							}
							else {
								current_set_match_count += matched_token_count;
							}
						}
						break;
						case ECS_TOKENIZE_RULE_ZERO_OR_ONE:
						{
							unsigned int matched_token_count = match_entry();
							current_set_match_count += matched_token_count == -1 ? 0 : matched_token_count;
						}
						break;
						case ECS_TOKENIZE_RULE_ONE_OR_MORE:
						{
							unsigned int matched_token_count = match_entry();
							if (matched_token_count == -1) {
								// We need to match at least once
								failed = true;
							}
							else {
								while (matched_token_count != -1) {
									current_set_match_count += matched_token_count;
									// If we got to the end, exit
									if (current_set_match_count == subrange.count) {
										matched_token_count = -1;
									}
									else {
										matched_token_count = match_entry();
									}
								}
							}
						}
						break;
						case ECS_TOKENIZE_RULE_ZERO_OR_MORE:
						{
							unsigned int matched_token_count = match_entry();
							while (matched_token_count != -1) {
								current_set_match_count += matched_token_count;
								// If we got to the end, exit
								if (current_set_match_count == subrange.count) {
									matched_token_count = -1;
								}
								else {
									matched_token_count = match_entry();
								}
							}
						}
						break;
						}

						if (failed) {
							current_set_match_count = -1;
							break;
						}
					}
				}
			}

			if (current_set_match_count != -1 && (current_set_match_count > maximum_match_count || maximum_match_count == -1)) {
				maximum_match_count = current_set_match_count;
				if (set_index_that_matched != nullptr) {
					*set_index_that_matched = set_index;
				}
			}
		}

		// Allow to return 0 in one case only, when the subrange count is 0 as well. This indicates that the optional
		// Field was matched
		if (maximum_match_count == 0 && subrange.count != 0) {
			// Returns -1, this is an error
			return -1;
		}

		return maximum_match_count;
	}

	void MatchTokenizeRuleTest() {
		const char* typedef_definition = R"DELIMITER(identifier_no_template = $G.
				identifier_template = $G.\<$T+/>
				identifier = identifier_no_template. | identifier_template.
				typedef. identifier. identifier_no_template.;.)DELIMITER";

		ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
		TokenizeRule typedef_rule = CreateTokenizeRule(typedef_definition, &stack_allocator, true, &error_message);
		ECS_ASSERT(!typedef_rule.IsEmpty());

		ResizableStream<Token> tokens(&stack_allocator, 0);
		TokenizedString string;
		string.string = "typedef valid valid;";
		string.tokens = &tokens;
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(MatchTokenizeRuleBacktracking(string, string.AsSubrange(), typedef_rule));

		string.string = "typedef template<MyTemplate, Value> valid;";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(MatchTokenizeRuleBacktracking(string, string.AsSubrange(), typedef_rule));

		string.string = "typedef . valid;";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(!MatchTokenizeRuleBacktracking(string, string.AsSubrange(), typedef_rule));

		string.string = "void myFunction();";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(!MatchTokenizeRuleBacktracking(string, string.AsSubrange(), typedef_rule));

		string.string = "typedef template<MyValue, YourValue valid;";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(!MatchTokenizeRuleBacktracking(string, string.AsSubrange(), typedef_rule));

		string.string = "typedef template>MyValue, <YourValue valid;";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(!MatchTokenizeRuleBacktracking(string, string.AsSubrange(), typedef_rule));

		const char* function_definition = R"DELIMITER(template_header = template. \< $T* />
														template_header? $G. $G* $G. \( $T* /) $G*;. | template_header? $G. $G* $G. \( $T* /) $G* \{ $T* /} )DELIMITER";
		TokenizeRule function_rule = CreateTokenizeRule(function_definition, &stack_allocator, true, &error_message);
		ECS_ASSERT(!function_rule.IsEmpty());

		string.string = "void MyFunction();";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(MatchTokenizeRuleBacktracking(string, string.AsSubrange(), function_rule));

		string.string = "void FunctionWithBody(A a, B b, C* const MyPOinter) { // It has a nested scope {} int myVariable; }";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(MatchTokenizeRuleBacktracking(string, string.AsSubrange(), function_rule));

		string.string = "template<class my_template> ECS_INLINE static constexpr void TemplateFunction() const;";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(MatchTokenizeRuleBacktracking(string, string.AsSubrange(), function_rule));

		string.string = "template<class my_template> ECS_INLINE static constexpr void TemplateFunction() const override { int my_array[50]; { another scope; } }";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(MatchTokenizeRuleBacktracking(string, string.AsSubrange(), function_rule));

		string.string = "int my_member_field;";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(!MatchTokenizeRuleBacktracking(string, string.AsSubrange(), function_rule));

		string.string = "ECS_REFLECT_SOA(10, 20, 30)";
		string.tokens.resizable_stream->FreeBuffer();
		TokenizeString(string, GetCppFileTokenSeparators(), true);
		ECS_ASSERT(!MatchTokenizeRuleBacktracking(string, string.AsSubrange(), function_rule));
	}

	bool ValidateTokenizeRule(const TokenizeRule& rule, CapacityStream<char>* error_message) {
		// At the moment, the only thing to validate is that the begin/end pairings are conforming
		for (size_t set_index = 0; set_index < rule.sets.size; set_index++) {
			size_t pairing_count = 0;
			for (size_t entry_index = 0; entry_index < rule.sets[set_index].entries.size; entry_index++) {
				ECS_TOKENIZE_RULE_COUNT_TYPE count_type = rule.sets[set_index][entry_index].count_type;
				if (count_type == ECS_TOKENIZE_RULE_ONE_PAIR_START) {
					pairing_count++;
				}
				else if (count_type == ECS_TOKENIZE_RULE_ONE_PAIR_END) {
					if (pairing_count == 0) {
						SetErrorMessage(error_message, "A pairing end character was encountered before any start character");
						return false;
					}
					pairing_count--;
				}
			}

			if (pairing_count != 0) {
				SetErrorMessage(error_message, "Pairing characters are not properly matched");
				return false;
			}
		}
		return true;
	}

#define FUNCTION_MODIFIERS "[ECS_INLINE, ECS_NOINLINE, static, constexpr, inline]*"

	void AddTokenizeRuleQualifiers(CapacityStream<char>& rule_string) {
		rule_string.AddStreamAssert("qualifier = &. | $** &?\n");
	}

	void AddTokenizeRuleIdentifierDefinitions(CapacityStream<char>& rule_string) {
		AddTokenizeRuleQualifiers(rule_string);
		rule_string.AddStreamAssert("identifier_no_template = const? unsigned? ($G. :.:.)* $G. qualifier?\n");
		rule_string.AddStreamAssert("identifier_template = const? unsigned? ($G. :.:.)* $G. \\< $T+ /> qualifier?\n");
		rule_string.AddStreamAssert("identifier = identifier_no_template. | identifier_template.\n");
	}

	TokenizeRule GetTokenizeRuleForFunctions(AllocatorPolymorphic allocator, bool allocator_is_temporary, bool include_declarations, bool include_definitions, bool include_template) {
		ECS_ASSERT(include_declarations || include_definitions, "Tokenize rule for functions requires declarations, definitions or both to be used.");

		ECS_STACK_CAPACITY_STREAM(char, rule_characters, 1024);
		if (include_template) {
			// Add the template header
			rule_characters.AddStreamAssert("template_header = template. \\< $T+ />\n");
		}
		AddTokenizeRuleIdentifierDefinitions(rule_characters);

#define FUNCTION_DECLARATION FUNCTION_MODIFIERS " identifier. ECS_VECTORCALL? $G. \\( $T* /) $G*;."
#define FUNCTION_DEFINITION FUNCTION_MODIFIERS " identifier. ECS_VECTORCALL? $G. \\( $T* /) $G* \\{ $T* /}"

		if (include_declarations) {
			if (include_template) {
				rule_characters.AddStreamAssert("template_header? ");
			}
			rule_characters.AddStreamAssert(FUNCTION_DECLARATION);
		}

		if (include_definitions) {
			if (include_declarations) {
				rule_characters.AddStreamAssert(" | ");
			}

			if (include_template) {
				rule_characters.AddStreamAssert("template_header? ");
			}
			rule_characters.AddStreamAssert(FUNCTION_DEFINITION);
		}

#undef FUNCTION_DECLARATION
#undef FUNCTION_DEFINITION

		return CreateTokenizeRule(rule_characters, allocator, allocator_is_temporary);
	}

	TokenizeRule GetTokenizeRuleForStructOperators(AllocatorPolymorphic allocator, bool allocator_is_temporary) {
		ECS_STACK_CAPACITY_STREAM(char, rule_string, 512);
		AddTokenizeRuleIdentifierDefinitions(rule_string);
		// Take into consideration normal = or ==, < or > operators, and type conversion operators
		rule_string.AddStreamAssert("operator_signature = " FUNCTION_MODIFIERS " identifier. operator. $S. $S? \\( $T* /) const? | " FUNCTION_MODIFIERS " operator. $G. $(. $). const?\n");
		rule_string.AddStreamAssert("operator_signature. (=. default.)? ;. | operator_signature. \\{ $T* /}");
		return CreateTokenizeRule(rule_string, allocator, allocator_is_temporary);
	}

	TokenizeRule GetTokenizeRuleForStructConstructors(AllocatorPolymorphic allocator, bool allocator_is_temporary) {
		// The second set for the argument list is handling calls to another constructor
		const char* rule_string = R"DELIMITER(argument_list = :. ($G. \( $T* /) ,.)* $G. \( $T* /)
												$G+ \( $T* /) (=. default.)? ;. | $G+ \( $T* /) argument_list? \{ $T* /} )DELIMITER";
		return CreateTokenizeRule(rule_string, allocator, allocator_is_temporary);
	}

	void TokenizeRuleMatcher::AddExcludeRule(const TokenizeRule& rule, bool deep_copy) {
		if (deep_copy) {
			exclude_rules.Add(rule.Copy(Allocator()));
		}
		else {
			exclude_rules.Add(&rule);
		}
	}

	void TokenizeRuleMatcher::AddPreExcludeAction(const TokenizeRuleAction& action, bool deep_copy) {
		TokenizeRuleAction copy;
		const TokenizeRuleAction* action_pointer = &action;
		if (deep_copy) {
			action_pointer = &copy;
			copy = action;
			copy.rule = action.rule.Copy(Allocator());
			copy.callback_data = CopyNonZero(Allocator(), action.callback_data);
		}
		pre_exclude_actions.Add(action_pointer);
	}

	void TokenizeRuleMatcher::AddPostExcludeAction(const TokenizeRuleAction& action, bool deep_copy) {
		TokenizeRuleAction copy;
		const TokenizeRuleAction* action_pointer = &action;
		if (deep_copy) {
			action_pointer = &copy;
			copy = action;
			copy.rule = action.rule.Copy(Allocator());
			copy.callback_data = CopyNonZero(Allocator(), action.callback_data);
		}
		post_exclude_actions.Add(action_pointer);
	}

	void TokenizeRuleMatcher::Deallocate(bool deallocate_exclude_rules, bool deallocate_actions) {
		if (deallocate_exclude_rules) {
			StreamDeallocateElements(exclude_rules, Allocator());
		}
		if (deallocate_actions) {
			for (unsigned int index = 0; index < pre_exclude_actions.size; index++) {
				pre_exclude_actions[index].callback_data.Deallocate(Allocator());
				pre_exclude_actions[index].rule.Deallocate(Allocator());
			}
			for (unsigned int index = 0; index < post_exclude_actions.size; index++) {
				post_exclude_actions[index].callback_data.Deallocate(Allocator());
				post_exclude_actions[index].rule.Deallocate(Allocator());
			}
		}

		exclude_rules.FreeBuffer();
		pre_exclude_actions.FreeBuffer();
		post_exclude_actions.FreeBuffer();
		custom_subrange_order.FreeBuffer();
	}

	void TokenizeRuleMatcher::Initialize(AllocatorPolymorphic allocator) {
		exclude_rules.Initialize(allocator, 0);
		pre_exclude_actions.Initialize(allocator, 0);
		post_exclude_actions.Initialize(allocator, 0);
		custom_subrange_order.Initialize(allocator, 0);
	}

	ECS_TOKENIZE_MATCHER_RESULT TokenizeRuleMatcher::MatchBacktracking(const TokenizedString& string, TokenizedString::Subrange subrange, void* call_specific_data) const {
		bool iterate_increasing = true;
		Stream<unsigned int> order_to_iterate;
		if (custom_subrange_order.size > 0) {
			order_to_iterate = custom_subrange_order;
			if (custom_subrange_order.Last() == -1) {
				order_to_iterate.size--;
			}
			else {
				iterate_increasing = false;
			}
		}

		while (subrange.count > 0) {
			// Returns 2 boolean values. The first one indicates whether the subrange was matched, and the second one
			// If the action callback returned true, to early exit
			auto test_subrange_count = [&](unsigned int sequence_count) -> bool2 {
				TokenizedString::Subrange current_subrange = subrange.GetSubrange(0, sequence_count);
				
				// Try the pre exclude rules first
				unsigned int pre_action_rule_index = 0;
				for (; pre_action_rule_index < pre_exclude_actions.size; pre_action_rule_index++) {
					unsigned int set_index_that_matched = -1;
					if (MatchTokenizeRuleBacktracking(string, current_subrange, pre_exclude_actions[pre_action_rule_index].rule, &set_index_that_matched)) {
						TokenizeRuleCallbackData callback_data(string);
						callback_data.subrange = current_subrange;
						callback_data.user_data = pre_exclude_actions[pre_action_rule_index].callback_data.buffer;
						callback_data.call_specific_data = call_specific_data;
						callback_data.set_index_that_matched = set_index_that_matched;
						ECS_TOKENIZE_RULE_CALLBACK_RESULT callback_result = pre_exclude_actions[pre_action_rule_index].callback(&callback_data);
						if (callback_result == ECS_TOKENIZE_RULE_CALLBACK_EXIT) {
							return { true, true };
						}
						else if (callback_result == ECS_TOKENIZE_RULE_CALLBACK_MATCHED) {
							return { true, false };
						}
					}
				}

				// Try to match the exclude rules now
				unsigned int exclude_index = 0;
				for (; exclude_index < exclude_rules.size; exclude_index++) {
					if (MatchTokenizeRuleBacktracking(string, current_subrange, exclude_rules[exclude_index])) {
						return { true, false };
					}
				}

				// The exclude rules did not match this subrange, try the post exclude rules
				unsigned int post_action_rule_index = 0;
				for (; post_action_rule_index < post_exclude_actions.size; post_action_rule_index++) {
					unsigned int set_index_that_matched = -1;
					if (MatchTokenizeRuleBacktracking(string, current_subrange, post_exclude_actions[post_action_rule_index].rule, &set_index_that_matched)) {
						TokenizeRuleCallbackData callback_data(string);
						callback_data.subrange = current_subrange;
						callback_data.user_data = post_exclude_actions[post_action_rule_index].callback_data.buffer;
						callback_data.call_specific_data = call_specific_data;
						callback_data.set_index_that_matched = set_index_that_matched;
						if (post_exclude_actions[post_action_rule_index].callback(&callback_data) == ECS_TOKENIZE_RULE_CALLBACK_EXIT) {
							return { true, true };
						}
						else {
							return { true, false };
						}
					}
				}

				return { false, false };
			};

			// Start with the custom order sequence counts
			size_t custom_index = 0;
			for (; custom_index < order_to_iterate.size; custom_index++) {
				if (order_to_iterate[custom_index] <= subrange.count) {
					bool2 success = test_subrange_count(order_to_iterate[custom_index]);
					if (success.x) {
						if (success.y) {
							return ECS_TOKENIZE_MATCHER_EARLY_EXIT;
						}
						else {
							subrange = subrange.GetSubrangeUntilEnd(order_to_iterate[custom_index]);
							break;
						}
					}
				}
			}

			// The custom order could not be verified
			if (custom_index == order_to_iterate.size) {
				if (iterate_increasing) {
					unsigned int sequence_count = 1;
					for (; sequence_count <= subrange.count; sequence_count++) {
						if (order_to_iterate.Find(sequence_count) == -1) {
							bool2 success = test_subrange_count(sequence_count);
							if (success.x) {
								if (success.y) {
									return ECS_TOKENIZE_MATCHER_EARLY_EXIT;
								}
								else {
									subrange = subrange.GetSubrangeUntilEnd(sequence_count);
									break;
								}
							}
						}
					}

					if (sequence_count > subrange.count) {
						// No subrange matched, exit the while
						return ECS_TOKENIZE_MATCHER_FAILED_TO_MATCH_ALL;
					}
				}
				else {
					// Exit the loop, we could not match the custom counts, and the user did not specify increasing test order
					ECS_TOKENIZE_MATCHER_FAILED_TO_MATCH_ALL;
				}
			}
		}
		return ECS_TOKENIZE_MATCHER_SUCCESS;
	}

	ECS_TOKENIZE_MATCHER_RESULT TokenizeRuleMatcher::MatchRulesWithFind(const TokenizedString& string, TokenizedString::Subrange subrange, void* call_specific_data) const {
		enum MATCHER_TYPE : unsigned char {
			PRE_EXCLUDE,
			EXCLUDE,
			POST_EXCLUDE
		};

		struct MatchedEntries {
			bool operator < (const MatchedEntries& other) const {
				// If this is missing, then return now
				if (match_count == -1) {
					return true;
				}

				if (other.match_count == -1) {
					return false;
				}

				if (match_count < other.match_count) {
					return true;
				}

				return type > other.type || ((type == other.type) && action_index < other.action_index);
			}

			ECS_INLINE bool operator == (const MatchedEntries& other) const {
				return type == other.type && match_count == other.match_count && action_index == other.action_index && set_index == other.set_index;
			}

			MATCHER_TYPE type;
			unsigned int match_count;
			unsigned int action_index;
			unsigned int set_index;
		};

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
		ResizableStream<MatchedEntries> matched_entries(&stack_allocator, 32);

		while (subrange.count > 0) {
			matched_entries.Reset();

			for (size_t index = 0; index < pre_exclude_actions.size; index++) {
				unsigned int set_index = 0;
				unsigned int current_count = FindTokenizeRuleMatchingRange(string, pre_exclude_actions[index].rule, subrange, &set_index);
				if (current_count != -1) {
					matched_entries.Add({ PRE_EXCLUDE, current_count, (unsigned int)index, set_index });
				}
			}

			for (size_t index = 0; index < exclude_rules.size; index++) {
				unsigned int set_index = 0;
				unsigned int current_count = FindTokenizeRuleMatchingRange(string, exclude_rules[index], subrange, &set_index);
				if (current_count != -1) {
					matched_entries.Add({ EXCLUDE, current_count, (unsigned int)index, set_index });
				}
			}

			for (size_t index = 0; index < post_exclude_actions.size; index++) {
				unsigned int set_index = 0;
				unsigned int current_count = FindTokenizeRuleMatchingRange(string, post_exclude_actions[index].rule, subrange, &set_index);
				if (current_count != -1) {
					matched_entries.Add({ POST_EXCLUDE, current_count, (unsigned int)index, set_index });
				}
			}

			// Sort the entries
			InsertionSort(matched_entries.buffer, matched_entries.size);

			// Get the first entry that accepts the parsed range. Exclude and post exclude
			// Actions are automatically accepting, pre-exclude can reject
			int64_t index = (int64_t)matched_entries.size - 1;
			for (; index >= 0; index--) {
				TokenizeRuleCallbackData callback_data(string);
				callback_data.subrange = subrange.GetSubrange(0, matched_entries[index].match_count);
				callback_data.call_specific_data = call_specific_data;
				callback_data.set_index_that_matched = matched_entries[index].set_index;

				ECS_TOKENIZE_RULE_CALLBACK_RESULT result;
				if (matched_entries[index].type == PRE_EXCLUDE) {
					callback_data.user_data = pre_exclude_actions[matched_entries[index].action_index].callback_data.buffer;
					result = pre_exclude_actions[matched_entries[index].action_index].callback(&callback_data);
				}
				else if (matched_entries[index].type == EXCLUDE) {
					result = ECS_TOKENIZE_RULE_CALLBACK_MATCHED;
				}
				else if (matched_entries[index].type == POST_EXCLUDE) {
					callback_data.user_data = post_exclude_actions[matched_entries[index].action_index].callback_data.buffer;
					result = post_exclude_actions[matched_entries[index].action_index].callback(&callback_data);
				}

				if (result == ECS_TOKENIZE_RULE_CALLBACK_EXIT) {
					return ECS_TOKENIZE_MATCHER_EARLY_EXIT;
				}
				else if (result == ECS_TOKENIZE_RULE_CALLBACK_MATCHED) {
					subrange = subrange.GetSubrangeUntilEnd(matched_entries[index].match_count);
					break;
				}
			}

			if (index < 0) {
				// No entry matched, exit
				return ECS_TOKENIZE_MATCHER_FAILED_TO_MATCH_ALL;
			}
		}

		return ECS_TOKENIZE_MATCHER_SUCCESS;
	}

	void TokenizeRuleMatcher::SetCustomSubrangeOrder(Stream<unsigned int> counts) {
		custom_subrange_order.CopyOther(counts);
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
		rule.name = name.Copy(allocator);
		rule.sets = StreamDeepCopy(sets, allocator);
		rule.minimum_token_count = minimum_token_count;
		return rule;
	}

	void TokenizeRule::Deallocate(AllocatorPolymorphic allocator) {
		StreamDeallocateElements(sets, allocator);
		sets.Deallocate(allocator);
		name.Deallocate(allocator);
	}

	void TokenizeRule::ComputeCachedValues() {
		unsigned int overall_minimum = UINT_MAX;
		bool is_overall_variable_length = false;

		for (size_t set_index = 0; set_index < sets.size; set_index++) {
			unsigned int set_minimum_count = 0;
			bool is_set_variable_length = false;
			for (size_t entry_index = 0; entry_index < sets[set_index].entries.size; entry_index++) {
				TokenizeRuleEntry& entry = sets[set_index][entry_index];
				if (entry.selection_type == ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE) {
					entry.selection_data.subrule.ComputeCachedValues();
				}

				// Counts that include zero don't contribute
				if (entry.count_type == ECS_TOKENIZE_RULE_ONE || entry.count_type == ECS_TOKENIZE_RULE_ONE_OR_MORE ||
					entry.count_type == ECS_TOKENIZE_RULE_ONE_PAIR_START || entry.count_type == ECS_TOKENIZE_RULE_ONE_PAIR_END) {
					// Subrules are a special case
					if (entry.selection_type == ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE) {
						set_minimum_count += entry.selection_data.subrule.minimum_token_count;
						is_set_variable_length |= entry.selection_data.subrule.is_variable_length;
					}
					else {
						// Normal case, one token required, at the very least
						set_minimum_count++;
					}
				}

				if (entry.count_type == ECS_TOKENIZE_RULE_ONE_OR_MORE || entry.count_type == ECS_TOKENIZE_RULE_ZERO_OR_MORE || 
					entry.count_type == ECS_TOKENIZE_RULE_ZERO_OR_ONE) {
					is_set_variable_length = true;
				}
			}
			sets[set_index].minimum_token_count = set_minimum_count;
			sets[set_index].is_variable_length = is_set_variable_length;
			overall_minimum = min(overall_minimum, set_minimum_count);
			is_overall_variable_length |= is_set_variable_length;
		}
		// If no set contains variable length entries, this rule can be variable length if the sets
		// Have different token counts
		if (!is_overall_variable_length) {
			for (size_t set_index = 0; set_index < sets.size - 1; set_index++) {
				if (sets[0].minimum_token_count != sets[set_index + 1].minimum_token_count) {
					is_overall_variable_length = true;
					break;
				}
			}
		}

		minimum_token_count = overall_minimum;
		is_variable_length = is_overall_variable_length;
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
		allocated_tokens->Initialize(allocator, 32);
		tokens = allocated_tokens;
	}

	void TokenizedString::DeallocateResizable() {
		AllocatorPolymorphic allocator = tokens.resizable_stream->allocator;
		tokens.resizable_stream->FreeBuffer();
		Deallocate(allocator, tokens.resizable_stream);
	}

}