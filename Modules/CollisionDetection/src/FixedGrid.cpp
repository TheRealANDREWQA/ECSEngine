#include "pch.h"
#include "FixedGrid.h"

unsigned int Djb2Hash(unsigned int x, unsigned int y, unsigned int z) {
	unsigned int hash = 5381;
	hash = ((hash << 5) + hash) + x;
	hash = ((hash << 5) + hash) + y;
	hash = ((hash << 5) + hash) + z;
	return hash;
}

uint3 CalculateGridIndices(float3 position, const FixedGrid* grid) {
	// Truncate the position to uints, then use modulo power of two trick
	uint3 int_position = position;
	int_position.x = int_position.x & (grid->cell_sizes[ECS_AXIS_X] - 1);
	int_position.y = int_position.y & (grid->cell_sizes[ECS_AXIS_Y] - 1);
	int_position.z = int_position.z & (grid->cell_sizes[ECS_AXIS_Z] - 1);
}

unsigned int HashGridIndices(uint3 indices) {
	return Djb2Hash(indices.x, indices.y, indices.z);
}