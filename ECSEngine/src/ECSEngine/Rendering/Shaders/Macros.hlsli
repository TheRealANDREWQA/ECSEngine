// These macros must be placed at the end of the line i.e. float2 position : Position; ***HERE***
// ECS_REFLECT_INCREMENT_INPUT_SLOT must be placed after the structure name
// If no format is specified, then it is considered to be the float DXGI format of the correspoding
// type width
// Compound basic types like float2x4 or float4x2 will be automatically assigned the vector semantic count
// e.g. float2x4 will be split into 2 semantic indices

#define ECS_REFLECT_INCREMENT_INPUT_SLOT /* each new property will have its input slot equal to its index inside the structure */
#define ECS_REFLECT_FORMAT(format) /* the DXGI_FORMAT that is to be supplied */
#define ECS_REFLECT_SEMANTIC_COUNT(semantic) /* the count for this semantic to be repeated */
#define ECS_REFLECT_INPUT_SLOT(slot) /* the slot of the vertex buffer in the input assembler stage */
#define ECS_REFLECT_INSTANCE(step) /* this makes the data to be considered per instance and it must be supplied the step - usually 1 */

// Alters DXGI formats
#define ECS_REFLECT_UINT_8 /* Each component is an UINT 8 bits */
#define ECS_REFLECT_SINT_8 /* Each component is an SINT 8 bits */
#define ECS_REFLECT_UINT_16 /* Each component is an UINT 16 bits */
#define ECS_REFLECT_SINT_16 /* Each component is an SINT 16 bits */
#define ECS_REFLECT_UINT_32 /* Each component is an UINT 32 bits */
#define ECS_REFLECT_SINT_32 /* Each component is an SINT 32 bits */
#define ECS_REFLECT_UNORM_8 /* Each component is an UINT 8 bits normalized to the float [0.0f; 1.0f] range */
#define ECS_REFLECT_SNORM_8 /* Each component is an SINT 8 bits normalized to the float [0.0f; 1.0f] range */
#define ECS_REFLECT_UNORM_16 /* Each component is an UINT 16 bits normalized to the float [0.0f; 1.0f] range */
#define ECS_REFLECT_SNORM_16 /* Each component is an SINT 16 bits normalized to the float [0.0f; 1.0f] range */

// Vertex buffer mappings
#define ECS_REFLECT_POSITION
#define ECS_REFLECT_NORMAL
#define ECS_REFLECT_UV
#define ECS_REFLECT_COLOR
#define ECS_REFLECT_TANGENT
#define ECS_REFLECT_BONE_WEIGHT
#define ECS_REFLECT_BONE_INFLUENCE