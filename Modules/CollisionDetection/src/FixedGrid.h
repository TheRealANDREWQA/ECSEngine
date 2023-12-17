#pragma once
#include "ECSEngineMath.h"
#include "ECSEngineEntities.h"
#include "ECSEngineContainers.h"

using namespace ECSEngine;

#define GRID_CHUNK_COUNT (8)

struct GridChunk {
	void AddEntry(AABBStorage aabb, Entity entity, unsigned char layer);

	ECS_INLINE bool IsFull() const {
		return count == GRID_CHUNK_COUNT;
	}

	AABBStorage colliders[GRID_CHUNK_COUNT];
	Entity entities[GRID_CHUNK_COUNT];
	unsigned char entity_layers[GRID_CHUNK_COUNT];
	unsigned char count;
	unsigned int next_chunk;
};

struct GridCell {
	unsigned int head_index;
};

struct CollisionLayer {
	// This is a boolean bit mask that tells whether or not
	unsigned char entries[32];
};

struct CollisionInfo {
	Entity entity;
	unsigned char layer;
};

struct FixedGrid {
	// Adds a new cell. Make sure it doesn't exist previosuly!
	// Returns the newly assigned chunk for it
	GridChunk* AddCell(uint3 indices);

	// Adds a new entry to the given chunk, or if it is full, it will chain a new chunk
	// And add to that one instead. Returns the "current" chunk - the chained one if it
	// is the case, else the original chunk
	GridChunk* AddToChunk(Entity entity, unsigned char layer, AABBStorage aabb, GridChunk* chunk);

	// Returns the cell x, y and z indices that can be used to retrieve a grid cell
	uint3 CalculateCell(float3 position) const;

	GridChunk* ChainChunk(GridChunk* current_chunk);

	// Fills in the collisions that the given AABB has with a given cell. Returns the last chunk of the cell
	// Or nullptr if the cell is not yet inserted
	GridChunk* CheckCollisions(uint3 cell_index, unsigned char layer, AABBStorage aabb, CapacityStream<CollisionInfo>* collisions);

	void Clear();

	void EndFrame();

	void DisableLayerCollisions(unsigned char layer_index, unsigned char collision_layer);

	void EnableLayerCollisions(unsigned char layer_index, unsigned char collision_layer);

	bool IsLayerCollidingWith(unsigned char layer_index, unsigned char collision_layer) const;

	void Initialize(AllocatorPolymorphic allocator, uint3 dimensions, uint3 cell_size_power_of_two, size_t deck_power_of_two);

	// The AABB needs to be transformed already. It will fill in the collisions
	// That it finds inside the given buffer
	void InsertEntry(Entity entity, unsigned char entity_layer, AABBStorage aabb, CapacityStream<CollisionInfo>* collisions);

	void InsertIntoCell(uint3 cell_indices, Entity entity, unsigned char layer, AABBStorage aabb);

	void StartFrame();

	void SetLayerMask(unsigned char layer_index, const CollisionLayer* mask);

	uint3 dimensions;
	// These cell sizes must be a power of two
	// We use modulo two trick to map the AABB
	uint3 cell_size_power_of_two;
	// We maintain the cells in a sparse hash table
	// The unsigned int as resource identifier is the actual hash value
	// The cell hash value must be computed by the user
	HashTable<GridCell, unsigned int, HashFunctionPowerOfTwo> cells;
	DeckPowerOfTwo<GridChunk> chunks;
	AllocatorPolymorphic allocator;

	// Record these values such that we can have a good approximation for the initial
	// Allocations for the cells and chunks
	unsigned int last_frame_chunk_count;
	unsigned int last_frame_cell_capacity;

	Stream<CollisionLayer> layers;
};

unsigned int Djb2Hash(unsigned int x, unsigned int y, unsigned int z);

unsigned int HashGridCellIndices(uint3 indices);