#include "ecspch.h"
#include "AllocatorCallsDebug.h"
#include "../Containers/HashTable.h"
#include "../Containers/Queues.h"
#include "../Containers/ResizableAtomicStream.h"
#include "../Utilities/File.h"
#include "AllocatorPolymorphic.h"
#include "../Utilities/Serialization/SerializationHelpers.h"

#include "MemoryManager.h"

namespace ECSEngine {
	
	// The file is going to be written in text form such that we don't need
	// a special reader for it

	struct TrackedAllocator {
		ECS_INLINE TrackedAllocator() {}

		ECS_INLINE TrackedAllocator(const TrackedAllocator& other) {
			memcpy(this, &other, sizeof(*this));
		}
		ECS_INLINE TrackedAllocator& operator = (const TrackedAllocator& other) {
			memcpy(this, &other, sizeof(*this));
			return *this;
		}

		ECS_INLINE unsigned int Size() const {
			return is_resizable ? resizable_allocations.stream.size.load(ECS_RELAXED) : queue_allocations.GetSize();
		}

		template<typename Functor>
		void ForEach(Functor&& functor) const {
			if (is_resizable) {
				for (unsigned int index = 0; index < resizable_allocations.stream.size; index++) {
					functor(resizable_allocations[index]);
				}
			}
			else {
				queue_allocations.ForEach(functor);
			}
		}

		union {
			ThreadSafeQueue<TrackedAllocation> queue_allocations;
			ResizableAtomicStream<TrackedAllocation> resizable_allocations;
		};
		bool is_resizable;
		ECS_ALLOCATOR_TYPE allocator_type;
		Stream<char> name;
	};

	// This is the structure that keeps track of the allocations
	// We are going to have a global one in order to not have
	// functions specify which instance to 
	struct DebugAllocatorManager {
		HashTable<TrackedAllocator, const void*, HashFunctionPowerOfTwo, PointerHashing> allocators;
		GlobalMemoryManager global_allocator;
		ECS_FILE_HANDLE file_handle;
		unsigned int default_ring_buffer_capacity;
		bool write_to_file_on_fill;
		ReadWriteLock hash_table_lock;
	};

	DebugAllocatorManager AllocatorManager = { {}, {}, -1, false, {} };

	static void InternalAddEntry(const void* allocator, ECS_ALLOCATOR_TYPE allocator_type, bool resizable, const char* name) {
		TrackedAllocator tracked_allocator;
		tracked_allocator.is_resizable = resizable;
		if (!resizable) {
			tracked_allocator.queue_allocations.Initialize(&AllocatorManager.global_allocator, AllocatorManager.default_ring_buffer_capacity);
		}
		else {
			tracked_allocator.resizable_allocations.Initialize({ nullptr }, 0);
		}

		if (name != nullptr) {
			tracked_allocator.name = name;
		}
		else {
			tracked_allocator.name = { nullptr, 0 };
		}
		tracked_allocator.allocator_type = allocator_type;
		AllocatorManager.allocators.InsertDynamic(&AllocatorManager.global_allocator, tracked_allocator, allocator);
	}

	void DebugAllocatorManagerInitialize(const DebugAllocatorManagerDescriptor* descriptor)
	{
		size_t global_allocator_capacity = 0;
		size_t backup_allocator_capacity = 0;

		switch (descriptor->capacity) {
		case ECS_DEBUG_ALLOCATOR_MANAGER_CAPACITY_LOW:
			global_allocator_capacity = ECS_KB * 256;
			backup_allocator_capacity = ECS_MB;
			break;
		case ECS_DEBUG_ALLOCATOR_MANAGER_CAPACITY_MEDIUM:
			global_allocator_capacity = ECS_MB * 10;
			backup_allocator_capacity = ECS_MB * 50;
			break;
		case ECS_DEBUG_ALLOCATOR_MANAGER_CAPACITY_HIGH:
			global_allocator_capacity = ECS_MB * 100;
			backup_allocator_capacity = ECS_MB * 500;
			break;
		}

		AllocatorManager.global_allocator = CreateGlobalMemoryManager(global_allocator_capacity, ECS_KB * 16, backup_allocator_capacity);
		const wchar_t* log_file = descriptor->log_file ? descriptor->log_file : ECS_DEBUG_ALLOCATOR_DEFAULT_FILE;
		if (descriptor->enable_global_write_to_file || descriptor->log_file) {
			ECS_HARD_ASSERT(FileCreate(log_file, &AllocatorManager.file_handle, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TRUNCATE_FILE) == ECS_FILE_STATUS_OK);
		}
		AllocatorManager.write_to_file_on_fill = descriptor->enable_global_write_to_file;
		AllocatorManager.hash_table_lock.Clear();
		AllocatorManager.allocators.Initialize(&AllocatorManager.global_allocator, 0);
		AllocatorManager.default_ring_buffer_capacity = descriptor->default_ring_buffer_size;
	}

	void DebugAllocatorManagerSetResizable(const void* allocator, ECS_ALLOCATOR_TYPE allocator_type)
	{
		DebugAllocatorManagerChangeOrAddEntry(allocator, nullptr, true, allocator_type);
	}

	void DebugAllocatorManagerSetName(const void* allocator, const char* name, ECS_ALLOCATOR_TYPE allocator_type) {
		DebugAllocatorManagerChangeOrAddEntry(allocator, name, false, allocator_type);
	}

	void DebugAllocatorManagerSetResizableAndName(const void* allocator, const char* name, ECS_ALLOCATOR_TYPE type)
	{
		DebugAllocatorManagerChangeOrAddEntry(allocator, name, true, type);
	}

	void DebugAllocatorManagerChangeOrAddEntry(
		const void* allocator, 
		const char* name, 
		bool resizable, 
		ECS_ALLOCATOR_TYPE allocator_type
	) {
		AllocatorManager.hash_table_lock.EnterRead();

		TrackedAllocator* tracked_allocator;
		if (AllocatorManager.allocators.TryGetValuePtr(allocator, tracked_allocator)) {
			if (resizable) {
				ECS_HARD_ASSERT(!tracked_allocator->is_resizable);
				ThreadSafeQueue<TrackedAllocation> queue_allocations = tracked_allocator->queue_allocations;
				unsigned int entries_count = queue_allocations.GetSize();
				tracked_allocator->resizable_allocations.Initialize({ nullptr }, entries_count);
				if (entries_count > 0) {
					queue_allocations.CopyTo(tracked_allocator->resizable_allocations.stream.buffer);
					tracked_allocator->resizable_allocations.stream.size = queue_allocations.GetSize();
				}
				// We can use thread safe deallocate since there can be multiple readers
				// The allocation which is made when an entry is pushed is made under write lock
				// so it doesn't conflict with this one
				AllocatorManager.global_allocator.Deallocate_ts(queue_allocations.GetQueue()->GetAllocatedBuffer());
			}
			if (name) {
				tracked_allocator->name = name;
			}
			AllocatorManager.hash_table_lock.ExitRead();
		}
		else {
			bool locked_by_us = AllocatorManager.hash_table_lock.TransitionReadToWriteLock();
			InternalAddEntry(allocator, allocator_type, resizable, name);
			AllocatorManager.hash_table_lock.ReadToWriteUnlock(locked_by_us);
		}
	}

	// This is the entry that is serialized
	/*struct SerializeEntry {
		void* allocator_pointer;
		Stream<TrackedAllocation> allocations;
	};*/

	// The file is written like this
	// FileHeader
	// entry
	// entry
	// entry
	// ...
	// It's a dynamic list of entries - there is no entry count specified
	// You determine when the list finishes when the read pointer has passed the end of the file

	static size_t AllocatorFileWriteSize(const TrackedAllocator& allocator) {
		// Use an estimate of 30 characters per allocator address 
		const size_t PER_ALLOCATOR_COST = 30;
		// Use an estimate of around 200 characters per function + file
		const size_t PER_DEBUG_INFO_COST = 200;
		// Estimate of around 100 characters for line, allocated_pointer and call type
		const size_t PER_TRACKED_ALLOCATION_COST = 100;

		return PER_ALLOCATOR_COST + allocator.Size() * (PER_DEBUG_INFO_COST + PER_TRACKED_ALLOCATION_COST);
	}

	static size_t FileWriteSize() {
		size_t total_size = 0;
		AllocatorManager.allocators.ForEachConst([&](const TrackedAllocator& allocator, const void* allocator_pointer) {
			total_size += AllocatorFileWriteSize(allocator);
		});

		// Add another 5MB just for good measure - rely on virtual memory
		// that it does not get committed
		return total_size + ECS_MB * 5;
	}

	static void DebugAllocatorManagerWriteAllocatorState(CapacityStream<char>* characters, const void* allocator_pointer, const TrackedAllocator& allocator) {
		ECS_FORMAT_STRING(*characters, "Allocator {#}\n", allocator_pointer);
		allocator.ForEach([&](const TrackedAllocation& allocation) {
			Stream<char> function_type_name = DebugAllocatorFunctionName(allocation.function_type);
			if (allocation.function_type == ECS_DEBUG_ALLOCATOR_ALLOCATE || allocation.function_type == ECS_DEBUG_ALLOCATOR_DEALLOCATE) {
				ECS_FORMAT_STRING(*characters, "\tType {#}\n\tPointer {#}\n\tFile {#}\n\tFunction {#}\n\tLine {#}\n\n",
					function_type_name, allocation.allocated_pointer, allocation.debug_info.file, allocation.debug_info.function, allocation.debug_info.line);
			}
			else if (allocation.function_type == ECS_DEBUG_ALLOCATOR_CLEAR || allocation.function_type == ECS_DEBUG_ALLOCATOR_FREE) {
				ECS_FORMAT_STRING(*characters, "\tType {#}\n\tFile {#}\n\tFunction {#}\n\tLine {#}\n\n",
					function_type_name, allocation.debug_info.file, allocation.debug_info.function, allocation.debug_info.line);
			}
			else if (allocation.function_type == ECS_DEBUG_ALLOCATOR_REALLOCATE) {
				ECS_FORMAT_STRING(*characters, "\tType {#}\n\tInitial Pointer {#}\n\tReallocated Pointer {#}\n\tFile {#}\n\tFunction {#}\n\tLine {#}\n\n",
					function_type_name, 
					allocation.allocated_pointer,
					allocation.secondary_pointer, 
					allocation.debug_info.file, 
					allocation.debug_info.function, 
					allocation.debug_info.line
				);
			}
			else if (allocation.function_type == ECS_DEBUG_ALLOCATOR_RETURN_TO_MARKER) {
				ECS_FORMAT_STRING(*characters, "\tType {#}\n\tMarker value {#}\n\tFile {#}\n\tFunction {#}\n\tLine {#}\n\n",
					function_type_name, (size_t)allocation.allocated_pointer, allocation.debug_info.file, allocation.debug_info.function, allocation.debug_info.line);
			}
			else {
				characters->AddStreamAssert("\tInvalid debug function type\n");
			}
		});
	}

	void DebugAllocatorManagerWriteState(const wchar_t* log_file)
	{
		ECS_FILE_HANDLE file_to_write_handle = 0;
		if (log_file) {
			ECS_HARD_ASSERT(FileCreate(log_file, &file_to_write_handle, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_TRUNCATE_FILE) == ECS_FILE_STATUS_OK);
		}
		else {
			file_to_write_handle = AllocatorManager.file_handle;
		}

		AllocatorManager.hash_table_lock.EnterWrite();

		// Use a simple manual file write
		size_t file_write_size = FileWriteSize();
		void* allocation = malloc(file_write_size);
		CapacityStream<char> text_stream = { allocation, 0, (unsigned int)file_write_size };
		
		AllocatorManager.allocators.ForEachConst([&](const TrackedAllocator& allocator, const void* allocator_pointer) {
			DebugAllocatorManagerWriteAllocatorState(&text_stream, allocator_pointer, allocator);
		});

		ECS_HARD_ASSERT(WriteToFile(file_to_write_handle, text_stream.ToStream()) != -1);

		free(allocation);
		if (log_file) {
			CloseFile(file_to_write_handle);
		}
		else {
			// Flush the file to disk
			FlushFileToDisk(file_to_write_handle);
		}
		AllocatorManager.hash_table_lock.ExitWrite();
	}

	void DebugAllocatorManagerAddEntry(const void* allocator, ECS_ALLOCATOR_TYPE allocator_type, const TrackedAllocation* allocation)
	{
		// Enter the read phase - if we find the allocator already there
		// We can insert into it's stream without disrupting other entries
		AllocatorManager.hash_table_lock.EnterRead();

		TrackedAllocator* tracked_allocator_ptr = nullptr;
		if (AllocatorManager.allocators.TryGetValuePtr(allocator, tracked_allocator_ptr)) {
			bool exit_read = true;

			if (tracked_allocator_ptr->is_resizable) {
				// We can push directly
				tracked_allocator_ptr->resizable_allocations.Add(*allocation);
			}
			else {
				// If we are full and we have the write to file activated, write to file
				tracked_allocator_ptr->queue_allocations.Lock();
				if (tracked_allocator_ptr->queue_allocations.GetSize() == tracked_allocator_ptr->queue_allocations.GetCapacity()) {
					if (AllocatorManager.write_to_file_on_fill) {
						size_t allocation_size = AllocatorFileWriteSize(*tracked_allocator_ptr);
						void* file_allocation = malloc(allocation_size);

						CapacityStream<char> text = { file_allocation, 0, (unsigned int)allocation_size };
						DebugAllocatorManagerWriteAllocatorState(&text, allocator, *tracked_allocator_ptr);
						// Transition the lock to the write since the file write needs to be single threaded
						bool locked_by_us = AllocatorManager.hash_table_lock.TransitionReadToWriteLock();
						ECS_HARD_ASSERT(WriteToFile(AllocatorManager.file_handle, text.ToStream()) != -1);
						// Flush the file to disk as well
						FlushFileToDisk(AllocatorManager.file_handle);
						AllocatorManager.hash_table_lock.ReadToWriteUnlock(locked_by_us);

						// Deactivate the read exit
						exit_read = false;
						free(file_allocation);
						// We can clear the allocations now
						tracked_allocator_ptr->queue_allocations.Reset();
						tracked_allocator_ptr->queue_allocations.PushNonAtomic(allocation);
					}
					else {
						// The first allocation is lost
						tracked_allocator_ptr->queue_allocations.PushNonAtomic(allocation);
					}
				}
				else {
					// We can push directly
					tracked_allocator_ptr->queue_allocations.PushNonAtomic(allocation);
				}
				tracked_allocator_ptr->queue_allocations.Unlock();
			}

			if (exit_read) {
				AllocatorManager.hash_table_lock.ExitRead();
			}
		}
		else {
			bool locked_by_us = AllocatorManager.hash_table_lock.TransitionReadToWriteLock();
			InternalAddEntry(allocator, allocator_type, false, nullptr);
			AllocatorManager.hash_table_lock.ReadToWriteUnlock(locked_by_us);
		}
	}

	const char* DEBUG_ALLOCATOR_FUNCTION_NAMES[] = {
		"Allocate",
		"Deallocate",
		"Clear",
		"Free",
		"Reallocate",
		"ReturnToMarker"
	};

	static_assert(std::size(DEBUG_ALLOCATOR_FUNCTION_NAMES) == ECS_DEBUG_ALLOCATOR_FUNCTION_COUNT);

	const char* DebugAllocatorFunctionName(ECS_DEBUG_ALLOCATOR_FUNCTION function)
	{
		ECS_HARD_ASSERT(function < ECS_DEBUG_ALLOCATOR_FUNCTION_COUNT);
		return DEBUG_ALLOCATOR_FUNCTION_NAMES[function];
	}

}