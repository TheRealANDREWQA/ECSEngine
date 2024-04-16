#include "pch.h"
#include "CollisionDetectionComponents.h"

bool CompareConvexCollider(const ConvexCollider* first, const ConvexCollider* second)
{
	return first->hull.vertex_size == second->hull.vertex_size && SoACompare(
		first->hull.vertex_size,
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
	destination->hull_size = source->hull_size;
	destination->hull.Copy(&source->hull, allocator, deallocate_existing_destination);
}

void DeallocateConvexCollider(ConvexCollider* collider, AllocatorPolymorphic allocator) {
	// Coalesced Allocation
	collider->hull.Deallocate(allocator);
	collider->hull_size = 0;
}