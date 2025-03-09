#include "ecspch.h"
#include "ReaderWriterInterface.h"
#include "BufferedFileReaderWriter.h"

namespace ECSEngine {

	template<typename AllocateFunctor>
	static WriteInstrument* FileWriteInstrumentTargetGetInstrumentImpl(
		const FileWriteInstrumentTarget& target,
		size_t buffering_capacity,
		AllocatorPolymorphic buffering_allocator,
		CapacityStream<char>* error_message,
		AllocateFunctor&& allocate_functor
	) {
		if (target.is_instrument) {
			return target.instrument;
		}

		if (target.rename_extension.size == 0) {
			// No extension, use a normal owning file instrument
			OwningBufferedFileWriteInstrument* file_instrument = (OwningBufferedFileWriteInstrument*)allocate_functor(sizeof(OwningBufferedFileWriteInstrument));
			new (file_instrument) OwningBufferedFileWriteInstrument(target.file, buffering_allocator, buffering_capacity, target.is_binary, error_message);
			return file_instrument->IsInitializationFailed() ? nullptr : file_instrument;
		}
		else {
			// Use a temporary rename file instrument
			TemporaryRenameBufferedFileWriteInstrument* file_instrument = (TemporaryRenameBufferedFileWriteInstrument*)allocate_functor(sizeof(TemporaryRenameBufferedFileWriteInstrument));
			new (file_instrument) TemporaryRenameBufferedFileWriteInstrument(target.file, target.rename_extension, buffering_allocator, buffering_capacity,
				target.is_binary, error_message);
			return file_instrument->IsInitializationFailed() ? nullptr : file_instrument;
		}
	}

	template<typename AllocateFunctor>
	static ReadInstrument* FileReadInstrumentTargetGetInstrumentImpl(
		const FileReadInstrumentTarget& target,
		size_t buffering_capacity,
		AllocatorPolymorphic buffering_allocator,
		CapacityStream<char>* error_message,
		AllocateFunctor&& allocate_functor
	) {
		if (target.is_instrument) {
			return target.instrument;
		}

		// Initialize a file instrument
		OwningBufferedFileReadInstrument* file_instrument = (OwningBufferedFileReadInstrument*)allocate_functor(sizeof(OwningBufferedFileReadInstrument));
		new (file_instrument) OwningBufferedFileReadInstrument(target.file, buffering_allocator, buffering_capacity, target.is_binary, error_message);
		return file_instrument->IsInitializationFailed() ? nullptr : file_instrument;
	}

	WriteInstrument* FileWriteInstrumentTarget::GetInstrument(
		CapacityStream<void>& stack_memory,
		size_t buffering_capacity,
		AllocatorPolymorphic buffering_allocator,
		CapacityStream<char>* error_message
	) const {
		return FileWriteInstrumentTargetGetInstrumentImpl(*this, buffering_capacity, buffering_allocator, error_message, [&](size_t instrument_size) {
			return stack_memory.Reserve(instrument_size);
		});
	}

	WriteInstrument* FileWriteInstrumentTarget::GetInstrument(
		AllocatorPolymorphic instrument_allocator,
		size_t buffering_capacity,
		AllocatorPolymorphic buffering_allocator,
		CapacityStream<char>* error_message
	) const {
		return FileWriteInstrumentTargetGetInstrumentImpl(*this, buffering_capacity, buffering_allocator, error_message, [&](size_t instrument_size) {
			return Allocate(instrument_allocator, instrument_size);
		});
	}

	ReadInstrument* FileReadInstrumentTarget::GetInstrument(
		CapacityStream<void>& stack_memory,
		size_t buffering_capacity,
		AllocatorPolymorphic buffering_allocator,
		CapacityStream<char>* error_message
	) const {
		return FileReadInstrumentTargetGetInstrumentImpl(*this, buffering_capacity, buffering_allocator, error_message, [&](size_t instrument_size) {
			return stack_memory.Reserve(instrument_size);
		});
	}

	ReadInstrument* FileReadInstrumentTarget::GetInstrument(
		AllocatorPolymorphic instrument_allocator,
		size_t buffering_capacity,
		AllocatorPolymorphic buffering_allocator,
		CapacityStream<char>* error_message
	) const {
		return FileReadInstrumentTargetGetInstrumentImpl(*this, buffering_capacity, buffering_allocator, error_message, [&](size_t instrument_size) {
			return Allocate(instrument_allocator, instrument_size);
		});
	}

}