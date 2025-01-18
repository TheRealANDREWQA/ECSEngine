// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Containers/HashTable.h"
#include "../Containers/Deck.h"
#include "../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	template<typename ChunkData>
	struct ECS_REFLECT SpatialGridChunk {
		ChunkData data;
		unsigned char count;
		unsigned int next_chunk;
	};

	struct SpatialGridDefaultCellIndicesHash {
		ECS_INLINE static unsigned int Hash(uint3 indices) {
			return Cantor(indices.x, indices.y, indices.z);
		}
	};

	// The ChunkData struct must have a function void Set(ChunkDataEntry entry, unsigned int index) to add a new entry
	// The ChunkData is memsetted to 0 when a new entry is created
	template<typename ChunkData, typename ChunkDataEntry, size_t chunk_entries, typename CellIndicesHash = SpatialGridDefaultCellIndicesHash, bool use_smaller_cell_size = false>
	struct ECS_REFLECT SpatialGrid {
		static_assert(chunk_entries < UCHAR_MAX, "SpatialGrid supports up to 255 per chunk entries");

		typedef unsigned int Cell;
		typedef SpatialGridChunk<ChunkData> Chunk;

		// Adds a new cell. Make sure it doesn't exist previosuly!
		// Returns the newly assigned chunk for it
		Chunk* AddCell(uint3 indices) {
			unsigned int chunk_index = chunks.ReserveAndUpdateSize();
			Cell cell = chunk_index;
			Chunk* chunk = &chunks[chunk_index];
			chunk->count = 0;
			chunk->next_chunk = -1;

			memset(&chunk->data, 0, sizeof(chunk->data));
			cells.InsertDynamic(allocator, cell, indices);
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
			unsigned int chunk_index = chunks.ReserveAndUpdateSize();
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

		ECS_INLINE bool ExistsCell(uint3 index) const {
			return cells.Find(index) != -1;
		}

		const Chunk* GetChunk(uint3 indices) const {
			Cell cell;
			if (cells.TryGetValue(indices, cell)) {
				unsigned int chunk_index = cell;
				return &chunks[chunk_index];
			}
			return nullptr;
		}

		ECS_INLINE Chunk* GetChunk(uint3 indices) {
			return (Chunk*)(((const SpatialGrid<ChunkData, ChunkDataEntry, chunk_entries, CellIndicesHash, use_smaller_cell_size>*)this)->GetChunk(indices));
		}

		// Returns nullptr if there is no next
		ECS_INLINE const Chunk* GetNextChunk(const Chunk* current_chunk) const {
			return current_chunk->next_chunk == -1 ? nullptr : &chunks[current_chunk->next_chunk];
		}

		// Returns nullptr if there is no next
		ECS_INLINE Chunk* GetNextChunk(const Chunk* current_chunk) {
			return current_chunk->next_chunk == -1 ? nullptr : &chunks[current_chunk->next_chunk];
		}

		// Returns the last chunk in the chain
		ECS_INLINE const Chunk* GetLastChunk(const Chunk* current_chunk) const {
			while (current_chunk->next_chunk != -1) {
				current_chunk = &chunks[current_chunk->next_chunk];
			}
			return current_chunk;
		}

		// Returns the last chunk in the chain
		ECS_INLINE Chunk* GetLastChunk(const Chunk* current_chunk) {
			Chunk* chunk = (Chunk*)current_chunk;
			while (chunk->next_chunk != -1) {
				chunk = &chunks[chunk->next_chunk];
			}
			return chunk;
		}

		// Returns the initial chunk if there is one for the cell, else creates
		// The cell and returns the chunk
		Chunk* GetInitialChunk(uint3 cell) {
			Chunk* chunk = GetChunk(cell);
			if (chunk == nullptr) {
				chunk = AddCell(cell);
			}
			return chunk;
		}
		
		// Iterates the given range between the min and the max cell
		// The functor receives as parameters (uint3 current_cell, SpatialGridChunk<>* initial_chunk)
		// Initial chunk can be nullptr - should be able to handle it
		// And must return true when to early exit, else false (in case it wants to early exit)
		template<bool early_exit = false, typename Functor>
		bool IterateRange(uint3 min_cell, uint3 max_cell, Functor&& functor) {
			// Keep in mind that the max cell can have smaller values that the min
			// In case it surpassed the cell threshold
			uint3 iteration_count;
			for (unsigned int index = 0; index < ECS_AXIS_COUNT; index++) {
				iteration_count[index] = (min_cell[index] <= max_cell[index] ? max_cell[index] - min_cell[index] : max_cell[index] - min_cell[index] + dimensions[index]) + 1;
			}

			// Use the min cell as the current cell, and increment at each step
			uint3 current_cell = min_cell;
			for (unsigned int x = 0; x < iteration_count.x; x++) {
				for (unsigned int y = 0; y < iteration_count.y; y++) {
					for (unsigned int z = 0; z < iteration_count.z; z++) {
						Chunk* initial_chunk = GetChunk(current_cell);
						if constexpr (early_exit) {
							bool should_exit = functor(current_cell, initial_chunk);
							if (should_exit) {
								return true;
							}
						}
						else {
							functor(current_cell, initial_chunk);
						}
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
			return false;
		}

		// Iterates the given range between the min and the max cell
		// Calls the functor and then insert the entry into each cell
		// The functor receives as parameters (uint3 current_cell, SpatialGridChunk<>* initial_chunk>)
		template<typename Functor>
		void InsertRange(uint3 min_cell, uint3 max_cell, const ChunkDataEntry& data_entry, Functor&& functor) {
			// Keep in mind that the max cell can have smaller values that the min
			// In case it surpassed the cell threshold
			uint3 iteration_count;
			for (unsigned int index = 0; index < ECS_AXIS_COUNT; index++) {
				iteration_count[index] = (min_cell[index] <= max_cell[index] ? max_cell[index] - min_cell[index] : max_cell[index] - min_cell[index] + dimensions[index]) + 1;
			}

			// Use the min cell as the current cell, and increment at each step
			uint3 current_cell = min_cell;
			for (unsigned int x = 0; x < iteration_count.x; x++) {
				for (unsigned int y = 0; y < iteration_count.y; y++) {
					for (unsigned int z = 0; z < iteration_count.z; z++) {
						Chunk* initial_chunk = GetInitialChunk(current_cell);
						functor(current_cell, initial_chunk);
						// Do the reserve after the functor, such that we don't introduce a ghost
						// Block with count 0 (even tho it should still be able to cope with it
						// Since, when adding a new cell, will result in a count of 0)
						Chunk* insert_chunk = ReserveEntriesInInitialChunk(initial_chunk, 1);
						insert_chunk->data.Set(data_entry, insert_chunk->count);
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

		// Iterates the given range between the min and the max cell
		// And adds the entry to each cell
		void InsertRange(uint3 min_cell, uint3 max_cell, const ChunkDataEntry& data_entry) {
			// Use an empty functor
			InsertRange(min_cell, max_cell, data_entry, [](uint3 cell, Chunk* initial_chunk) {});
		}

		// Iterates the given range between the min and the max cell
		// The functor receives as parameters (uint3 current_cell, SpatialGridChunk<>* initial_chunk)
		// Initial chunk can be nullptr - should be able to handle it
		// And must return true when the entry should be skipped and not added, else false.
		// This return true if the entry was skipped, else false
		template<typename Functor>
		bool InsertRangeLate(uint3 min_cell, uint3 max_cell, const ChunkDataEntry& data_entry, Functor&& functor) {
			// This is basically a merge between IterateRange and InsertRange
			// But we don't call them explicitely in order to avoid having to recalculate
			// The iteration counts again

			// Keep in mind that the max cell can have smaller values that the min
			// In case it surpassed the cell threshold
			uint3 iteration_count;
			for (unsigned int index = 0; index < ECS_AXIS_COUNT; index++) {
				iteration_count[index] = (min_cell[index] <= max_cell[index] ? max_cell[index] - min_cell[index] : max_cell[index] - min_cell[index] + dimensions[index]) + 1;
			}

			// Use the min cell as the current cell, and increment at each step
			uint3 current_cell = min_cell;
			for (unsigned int x = 0; x < iteration_count.x; x++) {
				for (unsigned int y = 0; y < iteration_count.y; y++) {
					for (unsigned int z = 0; z < iteration_count.z; z++) {
						Chunk* initial_chunk = GetChunk(current_cell);
						bool should_exit = functor(current_cell, initial_chunk);
						if (should_exit) {
							return true;
						}
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

			// It passed the tests - we can now insert
			current_cell = min_cell;
			for (unsigned int x = 0; x < iteration_count.x; x++) {
				for (unsigned int y = 0; y < iteration_count.y; y++) {
					for (unsigned int z = 0; z < iteration_count.z; z++) {
						Chunk* initial_chunk = GetInitialChunk(current_cell);
						Chunk* insert_chunk = ReserveEntriesInInitialChunk(initial_chunk, 1);
						insert_chunk->data.Set(data_entry, insert_chunk->count);
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

			return false;
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

		void InsertEntry(uint3 cell, const ChunkDataEntry& entry) {
			Chunk* chunk = ReserveEntriesInCell(cell, 1);
			chunk->data.Set(entry, chunk->count);
			chunk->count++;
		}

		// The functor receives as parameters (uint3 cell_index, SpatialGridChunk* initial_chunk)
		// And must return true if the value should be inserted, else false
		template<typename Functor>
		bool InsertPointTest(float3 point, const ChunkDataEntry& data_entry, Functor&& functor) {
			uint3 cell = CalculateCell(point);
			Chunk* initial_chunk = GetInitialChunk(cell);
			bool should_insert = functor(cell, initial_chunk);
			if (should_insert) {
				Chunk* insert_chunk = ReserveEntriesInInitialChunk(initial_chunk, 1);
				insert_chunk->data.Set(data_entry, insert_chunk->count);
				insert_chunk->count++;
			}
			return should_insert;
		}

		// The functor receives as parameters (uint3 cell_index, SpatialGridChunk* initial_chunk)
		// Initial chunk can be nullptr - should be able to handle it
		// And must return true when to early exit
		template<bool early_exit = false, typename Functor>
		bool IterateSphere(float3 center, float radius, Functor&& functor) {
			// TODO: Improve this case? At the moment it is a bit inefficient
			// Since we treat it as an enclosing AABB
			if (radius == 0.0f) {
				// Treat this as a point
				uint3 cell = CalculateCell(center);
				Chunk* initial_chunk = GetChunk(cell);
				if constexpr (early_exit) {
					return functor(cell, initial_chunk);
				}
				else {
					functor(cell, initial_chunk);
				}
				return false;
			}
			else {
				// We can treat the sphere case as an AABB case where we calculate the min and max
				float3 min = center - float3::Splat(radius);
				float3 max = center + float3::Splat(radius);
				return IterateAABB<early_exit>(min, max, functor);
			}
		}

		// The functor receives as parameters (uint3 current_cell, SpatialGridChunk<>* initial_chunk>)
		template<typename Functor>
		void InsertSphere(float3 center, float radius, const ChunkDataEntry& data_entry, Functor&& functor) {
			// TODO: Improve this case? At the moment it is a bit inefficient
			// Since we treat it as an enclosing AABB
			if (radius == 0.0f) {
				// Treat this as a point
				uint3 cell = CalculateCell(center);
				Chunk* initial_chunk = GetInitialChunk(cell);
				functor(cell, initial_chunk);
				Chunk* insert_chunk = ReserveEntriesInInitialChunk(initial_chunk, 1);
				insert_chunk->data.Set(data_entry, insert_chunk->count);
				insert_chunk->count++;
			}
			else {
				// We can treat the sphere case as an AABB case where we calculate the min and max
				float3 min = center - float3::Splat(radius);
				float3 max = center + float3::Splat(radius);
				return InsertAABB<early_exit>(min, max, data_entry, functor);
			}
		}

		void InsertSphere(float3 center, float radius, const ChunkDataEntry& data_entry) {
			InsertSphere(center, radius, data_entry, [](uint3 cell_indices, Chunk* initial_chunk) {});
		}

		// The functor receives as parameters (uint3 current_cell, SpatialGridChunk<>* initial_chunk)
		// Initial chunk can be nullptr - should be able to handle it
		// And must return true when the entry should be skipped and not added, else false.
		// This return true if the entry was skipped, else false
		template<typename Functor>
		bool InsertSphereLate(float3 center, float radius, const ChunkDataEntry& data_entry, Functor&& functor) {
			// TODO: Improve this case? At the moment it is a bit inefficient
			// Since we treat it as an enclosing AABB
			if (radius == 0.0f) {
				// Treat this as a point
				uint3 cell = CalculateCell(center);
				Chunk* initial_chunk = GetChunk(cell);
				if (functor(cell, initial_chunk)) {
					Chunk* insert_chunk = ReserveEntriesInInitialChunk(initial_chunk, 1);
					insert_chunk->data.Set(data_entry, insert_chunk->count);
					insert_chunk->count++;
					return true;
				}
				return false;
			}
			else {
				// We can treat the sphere case as an AABB case where we calculate the min and max
				float3 min = center - float3::Splat(radius);
				float3 max = center + float3::Splat(radius);
				return InsertAABBLate(min, max, data_entry, functor);
			}
		}

		// The functor receives as parameters (uint3 cell_index, SpatialGridChunk* initial_chunk)
		// Initial chunk can be nullptr - should be able to handle it
		// And must return true when to early exit, else false
		template<bool early_exit = false, typename Functor>
		bool IterateAABB(float3 min, float3 max, Functor&& functor) {
			// We are using min and max coordinates such that we don't have to include the AABB.h header
			
			// Determine the grid indices for the min and max points for the aabb
			uint3 min_cell = CalculateCell(min);
			uint3 max_cell = CalculateCell(max);

			return IterateRange<early_exit>(min_cell, max_cell, functor);
		}

		// The functor receives as parameters (uint3 current_cell, SpatialGridChunk<>* initial_chunk>)
		template<typename Functor>
		void InsertAABB(float3 min, float3 max, const ChunkDataEntry& data_entry, Functor&& functor) {
			// We are using min and max coordinates such that we don't have to include the AABB.h header

			// Determine the grid indices for the min and max points for the aabb
			uint3 min_cell = CalculateCell(min);
			uint3 max_cell = CalculateCell(max);

			InsertRange(min_cell, max_cell, data_entry, functor);
		}

		void InsertAABB(float3 min, float3 max, const ChunkDataEntry& data_entry) {
			InsertAABB(min, max, data_entry, [](uint3 cell, Chunk* initial_chunk) {});
		}

		// The functor receives as parameters (uint3 current_cell, SpatialGridChunk<>* initial_chunk)
		// Initial chunk can be nullptr - should be able to handle it
		// And must return true when the entry should be skipped and not added, else false.
		// This return true if the entry was skipped, else false
		template<typename Functor>
		bool InsertAABBLate(float3 min, float3 max, const ChunkDataEntry& data_entry, Functor&& functor) {
			// We are using min and max coordinates such that we don't have to include the AABB.h header

			// Determine the grid indices for the min and max points for the aabb
			uint3 min_cell = CalculateCell(min);
			uint3 max_cell = CalculateCell(max);

			return InsertRangeLate(min_cell, max_cell, data_entry, functor);
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
			if (initial_insert_chunk->count + count > chunk_entries) {
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
			Chunk* chunk = GetInitialChunk(cell_indices);
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
		HashTable<Cell, uint3, HashFunctionPowerOfTwo, CellIndicesHash> cells;
		DeckPowerOfTwo<Chunk> chunks;
		[[ECS_MAIN_ALLOCATOR]]
		AllocatorPolymorphic allocator;
		// This is the array with all the inserted cells
		ResizableStream<uint3> inserted_cells;
	};

}