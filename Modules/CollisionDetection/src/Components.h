// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineBasics.h"

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
			ECS_AXIS axis;
		};
		struct {
			// Convex hull
		};
	};
};

struct ECS_REFLECT_SETTINGS CollisionSettings {
	float factor;
	float inverse;
	unsigned int count;
};

//struct ECS_REFLECT_LINK_COMPONENT(Collider) Collider_Link {
//	bool;
//}