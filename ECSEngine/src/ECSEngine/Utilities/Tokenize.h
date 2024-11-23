#pragma once
#include "../Core.h"
#include "BasicTypes.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

#define ECS_TOKEN_TYPE_SEPARATOR (0)
#define ECS_TOKEN_TYPE_GENERAL (100)

	struct Token {
		unsigned int offset;
		unsigned int size;
		// This field holds a little bit more information about the type of the token
		// And the user can define its own values for the types and can write into this entry
		// But it most not conflict with ECS_TOKEN_TYPE_GENERAL or ECS_TOKEN_TYPE_SEPARATOR
		unsigned int type;
	};

	struct ECSENGINE_API TokenizedString {
		// A subrange inside the token
		struct Subrange {
			// Returns a subrange that starts from the token index (relative to this subrange)
			// Up until the end of this subrange
			ECS_INLINE Subrange GetSubrangeUntilEnd(unsigned int token_index) const {
				return { token_start_index + token_index, count - token_index };
			}

			ECS_INLINE Subrange GetSubrange(unsigned int subrange_start_index, unsigned int subrange_count) const {
				ECS_ASSERT(subrange_count <= count);
				return { token_start_index + subrange_start_index, subrange_count };
			}

			// Returns the overall token index based on the subrange relative index
			ECS_INLINE unsigned int operator[](unsigned int token_index) const {
				return token_start_index + token_index;
			}

			unsigned int token_start_index;
			unsigned int count;
		};

		// Must be deallocated with DeallocateResizable()
		void InitializeResizable(AllocatorPolymorphic allocator);

		void DeallocateResizable();

		// Returns a subrange that starts from the given index until the end of this token string
		ECS_INLINE Subrange GetSubrangeUntilEnd(unsigned int token_index) const {
			return { token_index, tokens.Size() - token_index };
		}

		// Returns the token string at the given token index
		ECS_INLINE Stream<char> operator[](size_t token_index) const {
			return { string.buffer + tokens[token_index].offset, tokens[token_index].size };
		}

		Stream<char> string;
		AdditionStream<Token> tokens;
	};

	// Tokenizes, meaning it will split the string into individual tokens, based on the separator and the whitespace characters given.
	// Both separators and whitespace characters split the string into tokens, the difference is that whitespace characters are discarded,
	// While separators are maintained. The boolean expand_separator_type_value can be used to expand the type to have a unique value per
	// Separator, i.e. the index of the separator, such that you can prune fast separators, in case you are interested in only a certain
	// Set of separators.
	ECSENGINE_API void TokenizeString(TokenizedString& string, Stream<char> separators, bool expand_separator_type_value, Stream<char> whitespace_characters = " \t\n");

	// Internal function. Used to check that the function TokenizeString works as expected
	ECSENGINE_API bool CheckTokenizedString(const TokenizedString& string, Stream<char> separators, bool expand_separator_type_value, Stream<char> whitespace_characters);

	// Returns the index of the first occurence inside the tokens array of the string of the token, if it finds it, else -1
	// You must specify the offset where the search starts from. This will iterate over all tokens, irrespective of their type.
	ECSENGINE_API unsigned int TokenizeFindToken(const TokenizedString& string, Stream<char> token, TokenizedString::Subrange token_subrange);

	// Returns the index of the first occurence inside the tokens array of the string of the token, if it finds it, else -1
	// You must specify the offset where the search starts from. It will take into consideration only the specified token type.
	ECSENGINE_API unsigned int TokenizeFindToken(const TokenizedString& string, Stream<char> token, unsigned int token_type, TokenizedString::Subrange token_subrange);

	// Returns the index of the first occurence inside the tokens array of the string of the token, if it finds it, else -1
	// You must specify the offset where the search starts from. Only the token type is compared, without the string itself.
	ECSENGINE_API unsigned int TokenizeFindTokenByType(const TokenizedString& string, unsigned int token_type, TokenizedString::Subrange token_subrange);

	// Returns the indices of the tokens that matched the open and the closed token pair. Returns { -1, -1 } if no such pair was found.
	// You must specify the offset where the search starts from
	ECSENGINE_API uint2 TokenizeFindMatchingPair(const TokenizedString& string, Stream<char> open_token, Stream<char> closed_token, TokenizedString::Subrange token_subrange);

	// Returns the indices of the tokens that matched the open and the closed token pair. Returns { -1, -1 } if no such pair was found.
	// This version uses only the token types to distinguish between entries. You must specify the offset where the search starts from
	ECSENGINE_API uint2 TokenizeFindMatchingPairByType(const TokenizedString& string, unsigned int open_type, unsigned int closed_type, TokenizedString::Subrange token_subrange);

	// Merges the tokens given by the sequence into a single string, allocated from the allocator
	ECSENGINE_API Stream<char> TokenizeMergeEntries(const TokenizedString& string, TokenizedString::Subrange sequence, AllocatorPolymorphic allocator);

}