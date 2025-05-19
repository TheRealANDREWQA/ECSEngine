#include "ecspch.h"
#include "Camera.h"
#include "../Utilities/Algorithms.h"
#include "../Math/MathHelpers.h"

namespace ECSEngine {

	Camera::Camera(float3 _translation, float3 _rotation) : translation(_translation), rotation(_rotation), is_orthographic(false), is_perspective_fov(false) {}

	Camera::Camera(Matrix _projection, float3 _translation, float3 _rotation) : projection(_projection),
		translation(_translation), rotation(_rotation), is_orthographic(false), is_perspective_fov(false) {}

	Camera::Camera(const CameraParameters& parameters) : is_perspective_fov(false) {
		translation = parameters.translation;
		rotation = parameters.rotation;

		if (parameters.is_orthographic) {
			is_orthographic = true;
			SetOrthographicProjection(parameters.width, parameters.height, parameters.near_z, parameters.far_z);
		}
		else {
			is_orthographic = false;
			SetPerspectiveProjection(parameters.width, parameters.height, parameters.near_z, parameters.far_z);
		}
	}

	Camera::Camera(const CameraParametersFOV& parameters) : is_perspective_fov(true) {
		translation = parameters.translation;
		rotation = parameters.rotation;

		SetPerspectiveProjectionFOV(parameters.fov, parameters.aspect_ratio, parameters.near_z, parameters.far_z);
	}

	void Camera::SetOrthographicProjection(float _width, float _height, float _near_z, float _far_z) {
		is_orthographic = true;
		is_perspective_fov = false;

		width = _width;
		height = _height;
		near_z = _near_z;
		far_z = _far_z;
		projection = MatrixOrthographic(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjection(float _width, float _height, float _near_z, float _far_z) {
		is_orthographic = false;
		is_perspective_fov = false;

		width = _width;
		height = _height;
		near_z = _near_z;
		far_z = _far_z;
		projection = MatrixPerspective(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjectionFOV(float _fov, float _aspect_ratio, float _near_z, float _far_z) {
		is_orthographic = false;
		is_perspective_fov = true;

		fov = _fov;
		aspect_ratio = _aspect_ratio;
		near_z = _near_z;
		far_z = _far_z;
		projection = MatrixPerspectiveFOV(fov, aspect_ratio, near_z, far_z);
	}

	Matrix ECS_VECTORCALL Camera::GetViewProjectionMatrix() const {
		return GetTranslation() * GetRotation() * GetProjection();
	}

	CameraParameters Camera::AsParameters() const
	{
		CameraParameters parameters;

		parameters.near_z = near_z;
		parameters.far_z = far_z;
		parameters.width = width;
		parameters.height = height;
		parameters.is_orthographic = is_orthographic;
		parameters.rotation = rotation;
		parameters.translation = translation;

		return parameters;
	}

	CameraParametersFOV Camera::AsParametersFOV() const
	{
		CameraParametersFOV parameters;

		parameters.near_z = near_z;
		parameters.far_z = far_z;
		parameters.aspect_ratio = aspect_ratio;
		parameters.fov = fov;
		parameters.rotation = rotation;
		parameters.translation = translation;

		return parameters;
	}

	void CameraParameters::Default()
	{
		translation = { 0.0f, 0.0f, 0.0f };
		rotation = { 0.0f, 0.0f, 0.0f };

		is_orthographic = false;
		width = 0.0f;
		height = 0.0f;
		near_z = ECS_CAMERA_DEFAULT_NEAR_Z;
		far_z = ECS_CAMERA_DEFAULT_FAR_Z;
	}

	void CameraParametersFOV::Default()
	{
		translation = { 0.0f, 0.0f, 0.0f };
		rotation = { 0.0f, 0.0f, 0.0f };

		fov = 60.0f;
		aspect_ratio = 16.0f / 9.0f;
		near_z = ECS_CAMERA_DEFAULT_NEAR_Z;
		far_z = ECS_CAMERA_DEFAULT_FAR_Z;
	}

	CameraCached::CameraCached(const Camera* camera)
	{
		is_orthographic = camera->is_orthographic;
		is_perspective_fov = camera->is_perspective_fov;
		translation = camera->translation;
		rotation = camera->rotation;
		near_z = camera->near_z;
		far_z = camera->far_z;

		if (is_orthographic || !is_perspective_fov) {
			width = camera->width;
			height = camera->height;

			fov = 0.0f;
			aspect_ratio = 0.0f;
			horizontal_fov = 0.0f;
		}
		else {
			fov = camera->fov;
			aspect_ratio = camera->aspect_ratio;
			horizontal_fov = HorizontalFOVFromVertical(fov, aspect_ratio);
		
			width = 0.0f;
			height = 0.0f;
		}

		rotation_matrix = camera->GetRotation();
		rotation_as_is_matrix = camera->GetRotationAsIs();
		projection_matrix = camera->projection;
		view_projection_matrix = camera->GetViewProjectionMatrix();
		inverse_view_projection_matrix = camera->GetInverseViewProjectionMatrix();
	}

	void CameraCached::Recalculate()
	{
		Camera camera;
		if (is_orthographic) {
			camera.SetOrthographicProjection(width, height, near_z, far_z);
		}
		else if (is_perspective_fov) {
			camera.SetPerspectiveProjectionFOV(fov, aspect_ratio, near_z, far_z);
		}
		else {
			camera.SetPerspectiveProjection(width, height, near_z, far_z);
		}

		camera.translation = translation;
		camera.rotation = rotation;
		*this = CameraCached(&camera);
	}

	CameraParameters CameraCached::AsParameters() const
	{
		CameraParameters parameters;

		parameters.near_z = near_z;
		parameters.far_z = far_z;
		parameters.width = width;
		parameters.height = height;
		parameters.is_orthographic = is_orthographic;
		parameters.rotation = rotation;
		parameters.translation = translation;

		return parameters;
	}

	CameraParametersFOV CameraCached::AsParametersFOV() const
	{
		CameraParametersFOV parameters;

		parameters.near_z = near_z;
		parameters.far_z = far_z;
		parameters.aspect_ratio = aspect_ratio;
		parameters.fov = fov;
		parameters.rotation = rotation;
		parameters.translation = translation;

		return parameters;
	}

	float2 MouseToNDC(uint2 window_size, int2 mouse_texel_position)
	{
		return MouseToNDC(float2(window_size), float2(mouse_texel_position));
	}

	float2 MouseToNDC(float2 window_size, float2 mouse_texel_position)
	{
		return float2(2.0f * mouse_texel_position.x / window_size.x - 1.0f, 1.0f - 2.0f * mouse_texel_position.y / window_size.y);
	}

	int2 NDCToViewportTexels(uint2 window_size, float2 position)
	{
		float2 percentage = { position.x * 0.5f + 0.5f, 0.5f - position.y * 0.5f };
		return int2(float2(window_size) * percentage);
	}

	// We cannot use the horizontal and vertical fov to determine the position
	// Since that will result in interpolation of angles - and we would have to
	// use spherical interpolation which would be expensive
	template<typename CameraType>
	float3 ViewportToWorldRayDirection(const CameraType* camera, uint2 window_size, int2 mouse_texel_position)
	{
		float2 normalized_values = MouseToNDC(window_size, mouse_texel_position);
		float4 near_clip_space_position(normalized_values.x, normalized_values.y, -1.0f, 1.0f);
		Matrix inverse_matrix = GetCameraInverseViewProjectionMatrix(camera);
		float4 eye_space_pos = MatrixVectorMultiply(near_clip_space_position, inverse_matrix);
		eye_space_pos = PerspectiveDivide(eye_space_pos);
		float3 ray_direction_world = eye_space_pos.xyz() - camera->translation;
		return Normalize(ray_direction_world);
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, ViewportToWorldRayDirection, const Camera*, const CameraCached*, uint2, int2);

	template<typename CameraType>
	int2 PositionToViewportTexels(const CameraType* camera, uint2 viewport_size, float3 position) {
		Matrix view_projection_matrix = GetCameraViewProjectionMatrix(camera);
		// Transform the point using the view projection matrix
		float4 transformed_point = TransformPoint(position, view_projection_matrix);
		// Perform the perspective divide
		// Now we have the NDC position of the point
		float4 ndc_point = ClipSpaceToNDC(transformed_point);
		return NDCToViewportTexels(viewport_size, ndc_point.xy());
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(int2, PositionToViewportTexels, const Camera*, const CameraCached*, uint2, float3);

	template<typename CameraType>
	float3 FocusCameraOnObjectViewSpace(
		const CameraType* camera,
		const AABBScalar& world_space_aabb,
		float screen_fill_factor
	)
	{
		float camera_view = 2.0f * tanf(0.5f * DegToRad(camera->VerticalFOV()));
		float3 aabb_extents = AABBExtents(world_space_aabb);
		float maximum_aabb_size = BasicTypeMax(aabb_extents);

		float clamped_factor = Clamp(screen_fill_factor, 0.01f, 1.0f);
		// In order to ensure a somewhat percentage fill, use the factor as a divisor
		float distance = (0.5f / clamped_factor) * maximum_aabb_size / camera_view;
		// If we always add this distance as a fixed value, we will rarely get to screen coverage of 100%,
		// So scale this value by a little, such that we get somewhat closer when the factor is 1.0
		distance += 0.5f * Length(aabb_extents) * (1.0f - clamped_factor * 0.5f);
		return AABBCenter(world_space_aabb) - GetCameraForwardVectorScalar(camera) * distance;
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, FocusCameraOnObjectViewSpace, const Camera*, const CameraCached*, const AABBScalar&, float);

	template<typename CameraType>
	float3 FocusCameraOnObjectViewSpace(
		const CameraType* camera,
		float3 object_translation,
		QuaternionScalar object_rotation,
		float3 object_scale,
		AABBScalar object_bounds,
		float screen_fill_factor
	) {
		AABBScalar world_space_aabb = TransformAABB(object_bounds, object_translation, QuaternionToMatrix(object_rotation), object_scale);
		return FocusCameraOnObjectViewSpace(camera, world_space_aabb, screen_fill_factor);

		// This is the iterative approach that was used previously. Kept here just as an example of BinaryIntoLinearSearch
		
		//ECS_ASSERT(view_space_proportion.x == 0.0f || view_space_proportion.y == 0.0f);

		//// Use a binary search of the right distance
		//const float min_distance = 0.0f;
		//const float max_distance = 10000.0f;

		//// TODO: Replace this with the formula for computing the distance of a camera
		//// Check https://discussions.unity.com/t/fit-object-exactly-into-perspective-cameras-field-of-view-focus-the-object/677696/4.
		//// https://stackoverflow.com/questions/71215446/how-to-focus-a-perspectie-or-orthographic-camera-on-gameobjects.

		//const float BINARY_SEARCH_EPSILON = 0.25f;
		//const float LINEAR_SEARCH_EPSILON = 0.1f;
		//const float LINEAR_SEARCH_STEP_SIZE = 0.001f;

		//// Transform the AABB into the world space position
		//AABBScalar transformed_bounds = TransformAABB(object_bounds, object_translation, QuaternionToMatrix(object_rotation), object_scale);
		//Matrix view_projection_matrix = GetCameraViewProjectionMatrix(camera);

		//// Bring the center of the world space AABB right at the camera
		//// And then offset by the length of the half extent vector along the camera forward
		//// Such that the camera won't penetrate the object
		//// Inside the binary search and linear search we will offset in alongside the 
		//// negative forward vector of the camera
		//float3 transformed_bounds_center = AABBCenter(transformed_bounds);
		//float3 camera_forward = GetCameraForwardVectorScalar(camera);
		//float3 camera_translation = camera->translation;

		//float3 half_extents = AABBHalfExtents(transformed_bounds);
		//float3 displacement = camera_translation - transformed_bounds_center + camera_forward * Length(half_extents);
		//transformed_bounds = TranslateAABB(transformed_bounds, displacement);
		//
		//// In case it is not possible to attain, the
		//float resulting_distance = FLT_MAX;
		//auto get_aabb_distance_info = [&](float current_distance, bool* is_smaller, float2* coverage, float* current_difference) {
		//	float3 aabb_translation = camera_forward * current_distance;
		//	AABBScalar current_aabb = TranslateAABB(transformed_bounds, aabb_translation);
		//	Vector3 aabb_corners = GetAABBCornersScalarToSIMD(current_aabb);

		//	// Project all corners of the aabb on the screen and determine the percentage of coverage
		//	Vector4 projected_corners = TransformPoint(aabb_corners, view_projection_matrix);
		//	Vector4 ndc_corners = ClipSpaceToNDC(projected_corners);
		//
		//	// We are interested in the maximum absolute difference on the x and y coordinates
		//	// We cannot perform that operation SIMD friendly, since it involves operations horizontally inside the register
		//	float scalar_corners_ndc_x[Vector4::ElementCount()];
		//	float scalar_corners_ndc_y[Vector4::ElementCount()];
		//	ndc_corners.x.store(scalar_corners_ndc_x);
		//	ndc_corners.y.store(scalar_corners_ndc_y);
		//	float2 current_coverage = float2(0.0f, 0.0f);
		//	for (size_t index = 0; index < Vector4::ElementCount(); index++) {
		//		for (size_t subindex = index + 1; subindex < Vector4::ElementCount(); subindex++) {
		//			current_coverage.x = max(current_coverage.x, AbsoluteDifferenceSingle(scalar_corners_ndc_x[index], scalar_corners_ndc_x[subindex]));
		//			current_coverage.y = max(current_coverage.y, AbsoluteDifferenceSingle(scalar_corners_ndc_y[index], scalar_corners_ndc_y[subindex]));
		//		}
		//	}

		//	bool are_points_inside_camera = horizontal_and(CullClipSpaceZMask(ndc_corners) && CullClipSpaceXMask(ndc_corners) && CullClipSpaceYMask(ndc_corners));

		//	*coverage = current_coverage;

		//	if (view_space_proportion.x == 0.0f) {
		//		// The Y proportion is specified
		//		*current_difference = are_points_inside_camera ? AbsoluteDifferenceSingle(view_space_proportion.y, coverage->y) : FLT_MAX;
		//		*is_smaller = coverage->y < view_space_proportion.y;
		//	}
		//	else {
		//		// The X proportion is specified
		//		*current_difference = are_points_inside_camera ? AbsoluteDifferenceSingle(view_space_proportion.x, coverage->x) : FLT_MAX;
		//		*is_smaller = coverage->x < view_space_proportion.x;
		//	}
		//};
		//auto aabb_compare = [&](float current_distance, float epsilon) {
		//	float current_difference = 0.0f;
		//	bool is_smaller = false;
		//	float2 coverage;
		//	get_aabb_distance_info(current_distance, &is_smaller, &coverage, &current_difference);
		//	if (!isnan(current_difference) && current_difference < epsilon) {
		//		return 0;
		//	}
		//	resulting_distance = min(current_distance, resulting_distance);
		//	return is_smaller ? 1 : -1;
		//};

		//// Do a precheck. In case this precheck returns that we need to be closer
		//bool initial_is_smaller = false;
		//float2 initial_coverage;
		//float initial_difference = 0.0f;
		//get_aabb_distance_info(0.0f, &initial_is_smaller, &initial_coverage, &initial_difference);
		//if (initial_is_smaller) {
		//	// Move along
		//	return transformed_bounds_center - camera_forward * Length(half_extents);
		//}

		//BinaryIntoLinearSearch(min_distance, max_distance, 0.001f, 2.0f, LINEAR_SEARCH_STEP_SIZE, &resulting_distance,
		//	[&](float current_distance) {
		//		return aabb_compare(current_distance, BINARY_SEARCH_EPSILON);
		//	},
		//	[&](float current_distance) {
		//		return aabb_compare(current_distance, LINEAR_SEARCH_EPSILON);
		//	}
		//);
		//resulting_distance += Length(half_extents);
		//return transformed_bounds_center - camera_forward * float3::Splat(resulting_distance);
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, FocusCameraOnObjectViewSpace, const Camera*, const CameraCached*, float3, QuaternionScalar, float3, AABBScalar, float);

	float2 GetCameraFrustumPlaneDimensions(float vertical_fov, float aspect_ratio, float z_depth)
	{
		float tan_half_fov = tan(0.5f * DegToRad(vertical_fov));
		float plane_height = 2.0f * z_depth * tan_half_fov;
		float plane_width = plane_height * aspect_ratio;
		return { plane_width, plane_height };
	}

	void GetCameraFrustumPlaneDimensions(float vertical_fov, float aspect_ratio, Stream<float> z_depth, float2* dimensions)
	{
		float tan_half_fov = tan(0.5f * DegToRad(vertical_fov));
		float factor = 2.0f * tan_half_fov;
		for (size_t index = 0; index < z_depth.size; index++) {
			dimensions[index].x = factor * z_depth[index];
			dimensions[index].y = dimensions[index].x * aspect_ratio;
		}
	}

	template<typename CameraType>
	float2 GetCameraFrustumNearPlaneDimensions(const CameraType* camera) {
		return GetCameraFrustumPlaneDimensions(GetCameraVerticalFOV(camera), GetCameraAspectRatio(camera), GetCameraNearZ(camera));
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float2, GetCameraFrustumNearPlaneDimensions, const Camera*, const CameraCached*);

	template<typename CameraType>
	float2 GetCameraFrustumFarPlaneDimensions(const CameraType* camera) {
		return GetCameraFrustumPlaneDimensions(GetCameraVerticalFOV(camera), GetCameraAspectRatio(camera), GetCameraFarZ(camera));
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float2, GetCameraFrustumFarPlaneDimensions, const Camera*, const CameraCached*);

	template<typename CameraType>
	float4 GetCameraFrustumNearAndFarPlaneDimensions(const CameraType* camera) {
		float2 dimensions[2];
		float z_depths[2];
		z_depths[0] = GetCameraNearZ(camera);
		z_depths[1] = GetCameraFarZ(camera);
		GetCameraFrustumPlaneDimensions(GetCameraVerticalFOV(camera), GetCameraAspectRatio(camera), { z_depths, 2 }, dimensions);

		return { dimensions[0], dimensions[1] };
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float4, GetCameraFrustumNearAndFarPlaneDimensions, const Camera*, const CameraCached*);
		
	template<typename CameraType>
	FrustumPoints GetCameraFrustumPoints(const CameraType* camera) {
		FrustumPoints points;

		float4 plane_dimensions = GetCameraFrustumNearAndFarPlaneDimensions(camera);
		float3 camera_direction = GetCameraForwardVectorScalar(camera);
		float3 camera_right = GetCameraRightVectorScalar(camera);
		float3 camera_up = GetCameraUpVectorScalar(camera);

		float3 near_plane_center = camera->translation + camera_direction * GetCameraNearZ(camera);
		float3 far_plane_center = camera->translation + camera_direction * GetCameraFarZ(camera);

		float2 half_near_plane_dimensions = plane_dimensions.xy() * float2::Splat(0.5f);
		float2 half_far_plane_dimensions = plane_dimensions.zw() * float2::Splat(0.5f);

		float3 near_width_offset = float3::Splat(half_near_plane_dimensions.x) * camera_right;
		float3 near_height_offset = float3::Splat(half_near_plane_dimensions.y) * camera_up;
		points.near_plane = GetRectangle3D(near_plane_center, near_width_offset, near_height_offset);

		float3 far_width_offset = float3::Splat(half_far_plane_dimensions.x) * camera_right;
		float3 far_height_offset = float3::Splat(half_far_plane_dimensions.y) * camera_up;
		points.far_plane = GetRectangle3D(far_plane_center, far_width_offset, far_height_offset);
		
		return points;
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(FrustumPoints, GetCameraFrustumPoints, const Camera*, const CameraCached*);

}