#include "ecspch.h"
#include "TransformTool.h"
#include "../Rendering/RenderingStructures.h"
#include "../Math/Intersection.h"
#include "Function.h"
#include "../ECS/Components.h"

namespace ECSEngine {

    Component TransformToolComponentID(ECS_TRANSFORM_TOOL tool) {
        switch (tool) {
        case ECS_TRANSFORM_TRANSLATION:
            return Translation::ID();
        case ECS_TRANSFORM_ROTATION:
            return Rotation::ID();
        case ECS_TRANSFORM_SCALE:
            return Scale::ID();
        }

        ECS_ASSERT(false, "Invalid transform tool when getting component id");
        return -1;
    }

    Stream<char> TransformToolComponentName(ECS_TRANSFORM_TOOL tool) {
        switch (tool) {
        case ECS_TRANSFORM_TRANSLATION:
            return STRING(Translation);
        case ECS_TRANSFORM_ROTATION:
            return STRING(Rotation);
        case ECS_TRANSFORM_SCALE:
            return STRING(Scale);
        }

        ECS_ASSERT(false, "Invalid transform tool when getting component name");
        return {};
    }

    float3 AxisDirection(ECS_TRANSFORM_TOOL_AXIS axis)
    {
        switch (axis) {
        case ECS_TRANSFORM_AXIS_X:
            return GetRightVector();
            break;
        case ECS_TRANSFORM_AXIS_Y:
            return GetUpVector();
            break;
        case ECS_TRANSFORM_AXIS_Z:
            return GetForwardVector();
            break;
        default:
            ECS_ASSERT(false, "Invalid axis");
        }

        return float3::Splat(0.0f);
    }

    float3 ECS_VECTORCALL AxisDirectionLocal(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation)
    {
        return RotateVectorQuaternion(rotation, AxisDirectionSIMD(axis));
    }

    Vector8 ECS_VECTORCALL AxisDirectionSIMD(ECS_TRANSFORM_TOOL_AXIS axis)
    {
        switch (axis) {
        case ECS_TRANSFORM_AXIS_X:
            return RightVector();
        case ECS_TRANSFORM_AXIS_Y:
            return UpVector();
        case ECS_TRANSFORM_AXIS_Z:
            return ForwardVector();
        default:
            ECS_ASSERT(false, "Invalid axis");
        }

        return ZeroVector();
    }

    Vector8 ECS_VECTORCALL AxisDirectionLocalSIMD(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation, bool copy_low_to_high)
    {
        if (copy_low_to_high) {
            rotation = rotation.SplatLow();
        }
        return RotateVectorQuaternionSIMD(rotation, AxisDirectionSIMD(axis));
    }

    float3 ECS_VECTORCALL AxisDirection(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation, ECS_TRANSFORM_SPACE space, bool copy_low_to_high)
    {
        if (space == ECS_TRANSFORM_WORLD_SPACE) {
            return AxisDirection(axis);
        }
        else if (space == ECS_TRANSFORM_LOCAL_SPACE) {
            if (copy_low_to_high) {
                rotation = rotation.SplatLow();
            }
            return AxisDirectionLocal(axis, rotation);
        }
        else {
            ECS_ASSERT(false, "Invalid transform space");
            return float3::Splat(0.0f);
        }
    }

    Vector8 ECS_VECTORCALL AxisDirectionSIMD(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation, ECS_TRANSFORM_SPACE space, bool copy_low_to_high)
    {
        if (space == ECS_TRANSFORM_WORLD_SPACE) {
            return AxisDirectionSIMD(axis);
        }
        else if (space == ECS_TRANSFORM_LOCAL_SPACE) {
            return AxisDirectionLocalSIMD(axis, rotation, copy_low_to_high);
        }
        else {
            ECS_ASSERT(false, "Invalid transform space");
            return ZeroVector();
        }
    }

    Plane ECS_VECTORCALL AxisPlane(ECS_TRANSFORM_TOOL_AXIS axis, float3 offset) {
        switch (axis) {
        case ECS_TRANSFORM_AXIS_X:
            return PlaneXY(offset.z);
        case ECS_TRANSFORM_AXIS_Y:
            return PlaneXY(offset.z);
        case ECS_TRANSFORM_AXIS_Z:
            return PlaneXZ(offset.y);
        }

        // Should not reach here
        ECS_ASSERT(false);
        return Plane();
    }

    Plane ECS_VECTORCALL AxisPlane(ECS_TRANSFORM_TOOL_AXIS axis, Vector8 view_direction_normalized, float3 offset) {
        Vector8 view_angle_threshold = DegToRad(10.0f);
        Plane current_plane;
        Plane other_plane;
        switch (axis) {
        case ECS_TRANSFORM_AXIS_X:
        {
            current_plane = PlaneXY(offset.z);
            other_plane = PlaneXZ(offset.y);
        }
        break;
        case ECS_TRANSFORM_AXIS_Y:
        {
            current_plane = PlaneXY(offset.z);
            other_plane = PlaneYZ(offset.x);
        }
        break;
        case ECS_TRANSFORM_AXIS_Z:
        {
            current_plane = PlaneXZ(offset.y);
            other_plane = PlaneYZ(offset.x);
        }
        break;
        default:
            ECS_ASSERT(false, "Invalid axis");
        }

        Vector8 select_mask = PlaneIsParallelAngleMask(current_plane, view_direction_normalized, view_angle_threshold);
        return Select(select_mask, other_plane.normal_dot, current_plane.normal_dot);
    }

    Plane ECS_VECTORCALL AxisPlaneLocal(ECS_TRANSFORM_TOOL_AXIS axis, Quaternion rotation, float3 offset, bool copy_low_to_high)
    {
        if (copy_low_to_high) {
            rotation = rotation.SplatLow();
        }
        return RotateVectorQuaternionSIMD<true>(rotation, AxisPlane(axis, offset).normal_dot);
    }

    Plane ECS_VECTORCALL AxisPlaneLocal(ECS_TRANSFORM_TOOL_AXIS axis, Vector8 view_direction_normalized, Quaternion rotation, float3 offset, bool copy_low_to_high)
    {
        if (copy_low_to_high) {
            rotation = rotation.SplatLow();
        }

        Vector8 view_angle_threshold = DegToRad(10.0f);
        Vector8 vector_offset = { offset, offset };
        
        // Calculate only the first plane
        // Calculate the second plane only if needed
        Plane current_plane;
        switch (axis) {
        case ECS_TRANSFORM_AXIS_X:
        {
            current_plane = PlaneXYRotated(rotation, vector_offset);
        }
        break;
        case ECS_TRANSFORM_AXIS_Y:
        {
            current_plane = PlaneXYRotated(rotation, vector_offset);
        }
        break;
        case ECS_TRANSFORM_AXIS_Z:
        {
            current_plane = PlaneXZRotated(rotation, vector_offset);
        }
        break;
        default:
            ECS_ASSERT(false, "Invalid axis");
        }

        Vector8 select_mask = PlaneIsParallelAngleMask(current_plane, view_direction_normalized, view_angle_threshold);
        if (select_mask.MaskResultNone<3>()) {
            // We can return already since there is no conflict
            return current_plane;
        }

        Plane other_plane;
        switch (axis) {
        case ECS_TRANSFORM_AXIS_X:
        {
            other_plane = PlaneXZRotated(rotation, vector_offset);
        }
        break;
        case ECS_TRANSFORM_AXIS_Y:
        {
            other_plane = PlaneYZRotated(rotation, vector_offset);
        }
        break;
        case ECS_TRANSFORM_AXIS_Z:
        {
            other_plane = PlaneYZRotated(rotation, vector_offset);
        }
        break;
        }
        return Select(select_mask, other_plane.normal_dot, current_plane.normal_dot);
    }

    Plane ECS_VECTORCALL AxisPlane(
        ECS_TRANSFORM_TOOL_AXIS axis, 
        Quaternion rotation, 
        ECS_TRANSFORM_SPACE space, 
        float3 offset, 
        bool copy_low_to_high
    )
    {
        if (space == ECS_TRANSFORM_LOCAL_SPACE) {
            return AxisPlaneLocal(axis, rotation, offset, copy_low_to_high);
        }
        else if (space == ECS_TRANSFORM_WORLD_SPACE) {
            return AxisPlane(axis, offset);
        }
        else {
            ECS_ASSERT(false, "Invalid transform space");
        }

        return Plane();
    }

    Plane ECS_VECTORCALL AxisPlane(
        ECS_TRANSFORM_TOOL_AXIS axis, 
        Vector8 view_direction_normalized, 
        Quaternion rotation, 
        ECS_TRANSFORM_SPACE space, 
        float3 offset, 
        bool copy_low_to_high
    )
    {
        if (space == ECS_TRANSFORM_LOCAL_SPACE) {
            return AxisPlaneLocal(axis, view_direction_normalized, rotation, offset, copy_low_to_high);
        }
        else if (space == ECS_TRANSFORM_WORLD_SPACE) {
            return AxisPlane(axis, view_direction_normalized, offset);
        }
        else {
            ECS_ASSERT(false, "Invalid transform space");
        }
        
        return Plane();
    }

    TransformAxisInfo ECS_VECTORCALL AxisInfo(
        ECS_TRANSFORM_TOOL_AXIS axis, 
        Quaternion rotation, 
        ECS_TRANSFORM_SPACE space, 
        float3 plane_offset, 
        bool copy_low_to_high
    )
    {
        TransformAxisInfo info;

        if (copy_low_to_high) {
            rotation = rotation.SplatLow();
        }
        info.axis_direction = AxisDirection(axis, rotation, space, false);
        info.plane = AxisPlane(axis, rotation, space, plane_offset, false);

        return info;
    }

    TransformAxisInfo ECS_VECTORCALL AxisInfo(
        ECS_TRANSFORM_TOOL_AXIS axis, 
        Quaternion rotation, 
        ECS_TRANSFORM_SPACE space, 
        Vector8 plane_view_direction_normalized, 
        float3 plane_offset, 
        bool copy_low_to_high
    )
    {
        TransformAxisInfo info;

        if (copy_low_to_high) {
            rotation = rotation.SplatLow();
        }
        info.axis_direction = AxisDirectionSIMD(axis, rotation, space, false);
        info.plane = AxisPlane(axis, plane_view_direction_normalized, rotation, space, plane_offset, false);

        return info;
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
        Vector8 eye_space_pos = MatrixVectorMultiplyLow(near_clip_space_position, inverse_matrix);
        eye_space_pos /= PerLaneBroadcast<3>(eye_space_pos);
        Vector8 ray_direction_world = eye_space_pos - camera->translation;
        return Normalize(ray_direction_world).AsFloat3Low();
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, ViewportToWorldRayDirection, const Camera*, const CameraCached*, uint2, int2);

    template<typename CameraType>
    int2 PositionToViewportTexels(const CameraType* camera, uint2 viewport_size, float3 position) {
        Matrix view_projection_matrix = GetCameraViewProjectionMatrix(camera);
        // Transform the point using the view projection matrix
        Vector8 transformed_point = TransformPoint(position, view_projection_matrix);
        // Perform the perspective divide
        transformed_point = PerspectiveDivide(transformed_point);

        // Now we have the NDC position of the point
        float4 ndc_point = transformed_point.AsFloat4Low();
        return NDCToViewportTexels(viewport_size, ndc_point.xy());
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(int2, PositionToViewportTexels, const Camera*, const CameraCached*, uint2, float3);

    template<typename CameraType>
    float3 HandleTranslationToolDelta(
        const CameraType* camera,
        float3 plane_point,
        Quaternion object_rotation,
        ECS_TRANSFORM_TOOL_AXIS axis,
        ECS_TRANSFORM_SPACE space,
        uint2 window_size, 
        int2 mouse_texel_position, 
        int2 mouse_delta
    )
    {
        // Get the difference in the ray position
        if (mouse_delta.x != 0 || mouse_delta.y != 0) {
            float3 previous_ray_direction = ViewportToWorldRayDirection(camera, window_size, mouse_texel_position - mouse_delta);
            float3 current_ray_direction = ViewportToWorldRayDirection(camera, window_size, mouse_texel_position);
            return HandleTranslationToolDelta(camera, plane_point, object_rotation, axis, space, previous_ray_direction, current_ray_direction);
        }
        return float3::Splat(0.0f);
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(
        float3, 
        HandleTranslationToolDelta, 
        const Camera*, 
        const CameraCached*, 
        float3, 
        Quaternion, 
        ECS_TRANSFORM_TOOL_AXIS, 
        ECS_TRANSFORM_SPACE, 
        uint2, 
        int2, 
        int2
    );

    template<typename CameraType>
    float3 HandleTranslationToolDelta(
        const CameraType* camera,
        float3 plane_point,
        Quaternion object_rotation,
        ECS_TRANSFORM_TOOL_AXIS axis,
        ECS_TRANSFORM_SPACE space,
        float3 previous_ray_direction,
        float3 current_ray_direction,
        bool2* success_status
    )
    {
        if (current_ray_direction != previous_ray_direction) {
            Vector8 camera_forward_vector = GetCameraForwardVector(camera);
            TransformAxisInfo axis_info = AxisInfo(axis, object_rotation, space, camera_forward_vector, plane_point);

            // Perform the ray intersection test
            Vector8 ray_start(camera->translation, camera->translation);
            Vector8 ray_direction(previous_ray_direction, current_ray_direction);
            Vector8 output;
            bool2 success = IntersectRayPlane(ray_start, ray_direction, axis_info.plane, output);

            if (success_status != nullptr) {
                *success_status = success;
            }

            if (success.x && success.y) {
                // Project the hit point onto the axis
                Vector8 plane_point_vector = { plane_point, plane_point };
                Vector8 projected_hit_point = ProjectPointOnLineDirection(plane_point_vector, axis_info.axis_direction, output);
                float3 hit_low = output.AsFloat3Low();
                float3 hit_high = output.AsFloat3High();


                // The previous hit point is in the low
                float3 previous_hit_point = projected_hit_point.AsFloat3Low();
                // The current hit point is in the high
                float3 current_hit_point = projected_hit_point.AsFloat3High();

                float3 translation_delta = current_hit_point - previous_hit_point;
                return translation_delta;
            }
        }
        return float3::Splat(0.0f);
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(
        float3, 
        HandleTranslationToolDelta, 
        const Camera*, 
        const CameraCached*, 
        float3,
        Quaternion,
        ECS_TRANSFORM_TOOL_AXIS, 
        ECS_TRANSFORM_SPACE,        
        float3, 
        float3, 
        bool2*
    );

    template<typename CameraType>
    float3 HandleTranslationToolDelta(
        const CameraType* camera, 
        float3 plane_point, 
        Quaternion object_rotation, 
        TranslationToolDrag* drag_tool, 
        float3 current_ray_direction
    )
    {
        bool2 success;
        if (drag_tool->last_successful_direction.x == FLT_MAX || drag_tool->last_successful_direction.y == FLT_MAX ||
            drag_tool->last_successful_direction.z == FLT_MAX) {
            drag_tool->last_successful_direction = current_ray_direction;
        }
        float3 delta = HandleTranslationToolDelta(
            camera, 
            plane_point, 
            object_rotation, 
            drag_tool->axis, 
            drag_tool->space, 
            drag_tool->last_successful_direction, 
            current_ray_direction, 
            &success
        );
        if (success.x && success.y) {
            drag_tool->last_successful_direction = current_ray_direction;
        }
        return delta;
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, HandleTranslationToolDelta, const Camera*, const CameraCached*, float3, Quaternion, TranslationToolDrag*, float3);

    template<typename CameraType>
    float3 HandleRotationToolDelta(
        const CameraType* camera, 
        float3 rotation_center, 
        Quaternion rotation_value,
        RotationToolDrag* drag_tool, 
        uint2 window_size, 
        int2 mouse_texel_position,
        float rotation_factor
    )
    {
        float3 axis_direction = AxisDirection(drag_tool->axis, rotation_value, drag_tool->space);
        if (drag_tool->projected_direction.x == FLT_MAX || drag_tool->projected_direction.y == FLT_MAX) {
            // We need to calculate the projected line
            ChangeLineSpace(&rotation_center, &axis_direction, GetCameraViewProjectionMatrix(camera));
            drag_tool->projected_point = { rotation_center.x, rotation_center.y };
            // We need to negate the y direction

            // We also need to determine on which half the point was grabbed since we need to invert
            // factors if the opposite half was grabbed
            // At the moment this is not yet implemented

            drag_tool->projected_direction = { axis_direction.x, -axis_direction.y };
        }

        // Calculate the distance from the current cursor position to the projected vector
        float distance_to_line = DistanceToLineNormalized(
            Vector8(float4(drag_tool->projected_point.x, drag_tool->projected_point.y, 0.0f, 0.0f)),
            Vector8(float4(drag_tool->projected_direction.x, drag_tool->projected_direction.y, 0.0f, 0.0f)),
            Vector8(float4(mouse_texel_position.x, mouse_texel_position.y, 0.0f, 0.0f))
        ).First();
        // If the distance is not yet assigned, assign now
        if (drag_tool->last_distance == FLT_MAX) {
            drag_tool->last_distance = distance_to_line;
        }

        // This is the order that corresponds with the visual changes
        float delta = drag_tool->last_distance - distance_to_line;
        drag_tool->last_distance = distance_to_line;
        delta *= rotation_factor;
        return float3::Splat(delta) * axis_direction;
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, HandleRotationToolDelta, const Camera*, const CameraCached*, float3, Quaternion, RotationToolDrag*, uint2, int2, float);

    template<typename CameraType>
    QuaternionStorage HandleRotationToolDeltaCircleMapping(
        const CameraType* camera,
        float3 rotation_center,
        Quaternion rotation_value,
        RotationToolDrag* drag_tool,
        uint2 window_size,
        int2 mouse_texel_position,
        float rotation_factor
    ) {
        Vector8 rotation_center_vector = rotation_center;
        if (drag_tool->rotation_center_ndc.x == FLT_MAX) {
            // Transform the center of the rotation into ndc
            Vector8 clip_space_position = TransformPoint(rotation_center_vector, GetCameraViewProjectionMatrix(camera));
            Vector8 ndc_position = ClipSpaceToNDC(clip_space_position);
            ndc_position.StorePartialConstant<2, true>(&drag_tool->rotation_center_ndc);
        }

        // Calculate the direction from the mouse position to the center in ndc
        float2 mouse_ndc = MouseToNDC(window_size, mouse_texel_position);
        // Calculate the normalized direction to the circle center
        float2 center_direction = mouse_ndc - drag_tool->rotation_center_ndc;
        center_direction = Normalize(center_direction);

        if (drag_tool->last_circle_direction.x == FLT_MAX) {
            drag_tool->last_circle_direction = center_direction;
        }

        // Calculate the cosine between the 2 directions using dot
        float cosine = Dot(center_direction, drag_tool->last_circle_direction);
        // Now determine if the rotation is clockwise or anticlockwise in order
        // to invert the value. We shouldn't apply the negation to the cosine
        // since when we do the acosf it will return a value in between [pi/2, pi]
        // and we don't want that
        bool is_clockwise = IsClockwise(center_direction, drag_tool->last_circle_direction);

        drag_tool->last_circle_direction = center_direction;

        // Now determine the angle difference
        float angle_radians = acosf(cosine);
        float angle_degrees = RadToDeg(angle_radians);
        // If the angle values is really small (due to floating point error)
        // clamp it to zero such that it doesn't cause ghosting rotation
        if (angle_degrees >= 0.05f) {
            angle_degrees = is_clockwise ? -angle_degrees : angle_degrees;

            Vector8 axis_direction = AxisDirectionSIMD(drag_tool->axis, rotation_value, drag_tool->space);
            // There is one more thing to take into consideration
            // If we are on different faces of the plane of rotation we need
            // to invert the rotation
            bool is_bellow_plane = !IsAbovePlaneLow(Plane(axis_direction, rotation_center_vector), camera->translation);
            angle_degrees = is_bellow_plane ? -angle_degrees : angle_degrees;

            // Construct a Quaternion from the axis direction with the given rotation
            Quaternion rotation_quaternion = QuaternionAngleFromAxis(axis_direction, angle_degrees * rotation_factor);
            return rotation_quaternion.StorageLow();
        }
        return QuaternionIdentity().StorageLow();
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(QuaternionStorage, HandleRotationToolDeltaCircleMapping, const Camera*, const CameraCached*, float3, Quaternion, RotationToolDrag*, uint2, int2, float);

    template<typename CameraType>
    float3 HandleScaleToolDelta(
        const CameraType* camera,
        float3 scale_center,
        Quaternion rotation_value,
        ScaleToolDrag* drag_tool,
        int2 mouse_texel_delta,
        float scale_factor
    ) {
        // This is a scaling with constant factor
        if (drag_tool->projected_direction_sign.x == FLT_MAX || drag_tool->projected_direction_sign.y == FLT_MAX) {
            float3 axis_direction = AxisDirectionLocal(drag_tool->axis, rotation_value);
            ChangeLineSpace(&scale_center, &axis_direction, GetCameraViewProjectionMatrix(camera));
            // We need to negate the y axis
            drag_tool->projected_direction_sign = BasicTypeSign(float2(axis_direction.x, -axis_direction.y));
        }

        float2 delta = { (float)mouse_texel_delta.x, (float)mouse_texel_delta.y };
        delta *= drag_tool->projected_direction_sign * float2::Splat(scale_factor);
        return float3::Splat(delta.x + delta.y) * AxisDirection(drag_tool->axis);
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, HandleScaleToolDelta, const Camera*, const CameraCached*, float3, Quaternion, ScaleToolDrag*, int2, float);

}