#include "pch.h"
#include "Components.h"

bool CompareConvexCollider(const ConvexCollider* first, const ConvexCollider* second)
{
	return first->hull.size == second->hull.size && SoACompare(
		first->hull.size,
		first->hull.vertices_x,
		second->hull.vertices_x,
		first->hull.vertices_y,
		second->hull.vertices_y,
		first->hull.vertices_z,
		second->hull.vertices_z
	);
}

void CopyConvexCollider(ConvexCollider* destination, const ConvexCollider* source, AllocatorPolymorphic allocator, bool deallocate_existing_destination) 
{
	if (deallocate_existing_destination) {
		SoAResize(allocator, destination->hull.size, source->hull.size, &destination->hull.vertices_x, &destination->hull.vertices_y, &destination->hull.vertices_z);
	}
	else {
		SoAInitialize(allocator, source->hull.size, &destination->hull.vertices_x, &destination->hull.vertices_y, &destination->hull.vertices_z);
	}
}

void DeallocateConvexCollider(ConvexCollider* collider, AllocatorPolymorphic allocator) {
	// Coalesced Allocation
	if (collider->hull.size > 0 && collider->hull.vertices_x != nullptr) {
		Deallocate(allocator, collider->hull.vertices_x);
	}
	memset(&collider->hull, 0, sizeof(collider->hull));
}