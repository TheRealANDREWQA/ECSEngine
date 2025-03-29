#pragma once
#include <type_traits>
#include <stdint.h>
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Containers/DataPointer.h"

namespace ECSEngine {

	// Helper structure that provides that allows functions to receive an iterable collection
	// ValueType must be fully qualified for const types, meaning the const should be added to the template parameter
	template<typename ValueType>
	struct IteratorInterface
	{
		ECS_INLINE IteratorInterface(size_t _remaining_count) : remaining_count(_remaining_count) {}

		typedef void (*Functor)(ValueType* value, void* data);

		typedef bool (*ExitFunctor)(ValueType* value, void* data);

		virtual ValueType* Get() = 0;

		// Returns true if the elements are contiguous and that a pointer and a size is enough to reference a subrange
		virtual bool IsContiguous() const = 0;

	protected:
		// Constructs an iterator that references the start of the current range for a given
		// Count of elements. The function must allocate a structure that it constructs and it returns.
		virtual IteratorInterface<ValueType>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) = 0;

	public:
		// Allocates an constructs an iterator the references the start of the current range for a given
		// Count of elements. If count is larger than the remaining element count, then it will reference
		// Only the remaining count of elements. If the remaining count is 0, then it will not allocate anything
		// and return nullptr.
		IteratorInterface<ValueType>* CreateSubIterator(AllocatorPolymorphic allocator, size_t count) {
			if (remaining_count == 0) {
				return nullptr;
			}
			size_t subrange_count = remaining_count >= count ? count : remaining_count;
			IteratorInterface<ValueType>* subiterator = CreateSubIteratorImpl(allocator, subrange_count);
			subiterator->remaining_count = count;
			remaining_count -= subrange_count;
			return subiterator;
		}

		// Fills in the given buffer with a given count of elements. If there are not enough elements, it will fill the
		// Remaining ones and return false. Returns true if the count of elements could be filled.
		bool GetSubrange(std::remove_const_t<ValueType>* values, size_t count) {
			size_t iterate_count = remaining_count >= count ? count : remaining_count;
			for (size_t index = 0; index < iterate_count; index++) {
				values[index] = *Get();
			}
			return iterate_count < count;
		}

		// Writes the entire remaining set of data to the given buffer. The remaining count will be 0 after this call.
		// It uses the fast path for contiguous iterators of memcpying the data directly.
		void WriteTo(std::remove_const_t<ValueType>* buffer) {
			if (IsContiguous()) {
				memcpy(buffer, Get(), sizeof(ValueType) * remaining_count);
			}
			else {
				GetSubrange(buffer, remaining_count);
			}
			remaining_count = 0;
		}

		// Writes the entire set of data to a buffer that is allocated. The remaining count will be 0 after this call.
		// It uses the fast path for contiguous iterators of memcpying the data directly. The count pointer is an optional
		// Value that can be set to the allocated count
		ValueType* WriteTo(AllocatorPolymorphic allocator, size_t* count = nullptr) {
			size_t allocation_size = sizeof(ValueType) * remaining_count;
			if (count != nullptr) {
				*count = remaining_count;
			}
			std::remove_const_t<ValueType>* buffer = (std::remove_const_t<ValueType>*)Allocate(allocator, allocation_size);
			WriteTo(buffer);
			return buffer;
		}

		void ForEach(Functor functor, void* data) {
			for (size_t index = 0; index < remaining_count; index++) {
				functor(Get(), data);
			}
			remaining_count = 0;
		}

		// Returns true if it early exited, else false
		bool ForEachExit(ExitFunctor functor, void* data) {
			bool should_exit = false;
			for (size_t index = 0; index < remaining_count && !should_exit; index++) {
				should_exit = functor(Get(), data);
			}
			remaining_count = 0;
			return should_exit;
		}

		// Helper function that allows using a lambda instead of creating a manual function
		// And its set of data. The functor receives as argument (Value& value);
		template<typename FunctorType>
		ECS_INLINE void ForEach(FunctorType&& functor) {
			auto wrapper = [](ValueType* value, void* data) {
				FunctorType* functor = (FunctorType*)data;
				(*functor)(value);
			};
			ForEach(wrapper, &functor);
		}

		// Helper function that allows using a lambda instead of creating a manual function
		// And its set of data. The functor receives as argument (Value& value);
		// Returns true if it early exited, else false
		template<typename FunctorType>
		ECS_INLINE bool ForEachExit(FunctorType&& functor) {
			auto wrapper = [](ValueType* value, void* data) {
				FunctorType* functor = (FunctorType*)data;
				return (*functor)(value);
			};
			return ForEachExit(wrapper, &functor);
		}

		template<typename = std::enable_if_t<!std::is_const_v<ValueType>>>
		ECS_INLINE IteratorInterface<const ValueType>* AsConst() const {
			// Can cast directly, the vtable pointers just work fine for this use case, since
			// The signature is the same
			return (IteratorInterface<const ValueType>*)this;
		}

		size_t remaining_count = 0;
	};

	template<typename ValueType>
	struct StreamIterator : IteratorInterface<ValueType> {
		ECS_INLINE StreamIterator(ValueType* _buffer, size_t _size, size_t _starting_index) : buffer(_buffer), size(_size), index(_starting_index), 
			IteratorInterface<ValueType>(_starting_index >= _size ? 0 : _size - _starting_index) {}

		ValueType* Get() override {
			ValueType* value = buffer + index;
			index++;
			return value;
		}

		bool IsContiguous() const override {
			return true;
		}

		IteratorInterface<ValueType>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
			StreamIterator<ValueType>* iterator = (StreamIterator<ValueType>*)Allocate(allocator, sizeof(StreamIterator<ValueType>));
			new (iterator) StreamIterator<ValueType>(buffer + index, count, 0);
			index += count;
			return iterator;
		}

		ValueType* buffer;
		size_t size;
		size_t index;
	};

	// Structure that represents a stable iterator which can be constructed
	// From a non-stable iterator. It contains the allocated buffer such that it can be deallocated
	template<typename ValueType>
	struct StableIterator {
		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
			void* buffer_pointer = buffer.GetPointer();
			if (buffer_pointer != nullptr) {
				ECSEngine::Deallocate(allocator, buffer_pointer);
				buffer = nullptr;
			}
			// The iterator is always allocated
			ECSEngine::Deallocate(allocator, iterator);
		}

		// Creates a stable iterator starting at the current location for the given count of elements.
		// The buffer will always be empty, such that calls to deallocate won't deallocate it. The allocator
		// Is needed to make the allocation for the iterator itself
		ECS_INLINE StableIterator<ValueType> GetSubrange(AllocatorPolymorphic allocator, size_t count) {
			StableIterator<ValueType> result;

			if (buffer.GetData() == 0) {
				// Mark the buffer such that next calls won't reference it
				// Mark the current buffer as well
				result.buffer = buffer;
				result.buffer.SetData(1);
				buffer = nullptr;
			}
			result.iterator = iterator->CreateSubIterator(allocator, count);

			return result;
		}

		// This represents the allocated buffer, but also a boolean value that indicates whether or not
		// The stable iterator advanced positions after calls to GetSubrange. It is used to allow the first
		// GetSubrange to return the buffer as is, such that a call to Deallocate will deallocate the buffer,
		// While subsequent GetSubrange calls will return the buffer as nullptr.
		DataPointer buffer;
		IteratorInterface<ValueType>* iterator = nullptr;
	};

	// Returns a stable iterator out of an existing iterator - do not call deallocate on it!
	template<typename ValueType>
	ECS_INLINE StableIterator<ValueType> ToStableIterator(IteratorInterface<ValueType>* iterator) {
		return { nullptr, iterator };
	}

	// If the given iterator is stable, then it will simply return the iterator back without allocating the underlying range,
	// but it will allocate an allocator instance in order to make the iterator pointer stable.
	// If the given iterator is unstable, it will allocate a buffer, retrieve all the entries into it, and creates
	// An iterator that can address that range, also allocated from the allocator.
	template<typename ValueType>
	StableIterator<ValueType> GetStableIterator(IteratorInterface<ValueType>* iterator, bool is_stable, AllocatorPolymorphic allocator) {
		StableIterator<ValueType> stable_iterator;

		if (is_stable) {
			stable_iterator.iterator = iterator->CreateSubIterator(allocator, iterator->remaining_count);
		}
		else {
			stable_iterator.buffer.SetPointer(Allocate(allocator, sizeof(ValueType) * iterator->remaining_count));
			StreamIterator<ValueType>* stream_iterator = (StreamIterator<ValueType>*)Allocate(allocator, sizeof(StreamIterator<ValueType>));
			*stream_iterator = StreamIterator<ValueType>((std::remove_const_t<ValueType>*)stable_iterator.buffer.GetPointer(), iterator->remaining_count, 0);
			stable_iterator.iterator = stream_iterator;

			// Fill in the buffer now
			iterator->WriteTo((std::remove_const_t<ValueType>*)stable_iterator.buffer.GetPointer());
		}

		return stable_iterator;
	}

}