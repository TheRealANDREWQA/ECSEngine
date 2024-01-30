#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Containers/HashTable.h"
#include "../Containers/Deck.h"

namespace ECSEngine {
	
	ECSENGINE_API unsigned int Djb2Hash(unsigned int x, unsigned int y, unsigned int z);

	ECS_INLINE unsigned int Djb2Hash(uint3 indices) {
		return Djb2Hash(indices.x, indices.y, indices.z);
	}

	struct SpatialGridDefaultCellIndicesHash {
		ECS_INLINE static unsigned int Hash(uint3 indices) {
			return Djb2Hash(indices);
		}
	};

	template<typename ChunkData>
	struct SpatialGridChunk {
		ChunkData data;
		unsigned char count;
		unsigned int next_chunk;
	};

	template<typename ChunkData, size_t chunk_entries, typename CellIndicesHash = SpatialGridDefaultCellIndicesHash, bool use_smaller_cell_size = false>
	struct SpatialGrid {
		static_assert(chunk_entries < UCHAR_MAX, "SpatialGrid supports up to 255 per chunk entries");

		typedef unsigned int Cell;
		typedef SpatialGridChunk<ChunkData> Chunk;

		// Adds a new cell. Make sure it doesn't exist previosuly!
		// Returns the newly assigned chunk for it
		Chunk* AddCell(uint3 indices) {
			unsigned int chunk_index = chunks.ReserveIndex();
			Cell cell = chunk_index;
			Chunk* chunk = &chunks[chunk_index];
			chunk->count = 0;
			chunk->next_chunk = -1;

			unsigned int hash = CellIndicesHash::Hash(indices);
			cells.InsertDynamic(allocator, cell, hash);
			inserted_cells.Add(indices);
			return chunk;
		}

		// Returns the cell x, y and z indices that can be used to retrieve a grid cell
		uint3 CalculateCell(float3 position) const {
			// Truncate the position to uints, then use modulo power of two trick
			if constexpr (use_smaller_cell_size) {
				position *= smaller_cell_size_factor;
			}
			float3 adjusted_position = position + half_cell_size;
			adjusted_position.x = floor(adjusted_position.x);
			adjusted_position.y = floor(adjusted_position.y);
			adjusted_position.z = floor(adjusted_position.z);
			int3 int_position = adjusted_position;
			int_position.x = (int_position.x >> cell_size_power_of_two.x) & (dimensions.x - 1);
			int_position.y = (int_position.y >> cell_size_power_of_two.y) & (dimensions.y - 1);
			int_position.z = (int_position.z >> cell_size_power_of_two.z) & (dimensions.z - 1);
			return int_position;
		}

		Chunk* ChainChunk(Chunk* current_chunk) {
			unsigned int chunk_index = chunks.ReserveIndex();
			Chunk* new_chunk = &chunks[chunk_index];
			new_chunk->count = 0;
			new_chunk->next_chunk = -1;
			current_chunk->next_chunk = chunk_index;
			return new_chunk;
		}

		void Clear() {
			cells.Deallocate(allocator);
			chunks.Deallocate();
			inserted_cells.FreeBuffer();
		}

		bool ExistsCell(uint3 index) const {
			unsigned int hash = CellIndicesHash::Hash(index);
			return cells.Find(hash) != -1;
		}

		const Chunk* GetChunk(uint3 indices) const {
			unsigned int cell_hash = CellIndicesHash::Hash(indices);
			Cell cell;
			if (cells.TryGetValue(cell_hash, cell)) {
				unsigned int chunk_index = cell;
				const Chunk* current_chunk = &chunks[chunk_index];
				if (current_chunk->next_chunk == -1) {
					return current_chunk;
				}

				while (chunk_index != -1) {
					current_chunk = &chunks[chunk_index];
					chunk_index = current_chunk->next_chunk;
				}
				return current_chunk;
			}
			return nullptr;
		}

		Chunk* GetChunk(uint3 indices) {
			return (Chunk*)(((const SpatialGrid<ChunkData, chunk_entries, CellIndicesHash>*)this)->GetChunk(indices));
		}

		// Returns nullptr if there is no next
		const Chunk* GetNextChunk(const Chunk* current_chunk) const {
			return current_chunk->next_chunk == -1 ? nullptr : &chunks[current_chunk->next_chunk];
		}

		// Returns nullptr if there is no next
		Chunk* GetNextChunk(const Chunk* current_chunk) {
			return current_chunk->next_chunk == -1 ? nullptr : &chunks[current_chunk->next_chunk];
		}

		// Returns the last chunk in the chain
		const Chunk* GetLastChunk(const Chunk* current_chunk) const {
			while (current_chunk->next_chunk != -1) {
				current_chunk = &chunks[current_chunk->next_chunk];
			}
			return current_chunk;
		}

		// Returns the last chunk in the chain
		Chunk* GetLastChunk(const Chunk* current_chunk) {
			Chunk* chunk = (Chunk*)current_chunk;
			while (chunk->next_chunk != -1) {
				chunk = &chunks[chunk->next_chunk];
			}
			return chunk;
		}

		// The dimensions needs to be a power of two
		void Initialize(
			AllocatorPolymorphic _allocator,
			uint3 _dimensions,
			uint3 _cell_size_power_of_two,
			size_t _deck_power_of_two
		) {
			ECS_ASSERT(IsPowerOfTwo(_dimensions.x) && IsPowerOfTwo(_dimensions.y) && IsPowerOfTwo(_dimensions.z), "Spatial grid dimensions must be a power of two");

			memset(this, 0, sizeof(*this));
			allocator = _allocator;
			dimensions = _dimensions;
			cell_size_power_of_two = _cell_size_power_of_two;
			half_cell_size = ulong3((size_t)1 << cell_size_power_of_two.x, (size_t)1 << cell_size_power_of_two.y, (size_t)1 << cell_size_power_of_two.z);
			half_cell_size *= float3::Splat(0.5f);
			chunks.Initialize(allocator, 0, (size_t)1 << _deck_power_of_two, _deck_power_of_two);
			inserted_cells.Initialize(allocator, 0);
			smaller_cell_size_factor = float3::Splat(1.0f);
		}

		// Returns the ChunkData where you can insert your value
		// The count inside the chunk was incremented
		// This is the same as reserving an entry and then incrementing the size
		ChunkData* InsertEntry(uint3 cell) {
			Chunk* chunk = ReserveEntriesInCell(cell, 1);
			chunk->count++;
			return &chunk->data;
		}

		// Returns the ChunkData where you can insert your value
		ChunkData* InsertPoint(float3 point) {
			uint3 cell = CalculateCell(point);
			return InsertEntry(cell);
		}

		// The functor receives as parameters (uint3 cell_index, SpatialGridChunk* initial_chunk, ChunkData* data, unsigned int count)
		// Such that it can write the value into the chunk or perform other operations
		// The initial chunk can be used to traverse the all entries (the entry to be inserted won't appear in the iteration)
		// The ChunkData is where the entry should be added
		// The return value of the functor needs to be true if the entry is to be inserted, else false
		// The return value of the function is the return of the functor
		template<typename Functor>
		bool InsertPointTest(float3 point, Functor&& functor) {
			uint3 cell = CalculateCell(point);
			Chunk* initial_chunk = GetChunk(cell);
			Chunk* last_chunk = nullptr;
			if (initial_chunk == nullptr) {
				initial_chunk = AddCell(cell);
				last_chunk = initial_chunk;
			}
			else {
				last_chunk = GetLastChunk(initial_chunk);
			}
			Chunk* insert_chunk = ReserveEntriesInChunk(last_chunk, 1);
			bool should_insert = functor(cell, initial_chunk, &insert_chunk->data, insert_chunk->count);
			if (should_insert) {
				insert_chunk->count++;
			}
			else {
				// In case no insert is needed, we have a chunk that can potentially be allocated
				// Without any entries in it. We can remove it by
				if (last_chunk->count == chunk_entries) {
					RemoveLastAllocatedChunk(last_chunk);
				}
			}
			return should_insert;
		}

		// The functor will be called with a parameters of (uint3 cell_index, SpatialGridChunk* initial_chunk, ChunkData* data, unsigned int count)
		// Such that it can write the value into the chunk or perform other operations
		// The initial chunk can be used to traverse the all entries (the entry to be inserted won't appear in the iteration)
		// The ChunkData is where the entry should be added
		template<typename Functor>
		void InsertAABB(float3 min, float3 max, Functor&& functor) {
			// We are using min and max coordinates such that we don't have to include the AABB.h header
			
			// Determine the grid indices for the min and max points for the aabb
			uint3 min_cell = CalculateCell(min);
			uint3 max_cell = CalculateCell(max);

			// Now insert the entry in all the cells that it collides with
			// Keep in mind that the max cell can have smaller values that the min
			// In case it surpassed the cell threshold
			uint3 iteration_count;
			for (unsigned int index = 0; index < ECS_AXIS_COUNT; index++) {
				iteration_count[index] = (min_cell[index] <= max_cell[index] ? max_cell[index] - min_cell[index] : max_cell[index] - min_cell[index] + dimensions[index]) + 1;
			}

			// For each cell that it collides with, test against the current entries
			// And add the entity inside that cell
			// Use the min cell as the current cell, and increment at each step
			uint3 current_cell = min_cell;
			for (unsigned int x = 0; x < iteration_count.x; x++) {
				for (unsigned int y = 0; y < iteration_count.y; y++) {
					for (unsigned int z = 0; z < iteration_count.z; z++) {
						Chunk* initial_chunk = GetChunk(current_cell);
						if (initial_chunk == nullptr) {
							initial_chunk = AddCell(current_cell);
						}
						Chunk* insert_chunk = ReserveEntriesInInitialChunk(initial_chunk, 1);
						functor(current_cell, initial_chunk, &insert_chunk->data, insert_chunk->count);
						insert_chunk->count++;
						current_cell.z++;
						current_cell.z = current_cell.z == dimensions.z ? 0 : current_cell.z;
					}
					current_cell.z = min_cell.z;
					current_cell.y++;
					current_cell.y = current_cell.y == dimensions.y ? 0 : current_cell.y;
				}
				current_cell.y = min_cell.y;
				current_cell.x++;
				current_cell.x = current_cell.x == dimensions.x ? 0 : current_cell.x;
			}
		}

		// The functor receives as arguments (ChunkData* data, unsigned int count)
		// The functor should return true if it wants to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool IterateChunks(Chunk* chunk, Functor&& functor, Chunk** last_chunk = nullptr) {
			if (last_chunk != nullptr) {
				*last_chunk = nullptr;
			}

			while (chunk != nullptr) {
				if constexpr (early_exit) {
					if (functor(&chunk->data, chunk->count)) {
						return true;
					}
				}
				else {
					functor(&chunk->data, chunk->count);
				}
				if (last_chunk != nullptr) {
					*last_chunk = chunk;
				}
				chunk = GetNextChunk(chunk);
			}
			return false;
		}

		// The functor receives as arguments (const ChunkData* data, unsigned int count)
		// The functor should return true if it wants to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool IterateChunks(const Chunk* chunk, Functor&& functor, const Chunk** last_chunk = nullptr) const {
			if (last_chunk != nullptr) {
				*last_chunk = nullptr;
			}

			while (chunk != nullptr) {
				if constexpr (early_exit) {
					if (functor(&chunk->data, chunk->count)) {
						return true;
					}
				}
				else {
					functor(&chunk->data, chunk->count);
				}
				if (last_chunk != nullptr) {
					*last_chunk = chunk;
				}
				chunk = GetNextChunk(chunk);
			}
			return false;
		}

		// The functor receives as arguments (ChunkData* data, unsigned int count)
		// The functor should return true if it wants to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool IterateCell(uint3 cell_indices, Functor&& functor, Chunk** last_chunk) {
			Chunk* initial_chunk = GetChunk(cell_indices);
			// The nullptr case is handled by iterate chunks
			return IterateChunks<early_exit>(initial_chunk, functor, last_chunk);
		}

		// The functor receives as arguments (const ChunkData* data, unsigned int count)
		// The functor should return true if it wants to early exit, else false
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool IterateCell(uint3 cell_indices, Functor&& functor, const Chunk** last_chunk) const {
			const Chunk* initial_chunk = GetChunk(cell_indices);
			// The nullptr case is handled by iterate chunks
			return IterateChunks<early_exit>(initial_chunk, functor, last_chunk);
		}

		// When a new chunk is allocated, you can deallocate it by using this function
		// But you need the chunk previous to it to remove the reference
		void RemoveLastAllocatedChunk(Chunk* previous_chunk) {
			previous_chunk->next_chunk = -1;
			// We can simply decrement the size of the last chunk since these are allocated
			// In increasing order
			chunks.buffers[chunks.buffers.size - 1].size--;
		}

		Chunk* ReserveEntriesInChunk(Chunk* chunk, unsigned int count) {
			unsigned int chunk_remaining_count = chunk_entries - chunk->count;
			unsigned int reserve_count = chunk_remaining_count >= count ? 0 : ((count - chunk_remaining_count + chunk_entries - 1) / chunk_entries);
			Chunk* initial_insert_chunk = chunk;
			for (unsigned int index = 0; index < reserve_count; index++) {
				chunk = ChainChunk(chunk);
			}
			if (initial_insert_chunk->count == chunk_entries) {
				return GetNextChunk(initial_insert_chunk);
			}
			return initial_insert_chunk;
		}

		// Returns the chunk at which the elements can be inserted (the last
		// chunk before reserving). The initial chunk must not be nullptr
		Chunk* ReserveEntriesInInitialChunk(Chunk* initial_chunk, unsigned int count) {
			Chunk* chunk = GetLastChunk(initial_chunk);
			return ReserveEntriesInChunk(chunk, count);
		}

		// Returns the chunk at which the elements can be inserted (the last
		// chunk before reserving)
		Chunk* ReserveEntriesInCell(uint3 cell_indices, unsigned int count) {
			Chunk* chunk = GetChunk(cell_indices);
			if (chunk == nullptr) {
				chunk = AddCell(cell_indices);
			}
			return ReserveEntriesInInitialChunk(chunk, count);
		}

		ECS_INLINE void SetSmallerCellSizeFactor(float3 factor) {
			smaller_cell_size_factor = factor;
		}

		uint3 dimensions;
		// These cell sizes must be a power of two
		// We use modulo two trick to map the AABB
		uint3 cell_size_power_of_two;
		// We need to offset entries based on the half of the size
		// In order to not have AABBs straddle incorrect cells
		float3 half_cell_size;
		// When the default 1.0f smallest cell size is too big for the use case,
		// We can multiply all positions by a factor, in order to simulate a smaller
		// A smaller sized cell size
		float3 smaller_cell_size_factor;
		// We maintain the cells in a sparse hash table
		// The unsigned int as resource identifier is the actual hash value
		// The cell hash value must be computed by the user
		HashTable<Cell, unsigned int, HashFunctionPowerOfTwo> cells;
		DeckPowerOfTwo<Chunk> chunks;
		AllocatorPolymorphic allocator;
		// This is the array with all the inserted cells
		ResizableStream<uint3> inserted_cells;
	};

}