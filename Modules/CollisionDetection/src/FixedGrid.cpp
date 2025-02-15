#include "pch.h"
#include "FixedGrid.h"

// These values are used to initialize the chunks and the cells to a somewhat
// Decent initial values such that there are not many small resizings taking place
#define EMPTY_INITIAL_CHUNK_COUNT 4
#define EMPTY_INITIAL_CELL_CAPACITY_POWER_OF_TWO 10

bool FixedGridResidencyFunction(uint3 index, void* data)
{
	FixedGrid* grid = (FixedGrid*)data;
	return grid->ExistsCell(index);
}

GridChunk* FixedGrid::AddCell(uint3 indices)
{
	return spatial_grid.AddCell(indices);
}

GridChunk* FixedGrid::AddToChunk(unsigned int identifier, unsigned char layer, const AABBScalar& aabb, GridChunk* chunk)
{
	GridChunk* insert_chunk = spatial_grid.ReserveEntriesInChunk(chunk, 1);
	insert_chunk->data.Set({ aabb, identifier, layer }, insert_chunk->count);
	insert_chunk->count++;
	return insert_chunk;
}

void FixedGrid::ChangeHandler(ThreadFunction _handler_function, void* _handler_data, size_t _handler_data_size)
{
	DeallocateIfBelongs(Allocator(), handler_data);

	handler_function = _handler_function;
	handler_data = CopyNonZero(Allocator(), _handler_data, _handler_data_size);
}

GridChunk* FixedGrid::CheckCollisions(
	unsigned int thread_id, 
	World* world, 
	uint3 cell_index,
	unsigned int identifier,
	unsigned char layer, 
	const AABBScalar& aabb, 
	CapacityStream<CollisionInfo>* collisions
)
{
	GridChunk* last_chunk = CheckCollisions(thread_id, world, spatial_grid.GetChunk(cell_index), identifier, layer, aabb, collisions);
	if (last_chunk == nullptr) {
		last_chunk = spatial_grid.AddCell(cell_index);
	}
	return last_chunk;
}

GridChunk* FixedGrid::CheckCollisions(
	unsigned int thread_id, 
	World* world, 
	GridChunk* chunk, 
	unsigned int identifier, 
	unsigned char layer, 
	const AABBScalar& aabb, 
	CapacityStream<CollisionInfo>* collisions
)
{
	GridChunk* last_chunk;
	spatial_grid.IterateChunks(chunk, [=](const GridChunkData* data, unsigned int count) {
		for (unsigned int index = 0; index < count; index++) {
			// Test the layer first
			if (IsLayerCollidingWith(layer, data->layers[index])) {
				if (AABBOverlap(aabb, data->colliders[index])) {
					// Check to see if this entity was already added
					bool already_collided = collisions->Find(data->identifiers[index], [](CollisionInfo collision_info) {
						return collision_info.identifier;
						}) != -1;
					if (!already_collided) {
						// Add the collision and call the handler
						collisions->AddAssert({ data->identifiers[index], data->layers[index] });
						FixedGridHandlerData callback_handler_data;
						callback_handler_data.grid = this;
						callback_handler_data.user_data = handler_data;
						callback_handler_data.first_identifier = identifier;
						callback_handler_data.first_layer = layer;
						callback_handler_data.second_identifier = data->identifiers[index];
						callback_handler_data.second_layer = data->layers[index];
						handler_function(thread_id, world, &callback_handler_data);
					}
				}
			}
		}
	}, &last_chunk);
	return last_chunk;
}

void FixedGrid::Clear()
{
	spatial_grid.Clear();
}

bool FixedGrid::ExistsCell(uint3 index) const
{
	return spatial_grid.ExistsCell(index);
}

void FixedGrid::EndFrame()
{
	// Record the counts such that for the next frame we have a rough estimate
	// Of the needed size
	last_frame_cell_capacity = spatial_grid.cells.GetCapacity();
	last_frame_chunk_count = spatial_grid.chunks.GetChunkCount();
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
	ECS_CRASH_CONDITION(_handler_function != nullptr, "Fixed grid handler function must be specified");
	
	allocator = _allocator;
	spatial_grid.Initialize(_allocator, _dimensions, _cell_size_power_of_two, deck_power_of_two);

	// Initialize the layers as well
	layers.Initialize(Allocator(), UCHAR_MAX);
	memset(layers.buffer, 0, layers.MemoryOf(layers.size));

	handler_function = _handler_function;
	handler_data = CopyNonZero(Allocator(), _handler_data, _handler_data_size);
}

void FixedGrid::InsertEntry(unsigned int thread_id, World* world, unsigned int identifier, unsigned char layer, const AABBScalar& aabb)
{
	// Just ignore the collisions
	ECS_STACK_CAPACITY_STREAM(CollisionInfo, collisions, ECS_KB);
	InsertEntry(thread_id, world, identifier, layer, aabb, &collisions);
}

void FixedGrid::InsertEntry(unsigned int thread_id, World* world, unsigned int identifier, unsigned char layer, const AABBScalar& aabb, CapacityStream<CollisionInfo>* collisions)
{
	spatial_grid.InsertAABB(aabb.min, aabb.max, { aabb, identifier, layer }, [&](uint3 cell_indices, GridChunk* initial_chunk) {
		CheckCollisions(thread_id, world, initial_chunk, identifier, layer, aabb, collisions);
	});
}

void FixedGrid::InsertIntoCell(uint3 cell_indices, unsigned int identifier, unsigned char layer, const AABBScalar& aabb)
{
	spatial_grid.InsertEntry(cell_indices, { aabb, identifier, layer });
}

void FixedGrid::StartFrame()
{
	// Set the initial allocations
	unsigned int chunk_initial_count = last_frame_chunk_count == 0 ? EMPTY_INITIAL_CHUNK_COUNT : last_frame_chunk_count;
	unsigned int cell_initial_capacity = last_frame_cell_capacity == 0 ? 1 << EMPTY_INITIAL_CELL_CAPACITY_POWER_OF_TWO : last_frame_cell_capacity;
	spatial_grid.cells.Initialize(Allocator(), cell_initial_capacity);
	spatial_grid.inserted_cells.ResizeNoCopy(cell_initial_capacity);
	spatial_grid.chunks.ResizeNoCopy(chunk_initial_count);
}

void FixedGrid::SetLayerMask(unsigned char layer_index, const CollisionLayer* mask)
{
	memcpy(&layers[layer_index], mask, sizeof(*mask));
}