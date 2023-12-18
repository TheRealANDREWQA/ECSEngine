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

	return chunk;
}

GridChunk* FixedGrid::AddToChunk(Entity entity, unsigned char layer, AABBStorage aabb, GridChunk* chunk)
{
	if (chunk->IsFull()) {
		chunk = ChainChunk(chunk);
	}
	chunk->AddEntry(aabb, entity, layer);
	return chunk;
}

uint3 FixedGrid::CalculateCell(float3 position) const
{
	// Truncate the position to uints, then use modulo power of two trick
	uint3 int_position = position;
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

GridChunk* FixedGrid::CheckCollisions(uint3 cell_index, unsigned char layer, AABBStorage aabb, CapacityStream<CollisionInfo>* collisions)
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
				if (IsLayerCollidingWith(layer, chunk->entity_layers[index])) {
					if (AABBOverlapStorage(aabb, chunk->colliders[index])) {
						collisions->AddAssert({ chunk->entities[index], chunk->entity_layers[index] });
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

void FixedGrid::Initialize(AllocatorPolymorphic _allocator, uint3 _dimensions, uint3 _cell_size_power_of_two, size_t deck_power_of_two)
{
	memset(this, 0, sizeof(*this));
	allocator = _allocator;
	dimensions = _dimensions;
	cell_size_power_of_two = _cell_size_power_of_two;
	chunks.Initialize(allocator, 0, (size_t)1 << deck_power_of_two, deck_power_of_two);

	// Initialize the layers as well
	layers.Initialize(allocator, UCHAR_MAX);
	memset(layers.buffer, 0, layers.MemoryOf(layers.size));
}

void FixedGrid::InsertEntry(Entity entity, unsigned char entity_layer, AABBStorage aabb, CapacityStream<CollisionInfo>* collisions)
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
				GridChunk* chunk = CheckCollisions(current_cell, entity_layer, aabb, collisions);
				AddToChunk(entity, entity_layer, aabb, chunk);
				current_cell.z++;
				current_cell.z = current_cell.z == dimensions.z ? 0 : current_cell.z;
			}
			current_cell.y++;
			current_cell.y = current_cell.y == dimensions.y ? 0 : current_cell.y;
		}
		current_cell.x++;
		current_cell.x = current_cell.x == dimensions.x ? 0 : current_cell.x;
	}
}

void FixedGrid::InsertIntoCell(uint3 cell_indices, Entity entity, unsigned char layer, AABBStorage aabb)
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
		AddToChunk(entity, layer, aabb, current_chunk);
	}
	else {
		// TODO: Small performance optimization - use the hash value from here
		// Instead of recalculating inside the function
		GridChunk* chunk = AddCell(cell_indices);
		chunk->AddEntry(aabb, entity, layer);
	}
}

void FixedGrid::StartFrame()
{
	// Set the initial allocations
	unsigned int chunk_initial_count = last_frame_chunk_count == 0 ? EMPTY_INITIAL_CHUNK_COUNT : last_frame_chunk_count;
	unsigned int cell_initial_capacity = last_frame_cell_capacity == 0 ? 1 << EMPTY_INITIAL_CELL_CAPACITY_POWER_OF_TWO : last_frame_cell_capacity;
	cells.Initialize(allocator, cell_initial_capacity);
	chunks.ResizeNoCopy(chunk_initial_count);
}

void FixedGrid::SetLayerMask(unsigned char layer_index, const CollisionLayer* mask)
{
	memcpy(&layers[layer_index], mask, sizeof(*mask));
}

void GridChunk::AddEntry(AABBStorage aabb, Entity entity, unsigned char layer)
{
	ECS_ASSERT(count < GRID_CHUNK_COUNT);
	colliders[count] = aabb;
	entities[count] = entity;
	entity_layers[count] = layer;
	count++;
}
