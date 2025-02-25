#pragma once

namespace ECSEngine {

#define ECS_DELTA_STATE_GENERIC_HEADER_CAPACITY 7

	struct WriteInstrument;
	struct ReadInstrument;

	struct DeltaStateWriterDeltaFunctionData;
	struct DeltaStateWriterEntireFunctionData;
	struct DeltaStateReaderDeltaFunctionData;
	struct DeltaStateReaderEntireFunctionData;
	struct DeltaStateWriterInitializeFunctorInfo;
	struct DeltaStateWriterInitializeInfo;
	struct DeltaStateReaderInitializeFunctorInfo;
	struct DeltaStateReaderInitializeInfo;
	struct DeltaStateWriter;
	struct DeltaStateReader;

	// A header that can be generically used by different delta state writers/readers
	struct DeltaStateGenericHeader {
		unsigned char version;
		unsigned char reserved[ECS_DELTA_STATE_GENERIC_HEADER_CAPACITY];
	};

}