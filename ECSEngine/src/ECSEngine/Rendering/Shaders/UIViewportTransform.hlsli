float2 TransformVertex(float2 position, float2 region_center_position, float2 region_half_dimensions_inverse)
{
    return float2((position.x - region_center_position.x), position.y + region_center_position.y) * region_half_dimensions_inverse;
}