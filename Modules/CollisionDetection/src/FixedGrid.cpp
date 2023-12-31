#include "pch.h"
#include "FixedGrid.h"

// These values are used to initialize the chunks and the cells to a somewhat
// Decent initial values such that there are not many small resizings taking place
#define EMPTY_INITIAL_CHUNK_COUNT 4
#define EMPTY_INITIAL_CELL_CAPACITY_POWER_OF_TWO 10

unsigned int Djb2Hash(unsigned int x, unsigned int y, unsigned int z) {
	unsigned int hash = 5381;
	hash = ((hash << 5) + hash) + x;
	hash = ((hash << 5) + hash) + y;
	hash = ((hash << 5) + hash) + z;
	return hash;
}

unsigned int HashGridCellIndices(uint3 indices) {
	return Djb2Hash(indices.x, indices.y, indices.z);
}

bool FixedGridResidencyFunction(uint3 index, void* data)
{
	FixedGrid* grid = (FixedGrid*)data;
	return grid->ExistsCell(index);
}

GridChunk* FixedGrid::AddCell(uint3 indices)
{
	unsigned int chunk_index = chunks.ReserveIndex();
	GridCell cell = { chunk_index };
	GridChunk* chunk = &chunks[chunk_index];
	chunk->count = 0;
	chunk->next_chunk = -1;
	
	unsigned int hash = HashGridCellIndices(indices);
	InsertIntoDynamicTable(cells, allocator, cell, hash);
	inserted_cells.Add(indices);
	return chunk;
}

GridChunk* FixedGrid::AddToChunk(unsigned int identifier, unsigned char layer, AABBScalar aabb, GridChunk* chunk)
{
	if (chunk->IsFull()) {
		chunk = ChainChunk(chunk);
	}
	chunk->AddEntry(aabb, identifier, layer);
	return chunk;
}

uint3 FixedGrid::CalculateCell(float3 position) const
{
	// Truncate the position to uints, then use modulo power of two trick
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

GridChunk* FixedGrid::ChainChunk(GridChunk* current_chunk)
{
	unsigned int chunk_index = chunks.ReserveIndex();
	GridChunk* new_chunk = &chunks[chunk_index];
	new_chunk->count = 0;
	new_chunk->next_chunk = -1;
	current_chunk->next_chunk = chunk_index;
	return new_chunk;
}

GridChunk* FixedGrid::CheckCollisions(
	unsigned int thread_id, 
	World* world, 
	uint3 cell_index,
	unsigned int identifier,
	unsigned char layer, 
	AABBScalar aabb, 
	CapacityStream<CollisionInfo>* collisions
)
{
	unsigned int cell_hash = HashGridCellIndices(cell_index);
	GridCell cell;
	if (cells.TryGetValue(cell_hash, cell)) {
		unsigned int current_chunk = cell.head_index;
		GridChunk* chunk = &chunks[current_chunk];
		while (current_chunk != -1) {
			chunk = &chunks[current_chunk];
			for (unsigned char index = 0; index < chunk->count; index++) {
				// Test the layer first
				if (IsLayerCollidingWith(layer, chunk->layers[index])) {
					if (AABBOverlap(aabb, chunk->colliders[index])) {
						// Check to see if this entity was already added
						bool already_collided = collisions->Find(chunk->identifiers[index], [](CollisionInfo collision_info) {
							return collision_info.identifier;
						}) != -1;
						if (!already_collided) {
							// Add the collision and call the handler
							collisions->AddAssert({ chunk->identifiers[index], chunk->layers[index] });
							/*FixedGridHandlerData callback_handler_data;
							callback_handler_data.grid = this;
							callback_handler_data.user_data = handler_data;
							callback_handler_data.first_identifier = identifier;
							callback_handler_data.first_layer = layer;
							callback_handler_data.second_identifier = chunk->identifiers[index];
							callback_handler_data.second_layer = chunk->layers[index];
							handler_function(thread_id, world, &callback_handler_data);*/
						}
					}
				}
			}
			current_chunk = chunk->next_chunk;
		}
		return chunk;
	}
	return AddCell(cell_index);
}

void FixedGrid::Clear()
{
	cells.Deallocate(allocator);
	chunks.Deallocate();
	inserted_cells.FreeBuffer();
}

bool FixedGrid::ExistsCell(uint3 index) const
{
	unsigned int hash = HashGridCellIndices(index);
	return cells.Find(hash) != -1;
}

void FixedGrid::EndFrame()
{
	// Record the counts such that for the next frame we have a rough estimate
	// Of the needed size
	last_frame_cell_capacity = cells.GetCapacity();
	last_frame_chunk_count = chunks.GetChunkCount();
	// We can now clear the grid
	Clear();
}

void FixedGrid::DisableLayerCollisions(unsigned char layer_index, unsigned char collision_layer)
{
	ClearBit((void*)layers[layer_index].entries, collision_layer);
}

void FixedGrid::EnableLayerCollisions(unsigned char layer_index, unsigned char collision_layer)
{
	SetBit((void*)layers[layer_index].entries, collision_layer);
}

bool FixedGrid::IsLayerCollidingWith(unsigned char layer_index, unsigned char collision_layer) const
{
	return GetBit((void*)layers[layer_index].entries, collision_layer);
}

void FixedGrid::Initialize(
	AllocatorPolymorphic _allocator, 
	uint3 _dimensions, 
	uint3 _cell_size_power_of_two, 
	size_t deck_power_of_two,
	ThreadFunction _handler_function,
	void* _handler_data,
	size_t _handler_data_size
)
{
	ECS_CRASH_CONDITION(IsPowerOfTwo(_dimensions.x) && IsPowerOfTwo(_dimensions.y) && IsPowerOfTwo(_dimensions.z), "Fixed grid dimensions must be a power of two");
	ECS_CRASH_CONDITION(_handler_function != nullptr, "Fixed grid handler function must be specified");

	memset(this, 0, sizeof(*this));
	allocator = _allocator;
	dimensions = _dimensions;
	cell_size_power_of_two = _cell_size_power_of_two;
	half_cell_size = ulong3((size_t)1 << cell_size_power_of_two.x, (size_t)1 << cell_size_power_of_two.y, (size_t)1 << cell_size_power_of_two.z);
	half_cell_size *= float3::Splat(0.5f);
	chunks.Initialize(allocator, 0, (size_t)1 << deck_power_of_two, deck_power_of_two);
	inserted_cells.Initialize(allocator, 0);

	// Initialize the layers as well
	layers.Initialize(allocator, UCHAR_MAX);
	memset(layers.buffer, 0, layers.MemoryOf(layers.size));

	//handler_function = _handler_function;
	//handler_data = CopyNonZero(allocator, _handler_data, _handler_data_size);
}

void FixedGrid::InsertEntry(unsigned int thread_id, World* world, unsigned int identifier, unsigned char layer, AABBScalar aabb)
{
	// Just ignore the collisions
	ECS_STACK_CAPACITY_STREAM(CollisionInfo, collisions, ECS_KB);
	InsertEntry(thread_id, world, identifier, layer, aabb, &collisions);
}

void FixedGrid::InsertEntry(unsigned int thread_id, World* world, unsigned int identifier, unsigned char layer, AABBScalar aabb, CapacityStream<CollisionInfo>* collisions)
{
	// Determine the grid indices for the min and max points for the aabb
	uint3 min_cell = CalculateCell(aabb.min);
	uint3 max_cell = CalculateCell(aabb.max);

	// Now insert the entity in all the cells that it collides with
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
				GridChunk* chunk = CheckCollisions(thread_id, world, current_cell, identifier, layer, aabb, collisions);
				AddToChunk(identifier, layer, aabb, chunk);
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

void FixedGrid::InsertIntoCell(uint3 cell_indices, unsigned int identifier, unsigned char layer, AABBScalar aabb)
{
	unsigned int hash = HashGridCellIndices(cell_indices);
	GridCell cell;
	if (cells.TryGetValue(hash, cell)) {
		unsigned int chunk_index = cell.head_index;
		GridChunk* current_chunk = &chunks[chunk_index];
		while (chunk_index != -1) {
			current_chunk = &chunks[chunk_index];
			chunk_index = current_chunk->next_chunk;
		}
		AddToChunk(identifier, layer, aabb, current_chunk);
	}
	else {
		// TODO: Small performance optimization - use the hash value from here
		// Instead of recalculating inside the function
		GridChunk* chunk = AddCell(cell_indices);
		chunk->AddEntry(aabb, identifier, layer);
	}
}

void FixedGrid::StartFrame()
{
	// Set the initial allocations
	unsigned int chunk_initial_count = last_frame_chunk_count == 0 ? EMPTY_INITIAL_CHUNK_COUNT : last_frame_chunk_count;
	unsigned int cell_initial_capacity = last_frame_cell_capacity == 0 ? 1 << EMPTY_INITIAL_CELL_CAPACITY_POWER_OF_TWO : last_frame_cell_capacity;
	cells.Initialize(allocator, cell_initial_capacity);
	inserted_cells.ResizeNoCopy(cell_initial_capacity);
	chunks.ResizeNoCopy(chunk_initial_count);
}

void FixedGrid::SetLayerMask(unsigned char layer_index, const CollisionLayer* mask)
{
	memcpy(&layers[layer_index], mask, sizeof(*mask));
}

void GridChunk::AddEntry(AABBScalar aabb, unsigned int identifier, unsigned char layer)
{
	ECS_ASSERT(count < GRID_CHUNK_COUNT);
	colliders[count] = aabb;
	identifiers[count] = identifier;
	layers[count] = layer;
	count++;
}
