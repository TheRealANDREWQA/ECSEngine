#pragma once
#include "Quaternion.h"
#include "Matrix.h"

#define ECS_TRANSFORM_SCALE_EPSILON 0.00005f

namespace ECSEngine {

	// The rotation is a quaternion lane
	struct Transform {
		ECS_INLINE Transform() {}
		ECS_INLINE Transform(float3 _position, QuaternionStorage _rotation, float3 _scale) : position(_position),
			scale(_scale), rotation(_rotation) {}

		// Translation - 0.0f, 0.0f, 0.0f
		// Scale - 1.0f, 1.0f, 1.0f
		// Rotation - Quaternion Identity
		ECS_INLINE void Default() {
			position = { 0.0f, 0.0f, 0.0f };
			scale = { 1.0f, 1.0f, 1.0f };
			rotation = QuaternionIdentity().StorageLow();
		}

		float3 position;
		float3 scale;
		QuaternionStorage rotation;
	};

	struct VectorTransform {
		ECS_INLINE VectorTransform() {}
		ECS_INLINE VectorTransform(Vector8 _position, Vector8 _scale, Quaternion _rotation) : position(_position),
			scale(_scale), rotation(_rotation) {}
		ECS_INLINE VectorTransform(Transform low) : position(low.position),
			scale(low.scale), rotation(low.rotation) {}
		ECS_INLINE VectorTransform(Transform low, Transform high) : position(low.position, high.position),
			scale(low.scale, high.scale), rotation(low.rotation, high.rotation) {}

		ECS_INLINE Transform Low() const {
			return { position.AsFloat3Low(), rotation.StorageLow(), scale.AsFloat3Low() };
		}

		ECS_INLINE Transform High() const {
			return { position.AsFloat3High(), rotation.StorageHigh(), scale.AsFloat3High() };
		}

		// Translation - 0.0f, 0.0f, 0.0f
		// Scale - 1.0f, 1.0f, 1.0f
		// Rotation - Quaternion Identity
		ECS_INLINE void Default() {
			position = ZeroVector();
			scale = VectorGlobals::ONE;
			rotation = QuaternionIdentity();
		}

		Vector8 position;
		Vector8 scale;
		Quaternion rotation;
	};

	// ---------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API VectorTransform ECS_VECTORCALL TransformCombine(VectorTransform parent, VectorTransform child);

	// ---------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API VectorTransform ECS_VECTORCALL TransformInverse(VectorTransform transform);

	// ---------------------------------------------------------------------------------------------------------------------

	// Linear blend
	ECSENGINE_API VectorTransform ECS_VECTORCALL TransformMix(VectorTransform from, VectorTransform to, Vector8 percentage);

	// ---------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void ECS_VECTORCALL TransformToMatrix(VectorTransform transform, Matrix* low, Matrix* high);

	ECSENGINE_API Matrix ECS_VECTORCALL TransformToMatrixLow(VectorTransform transform);

	// ---------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API VectorTransform ECS_VECTORCALL MatrixToVectorTransform(Matrix matrix);

	ECSENGINE_API VectorTransform ECS_VECTORCALL MatrixToTransform(Matrix matrix1, Matrix matrix2);

	// ---------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vector8 ECS_VECTORCALL TransformPoint(VectorTransform transform, Vector8 point);

	// ---------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vector8 ECS_VECTORCALL TransformVector(VectorTransform transform, Vector8 vector);

	// ---------------------------------------------------------------------------------------------------------------------

	// It will first select a slice inside the XZ plane using the rotation given by the x component
	// Then it will rotate in the plane determined by the diameter of that rotation and the Y axis
	// The point is centered around the origin, if a translation is desired, it should be added afterwards
	// Rotation is in degrees
	ECSENGINE_API float3 OrbitPoint(float radius, float2 rotation);

	// ---------------------------------------------------------------------------------------------------------------------

}