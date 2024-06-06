#include "ecspch.h"
#include "ComponentRuntimeHelpers.h"
#include "EntityManager.h"
#include "../Math/Transform.h"

namespace ECSEngine {

	TransformScalar GetEntityTransform(const EntityManager* entity_manager, Entity entity) {
		TransformScalar transform;

		const Translation* translation = entity_manager->TryGetComponent<Translation>(entity);
		const Rotation* rotation = entity_manager->TryGetComponent<Rotation>(entity);
		const Scale* scale = entity_manager->TryGetComponent<Scale>(entity);
		
		transform.position = GetTranslation(translation);
		transform.rotation = GetRotation(rotation);
		transform.scale = GetScale(scale);
	
		return transform;
	}

	void GetEntityTransform(EntityManager* entity_manager, Entity entity, Translation** translation, Rotation** rotation, Scale** scale) {
		*translation = entity_manager->TryGetComponent<Translation>(entity);
		*rotation = entity_manager->TryGetComponent<Rotation>(entity);
		*scale = entity_manager->TryGetComponent<Scale>(entity);
	}

	Matrix ECS_VECTORCALL GetEntityTransformMatrix(Translation translation, Rotation rotation, Scale scale)
	{
		return MatrixTRS(MatrixTranslation(translation.value), QuaternionToMatrix(rotation.value), MatrixScale(scale.value));
	}

	Matrix ECS_VECTORCALL GetEntityTransformMatrix(const Translation* translation, const Rotation* rotation, const Scale* scale) {
		float3 translation_value = GetTranslation(translation);
		QuaternionScalar rotation_value = GetRotation(rotation);
		float3 scale_value = GetScale(scale);

		return MatrixTRS(MatrixTranslation(translation_value), QuaternionToMatrix(rotation_value), MatrixScale(scale_value));
	}

}