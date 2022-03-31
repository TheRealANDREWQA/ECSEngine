#include "ecspch.h"
#include "SerializationHelpers.h"

namespace ECSEngine {

	// -----------------------------------------------------------------------------------------

	bool Write(CapacityStream<void>& stream, const void* data, size_t data_size) {
		memcpy((void*)((uintptr_t)stream.buffer + stream.size), data, data_size);
		stream.size += data_size;
		return stream.size < stream.capacity;
	}

	// -----------------------------------------------------------------------------------------

	bool Write(CapacityStream<void>& stream, Stream<void> data) {
		stream.Add(data);
		return stream.size < stream.capacity;
	}

	// -----------------------------------------------------------------------------------------

	void Write(uintptr_t* stream, const void* data, size_t data_size)
	{
		memcpy((void*)*stream, data, data_size);
		*stream += data_size;
	}

	// -----------------------------------------------------------------------------------------

	void Write(uintptr_t* stream, Stream<void> data)
	{
		return Write(stream, data.buffer, data.size);
	}

	// -----------------------------------------------------------------------------------------

	bool Read(CapacityStream<void>& stream, void* data, size_t data_size) {
		memcpy(data, (const void*)((uintptr_t)stream.buffer + stream.size), data_size);
		stream.size += data_size;
		return stream.size < stream.capacity;
	}

	// -----------------------------------------------------------------------------------------

	void Read(uintptr_t* stream, void* data, size_t data_size) {
		memcpy(data, (const void*)*stream, data_size);
		*stream += data_size;
	}

	// -----------------------------------------------------------------------------------------

	bool Read(uintptr_t* stream, CapacityStream<void>& data, size_t data_size) {
		memcpy((void*)((uintptr_t)data.buffer + data.size), (const void*)*stream, data_size);
		*stream += data_size;
		data.size += data_size;
		return data.size <= data.capacity;
	}

	// -----------------------------------------------------------------------------------------

	bool IgnoreFile(ECS_FILE_HANDLE file, size_t byte_size)
	{
		return SetFileCursor(file, byte_size, ECS_FILE_SEEK_CURRENT) != -1;
	}

	// -----------------------------------------------------------------------------------------

	bool Ignore(CapacityStream<void>& stream, size_t byte_size)
	{
		stream.size += byte_size;
		return stream.size < stream.capacity;
	}

	// -----------------------------------------------------------------------------------------

	bool Ignore(uintptr_t* stream, size_t byte_size)
	{
		*stream += byte_size;
		return true;
	}

	// -----------------------------------------------------------------------------------------
	
}