#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Containers/Stream.h"
#include "BasicTypes.h"
#include "File.h"

namespace ECSEngine {

	namespace function {

		// AVX2 accelerated memcpy, works best for workloads of at least 1KB
		template<bool unaligned = false>
		void avx2_copy_32multiple(void* destination, const void* source, size_t bytes);

		// AVX2 accelerated memcpy, works best for workloads of at least 1KB
		template<bool unaligned = false>
		void avx2_copy_128multiple(void* destination, const void* source, size_t bytes);

		// allocates memory on the heap;
		// if exact_length is false, a maximum of 1024 characters can be converted
		// if exact_length is true, the pointer will be strlen-ed 
		template<bool exact_length = false>
		wchar_t* ConvertASCIIToWide(const char* pointer);

		// Calculates the integral and fractional parts and then commits them into a float each that then get summed up
		template<typename Stream>
		float ConvertCharactersToFloat(Stream characters);
		
		// Calculates the integral and fractional parts and then commits them into a double each that then get summed up
		template<typename Stream>
		double ConvertCharactersToDouble(Stream characters);

		// Finds the number of characters that needs to be allocated and a stack buffer can be supplied in order
		// to fill it if the count is smaller than its capacity
		// The token can be changed from ' ' to another supplied value (e.g. \\ for paths or /)
		ECSENGINE_API size_t FindWhitespaceCharactersCount(const char* string, char separator_token = ' ', CapacityStream<unsigned int>*stack_buffer = nullptr);

		// it searches for spaces and next line characters
		template<typename Stream>
		void FindWhitespaceCharacters(Stream& spaces, const char* string, char separator_token = ' ');

		// finds the tokens that appear in the current string
		template<typename Stream>
		void FindToken(const char* string, char token, Stream& tokens);

		// finds the tokens that appear in the current string
		template<typename Stream>
		void FindToken(const char* ECS_RESTRICT string, const char* ECS_RESTRICT token, Stream& tokens);

		// it searches for spaces and next line characters; Stream must be uint2
		template<typename Stream>
		void ParseWordsFromSentence(const char* sentence, Stream& words);

		// given a token, it will separate words out from it; Stream must be uint2
		template<typename Stream>
		void ParseWordsFromSentence(const char* sentence, char token, Stream& words);

		// given multiple tokens, it will separate words based from them; Stream must be uint2
		template<typename Stream>
		void ParseWordsFromSentence(const char* ECS_RESTRICT sentence, const char* ECS_RESTRICT tokens, Stream& words);

		// condition ? first_value : second_value
		// The only full template because it is a one liner
		template<typename Type>
		Type Select(bool condition, Type first_value, Type second_value) {
			return condition ? first_value : second_value;
		}

		// Calculates the integral and fractional parts and then commits them into a floating point type each that then get summed up 
		// Returns the count of characters written
		template<typename Stream>
		size_t ConvertFloatingPointIntegerToChars(Stream& chars, int64_t integer, size_t precision);

		// Shifts the float by the power of 10 to the precision that then get cast into an integer;
		// Afterwards performs the necessary parsing for 0. values and case evaluation for rounding
		// Returns the count of characters written
		template<typename Stream>
		size_t ConvertFloatToChars(Stream& chars, float value, size_t precision = 2);

		// Shifts the float by the power of 10 to the precision that then get cast into an integer;
		// Afterwards performs the necessary parsing for 0. values and case evaluation for rounding
		// Returns the count of characters written
		template<typename Stream>
		size_t ConvertDoubleToChars(Stream& chars, double value, size_t precision = 2);

		// Works with decimal digits that then get pushed into the stream;
		// Adds apostrophes ' in order to have an easier way of distinguishing powers of 10
		// Returns the count of characters written
		template<typename Stream>
		size_t ConvertIntToCharsFormatted(Stream& chars, int64_t value);

		// Works with decimal digits that then get pushed into the stream;
		// Vanilla conversion; no apostrophes '
		// Returns the count of characters written
		template<typename Stream>
		size_t ConvertIntToChars(Stream& chars, int64_t value);

		// Non digit characters are discarded
		template<typename Integer, typename Stream>
		Integer ConvertCharactersToInt(Stream stream);

		// Non digit characters are discarded
		template<typename Integer, typename Stream>
		Integer ConvertCharactersToInt(Stream stream, size_t& digit_count);

		// Non digit characters are discarded
		template<typename FloatingPoint, typename Stream>
		FloatingPoint ConvertCharactersToFloatingPoint(Stream stream);

		template<typename CharacterType, typename ReturnType, typename Functor>
		ReturnType ParseType(const CharacterType** characters, Stream<CharacterType> delimiters, Functor&& functor) {
			Stream<CharacterType> stream_characters(*characters, 0);

			CharacterType current_character = **characters;
			bool continue_search = true;

			auto not_delimiter = [delimiters](CharacterType character) {
				for (size_t index = 0; index < delimiters.size; index++) {
					if (delimiters[index] == character) {
						return false;
					}
				}
				return true;
			};

			continue_search = not_delimiter(current_character);
			while (continue_search) {
				current_character = **characters;
				*characters++;
				continue_search = not_delimiter(current_character);
			}

			stream_characters.size = *characters - stream_characters.buffer;
			return functor(stream_characters);
		}

		template<typename Integer, typename CharacterType>
		Integer ParseInteger(const CharacterType** characters, Stream<CharacterType> delimiters) {
			return ParseType<CharacterType, Integer>(characters, delimiters, [](Stream<CharacterType> stream_characters) {
				return ConvertCharactersToInt<Integer>(stream_characters);
			});
		}

		template<typename CharacterType>
		ECSENGINE_API float ParseFloat(const CharacterType** characters, Stream<CharacterType> delimiters);

		template<typename CharacterType>
		ECSENGINE_API double ParseDouble(const CharacterType** characters, Stream<CharacterType> delimiters);

		template<typename CharacterType>
		ECSENGINE_API bool ParseBool(const CharacterType** characters, Stream<CharacterType> delimiters);

		ECSENGINE_API void* Copy(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment = 8);

		ECSENGINE_API Stream<void> Copy(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment = 8);

		template<typename Allocator>
		ECSENGINE_API void* Copy(Allocator* allocator, const void* data, size_t data_size, size_t alignment = 8);

		template<typename Allocator>
		ECSENGINE_API void* CopyTs(Allocator* allocator, const void* data, size_t data_size, size_t alignment = 8);

		template<typename Allocator>
		ECSENGINE_API Stream<void> Copy(Allocator* allocator, Stream<void> data, size_t alignment = 8);

		template<typename Allocator>
		ECSENGINE_API Stream<void> CopyTs(Allocator* allocator, Stream<void> data, size_t alignment = 8);

		// It will copy the null termination character
		ECSENGINE_API Stream<char> StringCopy(AllocatorPolymorphic allocator, const char* string);

		// It will copy the null termination character
		ECSENGINE_API Stream<char> StringCopy(AllocatorPolymorphic allocator, Stream<char> string);

		// It will copy the null termination character
		template<typename Allocator>
		ECSENGINE_API Stream<char> StringCopy(Allocator* allocator, const char* string);

		// It will copy the null termination character
		template<typename Allocator>
		ECSENGINE_API Stream<char> StringCopy(Allocator* allocator, Stream<char> string);

		// It will copy the null termination characters
		template<typename Allocator>
		ECSENGINE_API Stream<char> StringCopyTs(Allocator* allocator, const char* string);

		// It will copy the null termination characters
		template<typename Allocator>
		ECSENGINE_API Stream<char> StringCopyTs(Allocator* allocator, Stream<char> string);

		ECSENGINE_API Stream<wchar_t> StringCopy(AllocatorPolymorphic allocator, const wchar_t* string);

		ECSENGINE_API Stream<wchar_t> StringCopy(AllocatorPolymorphic allocator, Stream<wchar_t> string);

		// It will copy the null termination character
		template<typename Allocator>
		ECSENGINE_API Stream<wchar_t> StringCopy(Allocator* allocator, const wchar_t* string);

		// It will copy the null termination characters
		template<typename Allocator>
		ECSENGINE_API Stream<wchar_t> StringCopyTs(Allocator* allocator, const wchar_t* string);

		// It will copy the null termination character
		template<typename Allocator>
		ECSENGINE_API Stream<wchar_t> StringCopy(Allocator* allocator, Stream<wchar_t> string);

		// It will copy the null termination characters
		template<typename Allocator>
		ECSENGINE_API Stream<wchar_t> StringCopyTs(Allocator* allocator, Stream<wchar_t> string);

		template<typename Stream>
		ECSENGINE_API void MakeSequence(Stream stream);

		template<typename Stream>
		ECSENGINE_API void MakeDescendingSequence(Stream stream);

		// The size of data must contain the byte size of the element
		template<typename IndexStream>
		ECSENGINE_API void CopyStreamWithMask(void* ECS_RESTRICT buffer, Stream<void> data, IndexStream indices);

		// The size of data must contain the byte size of the element
		// Reports if an error occurred during writing
		template<typename IndexStream>
		ECSENGINE_API bool CopyStreamWithMask(ECS_FILE_HANDLE file, Stream<void> data, IndexStream indices);

	}

}