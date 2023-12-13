#pragma once
#include "ECSEngineMath.h"
#include "ECSEngineEntities.h"
#include "ECSEngineContainers.h"

using namespace ECSEngine;

struct GridChunk {
	AABBStorage colliders[8];
	Entity entities[8];
	unsigned char entity_layers[8];
	unsigned int next_chunk;
};

struct GridCell {
	unsigned int head_index;
};

struct FixedGrid {
	unsigned int dimensions[ECS_AXIS_COUNT];
	// These cell sizes must be a power of two
	// We use modulo two trick to map the AABB
	unsigned int cell_sizes[ECS_AXIS_COUNT];
	// We maintain the cells in a sparse hash table
	// The unsigned int as resource identifier is the actual hash value
	// The cell hash value must be computed by the user
	HashTable<GridCell, unsigned int, HashFunctionPowerOfTwo> cells;
};

unsigned int Djb2Hash(unsigned int x, unsigned int y, unsigned int z);

// Returns the cell x, y and z indices that can be used to retrieve a grid cell
uint3 CalculateGridIndices(float3 position, const FixedGrid* grid);

unsigned int HashGridIndices(uint3 indices);