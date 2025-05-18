#pragma once
#include <type_traits>
#include <stdint.h>
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Containers/DataPointer.h"

namespace ECSEngine {

	// This macro will define the appropriate copy constructors and assignment operators. We want
	// To define the assignment operators such that the vtable pointer gets copied during normal assignments
#define ECS_ITERATOR_COPY_AND_ASSIGNMENT_OPERATORS(type) ECS_CLASS_MEMCPY_ASSIGNMENT_OPERATORS(type); \
														 type(const type& other) : IteratorInterface(0) { memcpy(this, &other, sizeof(*this)); } \

	// Helper structure that provides that allows functions to receive an iterable collection
	// ValueType must be fully qualified for const types, meaning the const should be added to the template parameter
	template<typename ValueType>
	struct IteratorInterface
	{
		ECS_INLINE IteratorInterface(size_t _remaining_count) : remaining_count(_remaining_count) {}

		// These assignment operators are deleted such that derived classes are forced to defines these
		// As a memcpy such that they copy the vtable! By default, the assignment operators don't copy
		// The vtables, which is a bit annoying. The derived classes should use ECS_CLASS_MEMCPY_CONSTRUCTOR_AND_ASSIGNMENT_OPERATORS
		// To define the assignment operators and the constructors.
		IteratorInterface<ValueType>& operator=(const IteratorInterface<ValueType>& other) = delete;
		IteratorInterface<ValueType>& operator=(IteratorInterface<ValueType>&& other) = delete;

		typedef void (*Functor)(ValueType& value, void* data);

		typedef bool (*ExitFunctor)(ValueType& value, void* data);

	protected:
		// Should not decrement the remaining_count, it is handled by the normal Get function()
		virtual ValueType* GetImpl() = 0;

		// Called when IsUnbounded() returned true in order to compute the remaining count
		virtual void ComputeRemainingCount() {}

		// Constructs an iterator that references the start of the current range for a given
		// Count of elements. The function must allocate a structure that it constructs and it returns.
		virtual IteratorInterface<ValueType>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) = 0;

		template<typename ConcreteType>
		IteratorInterface<ValueType>* CloneHelper(AllocatorPolymorphic allocator) {
			ConcreteType* concrete_type = (ConcreteType*)Allocate(allocator, sizeof(ConcreteType));
			memcpy(concrete_type, this, sizeof(*concrete_type));
			return concrete_type;
		}

	public:
		ECS_INLINE ValueType* Get() {
			ValueType* value = GetImpl();
			// In case it is unbounded, it doesn't matter if we decrement this
			remaining_count--;
			return value;
		}

		ECS_INLINE size_t GetRemainingCount() {
			if (IsUnbounded()) {
				ComputeRemainingCount();
			}
			return remaining_count;
		}

		// Returns true if the elements are contiguous and that a pointer and a size is enough to reference a subrange
		virtual bool IsContiguous() const = 0;

		// Returns true if the remaining count of entries is currently not determined (like
		// For a node inside a tree), else false. If a function needs the remaining count in order
		// To prepare something ahead of time, it will check this function in order to determine that
		// Count. The count can be skipped if only the iteration is needed, since it can rely on
		// Get() to return nullptr when all entries have been exhausted
		virtual bool IsUnbounded() const {
			return false;
		}

		// Clones the contents of this iterator into an instance allocated from the given allocator, but it
		// Does not make a deep copy of the data that is being iterated! (important distinction). It only clones
		// The iterator itself, in case you want a stable iterator reference.
		virtual IteratorInterface<ValueType>* Clone(AllocatorPolymorphic allocator) = 0;

		// Allocates an constructs an iterator the references the start of the current range for a given
		// Count of elements. If count is larger than the remaining element count, then it will reference
		// Only the remaining count of elements. If the remaining count is 0, then it will not allocate anything
		// and return nullptr.
		IteratorInterface<ValueType>* CreateSubIterator(AllocatorPolymorphic allocator, size_t count) {
			// Unbounded iterators cannot at the moment create sub iterators.
			if (IsUnbounded() && remaining_count == 0) {
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
			size_t iterate_count = 0;
			ValueType* element = Get();
			while (element != nullptr && iterate_count < count) {
				values[iterate_count] = *element;
				element = Get();
				iterate_count++;
			}
			return iterate_count < count;
		}

		// Writes the entire remaining set of data to the given buffer. The remaining count will be 0 after this call.
		// It uses the fast path for contiguous iterators of memcpying the data directly.
		void WriteTo(std::remove_const_t<ValueType>* buffer) {
			ECS_ASSERT(!IsUnbounded(), "Unbounded iterators cannot write directly to buffers!");

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
			ECS_ASSERT(!IsUnbounded(), "Unbounded iterators cannot write directly to buffers!");

			size_t allocation_size = sizeof(ValueType) * remaining_count;
			if (count != nullptr) {
				*count = remaining_count;
			}
			std::remove_const_t<ValueType>* buffer = (std::remove_const_t<ValueType>*)Allocate(allocator, allocation_size);
			WriteTo(buffer);
			return buffer;
		}

		// The functor is called with (ValueType& element)
		void ForEach(Functor functor, void* data) {
			ValueType* element = Get();
			// Use the compare against nullptr to know when the iteration is finished,
			// In order to avoid the remaining count computation for unbounded iterators
			while (element != nullptr) {
				functor(*element, data);
				element = Get();
			}
			remaining_count = 0;
		}

		// The functor is called with (ValueType& element)
		// Returns true if it early exited, else false
		bool ForEachExit(ExitFunctor functor, void* data) {
			bool should_exit = false;
			ValueType* element = Get();
			// Use the compare against nullptr to know when the iteration is finished,
			// In order to avoid the remaining count computation for unbounded iterators
			while (element != nullptr && !should_exit) {
				should_exit = functor(*element, data);
				element = Get();
			}
			remaining_count = 0;
			return should_exit;
		}

		// Helper function that allows using a lambda instead of creating a manual function
		// And its set of data. The functor receives as argument (Value& value);
		template<typename FunctorType>
		ECS_INLINE void ForEach(FunctorType&& functor) {
			auto wrapper = [](ValueType& value, void* data) {
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
			auto wrapper = [](ValueType& value, void* data) {
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

	protected:
		size_t remaining_count = 0;
	};

	template<typename ValueType>
	struct StreamIterator : IteratorInterface<ValueType> {
		ECS_INLINE StreamIterator(ValueType* _buffer, size_t _size, size_t _starting_index) : buffer(_buffer), size(_size), index(_starting_index),
			IteratorInterface<ValueType>(_starting_index >= _size ? 0 : _size - _starting_index) {}

		ECS_ITERATOR_COPY_AND_ASSIGNMENT_OPERATORS(StreamIterator);

	protected:
		ValueType* GetImpl() override {
			if (index >= size) {
				return nullptr;
			}

			ValueType* value = buffer + index;
			index++;
			return value;
		}

		IteratorInterface<ValueType>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
			StreamIterator<ValueType>* iterator = (StreamIterator<ValueType>*)Allocate(allocator, sizeof(StreamIterator<ValueType>));
			new (iterator) StreamIterator<ValueType>(buffer + index, count, 0);
			index += count;
			return iterator;
		}

	public:
		bool IsContiguous() const override {
			return true;
		}

		IteratorInterface<ValueType>* Clone(AllocatorPolymorphic allocator) override { return CloneHelper<StreamIterator<ValueType>>(allocator); }

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
			stable_iterator.iterator = iterator->Clone(allocator);
		}
		else {
			size_t iterator_count = iterator->GetRemainingCount();

			stable_iterator.buffer.SetPointer(Allocate(allocator, sizeof(ValueType) * iterator_count));
			StreamIterator<ValueType>* stream_iterator = (StreamIterator<ValueType>*)Allocate(allocator, sizeof(StreamIterator<ValueType>));
			*stream_iterator = StreamIterator<ValueType>((std::remove_const_t<ValueType>*)stable_iterator.buffer.GetPointer(), iterator_count, 0);
			stable_iterator.iterator = stream_iterator;

			// Fill in the buffer now
			iterator->WriteTo((std::remove_const_t<ValueType>*)stable_iterator.buffer.GetPointer());
		}

		return stable_iterator;
	}

	// This struct allows for creating an iterator that is a composition of multiple other iterators
	// It has a maximum static storage count, to avoid any allocations.
	template<typename ValueType>
	struct StaticCombinedIterator : IteratorInterface<ValueType> {
		constexpr static
			size_t STATIC_STORAGE_COUNT = 8;

		StaticCombinedIterator() : entry_count(0), IteratorInterface<ValueType>(0), is_unbounded(false), current_iteration_entry(0) {}

		StaticCombinedIterator(IteratorInterface<ValueType>* first, IteratorInterface<ValueType>* second) : entry_count(2), IteratorInterface<ValueType>(0),
			is_unbounded(false), current_iteration_entry(0) {
			entries[0] = first;
			entries[1] = second;
			FinalizeInitialize();
		}

		StaticCombinedIterator(IteratorInterface<ValueType>* first, IteratorInterface<ValueType>* second, IteratorInterface<ValueType>* third) : entry_count(3),
			IteratorInterface<ValueType>(0), is_unbounded(false), current_iteration_entry(0) {
			entries[0] = first;
			entries[1] = second;
			entries[2] = third;
			FinalizeInitialize();
		}

		ECS_ITERATOR_COPY_AND_ASSIGNMENT_OPERATORS(StaticCombinedIterator);

	protected:
		IteratorInterface<ValueType>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
			// At the moment, don't allow this operation, implement it when there is a need for it.
			ECS_ASSERT(false, "StaticCombinedIterator does not support subiterators for the moment");
			return nullptr;
		}

		ValueType* GetImpl() override {
			if (current_iteration_entry >= entry_count) {
				return nullptr;
			}

			ValueType* element = entries[current_iteration_entry]->Get();
			while (element == nullptr) {
				current_iteration_entry++;
				if (current_iteration_entry >= entry_count) {
					break;
				}

				element = entries[current_iteration_entry]->Get();
			}
			return element;
		}

		void ComputeRemainingCount() override {
			remaining_count = 0;
			for (unsigned int index = current_iteration_entry; index < entry_count; index++) {
				remaining_count += entries[index]->GetRemainingCount();
			}
			is_unbounded = false;
		}

	public:
		bool IsContiguous() const override {
			// This can't be contiguous. It doesn't make too much sense to have a contiguous array
			// Be split into smaller chunks and be merged here.
			return false;
		}

		bool IsUnbounded() const override {
			return is_unbounded;
		}
			
		IteratorInterface<ValueType>* Clone(AllocatorPolymorphic allocator) override {
			// This is a special case in which we also want to clone the underlying iterators
			// Such that we know for sure that all pointers are stable.
			StaticCombinedIterator<ValueType>* clone = AllocateAndConstruct<StaticCombinedIterator<ValueType>>(allocator);
			for (unsigned int index = 0; index < entry_count; index++) {
				clone->entries[index] = entries[index]->Clone(allocator);
			}
			clone->entry_count = entry_count;
			clone->remaining_count = remaining_count;
			clone->is_unbounded = is_unbounded;
			return clone;
		}

		// Adds one more iterator entry. It asserts that it has enough space to add it.
		// Call FinalizeInitialize() once all entries have been added
		void AddEntry(IteratorInterface<ValueType>* entry) {
			ECS_ASSERT(entry_count < STATIC_STORAGE_COUNT, "StaticCombinedIterator storage exceeded");
			entries[entry_count++] = entry;
		}

		// After the entries are set, this should be called to perform some specific initialization
		void FinalizeInitialize() {
			// If any of the entries is unbounded, this iterator is unbounded
			is_unbounded = false;
			for (unsigned int index = 0; index < entry_count && !is_unbounded; index++) {
				is_unbounded |= entries[index]->IsUnbounded();
			}

			// If we are not unbounded, add all the remaining counts together
			if (!is_unbounded) {
				ComputeRemainingCount();
			}
		}

		IteratorInterface<ValueType>* entries[STATIC_STORAGE_COUNT];
		unsigned int entry_count;
		unsigned int current_iteration_entry;
		bool is_unbounded;
	};

	// Returns an iterator that can visit the given value. The value must be stable.
	// The iterator is allocated from the given allocator.
	template<typename ValueType>
	ECS_INLINE IteratorInterface<ValueType>* GetValueIterator(ValueType& value, AllocatorPolymorphic allocator) {
		// We can use a stream iterator for this.
		return AllocateAndConstruct<StreamIterator<ValueType>>(allocator, &value, 1, 0);
	}

	// Returns an iterator that can visit the given value, which is unstable. It will allocate both
	// The iterator itself and the value.
	template<typename ValueType>
	IteratorInterface<ValueType>* GetValueIteratorStable(const ValueType& value, AllocatorPolymorphic allocator) {
		ValueType* copied_value = (ValueType*)Allocate(allocator, sizeof(ValueType));
		*copied_value = value;
		return GetValueIterator(*copied_value, allocator);
	}

	// For a container, it adds the given iterator, by using a static interface, a structure with the following functions.
	// It must implement the following static functions:
	// 
	// Adds an entry with a check on the container size
	// void Add(const ValueType& value); 
	// 
	// Adds an entry without a check
	// void AddAssert(const ValueType& value); 
	// 
	// Ensures that the container can hold element_count additional elements.
	// void Reserve(size_t element_count);
	// 
	// Adds a contiguous range.
	// void AddStream(Stream<ValueType> elements); 
	template<typename ValueType, typename AddInterface>
	void AddIteratorToContainer(IteratorInterface<ValueType>* iterator, AddInterface& add_interface) {
		// For unbounded iterators, use ForEach, in order to not trigger a remaining count determination.
		if (iterator->IsUnbounded()) {
			iterator->ForEach([&](const ValueType& value) {
				add_interface.AddAssert(value);
			});
		}
		else {
			// If it is not unbounded, then it means that we can retrieve the remaining count
			// Without a computation.
			size_t element_count = iterator->GetRemainingCount();
			// Call reserve with the element count, such that we don't have to have a AddStreamAssert variant.
			add_interface.Reserve(element_count);
			if (iterator->IsContiguous()) {
				// If it is contiguous, we can call add all at once
				add_interface.AddStream({ iterator->Get(), element_count });
			}
			else {
				// Add each element one by one
				iterator->ForEach([&](const ValueType& value) {
					add_interface.Add(value);
				});
			}
		}
	}

	// This function is the same as AddIteratorToContainer, except that it implements a wrapper over a container
	// That is fixed, which implements only the functions: Add, AddAssert, AddStream and AssertCapacity(), which is a rename 
	// Over Reserve() such that you don't have to write the wrapper yourself.
	template<typename ValueType, typename AddInterface>
	void AddIteratorToContainerFixed(IteratorInterface<ValueType>* iterator, AddInterface& add_interface) {
		struct Wrapper {
			ECS_INLINE Wrapper(AddInterface& _add_interface) : add_interface(_add_interface) {}

			ECS_INLINE void Add(const ValueType& value) {
				add_interface.Add(value);
			}

			ECS_INLINE void AddAssert(const ValueType& value) {
				add_interface.AddAssert(value);
			}

			ECS_INLINE void Reserve(size_t element_count) {
				add_interface.AssertCapacity(element_count);
			}

			ECS_INLINE void AddStream(Stream<ValueType> elements) {
				add_interface.AddStream(elements);
			}

			AddInterface& add_interface;
		};

		Wrapper wrapper{ add_interface };
		AddIteratorToContainer(iterator, wrapper);
	}

	// This function is the same as AddIteratorToContainer, except that it implements a wrapper over a container
	// That is resizable, which implements only the functions: Add, Reserve, AddStream and synthetizes AddAssert
	// as a normal Add, such that you don't have to write the wrapper yourself.
	template<typename ValueType, typename AddInterface>
	void AddIteratorToContainerResizable(IteratorInterface<ValueType>* iterator, AddInterface& add_interface) {
		struct Wrapper {
			ECS_INLINE Wrapper(AddInterface& _add_interface) : add_interface(_add_interface) {}

			ECS_INLINE void Add(const ValueType& value) {
				add_interface.Add(value);
			}
			
			ECS_INLINE void AddAssert(const ValueType& value) {
				add_interface.Add(value);
			}
			
			ECS_INLINE void Reserve(size_t element_count) {
				add_interface.Reserve(element_count);
			}
			
			ECS_INLINE void AddStream(Stream<ValueType> elements) {
				add_interface.AddStream(elements);
			}

			AddInterface& add_interface;
		};

		Wrapper wrapper{ add_interface };
		AddIteratorToContainer(iterator, wrapper);
	}

}