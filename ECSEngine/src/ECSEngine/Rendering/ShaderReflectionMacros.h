#pragma once

// These macros need to be copied from the Macros.hlsli file
#define ECS_REFLECT_INCREMENT_INPUT_SLOT /* each new property will have its input slot equal to its index inside the structure */
#define ECS_REFLECT_FORMAT(format) /* the DXGI_FORMAT that is to be supplied */
#define ECS_REFLECT_SEMANTIC_COUNT(semantic) /* the count for this semantic to be repeated */
#define ECS_REFLECT_INPUT_SLOT(slot) /* the slot of the vertex buffer in the input assembler stage */
#define ECS_REFLECT_INSTANCE(step) /* this makes the data to be considered per instance and it must be supplied the step - usually 1 */

// Vertex buffer mappings - The default search string is the all capital variant of macro
// without the ECS_REFLECT_ e.g. POSITION for position, NORMAL for normal, UV for texture coordinates
#define ECS_REFLECT_POSITION
#define ECS_REFLECT_NORMAL
#define ECS_REFLECT_UV
#define ECS_REFLECT_COLOR
#define ECS_REFLECT_TANGENT
#define ECS_REFLECT_BONE_WEIGHT
#define ECS_REFLECT_BONE_INFLUENCE