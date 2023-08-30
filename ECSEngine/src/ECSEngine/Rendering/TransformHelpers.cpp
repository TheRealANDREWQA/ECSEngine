#include "ecspch.h"
#include "TransformHelpers.h"
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

}