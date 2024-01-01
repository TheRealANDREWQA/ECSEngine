#pragma once
#include "Components.h"
#include "InternalStructures.h"

namespace ECSEngine {

	struct EntityManager;
	struct TransformScalar;
	
	ECSENGINE_API void GetEntityTransform(const EntityManager* entity_manager, Entity entity, TransformScalar* transform);

	ECSENGINE_API void GetEntityTransform(EntityManager* entity_manager, Entity entity, Translation** translation, Rotation** rotation, Scale** scale);

	ECSENGINE_API Matrix ECS_VECTORCALL GetEntityTransformMatrix(Translation translation, Rotation rotation, Scale scale);

	// This will check for nullptrs and give default values if they are missing
	ECSENGINE_API Matrix ECS_VECTORCALL GetEntityTransformMatrix(const Translation* translation, const Rotation* rotation, const Scale* scale);

}