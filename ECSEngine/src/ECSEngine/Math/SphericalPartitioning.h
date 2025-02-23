#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/Deck.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	// This is an acceleration structure that helps to accelerate queries on directions
	// The directions need to be given in normalized form. It quantizez the [-1.0, 1.0]
	// Range into equidistant partitions where the directions are gathered based on a certain
	// Axis. Then you can perform accelerated queries against directions in a certain range

	// TODO: Change this name to DirectionPartitioning to better match what it does?

	template<ECS_AXIS axis, typename Entry>
	struct SphericalPartitioning {
		struct Chunk {
			float3 directions[8];
			Entry entries[8];
			unsigned int count;
			unsigned int next_chunk;
		};

		unsigned int GetPartitionIndex(float3 normalized_direction) const {
			float axis_value = normalized_direction.x;
			if constexpr (axis == ECS_AXIS_Y) {
				axis_value = normalized_direction.y;
			}
			else if constexpr (axis == ECS_AXIS_Z) {
				axis_value = normalized_direction.z;
			}
			axis_value += 1.0f;
			axis_value *= (float)(partitions.size / 2);
			unsigned int unsigned_value = (unsigned int)axis_value;
			return unsigned_value == partitions.size ? partitions.size - 1 : unsigned_value;
		}

		void Add(float3 normalized_direction, const Entry& entry) {
			unsigned int partition_index = GetPartitionIndex(normalized_direction);
			AddToPartition(partition_index, normalized_direction, entry);
		}

		Chunk* InsertOrGetChunk(unsigned int index) {
			Chunk* chunk_ptr = nullptr;
			unsigned int chunk_index = partitions[index];
			if (chunk_index == -1) {
				chunk_index = chunks.ReserveIndex();
				partitions[index] = chunk_index;
				chunk_ptr = &chunks[chunk_index];
				chunk_ptr->count = 0;
				chunk_ptr->next_chunk = -1;
			}
			else {
				chunk_ptr = &chunks[chunk_index];
			}
			return chunk_ptr;
		}

		Chunk* GetLastChunk(Chunk* chunk) {
			while (chunk->next_chunk != -1) {
				chunk = &chunks[chunk->next_chunk];
			}
			return chunk;
		}

		// Chunk must be the last chunk in a chain
		void AddToChunk(Chunk* chunk, float3 normalized_direction, const Entry& entry) {
			if (chunk->count < ECS_COUNTOF(chunk->directions)) {
				chunk->directions[chunk->count] = normalized_direction;
				chunk->entries[chunk->count] = entry;
				chunk->count++;
			}
			else {
				size_t new_entry_index = chunks.ReserveIndex();
				ECS_ASSERT(new_entry_index < UINT_MAX);
				unsigned int u_new_entry_index = (unsigned int)new_entry_index;
				chunk->next_chunk = u_new_entry_index;
				Chunk* new_chunk = &chunks[new_entry_index];
				new_chunk->directions[0] = normalized_direction;
				new_chunk->entries[0] = entry;
				new_chunk->count = 1;
				new_chunk->next_chunk = -1;
			}
		}

		void AddToPartition(unsigned int partition_index, float3 normal, const Entry& entry) {
			Chunk* partition_chunk = InsertOrGetChunk(partition_index);
			partition_chunk = GetLastChunk(partition_chunk);
			AddToChunk(partition_chunk, normal, entry);
		}

		// The functor receives as parameters (float3 normalized_direction, Entry entry);
		template<bool early_exit = false, typename Functor>
		bool ForEachInPartition(unsigned int partition_index, Functor&& functor) const {
			unsigned int chunk_index = partitions[partition_index];
			if (chunk_index != -1) {
				const Chunk* chunk = &chunks[chunk_index];
				while (chunk != nullptr) {
					for (unsigned int index = 0; index < chunk->count; index++) {
						if constexpr (early_exit) {
							if (functor(chunk->directions[index], chunk->entries[index])) {
								return true;
							}
						}
						else {
							functor(chunk->directions[index], chunk->entries[index]);
						}
					}
					chunk = chunk->next_chunk == -1 ? nullptr : &chunks[chunk->next_chunk];
				}
			}
			return false;
		}

		// The functor receives as parameters (float3 normalized_direction, Entry entry)
		// The strict mode means that only the main partitions is iterated. In non strict mode
		// The immediate lower and upper partitions are checked as well
		template<bool early_exit = false, typename Functor>
		bool ForEachEntry(float3 normalized_direction, Functor&& functor, bool strict_mode = true) const {
			unsigned int partition_index = GetPartitionIndex(normalized_direction);
			bool exit = ForEachInPartition<early_exit>(partition_index, functor);
			if (exit) {
				return true;
			}
			if (!strict_mode) {
				if (partition_index > 0) {
					exit = ForEachInPartition<early_exit>(partition_index - 1, functor);
					if (exit) {
						return true;
					}
				}
				if (partition_index < partitions.size - 1) {
					exit = ForEachInPartition<early_exit>(partition_index + 1, functor);
					if (exit) {
						return true;
					}
				}
			}
			return false;
		}

		// It will call the functor for each entry that has the components in the given epsilon range
		template<bool early_exit = false, typename Functor>
		bool ForEachEntryWithEpsilon(float3 normalized_direction, float epsilon, Functor&& functor, bool strict_mode = true) const {
			return ForEachEntry<early_exit>(normalized_direction, [&](float3 current_direction, const Entry& current_entry) {
				if (CompareMask(current_direction, normalized_direction, float3::Splat(epsilon))) {
					if constexpr (early_exit) {
						return functor(current_direction, current_entry);
					}
					else {
						functor(current_direction, current_entry);
					}
				}
				return false;
			}, strict_mode);
		}

		void Initialize(AllocatorPolymorphic allocator, size_t partition_size, size_t deck_power_exponent, size_t initial_chunk_count) {
			partitions.Initialize(allocator, partition_size);
			// Make the initial partitions -1
			memset(partitions.buffer, -1, partitions.MemoryOf(partition_size));
			chunks.Initialize(allocator, initial_chunk_count, deck_power_exponent);
		}

		DeckPowerOfTwo<Chunk> chunks;
		// Here store the indices into the values
		Stream<unsigned int> partitions;
	};

}