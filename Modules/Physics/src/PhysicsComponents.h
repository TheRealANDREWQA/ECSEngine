// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"
#include "ECSEngineBasics.h"
#include "ECSEngineAssetMacros.h"
#include "ECSEngineResourceTypes.h"

#define PHYSICS_COMPONENT_BASE ECS_CONSTANT_REFLECT(400)

#define PHYSICS_SHARED_COMPONENT_BASE ECS_CONSTANT_REFLECT(400)

#define PHYSICS_GLOBAL_COMPONENT_BASE ECS_CONSTANT_REFLECT(400)

// ------------------------ Copy Paste Helpers ------------------------------

// Unique
//struct ECS_REFLECT_COMPONENT ComponentName {
//	ECS_INLINE constexpr static short ID() {
//		return Physics_COMPONENT_BASE + 0;
//	}
//
//	ECS_INLINE constexpr static bool IsShared() {
//		return false;
//	}
// 
// 	ECS_INLINE constexpr static size_t AllocatorSize() {
//		return ECS_KB_R * 256;
//	}
// 
//};

// Shared
//struct ECS_REFLECT_COMPONENT ComponentName {
//	ECS_INLINE constexpr static short ID() {
//		return Physics_SHARED_COMPONENT_BASE + 0;
//	}
//
//	ECS_INLINE constexpr static bool IsShared() {
//		return true;
//	}
//	
//	ECS_INLINE constexpr static size_t AllocatorSize() {
//		return ECS_KB_R * 256;
//	}
// 
//};

// Global
//struct ECS_REFLECT_GLOBAL_COMPONENT ComponentName {
//	ECS_INLINE constexpr static short ID() {
//		return Physics_GLOBAL_COMPONENT_BASE + 0;
//	}
// 
//};

// ------------------------ Copy Paste Helpers ------------------------------