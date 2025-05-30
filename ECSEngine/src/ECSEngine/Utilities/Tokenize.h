#pragma once
#include "../Core.h"
#include "BasicTypes.h"
#include "../Containers/Stream.h"
#include <vector>

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

			ECS_INLINE Subrange GetSubrangeAfterUntilEnd(unsigned int token_index) const {
				return GetSubrangeUntilEnd(token_index).AdvanceReturn();
			}

			ECS_INLINE Subrange GetSubrange(unsigned int subrange_start_index, unsigned int subrange_count) const {
				ECS_ASSERT(subrange_count <= count);
				return { token_start_index + subrange_start_index, subrange_count };
			}

			// Advances by one token (from the beginning) and returns the subrange
			ECS_INLINE Subrange AdvanceReturn() const {
				return { token_start_index + 1, count - 1 };
			}

			// Returns the overall token index based on the subrange relative index
			ECS_INLINE unsigned int operator[](unsigned int token_index) const {
				return token_start_index + token_index;
			}

			unsigned int token_start_index = 0;
			unsigned int count = 0;
		};

		// Must be deallocated with DeallocateResizable()
		void InitializeResizable(AllocatorPolymorphic allocator);

		void DeallocateResizable();

		// Returns a subrange that starts from the given index until the end of this token string
		ECS_INLINE Subrange GetSubrangeUntilEnd(unsigned int token_index) const {
			return { token_index, tokens.Size() - token_index };
		}

		ECS_INLINE Subrange AsSubrange() const {
			return GetSubrangeUntilEnd(0);
		}

		// Returns the token string at the given token index
		ECS_INLINE Stream<char> operator[](size_t token_index) const {
			return { string.buffer + tokens[token_index].offset, tokens[token_index].size };
		}
		
		// Returns the contiguous characters that correspond to the given subrange
		ECS_INLINE Stream<char> GetStreamForSubrange(Subrange subrange) const {
			if (subrange.count == 0) {
				return {};
			}

			unsigned int first_offset = tokens[subrange[0]].offset;
			return { string.buffer + first_offset, tokens[subrange[subrange.count - 1]].offset - first_offset + tokens[subrange[subrange.count - 1]].size };
		}

		// Returns a reference to this internal string arrays for a given subrange that can be treated
		// Like an individual tokenized string. It requires a few temporary bytes to write some values,
		// These bytes must be valid for the entire duration of the returned value.
		TokenizedString GetSubrangeAsTokenizedString(Subrange subrange, char temp_memory[32]) const {
			TokenizedString result;
			// Keep the string the same, since the token offsets are maintained the same
			result.string = string;
			result.tokens = (CapacityStream<Token>*)temp_memory;

			if (subrange.count == 0) {
				result.tokens.capacity_stream->Reset();
				return result;
			}

			// Always use the tokens as a capacity stream for this returned string, the resizable cannot be used
			// Because it is not standalone. Must use an explicit capacity stream, not a simple Stream<> as what
			// SliceAt returns because this is the type that is referenced
			*result.tokens.capacity_stream = tokens.capacity_stream->SliceAt(subrange.token_start_index, subrange.count);

			return result;
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

	// Returns the index of the first occurence inside the tokens array of the string of the token in reverse order, if it finds it, else -1
	// You must specify the offset where the search starts from. This will iterate over all tokens, irrespective of their type.
	ECSENGINE_API unsigned int TokenizeFindTokenReverse(const TokenizedString& string, Stream<char> token, TokenizedString::Subrange token_subrange);

	// Returns the index of the first occurence inside the tokens array of the string of the token, if it finds it, else -1
	// You must specify the offset where the search starts from. It will take into consideration only the specified token type.
	ECSENGINE_API unsigned int TokenizeFindToken(const TokenizedString& string, Stream<char> token, unsigned int token_type, TokenizedString::Subrange token_subrange);

	// Returns the index of the first occurence inside the tokens array of the string of the token, if it finds it, else -1
	// You must specify the offset where the search starts from. Only the token type is compared, without the string itself.
	ECSENGINE_API unsigned int TokenizeFindTokenByType(const TokenizedString& string, unsigned int token_type, TokenizedString::Subrange token_subrange);

	// Returns the index of the first occurence inside the tokens array of the string of the token in reverse order, if it finds it, else -1
	// You must specify the offset where the search starts from. Only the token type is compared, without the string itself.
	ECSENGINE_API unsigned int TokenizeFindTokenByTypeReverse(const TokenizedString& string, unsigned int token_type, TokenizedString::Subrange token_subrange);

	// Returns the indices of the tokens that matched the open and the closed token pair. Returns { -1, -1 } if no such pair was found.
	// You must specify the offset where the search starts from. If the search_closed_only option is enablend, then it considers that one open
	// Token was already found, and only the search for its pair is needed
	ECSENGINE_API uint2 TokenizeFindMatchingPair(const TokenizedString& string, Stream<char> open_token, Stream<char> closed_token, TokenizedString::Subrange token_subrange, bool search_closed_only = false);

	// Returns the indices of the tokens that matched the open and the closed token pair. Returns { -1, -1 } if no such pair was found.
	// This version uses only the token types to distinguish between entries. You must specify the offset where the search starts from. 
	// If the search_closed_only option is enablend, then it considers that one open token was already found, and only the search for its pair is needed
	ECSENGINE_API uint2 TokenizeFindMatchingPairByType(const TokenizedString& string, unsigned int open_type, unsigned int closed_type, TokenizedString::Subrange token_subrange, bool search_closed_only = false);

	// Merges the tokens given by the sequence into a single string, allocated from the allocator
	ECSENGINE_API Stream<char> TokenizeMergeEntries(const TokenizedString& string, TokenizedString::Subrange sequence, AllocatorPolymorphic allocator);

	// Splits the tokens by the given separator. The token ranges are relative to the original string, not to the token subrange
	ECSENGINE_API void TokenizeSplitBySeparator(const TokenizedString& string, Stream<char> separator, TokenizedString::Subrange token_subrange, AdditionStream<TokenizedString::Subrange> token_ranges);

	// Splits the tokens by the given separator. The token ranges are relative to the original string, not to the token subrange
	ECSENGINE_API void TokenizeSplitBySeparatorType(const TokenizedString& string, unsigned int separator_type, TokenizedString::Subrange token_subrange, AdditionStream<TokenizedString::Subrange> token_ranges);
	
#pragma region Tokenize Rule

	// How many tokens for the rule should match
	enum ECS_TOKENIZE_RULE_COUNT_TYPE : unsigned char {
		ECS_TOKENIZE_RULE_ZERO_OR_MORE,
		ECS_TOKENIZE_RULE_ONE_OR_MORE,
		ECS_TOKENIZE_RULE_ONE,
		ECS_TOKENIZE_RULE_ZERO_OR_ONE,
		// This must be used in conjuction with ONE_PAIR_END and only a
		// It will use a matched find to find the closing pair. Can be used only
		// With the selection of MATCH_BY_TYPE or MATCH_BY_STRING with a single value
		ECS_TOKENIZE_RULE_ONE_PAIR_START,
		// This must be used in conjunction with ONE_PAIR_START
		ECS_TOKENIZE_RULE_ONE_PAIR_END,
	};

	// The type of token that it can match
	enum ECS_TOKENIZE_RULE_SELECTION_TYPE : unsigned char {
		ECS_TOKENIZE_RULE_SELECTION_ANY,
		ECS_TOKENIZE_RULE_SELECTION_SEPARATOR,
		ECS_TOKENIZE_RULE_SELECTION_GENERAL,
		ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_TYPE,
		ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING,
		ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_SUBRULE,
	};

	struct TokenizeRuleEntry;

	// A structure that describes a sequence of tokens that satisfy a rule. 
	// Each rule can contain one or more sets, the rule being satisfied if one of the sets is matched
	struct ECSENGINE_API TokenizeRule {
		struct EntrySet {
			ECS_INLINE EntrySet Copy(AllocatorPolymorphic allocator) const {
				return { StreamDeepCopy(entries, allocator), minimum_token_count };
			}

			ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
				StreamDeallocateElements(entries, allocator);
				entries.Deallocate(allocator);
			}

			ECS_INLINE TokenizeRuleEntry& operator[](size_t index) {
				return entries[index];
			}
			
			ECS_INLINE const TokenizeRuleEntry& operator[](size_t index) const {
				return entries[index];
			}

			Stream<TokenizeRuleEntry> entries;
			// This is a cached value that helps in quickly retrieving the minimum amount of tokens for this set
			unsigned int minimum_token_count;
			bool is_variable_length;
		};

		// Returns true if the entries of both rules are of the same type and data, else false
		bool Compare(const TokenizeRule& other) const;

		TokenizeRule Copy(AllocatorPolymorphic allocator) const;

		void Deallocate(AllocatorPolymorphic allocator);

		// Computes and caches the value of the minimum token count needed by this rule. It will
		// Call transiently this function on subrules as well
		void ComputeCachedValues();

		ECS_INLINE bool IsEmpty() const {
			return sets.size == 0;
		}

		// This field exists for debugging purposes, it does need to exist for functional purposes
		Stream<char> name;
		Stream<EntrySet> sets;
		// This is a cached value that helps in quickly retrieving the minimum amount of tokens for this rule
		unsigned int minimum_token_count;
		// This is a cached value for the info of 
		bool is_variable_length;
	};

	// Extra data for selection rules
	union TokenizeRuleSelectionData {
		// To satisfy the compiler
		ECS_INLINE TokenizeRuleSelectionData() {}

		// Data for ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_TYPE
		struct {
			bool is_multiple_type_indices;
			union {
				Stream<unsigned int> type_indices;
				unsigned int type_index;
			};
		};

		// Data for ECS_TOKENIZE_RULE_SELECTION_MATCH_BY_STRING
		struct {
			bool is_multiple_strings;
			union {
				Stream<Stream<char>> strings;
				Stream<char> string;
			};
		};

		// Data for ECS_TOKENIZE_RULE_SELECTION_MATCH_SUBRULE
		struct {
			TokenizeRule subrule;
		};
	};

	// A structure that contains one element of a rule.
	struct ECSENGINE_API TokenizeRuleEntry {
		// Returns true if the given entry is of the same type and same data, else false
		bool Compare(const TokenizeRuleEntry& other) const;

		// The same as Compare, but it doesn't check for the count type field to be the same
		bool CompareExceptCountType(const TokenizeRuleEntry& other) const;

		TokenizeRuleEntry Copy(AllocatorPolymorphic allocator) const;

		void Deallocate(AllocatorPolymorphic allocator);

		ECS_TOKENIZE_RULE_COUNT_TYPE count_type;
		ECS_TOKENIZE_RULE_SELECTION_TYPE selection_type;
		bool is_selection_negated;
		TokenizeRuleSelectionData selection_data;
	};

	struct TokenizeRuleCallbackData {
		// Only the string is assigned
		ECS_INLINE TokenizeRuleCallbackData(const TokenizedString& string) : string(string) {}

		const TokenizedString& string;
		TokenizedString::Subrange subrange;
		// This is data that is assigned at matcher time
		void* user_data;
		// This is data that is given at the Match call
		void* call_specific_data;
		// The index of the set that matched
		unsigned int set_index_that_matched;
	};

	enum ECS_TOKENIZE_RULE_CALLBACK_RESULT : unsigned char {
		// When this value is returned, the matching is abandoned
		ECS_TOKENIZE_RULE_CALLBACK_EXIT,
		// This should be returned always on success by the post exclude callbacks, while for pre exclude callbacks
		// This indicates that this rule matched the input and it handled it
		ECS_TOKENIZE_RULE_CALLBACK_MATCHED,
		// This should be used by the pre exclude callbacks only, it signals that the tokens matched the rule, but the
		// Callback decided to not handle this and that matching should continue forwards
		ECS_TOKENIZE_RULE_CALLBACK_UNMATCHED,
	};

	// This is a callback that is called when a rule is matched. It receives the tokenized string and the subrange that matched the rule,
	// It needs to return an appropriate result type.
	typedef ECS_TOKENIZE_RULE_CALLBACK_RESULT (*TokenizeRuleCallback)(const TokenizeRuleCallbackData* data);
	
	struct TokenizeRuleAction {
		TokenizeRule rule;
		TokenizeRuleCallback callback;
		Stream<void> callback_data;
	};

	enum ECS_TOKENIZE_MATCHER_RESULT : unsigned char {
		ECS_TOKENIZE_MATCHER_EARLY_EXIT,
		ECS_TOKENIZE_MATCHER_FAILED_TO_MATCH_ALL,
		ECS_TOKENIZE_MATCHER_SUCCESS
	};

	// A structure that encompasses multiple excluding rules and actions to be performed on a tokenized string
	struct ECSENGINE_API TokenizeRuleMatcher {
		ECS_INLINE AllocatorPolymorphic Allocator() const {
			return pre_exclude_actions.allocator;
		}

		// By default, it will make a deep copy of the rule. You can disable it with the last argument
		void AddExcludeRule(const TokenizeRule& rule, bool deep_copy = true);

		// By default, it will make a deep copy of both the rule and the callback. You can disable that with the last argument
		void AddPreExcludeAction(const TokenizeRuleAction& action, bool deep_copy = true);

		// By default, it will make a deep copy of both the rule and the callback. You can disable that with the last argument
		void AddPostExcludeAction(const TokenizeRuleAction& action, bool deep_copy = true);

		// If no deep copies were made per each type, you can omit them from deallocating
		void Deallocate(bool deallocate_exclude_rules = true, bool deallocate_actions = true);

		void Initialize(AllocatorPolymorphic allocator);

		// It will match the given token string subrange with the stored actions, by using a backtracking search. It returns true if it early existed, else false.
		// The call specific data will be passed to callbacks, to use it as they see fit
		ECS_TOKENIZE_MATCHER_RESULT MatchBacktracking(const TokenizedString& string, TokenizedString::Subrange subrange, void* call_specific_data) const;

		// It will find the rule that matches the highest amount of tokens and use that one. It returns true if it early existed, else false.
		// The call specific data will be passed to callbacks, to use it as they see fit
		ECS_TOKENIZE_MATCHER_RESULT MatchRulesWithFind(const TokenizedString& string, TokenizedString::Subrange subrange, void* call_specific_data) const;

		// If you want to iterate certain token counts before others, you can specify them here, such that the relative ordering is maintained.
		// You can use the value of -1 as a last value to indicate to start matching from count 1 to the max, without retesting existing counts
		void SetCustomSubrangeOrder(Stream<unsigned int> counts);

		// This array contains rules that discard sequences of tokens.
		ResizableStream<TokenizeRule> exclude_rules;
		// This array contains the entries that contain actions to be performed, which are checked before the exclude rules
		ResizableStream<TokenizeRuleAction> pre_exclude_actions;
		ResizableStream<TokenizeRuleAction> post_exclude_actions;
		// The user can supplies the order the subranges are tested in, such that the rule has a chance
		// To be called on a subrange count value before another one
		ResizableStream<unsigned int> custom_subrange_order;
	};

	// The rules for a TokenizeRule made out of strings - which is easier to write down than to create each individual
	// Entry by hand, is the following: 
	// $T - matches any token
	// $S - matches any separator
	// $G - matches any general token
	// [] - matches any character from inside the brackets. Cannot nest [], () or definitions inside it
	// If multiple entire strings are to be matched, you must separate them using a comma , which means
	// that you cannot use comma as a character inside [].
	// () - enclose a subrule
	// ! - before an expression, it means to negate that match. For special token types,
	// like $T, it must be placed before $, i.e. !$T or !$(
	// $( - match the ( character
	// $) - match the ) character
	// $[ - match the [ character
	// $] - match the ] character
	// $. - match the . character
	// $? - match the ? character
	// $+ - match the + character
	// $* - match the * character
	// \ - when it appears before a character, it signals that this is the start of a paired character
	// / - when it appears before a character, it signals that this is the end of a paired character
	// | - logical or between expressions
	// . - match the previous token selection only once
	// ? - match the previous token selection once or zero times (optional)
	// + - match the previous token selection one or more times
	// * - match the previous token selection zero or more times
	// !, $, \, / and | currently cannot be used at all as characters
	// You can use defines, like 
	// identifier_no_template = $G.
	// identifier_template = $G.\<$T*/>
	// identifier = identifier_no_template | identifier_template
	// Each definition must be on its own line. The final rule should be on the last line
	// If the rule is not valid, like the paired characters could not be determined, it will return an empty rule
	// And it will optionally fill in an error message. The boolean allocator_is_temporary can be used to tell the
	// Function that even temporary allocations can be made from it. It will result in a faster call since it will
	// Avoid having to work on temporaries that are committed in the parameter allocator at the end
	// The resulting rule doesn't have a name assigned to it
	ECSENGINE_API TokenizeRule CreateTokenizeRule(Stream<char> string_rule, AllocatorPolymorphic allocator, bool allocator_is_temporary, CapacityStream<char>* error_message = nullptr);

	// Returns true if the given rule matches the string, else false. If it matches the string, it can optionally report how many
	// Tokens each entry used. The cached values for the rule must be computed beforehand! You can optionally retrieve the set that matched the rule
	ECSENGINE_API bool MatchTokenizeRuleBacktracking(const TokenizedString& string, TokenizedString::Subrange subrange, const TokenizeRule& rule, unsigned int* set_index_that_matched = nullptr, CapacityStream<unsigned int>* matched_token_counts = nullptr);

	// Returns the longest range of consecutive tokens that matches the rule that starts from the beginning of the subrange, or -1 if there is no such range.
	// You can optionally retrieve the index of the set that matched the range, in case a match is found
	ECSENGINE_API unsigned int FindTokenizeRuleMatchingRange(const TokenizedString& string, const TokenizeRule& rule, TokenizedString::Subrange subrange, unsigned int* set_index_that_matched = nullptr);

	// A function for testing the matching
	ECSENGINE_API void MatchTokenizeRuleTest();

	// Returns true if the rule is valid, else false. It can optionally fill in an error message in case it is not valid
	ECSENGINE_API bool ValidateTokenizeRule(const TokenizeRule& rule, CapacityStream<char>* error_message = nullptr);

	// This adds the definition for qualifier, which you can reference later one. Qualifier handles the existence of & or **
	ECSENGINE_API void AddTokenizeRuleQualifiers(CapacityStream<char>& rule_string);

	// Adds the definitions for type identifiers, which include the & and * qualifiers as well (by adding the qualifier definition).
	ECSENGINE_API void AddTokenizeRuleIdentifierDefinitions(CapacityStream<char>& rule_string);

	// Returns a rule that matches function definitions/declarations. You can decide to allow only definitions, only declarations, both,
	// And whether or not templates should be accepted or not
	ECSENGINE_API TokenizeRule GetTokenizeRuleForFunctions(AllocatorPolymorphic allocator, bool allocator_is_temporary, bool include_declarations = true, bool include_definitions = true, bool include_template = true);

	// Returns a rule that matches struct operators, like operator+, operator ==, while taking into account
	// The fact that the operators can be made default (like for =)
	ECSENGINE_API TokenizeRule GetTokenizeRuleForStructOperators(AllocatorPolymorphic allocator, bool allocator_is_temporary);

	// Returns a rule that matches struct constructors, including the case of = default, and takes care of inline
	// Definition of the constructor
	ECSENGINE_API TokenizeRule GetTokenizeRuleForStructConstructors(AllocatorPolymorphic allocator, bool allocator_is_temporary);

	// Must be kept in sync with the function GetCppFileTokenSeparators
	// These are the separators that can appear in a CPP file that we are interested in
	enum ECS_CPP_FILE_SEPARATOR_TYPE : unsigned char {
		ECS_CPP_FILE_SEPARATOR_PLUS,
		ECS_CPP_FILE_SEPARATOR_MINUS,
		ECS_CPP_FILE_SEPARATOR_ASTERISK,
		ECS_CPP_FILE_SEPARATOR_MODULO,
		ECS_CPP_FILE_SEPARATOR_XOR,
		ECS_CPP_FILE_SEPARATOR_DIVIDE,
		ECS_CPP_FILE_SEPARATOR_OR,
		ECS_CPP_FILE_SEPARATOR_AND,
		ECS_CPP_FILE_SEPARATOR_DOT,
		ECS_CPP_FILE_SEPARATOR_COMMA,
		ECS_CPP_FILE_SEPARATOR_COLON,
		ECS_CPP_FILE_SEPARATOR_SEMICOLON,
		ECS_CPP_FILE_SEPARATOR_EQUALS,
		ECS_CPP_FILE_SEPARATOR_OPEN_PAREN,
		ECS_CPP_FILE_SEPARATOR_CLOSED_PAREN,
		ECS_CPP_FILE_SEPARATOR_OPEN_SCOPE,
		ECS_CPP_FILE_SEPARATOR_CLOSED_SCOPE,
		ECS_CPP_FILE_SEPARATOR_OPEN_SQUARE_BRACKET,
		ECS_CPP_FILE_SEPARATOR_CLOSED_SQUARE_BRACKET,
	};

	ECS_INLINE Stream<char> GetCppFileTokenSeparators() {
		// Must be kept in sync with the above enum
		return "+-*%^/|&.,:;=()<>{}[]!";
	}

	ECS_INLINE Stream<char> GetCppTypeTokenSeparators() {
		return "<>,&*:";
	}

	// Must be kept in sync with the function GetCppEnumTokenSeparators
	enum ECS_CPP_ENUM_TOKEN_SEPARATOR_TYPE : unsigned char {
		ECS_CPP_ENUM_TOKEN_SEPARATOR_COMMA,
		ECS_CPP_ENUM_TOKEN_SEPARATOR_EQUALS
	};

	ECS_INLINE Stream<char> GetCppEnumTokenSeparators() {
		// Must be kept in sync with the above enum
		return ",=";
	}

#pragma endregion

}