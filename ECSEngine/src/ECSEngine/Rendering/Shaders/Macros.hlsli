// These macros must be placed at the end of the line i.e. float2 position : Position; ***HERE***
// ECS_REFLECT_FORMAT is obligatory for every field and to be placed as the first modifier
#define ECS_REFLECT_FORMAT(format) /* the DXGI_FORMAT that is to be supplied */
#define ECS_REFLECT_SEMANTIC_COUNT(semantic) /* the count for this semantic to be repeated */
#define ECS_REFLECT_INPUT_SLOT(slot) /* the slot of the vertex buffer in the input assembler stage */
#define ECS_REFLECT_INSTANCE(step) /* this makes the data to be considered per instance and it must be supplied the step - usually 1 */