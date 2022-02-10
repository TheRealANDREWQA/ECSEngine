#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"

namespace ECSEngine {

	namespace Reflection {

		enum class ReflectionBasicFieldType : unsigned char {
			Int8,
			UInt8,
			Int16,
			UInt16,
			Int32,
			UInt32,
			Int64,
			UInt64,
			Wchar_t,
			Float,
			Double,
			Bool,
			Enum,
			Char2,
			UChar2,
			Short2,
			UShort2,
			Int2,
			UInt2,
			Long2,
			ULong2,
			Char3,
			UChar3,
			Short3,
			UShort3,
			Int3,
			UInt3,
			Long3,
			ULong3,
			Char4,
			UChar4,
			Short4,
			UShort4,
			Int4,
			UInt4,
			Long4,
			ULong4,
			Float2,
			Float3,
			Float4,
			Double2,
			Double3,
			Double4,
			Bool2,
			Bool3,
			Bool4,
			UserDefined,
			Unknown,
			COUNT
		};

		// The extended type indicates whether or not this is a stream/pointer/embedded array
		enum class ReflectionStreamFieldType : unsigned char {
			Basic,
			Pointer,
			BasicTypeArray,
			Stream,
			CapacityStream,
			ResizableStream,
			Unknown,
			COUNT
		};

		struct ECSENGINE_API ReflectionFieldInfo {
			ReflectionFieldInfo() {}
			ReflectionFieldInfo(ReflectionBasicFieldType _basic_type, ReflectionStreamFieldType _extended_type, unsigned short _byte_size, unsigned short _basic_type_count)
				: stream_type(_extended_type), basic_type(_basic_type), byte_size(_byte_size), basic_type_count(_basic_type_count) {}

			ReflectionFieldInfo(const ReflectionFieldInfo& other) = default;
			ReflectionFieldInfo& operator = (const ReflectionFieldInfo& other) = default;

			ReflectionStreamFieldType stream_type;
			ReflectionBasicFieldType basic_type;
			unsigned short basic_type_count;
			unsigned short additional_flags;
			unsigned short byte_size;
			unsigned short pointer_offset;
		};

		struct ReflectionField {
			const char* name;
			const char* definition;
			ReflectionFieldInfo info;
		};

		struct ReflectionType {
			const char* name;
			containers::Stream<ReflectionField> fields;
			unsigned int folder_hierarchy_index;
		};

		struct ReflectionEnum {
			const char* name;
			containers::Stream<const char*> fields;
			unsigned int folder_hierarchy_index;
		};

	}

}