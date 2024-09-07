#include "ecspch.h"
#include "DeltaStateWriter.h"

namespace ECSEngine {

	void DeltaStateWriter::Deallocate() {

	}

	void DeltaStateWriter::DisableHeader() {
		header.Deallocate(allocator);
	}

	size_t DeltaStateWriter::GetWriteSize() const {

	}

	void* DeltaStateWriter::Initialize(const DeltaStateWriterInitializeInfo* initialize_info) {
		allocator = initialize_info->allocator;
		entire_state_write_seconds_tick = initialize_info->entire_state_write_seconds_tick;
	}

	ECS_DELTA_STATE_WRITER_STATUS DeltaStateWriter::Register(const void* current_data, float elapsed_seconds) {

	}

}