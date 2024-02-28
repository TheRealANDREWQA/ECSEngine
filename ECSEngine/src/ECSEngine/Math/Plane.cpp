#include "ecspch.h"
#include "Plane.h"

namespace ECSEngine {
	
	PlaneScalar::PlaneScalar(float3 axis_direction, float3 point)
	{
		axis_direction = NormalizeIfNot(axis_direction);
		dot = Dot(axis_direction, point);
		normal = axis_direction;
	}

	PlaneScalar PlaneScalar::FromNormalized(float3 axis_direction_normalized, float3 point)
	{
		PlaneScalar result;
		result.normal = axis_direction_normalized;
		result.dot = Dot(axis_direction_normalized, point);
		return result;
	}

	void PlaneScalar::Normalize() {
		// We need to divide both the normal and the dot with the normal's length
		float reciprocal_length = ECSEngine::ReciprocalLength(normal);
		normal *= float3::Splat(reciprocal_length);
		dot *= reciprocal_length;
	}

	Plane::Plane(Vector3 axis_direction, Vector3 point, size_t vector_count) {
		axis_direction = NormalizeIfNot(axis_direction, vector_count);
		dot = ECSEngine::Dot(axis_direction, point);
		normal = axis_direction;
	}

	Plane Plane::FromNormalized(Vector3 axis_direction_normalized, Vector3 point)
	{
		Plane result;
		result.normal = axis_direction_normalized;
		result.dot = ECSEngine::Dot(axis_direction_normalized, point);
		return result;
	}

	void Plane::Normalize() {
		// We need to divide both the normal and the dot with the normal's length
		Vec8f reciprocal_length = ECSEngine::ReciprocalLength(normal);
		normal *= Vector3::Splat(reciprocal_length);
		dot *= reciprocal_length;
	}

	PlaneScalar ECS_VECTORCALL Plane::At(size_t index) const
	{
		PlaneScalar return_value;
		return_value.normal = normal.At(index);
		return_value.dot = VectorAt(dot, index);
		return return_value;
	}

	void Plane::Set(PlaneScalar plane, size_t index)
	{
		normal.Set(plane.normal, index);
		// Broadcast and blend with that single value
		Vec8f dot_broadcast = plane.dot;
		dot = BlendSingleSwitch(dot, dot_broadcast, index);
	}

	template<typename Plane, typename Vector>
	static ECS_INLINE Plane ECS_VECTORCALL ComputePlaneImpl(Vector a, Vector b, Vector c) {
		auto normal = Normalize(Cross(b - a, c - a));
		auto dot = Dot(normal, a);
		return { normal, dot };
	}

	PlaneScalar ComputePlane(float3 a, float3 b, float3 c) {
		return ComputePlaneImpl<PlaneScalar>(a, b, c);
	}

	Plane ECS_VECTORCALL ComputePlane(Vector3 a, Vector3 b, Vector3 c) {
		return ComputePlaneImpl<Plane>(a, b, c);
	}

	template<typename Plane, typename Vector>
	static ECS_INLINE Plane ECS_VECTORCALL ComputePlaneAwayImpl(Vector a, Vector b, Vector c, Vector reference_point) {
		Plane plane = ComputePlane(a, b, c);
		auto plane_distance = DistanceToPlane(plane, reference_point);
		auto is_facing = plane_distance > SingleZeroVector<Vector>();
		// Both the normal and the dot need to be negated when facing the reference point
		plane.normal = Select(is_facing, -plane.normal, plane.normal);
		plane.dot = SelectSingle(is_facing, -plane.dot, plane.dot);
		return plane;
	}

	PlaneScalar ComputePlaneAway(float3 a, float3 b, float3 c, float3 reference_point) {
		return ComputePlaneAwayImpl<PlaneScalar>(a, b, c, reference_point);
	}

	Plane ECS_VECTORCALL ComputePlaneAway(Vector3 a, Vector3 b, Vector3 c, Vector3 reference_point) {
		return ComputePlaneAwayImpl<Plane>(a, b, c, reference_point);
	}

	template<typename Plane, typename Vector, typename OffsetType>
	static ECS_INLINE Plane ECS_VECTORCALL PlaneFromAxisImpl(Vector normal, OffsetType offset, bool invert_normal) {
		if (invert_normal) {
			normal = -normal;
			offset = -offset;
		}
		// Here the plane dot is directly the given offset
		return Plane::FromNormalized(normal, Vector::Splat(offset));
	}

	template<typename Plane, typename Vector, typename Quaternion>
	ECS_INLINE Plane ECS_VECTORCALL PlaneFromAxisRotatedImpl(Vector normal_normalized, Quaternion rotation, Vector offset, bool invert_normal) {
		normal_normalized = RotateVector(normal_normalized, rotation);
		if (invert_normal) {
			normal_normalized = -normal_normalized;
		}
		return Plane::FromNormalized(normal_normalized, offset);
	}

	PlaneScalar PlaneXYScalar(float z_offset, bool invert_normal) {
		float3 normal = GetForwardVector();
		return PlaneFromAxisImpl<PlaneScalar>(normal, z_offset, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneXY(Vec8f z_offset, bool invert_normal) {
		Vector3 normal = ForwardVector();
		return PlaneFromAxisImpl<Plane>(normal, z_offset, invert_normal);
	}

	PlaneScalar PlaneXZScalar(float y_offset, bool invert_normal) {
		float3 normal = GetUpVector();
		return PlaneFromAxisImpl<PlaneScalar>(normal, y_offset, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneXZ(Vec8f y_offset, bool invert_normal) {
		Vector3 normal = UpVector();
		return PlaneFromAxisImpl<Plane>(normal, y_offset, invert_normal);
	}

	PlaneScalar PlaneYZScalar(float x_offset, bool invert_normal) {
		float3 normal = GetRightVector();
		return PlaneFromAxisImpl<PlaneScalar>(normal, x_offset, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneYZ(Vec8f x_offset, bool invert_normal) {
		Vector3 normal = RightVector();
		return PlaneFromAxisImpl<Plane>(normal, x_offset, invert_normal);
	}

	PlaneScalar PlaneXYRotated(QuaternionScalar rotation, float3 point, bool invert_normal) {
		float3 normal = GetForwardVector();
		return PlaneFromAxisRotatedImpl<PlaneScalar>(normal, rotation, point, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneXYRotated(Quaternion rotation, Vector3 point, bool invert_normal) {
		Vector3 normal = ForwardVector();
		return PlaneFromAxisRotatedImpl<Plane>(normal, rotation, point, invert_normal);
	}

	PlaneScalar PlaneXZRotated(QuaternionScalar rotation, float3 point, bool invert_normal) {
		float3 normal = GetUpVector();
		return PlaneFromAxisRotatedImpl<PlaneScalar>(normal, rotation, point, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneXZRotated(Quaternion rotation, Vector3 point, bool invert_normal) {
		Vector3 normal = UpVector();
		return PlaneFromAxisRotatedImpl<Plane>(normal, rotation, point, invert_normal);
	}

	PlaneScalar PlaneYZRotated(QuaternionScalar rotation, float3 point, bool invert_normal) {
		float3 normal = GetRightVector();
		return PlaneFromAxisRotatedImpl<PlaneScalar>(normal, rotation, point, invert_normal);
	}

	Plane ECS_VECTORCALL PlaneYZRotated(Quaternion rotation, Vector3 point, bool invert_normal) {
		Vector3 normal = RightVector();
		return PlaneFromAxisRotatedImpl<Plane>(normal, rotation, point, invert_normal);
	}

	template<typename Mask, typename Plane, typename Vector>
	static ECS_INLINE Mask ECS_VECTORCALL IsAbovePlaneMaskImpl(Plane plane, Vector point) {
		// Get a point on the plane, calculate the direction to the original point
		// If the cosine of the normal and the direction is positive, then the point
		// is above. If the value is 0, then it's on the plane and negative means bellow the plane
		Vector plane_point = plane.normal * plane.dot;
		Vector direction = point - plane_point;
		auto cosine = Dot(plane.normal, direction);
		return cosine > SingleZeroVector<Vector>();
	}

	bool IsAbovePlaneMask(PlaneScalar plane, float3 point) {
		return IsAbovePlaneMaskImpl<bool>(plane, point);
	}

	SIMDVectorMask ECS_VECTORCALL IsAbovePlaneMask(Plane plane, Vector3 point) {
		return IsAbovePlaneMaskImpl<SIMDVectorMask>(plane, point);
	}

	template<typename Mask, typename Plane, typename Vector>
	static ECS_INLINE Mask ECS_VECTORCALL IsOnSamePlaneSideImpl(Plane plane, Vector point, Vector reference_point) {
		auto point_distance = DistanceToPlane(plane, point);
		auto reference_point_distance = DistanceToPlane(plane, reference_point);

		auto zero = SingleZeroVector<Vector>();
		// They are on the same side if the distance has the same sign
		// We could test by multiplying and seeing if the value is positive
		return (point_distance * reference_point_distance) > zero;
	}

	bool ArePointsSamePlaneSide(PlaneScalar plane, float3 point, float3 reference_point) {
		return IsOnSamePlaneSideImpl<bool>(plane, point, reference_point);
	}

	SIMDVectorMask ECS_VECTORCALL ArePointsSamePlaneSide(Plane plane, Vector3 point, Vector3 reference_point) {
		return IsOnSamePlaneSideImpl<SIMDVectorMask>(plane, point, reference_point);
	}

	template<typename ReturnType, typename Plane>
	static ECS_INLINE ReturnType ECS_VECTORCALL AngleBetweenPlanesRadImpl(Plane plane_a, Plane plane_b) {
		return acos(PlanesDotProduct(plane_a, plane_b));
	}

	float AngleBetweenPlanesRad(PlaneScalar plane_a, PlaneScalar plane_b) {
		return AngleBetweenPlanesRadImpl<float>(plane_b, plane_b);
	}

	Vec8f ECS_VECTORCALL AngleBetweenPlanesRad(Plane plane_a, Plane plane_b) {
		return AngleBetweenPlanesRadImpl<Vec8f>(plane_b, plane_b);
	}

	float3 ProjectPointOnPlane(PlaneScalar plane, float3 point) {
		return point - plane.normal * DistanceToPlane(plane, point);
	}

	Vector3 ECS_VECTORCALL ProjectPointOnPlane(Plane plane, Vector3 point) {
		return point - plane.normal * DistanceToPlane(plane, point);
	}

	float DistanceToPlane(PlaneScalar plane, float3 point) {
		return Dot(point, plane.normal) - plane.dot;
	}

	Vec8f ECS_VECTORCALL DistanceToPlane(Plane plane, Vector3 point) {
		return Dot(point, plane.normal) - plane.dot;
	}

	bool IsPointOnPlane(PlaneScalar plane, float3 point)
	{
		return CompareMaskSingle<float>(Dot(plane.normal, point), plane.dot);
	}

	SIMDVectorMask ECS_VECTORCALL IsPointOnPlane(Plane plane, Vector3 point)
	{
		return CompareMaskSingle<Vec8f>(Dot(plane.normal, point), plane.dot);
	}

	bool ArePointsCoplanar(Stream<float3> points) {
		if (points.size <= 3) {
			return true;
		}

		// Make a plane out of the first 3 non-collinear points, and then test each other point that it lies
		// In the same plane
		size_t second_point_index = 1;
		if (points[0] == points[1]) {
			// Keep searching for the second point
			while (points[0] == points[second_point_index] && second_point_index < points.size) {
				second_point_index++;
			}
		}
		size_t third_point_index = second_point_index + 1;
		if (third_point_index >= points.size) {
			// All points are equal
			return true;
		}

		while (IsPointCollinear(points[0], points[second_point_index], points[third_point_index]) && third_point_index < points.size) {
			third_point_index++;
		}
		if (third_point_index >= points.size - 1) {
			// All points are collinear or all but one are collinear
			return true;
		}

		PlaneScalar plane = ComputePlane(points[0], points[second_point_index], points[third_point_index]);
		for (size_t index = third_point_index + 1; index < points.size; index++) {
			// The points are on the same
			if (!IsPointOnPlane(plane, points[index])) {
				return false;
			}
		}

		return true;
	}

	bool ArePointsCoplanar(float3 A, float3 B, float3 C, float3 D) {
		// We have multiple ways of detecting this
		// If the dot product of the normals formed by the triangle ABC and ABD is near 0,
		// Then they are coplanar. Or we can create a plane out of ABC and verify that the distance
		// To the plane is 0.0f. Probably the latter is a better choice, since if we go with 2 unnormalized
		// Cross products, if they have large magnitudes, we could miss the coplanarity due to the relative
		// High product value
		PlaneScalar ABC_plane = ComputePlane(A, B, C);
		return CompareMaskSingle<float>(DistanceToPlane(ABC_plane, D), 0.0f, 0.001f);
	}
}