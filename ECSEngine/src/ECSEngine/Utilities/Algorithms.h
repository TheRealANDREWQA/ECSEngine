#pragma once
#include "../Core.h"

namespace ECSEngine {

#ifndef ECS_BYTE_EXTRACT
#define ECS_BYTE_EXTRACT
#define ECS_SCALAR_BYTE1_EXTRACT [](unsigned int a) { return a & 0x000000FF; }
#define ECS_SCALAR_BYTE2_EXTRACT [](unsigned int a) { return (a & 0x0000FF00) >> 8; }
#define ECS_SCALAR_BYTE3_EXTRACT [](unsigned int a) { return (a & 0x00FF0000) >> 16; }
#define ECS_SCALAR_BYTE4_EXTRACT [](unsigned int a) { return (a & 0xFF000000) >> 24; }
#define ECS_SCALAR_BYTE12_EXTRACT [](unsigned int a) { return (unsigned int)(a & 0x0000FFFF); }
#define ECS_SCALAR_BYTE34_EXTRACT [](unsigned int a) { return (unsigned int)((a & 0xFFFF0000) >> 16); }
#define ECS_VECTOR_BYTE1_EXTRACT [](Vec8ui a) { return a & 0x000000FF; }
#define ECS_VECTOR_BYTE2_EXTRACT [](Vec8ui a) { return (a & 0x0000FF00) >> 8; }
#define ECS_VECTOR_BYTE3_EXTRACT [](Vec8ui a) { return (a & 0x00FF0000) >> 16; }
#define ECS_VECTOR_BYTE4_EXTRACT [](Vec8ui a) { return (a & 0xFF000000) >> 24; }
#define ECS_VECTOR_BYTE12_EXTRACT [](Vec8ui a) { return a & 0x0000FFFF; }
#define ECS_VECTOR_BYTE34_EXTRACT [](Vec8ui a) { return (a & 0xFFFF0000) >> 16; }
#define ECS_SCALAR_BYTE1_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return (a - reduction) & 0x000000FF; }
#define ECS_SCALAR_BYTE2_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return ((a - reduction) & 0x0000FF00) >> 8; }
#define ECS_SCALAR_BYTE3_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return ((a - reduction) & 0x00FF0000) >> 16; }
#define ECS_SCALAR_BYTE4_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return ((a - reduction) & 0xFF000000) >> 24; }
#define ECS_SCALAR_BYTE12_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return (a - reduction) & 0x0000FFFF; }
#define ECS_SCALAR_BYTE34_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return ((a - reduction) & 0xFFFF0000) >> 16; }
#define ECS_VECTOR_BYTE1_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return (a - reduction) & 0x000000FF; }
#define ECS_VECTOR_BYTE2_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return ((a - reduction) & 0x0000FF00) >> 8; }
#define ECS_VECTOR_BYTE3_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return ((a - reduction) & 0x00FF0000) >> 16; }
#define ECS_VECTOR_BYTE4_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return ((a - reduction) & 0xFF000000) >> 24; }
#define ECS_VECTOR_BYTE12_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return (a - reduction) & 0x0000FFFF; }
#define ECS_VECTOR_BYTE34_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return ((a - reduction) & 0xFFFF0000) >> 16; }
#endif

	// General binary search algorithm
	// Returns true if it found a value, else false
	// The evaluate functor must return an int with the following meaning
	// 0 - the value was found
	// -1 - the value is greater than the current mid
	// 1 - the value is smaller than the current mid
	// The epsilon is a value that is added/subtracted to the left or right
	// when a new step is needed. Divide by two is a value that is used to
	// divide the difference between left and right to obtain the mid
	template<typename Value, typename EvaluateFunctor>
	bool BinarySearch(Value left, Value right, Value epsilon, Value divide_by_two, Value* result, EvaluateFunctor&& evaluate_functor) {
		while (left <= right) {
			Value mid = left + (right - left) / divide_by_two;
			int evaluation = evaluate_functor(mid);
			if (evaluation == 0) {
				// We found the value
				*result = mid;
				return true;
			}
			else if (evaluation == -1) {
				left = mid + epsilon;
			}
			else {
				right = mid - epsilon;
			}
		}

		return false;
	}

	// General binary search algorithm that switches to a linear step size search
	// Returns true if it found a value, else false
	// The evaluate functor must return an int with the following meaning
	// 0 - switch to linear step size search when in binary search, else the value is found
	// -1 - the value is greater than the current mid
	// 1 - the value is smaller than the current mid
	// The epsilon is a value that is added/subtracted to the left or right
	// when a new binary search step is needed. Divide by two is a value that is used to
	// divide the difference between left and right to obtain the mid
	template<typename Value, typename BinaryEvaluateFunctor, typename LinearEvaluateFunctor>
	bool BinaryIntoLinearSearch(
		Value left,
		Value right,
		Value epsilon,
		Value divide_by_two,
		Value linear_step,
		Value* result,
		BinaryEvaluateFunctor&& binary_evaluate_functor,
		LinearEvaluateFunctor&& linear_evaluate_functor
	) {
		while (left <= right) {
			Value mid = left + (right - left) / divide_by_two;
			int evaluation = binary_evaluate_functor(mid);
			if (evaluation == 0) {
				// Exit the binary search
				break;
			}
			else if (evaluation == -1) {
				left = mid + epsilon;
			}
			else {
				right = mid - epsilon;
			}
		}

		// Linear search mode
		while (left <= right) {
			Value current_value = left;
			int evaluation = linear_evaluate_functor(current_value);
			if (evaluation == 0) {
				*result = current_value;
				return true;
			}
			// If it returns 1, meaning that the value is smaller than the current left
			// Then we overstepped the value. Return the midpoint between the current left
			// and the previous iteration
			if (evaluation == 1) {
				*result = left - linear_step / divide_by_two;
				return true;
			}
			left += linear_step;
		}

		return false;
	}

	template<bool ascending = true, typename T>
	void insertion_sort(T* buffer, size_t size, int increment = 1) {
		size_t i = 0;
		while (i + increment < size) {
			if constexpr (ascending) {
				while (i + increment < size && buffer[i] <= buffer[i + increment]) {
					i += increment;
				}
				int64_t j = i + increment;
				if (j >= size) {
					return;
				}
				while (j - increment >= 0 && buffer[j] < buffer[j - increment]) {
					T temp = buffer[j];
					buffer[j] = buffer[j - increment];
					buffer[j - increment] = temp;
					j -= increment;
				}
			}
			else {
				while (i + increment < size && buffer[i] >= buffer[i + increment]) {
					i += increment;
				}
				int64_t j = i + increment;
				if (j >= size) {
					return;
				}
				while (j - increment >= 0 && buffer[j] < buffer[j - increment]) {
					T temp = buffer[j];
					buffer[j] = buffer[j - increment];
					buffer[j - increment] = temp;
					j -= increment;
				}
			}
		}
	}

	// The comparator takes a left and a right T element
	// It should return -1 if left is smaller than right, 0 if they are equal and 1 if left is greater than right
	template<typename T, typename Comparator>
	void insertion_sort(T* buffer, size_t size, int increment, Comparator&& comparator) {
		size_t i = 0;
		while (i + increment < size) {
			while (i + increment < size) {
				int compare = comparator(buffer[i], buffer[i + increment]);
				if (compare == 1) {
					// Break if left is greater
					break;
				}
				i += increment;
			}
			int64_t j = i + increment;
			if (j >= size) {
				return;
			}
			while (j - increment >= 0 && comparator(buffer[j], buffer[j - increment])) {
				int compare = comparator(buffer[j], buffer[j - increment]);
				if (compare != -1) {
					// Break if left is not smaller than right
					break;
				}
				T temp = buffer[j];
				buffer[j] = buffer[j - increment];
				buffer[j - increment] = temp;
				j -= increment;
			}
		}
	}

	template<typename BufferType, typename ExtractKey>
	void byte_counting_sort(BufferType buffer, size_t size, BufferType result, ExtractKey&& extract_key)
	{
		size_t counts[257] = { 0 };

		// building the frequency count
		for (size_t i = 0; i < size; i++)
		{
			counts[extract_key(buffer[i]) + 1]++;
		}

		// building the array positions
		for (int i = 2; i < 257; i++)
		{
			counts[i] += counts[i - 1];
		}

		// placing the elements into the array
		for (size_t i = 0; i < size; i++) {
			auto key = extract_key(buffer[i]);
			result[counts[key]++] = buffer[i];
		}

	}

	template<typename BufferType, typename ExtractKey>
	void byte_counting_sort(BufferType buffer, size_t size, BufferType result, ExtractKey&& extract_key, unsigned int* counts)
	{
		// building the frequency count
		for (size_t i = 0; i < size; i++)
		{
			counts[extract_key(buffer[i]) + 1]++;
		}

		// building the array positions
		for (int i = 2; i < 65537; i++)
		{
			counts[i] += counts[i - 1];
		}

		// placing the elements into the array
		for (size_t i = 0; i < size; i++) {
			auto key = extract_key(buffer[i]);
			result[counts[key]++] = buffer[i];
		}

	}

	template<typename BufferType, typename ExtractKey>
	void byte_counting_sort(BufferType buffer, size_t size, BufferType result, ExtractKey&& extract_key, unsigned int reduction)
	{
		size_t counts[257] = { 0 };

		// building the frequency count
		for (size_t i = 0; i < size; i++)
		{
			counts[extract_key(buffer[i], reduction) + 1]++;
		}

		// building the array positions
		for (int i = 2; i < 257; i++)
		{
			counts[i] += counts[i - 1];
		}

		// placing the elements into the array
		for (size_t i = 0; i < size; i++) {
			auto key = extract_key(buffer[i], reduction);
			result[counts[key]++] = buffer[i];
		}

	}

	template<typename BufferType, typename ExtractKey>
	void byte_counting_sort(BufferType buffer, size_t size, BufferType result, ExtractKey&& extract_key, unsigned int reduction, unsigned int* counts)
	{
		// building the frequency count
		for (size_t i = 0; i < size; i++)
		{
			counts[extract_key(buffer[i], reduction) + 1]++;
		}

		// building the array positions
		for (int i = 2; i < 65537; i++)
		{
			counts[i] += counts[i - 1];
		}

		// placing the elements into the array
		for (size_t i = 0; i < size; i++) {
			auto key = extract_key(buffer[i], reduction);
			result[counts[key]++] = buffer[i];
		}

	}

	// returns true if the sorted buffer is the intermediate one
	template<bool randomize = false>
	bool unsigned_integer_radix_sort(
		unsigned int* buffer,
		unsigned int* intermediate,
		size_t size,
		unsigned int maximum_element,
		unsigned int minimum_element
	)
	{
		if constexpr (randomize) {
			for (size_t index = size; index < size + (size >> 8); index++) {
				buffer[index] = rand() + maximum_element;
			}
			size += size >> 8;
			maximum_element += RAND_MAX;
		}
		unsigned int reduction = maximum_element - minimum_element;
		bool flag = true;
		if (reduction < 256 && maximum_element >= 256) {
			auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT_REDUCTION;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction);
		}
		else if (reduction < 256 && maximum_element < 256) {
			auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key1));
		}
		else if (reduction < 65'536 && maximum_element >= 65'536) {
			auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT_REDUCTION;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction);
			auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT_REDUCTION;
			byte_counting_sort(intermediate, size, buffer, std::move(extract_key2), reduction);
			flag = false;
		}
		else if (reduction < 65'536 && maximum_element < 65'536) {
			auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key1));
			auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT;
			byte_counting_sort(intermediate, size, buffer, std::move(extract_key2));
			flag = false;
		}
		else if (reduction < 16'777'216 && maximum_element >= 16'777'216) {
			auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT_REDUCTION;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction);
			auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT_REDUCTION;
			byte_counting_sort(intermediate, size, buffer, std::move(extract_key2), reduction);
			auto extract_key3 = ECS_SCALAR_BYTE3_EXTRACT_REDUCTION;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key3), reduction);
			flag = true;
		}
		else if (reduction < 16'777'216 && maximum_element < 16'777'216) {
			auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key1));
			auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT;
			byte_counting_sort(intermediate, size, buffer, std::move(extract_key2));
			auto extract_key3 = ECS_SCALAR_BYTE3_EXTRACT;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key3));
			flag = true;
		}
		else if (reduction >= 16'777'216) {
			auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key1));
			auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT;
			byte_counting_sort(intermediate, size, buffer, std::move(extract_key2));
			auto extract_key3 = ECS_SCALAR_BYTE3_EXTRACT;
			byte_counting_sort(buffer, size, intermediate, std::move(extract_key3));
			auto extract_key4 = ECS_SCALAR_BYTE4_EXTRACT;
			byte_counting_sort(intermediate, size, buffer, std::move(extract_key4));
			flag = false;
		}
		return flag;
	}

	//// 2 bytes at a time
	//static int unsigned_integer_radix_sort(unsigned int* buffer, unsigned int* intermediate, size_t size,
	//	unsigned int maximum_element, unsigned int minimum_element, unsigned int* counts)
	//{
	//	for (size_t index = size; index < size + (size >> 8); index++) {
	//		buffer[index] = rand() + maximum_element;
	//	}
	//	size += size >> 8;
	//	maximum_element += RAND_MAX;
	//	unsigned int reduction = maximum_element - minimum_element;
	//	int flag = 1;
	//	if (reduction < 65536 && maximum_element >= 65536) {
	//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT_REDUCTION;
	//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction, counts);
	//	}
	//	else if (reduction < 65536 && maximum_element < 65536) {
	//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT;
	//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), counts);
	//	}
	//	else {
	//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT;
	//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), counts);
	//		memset(counts, 0, sizeof(unsigned int) * 65537);
	//		auto extract_key2 = ECS_SCALAR_BYTE34_EXTRACT;
	//		byte_counting_sort(intermediate, size, buffer, std::move(extract_key2), counts);
	//		flag = 0;
	//	}
	//	return flag;
	//}

	//// 2 bytes at a time
	//static int unsigned_integer_radix_sort(unsigned int* buffer, unsigned int* intermediate, size_t size,
	//	unsigned int maximum_element, unsigned int minimum_element, unsigned int* counts, bool randomize)
	//{
	//	unsigned int reduction = maximum_element - minimum_element;
	//	int flag = 1;
	//	if (reduction < 65536 && maximum_element >= 65536) {
	//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT_REDUCTION;
	//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction, counts);
	//	}
	//	else if (reduction < 65536 && maximum_element < 65536) {
	//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT;
	//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), counts);
	//	}
	//	else {
	//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT;
	//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), counts);
	//		memset(counts, 0, sizeof(unsigned int) * 65537);
	//		auto extract_key2 = ECS_SCALAR_BYTE34_EXTRACT;
	//		byte_counting_sort(intermediate, size, buffer, std::move(extract_key2), counts);
	//		flag = 0;
	//	}
	//	return flag;
	//}

}