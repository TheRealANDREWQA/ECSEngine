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

	struct ReleaseMalloca {
		void operator() () const {
			ECS_FREEA(buffer);
		}

		void* buffer;
	};

	typedef StackScope<ReleaseMalloca> ScopedMalloca;

	struct NullTerminate {
		void operator()() {
			if (previous_character != '\0') {
				stream[stream.size] = previous_character;
			}
		}

		char previous_character;
		Stream<char> stream;
	};

	typedef StackScope<NullTerminate> ScopedNullTerminate;

	struct NullTerminateWide {
		void operator() () {
			if (previous_character != L'\0') {
				stream[stream.size] = previous_character;
			}
		}

		wchar_t previous_character;
		Stream<wchar_t> stream;
	};

	typedef StackScope<NullTerminateWide> ScopedNullTerminateWide;

	struct StackClearAllocator {
		void operator() () {
			ClearAllocator(allocator);
		}

		AllocatorPolymorphic allocator;
	};

	typedef StackScope<StackClearAllocator> ScopedClearAllocator;

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

}