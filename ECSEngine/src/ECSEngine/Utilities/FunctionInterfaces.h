#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Containers/Stream.h"
#include "BasicTypes.h"

ECS_CONTAINERS;

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
		ECSENGINE_API size_t FindWhitespaceCharactersCount(const char* string, CapacityStream<unsigned int>* stack_buffer = nullptr);

		// it searches for spaces and next line characters
		template<typename Stream>
		void FindWhitespaceCharacters(const char* string, Stream& spaces);

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

		// first_value * condition + (1 - condition) * second_value;
		template<typename Type>
		Type PredicateValue(bool condition, Type first_value, Type second_value);

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

		// Walks down the root and allocates the necessary memory in order to have each directory saved separetely
		template<typename Allocator>
		void GetRecursiveDirectories(Allocator* allocator, const wchar_t* root, CapacityStream<const wchar_t*>& directories_paths);

		// Walks down the root and allocates the necessary memory in a single chunk that can then easily be discarded
		template<typename Allocator>
		void GetRecursiveDirectoriesBatchedAllocation(Allocator* allocator, const wchar_t* root, CapacityStream<const wchar_t*>& directories_paths);

		// Walks down the root and allocates the necessary memory in order to have each directory saved separetely
		template<typename Allocator>
		void GetRecursiveDirectories(Allocator* allocator, const wchar_t* root, ResizableStream<const wchar_t*, Allocator>& directories_paths);

		// Walks down the root and allocates the necessary memory in a single chunk that can then easily be discarded
		template<typename Allocator>
		void GetRecursiveDirectoriesBatchedAllocation(Allocator* allocator, const wchar_t* root, ResizableStream<const wchar_t*, Allocator>& directories_paths);

		// Walks down the root and allocates the necessary memory in order to have each file saved separetely
		template<typename Allocator>
		void GetDirectoryFiles(Allocator* allocator, const wchar_t* directory, CapacityStream<const wchar_t*>& file_paths);

		// Walks down the root and allocates the necessary memory in a single chunk that can then easily be discarded
		template<typename Allocator>
		void GetDirectoryFilesBatchedAllocation(Allocator* allocator, const wchar_t* directory, CapacityStream<const wchar_t*>& file_paths);

		// Walks down the root and allocates the necessary memory in order to have each directory saved separetely
		template<typename Allocator>
		void GetDirectoryFiles(Allocator* allocator, const wchar_t* directory, ResizableStream<const wchar_t*, Allocator>& file_paths);

		// Walks down the root and allocates the necessary memory in a single chunk that can then easily be discarded
		template<typename Allocator>
		void GetDirectoryFilesBatchedAllocation(Allocator* allocator, const wchar_t* directory, ResizableStream<const wchar_t*, Allocator>& file_paths);

		template<typename Allocator>
		ECSENGINE_API void* Copy(Allocator* allocator, const void* data, size_t data_size, size_t alignment = 8);

		template<typename Allocator>
		ECSENGINE_API void* CopyTs(Allocator* allocator, const void* data, size_t data_size, size_t alignment = 8);

		template<typename Allocator>
		ECSENGINE_API Stream<void> Copy(Allocator* allocator, Stream<void> data, size_t alignment = 8);

		template<typename Allocator>
		ECSENGINE_API Stream<void> CopyTs(Allocator* allocator, Stream<void> data, size_t alignment = 8);

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

		// Stream should contain as elements Stream<char> or CapacityStream<char>, Stream cannot be ResizableStream
		// But can be easily converted into a normal Stream; returns the index at which the string was found
		template<typename Stream>
		ECSENGINE_API unsigned int IsStringInStream(const char* string, Stream stream);

		// Stream should contain as elements Stream<char> or CapacityStream<char>, Stream cannot be ResizableStream
		// But can be easily converted into a normal Stream; returns the index at which the string was found
		template<typename StreamType>
		ECSENGINE_API unsigned int IsStringInStream(containers::Stream<char> string, StreamType stream);

		// Stream should contain as elements Stream<wchar_t> or CapacityStream<wchar_t>, Stream cannot be ResizableStream
		// But can be easily converted into a normal Stream; returns the index at which the string was found
		template<typename Stream>
		ECSENGINE_API unsigned int IsStringInStream(const wchar_t* string, Stream stream);

		// Stream should contain as elements Stream<wchar_t> or CapacityStream<wchar_t>, Stream cannot be ResizableStream
		// But can be easily converted into a normal Stream; returns the index at which the string was found
		template<typename StreamType>
		ECSENGINE_API unsigned int IsStringInStream(containers::Stream<wchar_t> string, StreamType stream);

		template<typename Stream>
		ECSENGINE_API void MakeSequence(Stream stream);

		template<typename Stream>
		ECSENGINE_API void MakeDescendingSequence(Stream stream);

		template<typename IndexStream>
		ECSENGINE_API void CopyStreamWithMask(void* ECS_RESTRICT buffer, const void* ECS_RESTRICT data, size_t data_element_size, IndexStream indices);

		template<typename IndexStream>
		ECSENGINE_API void CopyStreamWithMask(void* ECS_RESTRICT buffer, Stream<void> data, IndexStream indices);

		template<typename IndexStream>
		ECSENGINE_API void CopyStreamWithMask(std::ofstream& stream, const void* ECS_RESTRICT data, size_t data_element_size, IndexStream indices);

		template<typename IndexStream>
		ECSENGINE_API void CopyStreamWithMask(std::ofstream& stream, Stream<void> data, IndexStream indices);

	}

}