/*
    This file contains macro definitions that can be used as tags for pixel and vertex shader constant buffers
    in order to indicate to the engine certain characteristics to be injected.
*/

// For all of these macros there needs to be specified the byte offset
// At the moment in hlsl there are no variadic macros to make this optional

#define ECS_INJECT_CAMERA_POSITION
#define ECS_INJECT_OBJECT_MATRIX
#define ECS_INJECT_MVP_MATRIX