#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "Vector.h"

#define ECS_MATRIX_EPSILON 0.001f

namespace ECSEngine {

	struct ECSENGINE_API Matrix {
		ECS_INLINE Matrix() {}

		Matrix(float x00, float x01, float x02, float x03,
			float x10, float x11, float x12, float x13,
			float x20, float x21, float x22, float x23,
			float x30, float x31, float x32, float x33);

		Matrix(const float* values);

		Matrix(const float4& row0, const float4& row1, const float4& row2, const float4& row3);

		ECS_INLINE Matrix(Vec8f _v1, Vec8f _v2) {
			v[0] = _v1;
			v[1] = _v2;
		}

		Matrix(const Matrix& other) = default;
		Matrix& ECS_VECTORCALL operator = (const Matrix& other) = default;

		bool ECS_VECTORCALL operator == (Matrix other);

		bool ECS_VECTORCALL operator != (Matrix other);

		Matrix ECS_VECTORCALL operator + (Matrix other) const;

		Matrix ECS_VECTORCALL operator - (Matrix other) const;

		Matrix ECS_VECTORCALL operator * (float value) const;

		Matrix ECS_VECTORCALL operator / (float value) const;

		Matrix ECS_VECTORCALL operator * (Matrix other) const;

		Matrix& ECS_VECTORCALL operator += (Matrix other);

		Matrix& ECS_VECTORCALL operator -= (Matrix other);

		Matrix& operator *= (float value);

		Matrix& operator /= (float value);

		Matrix& Load(const void* values);

		Matrix& LoadAligned(const void* values);

		void Store(void* values) const;

		void StoreAligned(void* values) const;

		void StoreStreamed(void* values) const;

		Vec8f v[2];
	};

	// -----------------------------------------------------------------------------------------------------

	ECSENGINE_API void ScalarMatrixMultiply(const float* ECS_RESTRICT a, const float* ECS_RESTRICT b, float* ECS_RESTRICT destination);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixMultiply(Matrix a, Matrix b);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixTranspose(Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixIdentity();

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixNull();

	// --------------------------------------------------------------------------------------------------------------

	// Returns the matrix with the last column and row set to 0.0f
	ECSENGINE_API Matrix ECS_VECTORCALL Matrix3x3(Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	// Returns the matrix with the last column set to 0.0f
	ECSENGINE_API Matrix ECS_VECTORCALL Matrix4x3(Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	// Returns the matrix with the last row set to 0.0f
	ECSENGINE_API Matrix ECS_VECTORCALL Matrix3x4(Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Vec4f ECS_VECTORCALL MatrixVec4fDeterminant(Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float ECS_VECTORCALL MatrixDeterminant(Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	// Register pressure present, many memory swaps are needed to keep permute indices into registers
	ECSENGINE_API Matrix ECS_VECTORCALL MatrixInverse(Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	// Will use doubles for calculations and will convert at the end in floats
	ECSENGINE_API Matrix ECS_VECTORCALL MatrixInversePrecise(Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixTranslation(float x, float y, float z);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixTranslation(float3 translation);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixScale(float x, float y, float z);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixScale(float3 scale);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationXRad(float angle_radians);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationX(float angle);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationYRad(float angle_radians);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationY(float angle);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationZRad(float angle_radians);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationZ(float angle);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationRad(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotation(float3 rotation);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationXZY(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationZXY(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationZYX(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationYXZ(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationYZX(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationXZYRad(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationZXYRad(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationZYXRad(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationYXZRad(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationYZXRad(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationCamera(float3 rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRotationCameraRad(float3 rotation);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixTRS(Matrix translation, Matrix rotation, Matrix scale);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixRS(Matrix rotation, Matrix scale);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixTR(Matrix translation, Matrix rotation);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixTS(Matrix translation, Matrix scale);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixMVP(Matrix object_matrix, Matrix camera_matrix);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixGPU(Matrix matrix);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixMVPToGPU(Matrix object_matrix, Matrix camera_matrix);

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixMVPToGPU(Matrix translation, Matrix rotation, Matrix scale, Matrix camera_matrix);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixLookTo(float3 origin, float3 direction, float3 up);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixLookAt(float3 origin, float3 focus_point, float3 up);

	// --------------------------------------------------------------------------------------------------------------

	// Left should be smaller than right
	// Top should be smaller than bottom
	// Near_z should be smaller than far_z
	ECSENGINE_API Matrix ECS_VECTORCALL MatrixPerspective(float left, float right, float top, float bottom, float near_z, float far_z);

	// --------------------------------------------------------------------------------------------------------------

	// It will align the matrix to the center of a rectangle with the given width and height
	// Near_z should be smaller than far_z
	ECSENGINE_API Matrix ECS_VECTORCALL MatrixPerspective(float width, float height, float near_z, float far_z);

	// --------------------------------------------------------------------------------------------------------------

	// The FOV angle needs to be expressed in degrees
	ECSENGINE_API Matrix ECS_VECTORCALL MatrixPerspectiveFOV(float fov, float aspect_ratio, float near_z, float far_z);

	// --------------------------------------------------------------------------------------------------------------

	// The fov is expressed in radians
	ECSENGINE_API float HorizontalFOVFromVerticalRad(float fov_rad, float aspect_ratio);

	// The fov is expressed in angles
	ECSENGINE_API float HorizontalFOVFromVertical(float fov_angles, float aspect_ratio);

	// The fov is expressed in radians
	ECSENGINE_API float VerticalFOVFromHorizontalRad(float fov_radians, float aspect_ratio);

	// The fov is expressed in angles
	ECSENGINE_API float VerticalFOVFromHorizontal(float fov_angles, float aspect_ratio);

	// --------------------------------------------------------------------------------------------------------------

	// Left should be smaller than right
	// Top should be smaller than bottom
	// Near_z should be smaller than far_z
	ECSENGINE_API Matrix ECS_VECTORCALL MatrixOrthographic(float left, float right, float top, float bottom, float near_z, float far_z);

	// It will align the matrix to the center of a rectangle with the given width and height
	// Near_z should be smaller than far_z
	ECSENGINE_API Matrix ECS_VECTORCALL MatrixOrthographic(float width, float height, float near_z, float far_z);

	// --------------------------------------------------------------------------------------------------------------

#undef near
#undef far

	ECSENGINE_API Matrix ECS_VECTORCALL MatrixFrustum(float left, float right, float top, float bottom, float near, float far);

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float3 ECS_VECTORCALL MatrixVectorMultiply(float3 vector, Matrix matrix);

	ECSENGINE_API float4 ECS_VECTORCALL MatrixVectorMultiply(float4 vector, Matrix matrix);

	ECSENGINE_API Vector3 ECS_VECTORCALL MatrixVectorMultiply(Vector3 vector, Matrix matrix);

	ECSENGINE_API Vector4 ECS_VECTORCALL MatrixVectorMultiply(Vector4 vector, Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 ECS_VECTORCALL RotateVector(float3 direction, Matrix rotation_matrix) {
		return MatrixVectorMultiply(direction, rotation_matrix);
	}

	ECS_INLINE Vector3 ECS_VECTORCALL RotateVector(Vector3 direction, Matrix rotation_matrix) {
		return MatrixVectorMultiply(direction, rotation_matrix);
	}

	ECS_INLINE float3 RotatePoint(float3 point, Matrix rotation_matrix) {
		// We can treat the position as a displacement vector
		return RotateVector(point, rotation_matrix);
	}

	ECS_INLINE Vector3 ECS_VECTORCALL RotatePoint(Vector3 point, Matrix rotation_matrix) {
		// We can treat the position as a displacement vector
		return RotateVector(point, rotation_matrix);
	}

	// --------------------------------------------------------------------------------------------------------------

	// It is still a matrix vector multiplication, but it better transmits the intent of the code
	ECS_INLINE float3 ECS_VECTORCALL TransformVector(float3 vector, Matrix matrix) {
		return MatrixVectorMultiply(vector, matrix);
	}

	// It is still a matrix vector multiplication, but it better transmits the intent of the code
	ECS_INLINE Vector3 ECS_VECTORCALL TransformVector(Vector3 vector, Matrix matrix) {
		return MatrixVectorMultiply(vector, matrix);
	}

	// It will set the 4th component to 1.0f as points need that and transform with it
	ECS_INLINE float4 ECS_VECTORCALL TransformPoint(float4 point, Matrix matrix) {
		return MatrixVectorMultiply(point, matrix);
	}

	// It will set the 4th component to 1.0f as points need that and transform with it
	ECS_INLINE float4 ECS_VECTORCALL TransformPoint(float3 point, Matrix matrix) {
		return TransformPoint(float4(point, 1.0f), matrix);
	}

	// It will set the 4th component to 1.0f as points need that and transform with it
	ECS_INLINE Vector4 ECS_VECTORCALL TransformPoint(Vector4 point, Matrix matrix) {
		return MatrixVectorMultiply(point, matrix);
	}

	// It will set the 4th component to 1.0f as points need that and transform with it
	ECS_INLINE Vector4 ECS_VECTORCALL TransformPoint(Vector3 point, Matrix matrix) {
		return TransformPoint({ point, VectorGlobals::ONE }, matrix);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Rotation in radians - the rotation matrix version
	ECSENGINE_API Matrix ECS_VECTORCALL MatrixTransformRad(float3 translation, float3 rotation, float3 scale);

	// Rotation in degrees - the rotation matrix version
	ECSENGINE_API Matrix ECS_VECTORCALL MatrixTransform(float3 translation, float3 rotation, float3 scale);

	// --------------------------------------------------------------------------------------------------------------

	// Changes a line defined by a point and a direction from a space to another space

	// The direction needs to be normalized before hand, the output it's going to be normalized as well
	ECSENGINE_API void ECS_VECTORCALL ChangeLineSpace(float3* line_point, float3* line_direction_normalized, Matrix matrix);

	// --------------------------------------------------------------------------------------------------------------

	// The points must be 3D, not 4D
	ECSENGINE_API void ECS_VECTORCALL TransformPoints(
		Matrix matrix, 
		const float* ECS_RESTRICT input_points_soa, 
		size_t count, 
		size_t capacity,
		float* ECS_RESTRICT output_points_soa,
		size_t output_capacity
	);

	// --------------------------------------------------------------------------------------------------------------

}