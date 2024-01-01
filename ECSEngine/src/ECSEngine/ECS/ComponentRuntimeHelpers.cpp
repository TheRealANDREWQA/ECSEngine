#include "ecspch.h"
#include "ComponentRuntimeHelpers.h"
#include "EntityManager.h"
#include "../Math/Transform.h"

namespace ECSEngine {

	void GetEntityTransform(const EntityManager* entity_manager, Entity entity, TransformScalar* transform) {
		const Translation* translation = entity_manager->TryGetComponent<Translation>(entity);
		const Rotation* rotation = entity_manager->TryGetComponent<Rotation>(entity);
		const Scale* scale = entity_manager->TryGetComponent<Scale>(entity);
		
		transform->position = translation != nullptr ? translation->value : float3::Splat(0.0f);
		transform->rotation = rotation != nullptr ? rotation->value : QuaternionIdentityScalar();
		transform->scale = scale != nullptr ? scale->value : float3::Splat(1.0f);
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
		float3 translation_value;
		if (translation != nullptr) {
			translation_value = translation->value;
		}
		else {
			translation_value = float3::Splat(0.0f);
		}

		QuaternionScalar rotation_value;
		if (rotation != nullptr) {
			rotation_value = rotation->value;
		}
		else {
			rotation_value = QuaternionIdentityScalar();
		}

		float3 scale_value;
		if (scale != nullptr) {
			scale_value = scale->value;
		}
		else {
			scale_value = float3::Splat(1.0f);
		}

		return MatrixTRS(MatrixTranslation(translation_value), QuaternionToMatrix(rotation_value), MatrixScale(scale_value));
	}

}