#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	template<typename Deallocator>
	struct StackScope {
		StackScope() {}
		StackScope(Deallocator&& _deallocator) : deallocator(_deallocator) {}
		~StackScope() { deallocator(); }

		StackScope(const StackScope& other) = default;
		StackScope& operator = (const StackScope& other) = default;

		Deallocator deallocator;
	};

	// Deduction rule for lambdas
	template<typename Deallocator>
	StackScope(Deallocator&&) -> StackScope<Deallocator>;

	struct ScopedMalloca {
		ECS_INLINE ~ScopedMalloca() {
			ECS_FREEA(buffer);
		}

		ECS_INLINE void operator() () const {
			ECS_FREEA(buffer);
		}

		void* buffer;
	};

	// The same as ECS_MALLOCA_ALLOCATOR, with the difference in that it creates a ScopedMalloc object
	// That automatically releases the allocation. In order to access the allocation value, use object_name.buffer
#define ECS_MALLOCA_ALLOCATOR_SCOPED(object_name, size, stack_size, allocator) ScopedMalloca object_name{ ECS_MALLOCA_ALLOCATOR(size, stack_size, allocator) }

	struct NullTerminate {
		ECS_INLINE void operator()() {
			if (previous_character != '\0') {
				stream[stream.size] = previous_character;
			}
		}

		char previous_character;
		Stream<char> stream;
	};

	typedef StackScope<NullTerminate> ScopedNullTerminate;

	struct NullTerminateWide {
		ECS_INLINE void operator() () {
			if (previous_character != L'\0') {
				stream[stream.size] = previous_character;
			}
		}

		wchar_t previous_character;
		Stream<wchar_t> stream;
	};

	struct NullTerminateWidePotentialEmpty {
		ECS_INLINE void operator() () {
			if (stream.size > 0 && previous_character != L'\0') {
				stream[stream.size] = previous_character;
			}
		}

		wchar_t previous_character;
		Stream<wchar_t> stream;
	};

	typedef StackScope<NullTerminateWide> ScopedNullTerminateWide;
	typedef StackScope<NullTerminateWidePotentialEmpty> ScopedNullTerminateWidePotentialEmpty;

	struct StackClearAllocator {
		ECS_INLINE void operator() () {
			ClearAllocator(allocator);
		}

		AllocatorPolymorphic allocator;
	};

	typedef StackScope<StackClearAllocator> ScopedClearAllocator;

	struct StackFreeAllocator {
		ECS_INLINE void operator() () {
			FreeAllocator(allocator);
		}

		AllocatorPolymorphic allocator;
	};

	typedef StackScope<StackFreeAllocator> ScopedFreeAllocator;
	
	// Do a test before writing because for const char* from source code
	// it will produce a SEG fault when trying to write
#define NULL_TERMINATE(stream) char _null_terminator_before##stream = stream[stream.size]; \
								if (stream[stream.size] != '\0') { \
									stream[stream.size] = '\0'; \
								} \
								ECSEngine::ScopedNullTerminate scope##stream({ _null_terminator_before##stream, stream });

	// Do a test before writing because for const wchar_t* from source code
	// it will produce a SEG fault when trying to write
#define NULL_TERMINATE_WIDE(stream)	wchar_t _null_terminator_before##stream = stream[stream.size]; \
								if (stream[stream.size] != L'\0') { \
									stream[stream.size] = L'\0'; \
								} \
								ECSEngine::ScopedNullTerminateWide scope##stream({ _null_terminator_before##stream, stream });

	// The same as NULL_TERMINATE_WIDE, but if the given stream is empty,
	// It won't do anything, as to not cause an incorrect access
#define NULL_TERMINATE_WIDE_POTENTIAL_EMPTY(stream) wchar_t _null_terminator_before##stream = stream.size > 0 ? stream[stream.size] : L'\0'; \
								if (_null_terminator_before##stream != L'\0') { \
									stream[stream.size] = L'\0'; \
								} \
								ECSEngine::ScopedNullTerminateWidePotentialEmpty scope##stream({ _null_terminator_before##stream, stream });

}