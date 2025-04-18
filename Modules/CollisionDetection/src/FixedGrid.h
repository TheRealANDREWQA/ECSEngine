// ECS_REFLECT
#pragma once
#include "ECSEngineMath.h"
#include "ECSEngineContainers.h"
#include "Export.h"
#include "ECSEngineReflectionMacros.h"
#include "CollisionDetectionComponents.h"

using namespace ECSEngine;

#define GRID_CHUNK_COUNT ECS_CONSTANT_REFLECT(8)

struct ECS_REFLECT GridChunkDataEntry {
	AABBScalar aabb;
	unsigned int identifier;
	unsigned char layer;
};

struct ECS_REFLECT GridChunkData {
	void Set(GridChunkDataEntry entry, unsigned int count) {
		colliders[count] = entry.aabb;
		identifiers[count] = entry.identifier;
		layers[count] = entry.layer;
	}

	AABBScalar colliders[GRID_CHUNK_COUNT];
	unsigned int identifiers[GRID_CHUNK_COUNT];
	unsigned char layers[GRID_CHUNK_COUNT];
};

typedef SpatialGridChunk<GridChunkData> GridChunk;

struct CollisionInfo {
	unsigned int identifier;
	unsigned char layer;
};

struct ECS_REFLECT CollisionLayer {
	// This is a boolean bit mask that tells whether or not
	unsigned char entries[32];
};

struct FixedGrid;

struct FixedGridHandlerData {
	FixedGrid* grid;
	void* user_data;
	unsigned int first_identifier;
	unsigned int second_identifier;
	unsigned char first_layer;
	unsigned char second_layer;
};

// When a collision is detected, a collision handler will be called to perform the necessary action
// The handler must take as parameter a FixedGridHandlerData* pointer and then it can cast its own data
struct COLLISIONDETECTION_API ECS_REFLECT_GLOBAL_COMPONENT_PRIVATE FixedGrid {
	ECS_INLINE constexpr static short ID() {
		return COLLISION_DETECTION_GLOBAL_COMPONENT_BASE + 0;
	}

	ECS_INLINE AllocatorPolymorphic Allocator() const {
		return allocator;
	}

	// Adds a new cell. Make sure it doesn't exist previosuly!
	// Returns the newly assigned chunk for it
	GridChunk* AddCell(uint3 indices);

	// Adds a new entry to the given chunk, or if it is full, it will chain a new chunk
	// And add to that one instead. Returns the "current" chunk - the chained one if it
	// is the case, else the original chunk
	GridChunk* AddToChunk(unsigned int identifier, unsigned char layer, const AABBScalar& aabb, GridChunk* chunk);

	// Returns the cell x, y and z indices that can be used to retrieve a grid cell
	ECS_INLINE uint3 CalculateCell(float3 position) const {
		return spatial_grid.CalculateCell(position);
	}

	void ChangeHandler(
		ThreadFunction handler_function,
		void* handler_data,
		size_t handler_data_size
	);

	// Fills in the collisions that the given AABB has with a given cell. Returns the last chunk of the cell
	// It does not call the handler in this case - it will report them directly to you
	// The world is needed to be passed to the handler
	GridChunk* CheckCollisions(
		unsigned int thread_id, 
		World* world, 
		uint3 cell_index, 
		unsigned int identifier,
		unsigned char layer, 
		const AABBScalar& aabb, 
		CapacityStream<CollisionInfo>* collisions
	);

	// Fills in the collisions that the given AABB has with a given cell. Returns the last chunk of the cell
	// It does not call the handler in this case - it will report them directly to you
	// The world is needed to be passed to the handler
	GridChunk* CheckCollisions(
		unsigned int thread_id,
		World* world,
		GridChunk* chunk,
		unsigned int identifier,
		unsigned char layer,
		const AABBScalar& aabb,
		CapacityStream<CollisionInfo>* collisions
	);

	void Clear();
	
	bool ExistsCell(uint3 index) const;

	void EndFrame();

	void DisableLayerCollisions(unsigned char layer_index, unsigned char collision_layer);

	void EnableLayerCollisions(unsigned char layer_index, unsigned char collision_layer);

	bool IsLayerCollidingWith(unsigned char layer_index, unsigned char collision_layer) const;

	// If the handler data size is 0, it will reference it, otherwise it will copy the data
	void Initialize(
		AllocatorPolymorphic allocator, 
		uint3 dimensions, 
		uint3 cell_size_power_of_two, 
		size_t deck_power_of_two, 
		ThreadFunction handler_function,
		void* handler_data,
		size_t handler_data_size
	);

	// The AABB needs to be transformed already. The collision handler will be called for each collision
	void InsertEntry(unsigned int thread_id, World* world, unsigned int identifier, unsigned char layer, const AABBScalar& aabb);

	// The AABB needs to be transformed already. It will fill in the collisions
	// That it finds inside the given buffer and calls the functor for each collision pair that it finds
	void InsertEntry(unsigned int thread_id, World* world, unsigned int identifier, unsigned char layer, const AABBScalar& aabb, CapacityStream<CollisionInfo>* collisions);

	void InsertIntoCell(uint3 cell_indices, unsigned int identifier, unsigned char layer, const AABBScalar& aabb);

	void StartFrame();

	void SetLayerMask(unsigned char layer_index, const CollisionLayer* layer_mask);

	// This is the overall allocator for the entire type, to respect the global component requirements and for better locality
	[[ECS_MAIN_ALLOCATOR, ECS_REFERENCE_ALLOCATOR]]
	AllocatorPolymorphic allocator;
	SpatialGrid<GridChunkData, GridChunkDataEntry, GRID_CHUNK_COUNT> spatial_grid;

	// Record these values such that we can have a good approximation for the initial
	// Allocations for the cells and chunks
	unsigned int last_frame_chunk_count;
	unsigned int last_frame_cell_capacity;

	Stream<CollisionLayer> layers;

	// This is the function that will be called to handle the collisions
	ThreadFunction handler_function; ECS_SKIP_REFLECTION(static_assert(sizeof(ThreadFunction) == 8))
	void* handler_data; ECS_SKIP_REFLECTION()
};

// Data should be the fixed grid
COLLISIONDETECTION_API bool FixedGridResidencyFunction(uint3 index, void* data);