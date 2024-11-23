#include "ecspch.h"
#include "Tokenize.h"
#include "StringUtilities.h"
#include "Utilities.h"

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