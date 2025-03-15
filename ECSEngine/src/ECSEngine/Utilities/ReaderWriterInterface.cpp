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
			Optional<OwningBufferedFileWriteInstrument> instrument = OwningBufferedFileWriteInstrument::Initialize(target.file, buffering_allocator, buffering_capacity, target.is_binary, error_message);
			if (instrument.has_value) {
				OwningBufferedFileWriteInstrument* file_instrument = (OwningBufferedFileWriteInstrument*)allocate_functor(sizeof(OwningBufferedFileWriteInstrument));
				*file_instrument = std::move(instrument.value);
				return file_instrument;
			}
			return nullptr;
		}
		else {
			// Use a temporary rename file instrument
			Optional<TemporaryRenameBufferedFileWriteInstrument> instrument = TemporaryRenameBufferedFileWriteInstrument::Initialize(target.file, buffering_allocator, buffering_capacity, target.is_binary, target.rename_extension, error_message);
			if (instrument.has_value) {
				TemporaryRenameBufferedFileWriteInstrument* file_instrument = (TemporaryRenameBufferedFileWriteInstrument*)allocate_functor(sizeof(TemporaryRenameBufferedFileWriteInstrument));
				*file_instrument = std::move(instrument.value);
				return file_instrument;
			}
			return nullptr;
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
		Optional<OwningBufferedFileReadInstrument> instrument = OwningBufferedFileReadInstrument::Initialize(target.file, buffering_allocator, buffering_capacity, target.is_binary, error_message);
		if (instrument.has_value) {
			OwningBufferedFileReadInstrument* file_instrument = (OwningBufferedFileReadInstrument*)allocate_functor(sizeof(OwningBufferedFileReadInstrument));
			*file_instrument = std::move(instrument.value);
			return file_instrument;
		}
		return nullptr;
	}

	bool FileWriteInstrumentTarget::Write(CapacityStream<void>& buffering, CapacityStream<char>* error_message, bool(*functor)(void* data, WriteInstrument* write_instrument), void* data) const
	{
		if (is_instrument) {
			bool success = functor(data, instrument);
			instrument->SetSuccess(success);
			return success;
		}
		else {
			if (rename_extension.size == 0) {
				Optional<OwningBufferedFileWriteInstrument> file_instrument = OwningBufferedFileWriteInstrument::Initialize(file, buffering, is_binary, error_message);
				if (file_instrument.has_value) {
					bool success = functor(data, &file_instrument.value);
					file_instrument.value.SetSuccess(success);
					return success;
				}
			}
			else {
				Optional<TemporaryRenameBufferedFileWriteInstrument> file_instrument = TemporaryRenameBufferedFileWriteInstrument::Initialize(file, buffering, is_binary, rename_extension, error_message);
				if (file_instrument.has_value) {
					bool success = functor(data, &file_instrument.value);
					file_instrument.value.SetSuccess(success);
					return success;
				}
			}
		}

		return false;
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

	bool FileReadInstrumentTarget::Read(CapacityStream<void>& buffering, CapacityStream<char>* error_message, bool(*functor)(void* data, ReadInstrument* read_instrument), void* data) const
	{
		if (is_instrument) {
			return functor(data, instrument);
		}
		else {
			Optional<OwningBufferedFileReadInstrument> file_instrument = OwningBufferedFileReadInstrument::Initialize(file, buffering, is_binary, error_message);
			if (file_instrument.has_value) {
				// At the moment, there is no set success needed
				return functor(data, &file_instrument.value);
			}
		}

		return false;
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