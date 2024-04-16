// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineBasics.h"
#include "ConvexHull.h"
#include "ECSEngineCamera.h"

#define COLLISION_DETECTION_COMPONENT_BASE ECS_CONSTANT_REFLECT(200)

#define COLLISION_DETECTION_SHARED_COMPONENT_BASE ECS_CONSTANT_REFLECT(200)

#define COLLISION_DETECTION_GLOBAL_COMPONENT_BASE ECS_CONSTANT_REFLECT(200)

enum ECS_REFLECT COLLIDER_TYPE : unsigned char {
	COLLIDER_SPHERE,
	COLLIDER_CAPSULE,
	COLLIDER_CONVEX_HULL,
	COLLIDER_TYPE_COUNT
};

struct ECS_REFLECT_COMPONENT Collider {
	constexpr static ECS_INLINE short ID() {
		return COLLISION_DETECTION_COMPONENT_BASE + 0;
	}

	constexpr static ECS_INLINE bool IsShared() {
		return false;
	}

	COLLIDER_TYPE type;
	union {
		struct {
			float3 center_offset;
			float radius;
		};
		struct {
			float3 center_offset;
			float radius;
			float length;
		};
		struct {
			// Convex hull
		};
	};
};

struct ECS_REFLECT_COMPONENT SphereCollider {
	constexpr static ECS_INLINE short ID() {
		return COLLISION_DETECTION_COMPONENT_BASE + 1;
	}

	constexpr static ECS_INLINE bool IsShared() {
		return false;
	}

	float3 center_offset;
	float radius;
	CameraParametersFOV fov;
};

struct ECS_REFLECT_COMPONENT CapsuleCollider {
	constexpr static ECS_INLINE short ID() {
		return COLLISION_DETECTION_COMPONENT_BASE + 2;
	}

	constexpr static ECS_INLINE bool IsShared() {
		return false;
	}

	float3 center_offset;
	float radius;
	float length;
	ECS_AXIS axis;
};

struct ECS_REFLECT_COMPONENT ConvexCollider {
	constexpr static ECS_INLINE short ID() {
		return COLLISION_DETECTION_COMPONENT_BASE + 3;
	}

	constexpr static ECS_INLINE bool IsShared() {
		return true;
	}

	constexpr static ECS_INLINE size_t AllocatorSize() {
		return ECS_MB_R * 128;
	}

	unsigned int hull_size;
	ConvexHull hull;
};

bool CompareConvexCollider(const ConvexCollider* first, const ConvexCollider* second);

void CopyConvexCollider(ConvexCollider* destination, const ConvexCollider* source, AllocatorPolymorphic allocator, bool deallocate_existing_destination);

void DeallocateConvexCollider(ConvexCollider* collider, AllocatorPolymorphic allocator);

struct ECS_REFLECT_SETTINGS CollisionSettings {
	float factor;
	float inverse;
	unsigned int count;
};

//struct ECS_REFLECT_LINK_COMPONENT(Collider) Collider_Link {
//	bool;
//}