// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/Hashing.h"
#include "../Math/AABB.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Utilities/StringUtilities.h"
#include "ColorUtilities.h"
#include "../Utilities/Utilities.h"

#ifdef ECSENGINE_PLATFORM_WINDOWS
#include <d3d11.h>
#endif

#define ECS_GRAPHICS_BUFFERS(function) /* Useful for macro expansion */ function(VertexBuffer); \
function(IndexBuffer); \
function(ConstantBuffer); \
function(StandardBuffer); \
function(StructuredBuffer); \
function(UABuffer); 

#define ECS_GRAPHICS_TEXTURES(function) /* Useful for macro expansion */ function(Texture1D); \
function(Texture2D); \
function(Texture3D); \
function(TextureCube);

#define ECS_GRAPHICS_TEXTURES_NO_CUBE(function) /* Useful for macro expansion */ function(Texture1D); \
function(Texture2D); \
function(Texture3D);

#define ECS_GRAPHICS_RESOURCES(function) /* Useful for macro expansion */ ECS_GRAPHICS_TEXTURES(function); \
ECS_GRAPHICS_BUFFERS(function); 

#define ECS_GRAPHICS_VIEWS(function) function(ResourceView); \
function(RenderTargetView); \
function(DepthStencilView); \
function(UAView);

#define ECS_MATERIAL_VERTEX_CONSTANT_BUFFER_COUNT 4
#define ECS_MATERIAL_PIXEL_CONSTANT_BUFFER_COUNT 6
#define ECS_MATERIAL_VERTEX_TEXTURES_COUNT 2
#define ECS_MATERIAL_PIXEL_TEXTURES_COUNT 8
#define ECS_MATERIAL_UAVIEW_COUNT 4
#define ECS_MATERIAL_TAG_STORAGE_CAPACITY 256
#define ECS_MATERIAL_VERTEX_TAG_COUNT 4
#define ECS_MATERIAL_PIXEL_TAG_COUNT 4

namespace ECSEngine {

	enum ECS_REFLECT ECS_MESH_INDEX : unsigned char {
		ECS_MESH_POSITION,
		ECS_MESH_NORMAL,
		ECS_MESH_UV,
		ECS_MESH_COLOR,
		ECS_MESH_BONE_WEIGHT,
		ECS_MESH_BONE_INFLUENCE,
		ECS_MESH_BUFFER_COUNT
	};

	enum ECS_REFLECT ECS_SHADER_TYPE : unsigned char {
		ECS_SHADER_VERTEX,
		ECS_SHADER_PIXEL,
		ECS_SHADER_DOMAIN,
		ECS_SHADER_HULL,
		ECS_SHADER_GEOMETRY,
		ECS_SHADER_COMPUTE,
		ECS_SHADER_TYPE_COUNT
	};

#define ECS_SHADER_EXTENSION L".hlsl"
#define ECS_SHADER_INCLUDE_EXTENSION L".hlsli"

	// At the moment, map the values directly to their DXGI equivalents for "fast" retrieval
	enum ECS_GRAPHICS_FORMAT {
		ECS_GRAPHICS_FORMAT_UNKNOWN = DXGI_FORMAT_UNKNOWN,

		// -------------- Int formats --------------------
		ECS_GRAPHICS_FORMAT_R8_UINT = DXGI_FORMAT_R8_UINT,
		ECS_GRAPHICS_FORMAT_RG8_UINT = DXGI_FORMAT_R8G8_UINT,
		ECS_GRAPHICS_FORMAT_RGBA8_UINT = DXGI_FORMAT_R8G8B8A8_UINT,
		ECS_GRAPHICS_FORMAT_R16_UINT = DXGI_FORMAT_R16_UINT,
		ECS_GRAPHICS_FORMAT_RG16_UINT = DXGI_FORMAT_R16G16_UINT,
		ECS_GRAPHICS_FORMAT_RGBA16_UINT = DXGI_FORMAT_R16G16B16A16_UINT,
		ECS_GRAPHICS_FORMAT_R32_UINT = DXGI_FORMAT_R32_UINT,
		ECS_GRAPHICS_FORMAT_RG32_UINT = DXGI_FORMAT_R32G32_UINT,
		ECS_GRAPHICS_FORMAT_RGB32_UINT = DXGI_FORMAT_R32G32B32_UINT,
		ECS_GRAPHICS_FORMAT_RGBA32_UINT = DXGI_FORMAT_R32G32B32A32_UINT,

		ECS_GRAPHICS_FORMAT_R8_SINT = DXGI_FORMAT_R8_SINT,
		ECS_GRAPHICS_FORMAT_RG8_SINT = DXGI_FORMAT_R8G8_SINT,
		ECS_GRAPHICS_FORMAT_RGBA8_SINT = DXGI_FORMAT_R8G8B8A8_SINT,
		ECS_GRAPHICS_FORMAT_R16_SINT = DXGI_FORMAT_R16_SINT,
		ECS_GRAPHICS_FORMAT_RG16_SINT = DXGI_FORMAT_R16G16_SINT,
		ECS_GRAPHICS_FORMAT_RGBA16_SINT = DXGI_FORMAT_R16G16B16A16_SINT,
		ECS_GRAPHICS_FORMAT_R32_SINT = DXGI_FORMAT_R32_SINT,
		ECS_GRAPHICS_FORMAT_RG32_SINT = DXGI_FORMAT_R32G32_SINT,
		ECS_GRAPHICS_FORMAT_RGB32_SINT = DXGI_FORMAT_R32G32B32_SINT,
		ECS_GRAPHICS_FORMAT_RGBA32_SINT = DXGI_FORMAT_R32G32B32A32_SINT,

		ECS_GRAPHICS_FORMAT_R8_UNORM = DXGI_FORMAT_R8_UNORM,
		ECS_GRAPHICS_FORMAT_RG8_UNORM = DXGI_FORMAT_R8G8_UNORM,
		ECS_GRAPHICS_FORMAT_RGBA8_UNORM = DXGI_FORMAT_R8G8B8A8_UNORM,
		ECS_GRAPHICS_FORMAT_R16_UNORM = DXGI_FORMAT_R16_UNORM,
		ECS_GRAPHICS_FORMAT_RG16_UNORM = DXGI_FORMAT_R16G16_UNORM,
		ECS_GRAPHICS_FORMAT_RGBA16_UNORM = DXGI_FORMAT_R16G16B16A16_UNORM,
		ECS_GRAPHICS_FORMAT_R24G8_UNORM = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,

		ECS_GRAPHICS_FORMAT_R8_SNORM = DXGI_FORMAT_R8_SNORM,
		ECS_GRAPHICS_FORMAT_RG8_SNORM = DXGI_FORMAT_R8G8_SNORM,
		ECS_GRAPHICS_FORMAT_RGBA8_SNORM = DXGI_FORMAT_R8G8B8A8_SNORM,
		ECS_GRAPHICS_FORMAT_R16_SNORM = DXGI_FORMAT_R16_SNORM,
		ECS_GRAPHICS_FORMAT_RG16_SNORM = DXGI_FORMAT_R16G16_SNORM,
		ECS_GRAPHICS_FORMAT_RGBA16_SNORM = DXGI_FORMAT_R16G16B16A16_SNORM,

		ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,

		// -------------- Int formats --------------------

		// ------------- Float formats -------------------
		
		ECS_GRAPHICS_FORMAT_R16_FLOAT = DXGI_FORMAT_R16_FLOAT,
		ECS_GRAPHICS_FORMAT_RG16_FLOAT = DXGI_FORMAT_R16G16_FLOAT,
		ECS_GRAPHICS_FORMAT_RGBA16_FLOAT = DXGI_FORMAT_R16G16B16A16_FLOAT,
		ECS_GRAPHICS_FORMAT_R32_FLOAT = DXGI_FORMAT_R32_FLOAT,
		ECS_GRAPHICS_FORMAT_RG32_FLOAT = DXGI_FORMAT_R32G32_FLOAT,
		ECS_GRAPHICS_FORMAT_RGB32_FLOAT = DXGI_FORMAT_R32G32B32_FLOAT,
		ECS_GRAPHICS_FORMAT_RGBA32_FLOAT = DXGI_FORMAT_R32G32B32A32_FLOAT,
		ECS_GRAPHICS_FORMAT_R11G11B10_FLOAT = DXGI_FORMAT_R11G11B10_FLOAT,

		// ------------- Float formats --------------------

		// ------------- Depth formats --------------------

		ECS_GRAPHICS_FORMAT_D16_UNORM = DXGI_FORMAT_D16_UNORM,
		ECS_GRAPHICS_FORMAT_D24_UNORM_S8_UINT = DXGI_FORMAT_D24_UNORM_S8_UINT,
		ECS_GRAPHICS_FORMAT_D32_FLOAT = DXGI_FORMAT_D32_FLOAT,

		// ------------- Depth formats --------------------

		// -------- Block compressed formats --------------

		ECS_GRAPHICS_FORMAT_BC1 = DXGI_FORMAT_BC1_UNORM,
		ECS_GRAPHICS_FORMAT_BC1_SRGB = DXGI_FORMAT_BC1_UNORM_SRGB,
		ECS_GRAPHICS_FORMAT_BC3 = DXGI_FORMAT_BC3_UNORM,
		ECS_GRAPHICS_FORMAT_BC3_SRGB = DXGI_FORMAT_BC3_UNORM_SRGB,
		ECS_GRAPHICS_FORMAT_BC4 = DXGI_FORMAT_BC4_UNORM,
		ECS_GRAPHICS_FORMAT_BC5 = DXGI_FORMAT_BC5_UNORM,
		ECS_GRAPHICS_FORMAT_BC6 = DXGI_FORMAT_BC6H_UF16,
		ECS_GRAPHICS_FORMAT_BC7 = DXGI_FORMAT_BC7_UNORM,
		ECS_GRAPHICS_FORMAT_BC7_SRGB = DXGI_FORMAT_BC7_UNORM_SRGB,

		// -------- Block compressed formats --------------

		// ----------- Typeless formats -------------------

		ECS_GRAPHICS_FORMAT_R8_TYPELESS = DXGI_FORMAT_R8_TYPELESS,
		ECS_GRAPHICS_FORMAT_RG8_TYPELESS = DXGI_FORMAT_R8G8_TYPELESS,
		ECS_GRAPHICS_FORMAT_RGBA8_TYPELESS = DXGI_FORMAT_R8G8B8A8_TYPELESS,
		ECS_GRAPHICS_FORMAT_R16_TYPELESS = DXGI_FORMAT_R16_TYPELESS,
		ECS_GRAPHICS_FORMAT_RG16_TYPELESS = DXGI_FORMAT_R16G16_TYPELESS,
		ECS_GRAPHICS_FORMAT_RGBA16_TYPELESS = DXGI_FORMAT_R16G16B16A16_TYPELESS,
		ECS_GRAPHICS_FORMAT_R24G8_TYPELESS = DXGI_FORMAT_R24G8_TYPELESS,
		ECS_GRAPHICS_FORMAT_R32_TYPELESS = DXGI_FORMAT_R32_TYPELESS,
		ECS_GRAPHICS_FORMAT_RG32_TYPELESS = DXGI_FORMAT_R32G32_TYPELESS,
		ECS_GRAPHICS_FORMAT_RGB32_TYPELESS = DXGI_FORMAT_R32G32B32_TYPELESS,
		ECS_GRAPHICS_FORMAT_RGBA32_TYPELESS = DXGI_FORMAT_R32G32B32A32_TYPELESS
		
		// ----------- Typeless formats -------------------
	};

	// At the moment map the values directly for "fast" retrieval
	enum ECS_GRAPHICS_BIND_TYPE {
		ECS_GRAPHICS_BIND_NONE = 0,
		ECS_GRAPHICS_BIND_VERTEX_BUFFER = D3D11_BIND_VERTEX_BUFFER,
		ECS_GRAPHICS_BIND_INDEX_BUFFER  = D3D11_BIND_INDEX_BUFFER,
		ECS_GRAPHICS_BIND_CONSTANT_BUFFER = D3D11_BIND_CONSTANT_BUFFER,
		ECS_GRAPHICS_BIND_SHADER_RESOURCE = D3D11_BIND_SHADER_RESOURCE,
		ECS_GRAPHICS_BIND_STREAM_OUTPUT = D3D11_BIND_STREAM_OUTPUT,
		ECS_GRAPHICS_BIND_RENDER_TARGET = D3D11_BIND_RENDER_TARGET,
		ECS_GRAPHICS_BIND_DEPTH_STENCIL = D3D11_BIND_DEPTH_STENCIL,
		ECS_GRAPHICS_BIND_UNORDERED_ACCESS = D3D11_BIND_UNORDERED_ACCESS
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_GRAPHICS_BIND_TYPE);

	// At the moment map the values directly for "fast" retrieval
	enum ECS_GRAPHICS_CPU_ACCESS {
		ECS_GRAPHICS_CPU_ACCESS_NONE = 0,
		ECS_GRAPHICS_CPU_ACCESS_READ = D3D11_CPU_ACCESS_READ,
		ECS_GRAPHICS_CPU_ACCESS_WRITE = D3D11_CPU_ACCESS_WRITE,
		ECS_GRAPHICS_CPU_ACCESS_READ_WRITE = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE
	};

	// At the moment map the values directly for "fast" retrieval
	enum ECS_GRAPHICS_MISC_FLAGS {
		ECS_GRAPHICS_MISC_NONE = 0,
		ECS_GRAPHICS_MISC_GENERATE_MIPS = D3D11_RESOURCE_MISC_GENERATE_MIPS,
		ECS_GRAPHICS_MISC_SHARED = D3D11_RESOURCE_MISC_SHARED,
		ECS_GRAPHICS_MISC_TEXTURECUBE = D3D11_RESOURCE_MISC_TEXTURECUBE,
		ECS_GRAPHICS_MISC_INDIRECT_BUFFER = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS,
		ECS_GRAPHICS_MISC_BYTE_BUFFER = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS,
		ECS_GRAPHICS_MISC_STRUCTURED_BUFFER = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
		ECS_GRAPHICS_MISC_VIRTUAL_TEXTURE = D3D11_RESOURCE_MISC_RESOURCE_CLAMP,
		ECS_GRAPHICS_MISC_TILE_POOL = D3D11_RESOURCE_MISC_TILE_POOL,
		ECS_GRAPHICS_MISC_TILED_RESOURCE = D3D11_RESOURCE_MISC_TILED
	};
	
	ECS_ENUM_BITWISE_OPERATIONS(ECS_GRAPHICS_MISC_FLAGS);

	// At the moment map the values directly for "fast" retrieval
	enum ECS_GRAPHICS_USAGE {
		ECS_GRAPHICS_USAGE_IMMUTABLE = D3D11_USAGE_IMMUTABLE,
		ECS_GRAPHICS_USAGE_DEFAULT = D3D11_USAGE_DEFAULT,
		ECS_GRAPHICS_USAGE_DYNAMIC = D3D11_USAGE_DYNAMIC,
		ECS_GRAPHICS_USAGE_STAGING = D3D11_USAGE_STAGING,
		// All the others go to 3
		ECS_GRAPHICS_USAGE_NONE = 4
	};

	enum ECS_GRAPHICS_MAP_TYPE {
		ECS_GRAPHICS_MAP_READ = D3D11_MAP_READ,
		ECS_GRAPHICS_MAP_WRITE = D3D11_MAP_WRITE,
		ECS_GRAPHICS_MAP_WRITE_DISCARD = D3D11_MAP_WRITE_DISCARD,
		ECS_GRAPHICS_MAP_WRITE_NO_OVERWRITE = D3D11_MAP_WRITE_NO_OVERWRITE,
		ECS_GRAPHICS_MAP_READ_WRITE = D3D11_MAP_READ_WRITE
	};

	ECS_INLINE DXGI_FORMAT GetGraphicsNativeFormat(ECS_GRAPHICS_FORMAT format) {
		// Can return as is for the moment
		return (DXGI_FORMAT)format;
	}

	ECS_INLINE ECS_GRAPHICS_FORMAT GetGraphicsFormatFromNative(DXGI_FORMAT format) {
		return (ECS_GRAPHICS_FORMAT)format;
	}

	ECS_INLINE D3D11_BIND_FLAG GetGraphicsNativeBind(ECS_GRAPHICS_BIND_TYPE bind_type) {
		// Can return as is for the moment
		return (D3D11_BIND_FLAG)bind_type;
	}

	ECS_INLINE ECS_GRAPHICS_BIND_TYPE GetGraphicsBindFromNative(D3D11_BIND_FLAG bind_type) {
		return (ECS_GRAPHICS_BIND_TYPE)bind_type;
	}

	ECS_INLINE ECS_GRAPHICS_BIND_TYPE GetGraphicsBindFromNative(int bind_type) {
		return (ECS_GRAPHICS_BIND_TYPE)bind_type;
	}

	ECS_INLINE D3D11_CPU_ACCESS_FLAG GetGraphicsNativeCPUAccess(ECS_GRAPHICS_CPU_ACCESS cpu_access) {
		return (D3D11_CPU_ACCESS_FLAG)cpu_access;
	}

	ECS_INLINE ECS_GRAPHICS_CPU_ACCESS GetGraphicsCPUAccessFromNative(unsigned int cpu_flag) {
		return (ECS_GRAPHICS_CPU_ACCESS)cpu_flag;
	}

	ECS_INLINE D3D11_USAGE GetGraphicsNativeUsage(ECS_GRAPHICS_USAGE usage) {
		return (D3D11_USAGE)usage;
	}

	ECS_INLINE ECS_GRAPHICS_USAGE GetGraphicsUsageFromNative(D3D11_USAGE usage) {
		return (ECS_GRAPHICS_USAGE)usage;
	}

	ECS_INLINE D3D11_MAP GetGraphicsNativeMapType(ECS_GRAPHICS_MAP_TYPE map_type) {
		return (D3D11_MAP)map_type;
	}

	ECS_INLINE ECS_GRAPHICS_MAP_TYPE GetGraphicsMapTypeFromNative(D3D11_MAP map_type) {
		return (ECS_GRAPHICS_MAP_TYPE)map_type;
	}

	ECS_INLINE unsigned int GetGraphicsNativeMiscFlags(ECS_GRAPHICS_MISC_FLAGS flags) {
		return (unsigned int)flags;
	}

	ECS_INLINE ECS_GRAPHICS_MISC_FLAGS GetGraphicsMiscFlagsFromNative(unsigned int flags) {
		return (ECS_GRAPHICS_MISC_FLAGS)flags;
	}

	ECSENGINE_API void SetDepthStencilDescOP(
		D3D11_DEPTH_STENCIL_DESC* descriptor,
		bool front_face,
		D3D11_COMPARISON_FUNC comparison_func,
		D3D11_STENCIL_OP fail_op,
		D3D11_STENCIL_OP depth_fail_op,
		D3D11_STENCIL_OP pass_op
	);

	// It doesn't work for BC, depth or typeless formats - it will simply fill in DBL_MAX
	ECSENGINE_API void ExtractPixelFromGraphicsFormat(ECS_GRAPHICS_FORMAT format, size_t count, const void* pixels, double4* values);

	ECSENGINE_API bool IsGraphicsFormatUINT(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API bool IsGraphicsFormatSINT(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API bool IsGraphicsFormatUNORM(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API bool IsGraphicsFormatSNORM(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API bool IsGraphicsFormatFloat(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API bool IsGraphicsFormatDepth(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API bool IsGraphicsFormatTypeless(ECS_GRAPHICS_FORMAT format);

	// Includes SRGB types as well
	ECSENGINE_API bool IsGraphicsFormatBC(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API bool IsGraphicsFormatSRGB(ECS_GRAPHICS_FORMAT format);

	// Returns the byte size of an element of the given type (for position is sizeof(float3))
	ECSENGINE_API size_t GetMeshIndexElementByteSize(ECS_MESH_INDEX type);

	// If the format is an srgb one, it will make it non srgb
	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatNoSRGB(ECS_GRAPHICS_FORMAT format);
	
	// If the format can have an srgb format, it will make it srgb
	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatWithSRGB(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToUNORM(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToSNORM(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToUINT(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToSINT(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToFloat(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToDepth(ECS_GRAPHICS_FORMAT format);

	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatToTypeless(ECS_GRAPHICS_FORMAT format);

	// Returns true for all formats that can be rendered from a shader as normalized float
	// Can optionally choose to include normal 16 and 32 bit float values into consideration as well
	ECSENGINE_API bool IsGraphicsFormatFloatCompatible(ECS_GRAPHICS_FORMAT format, bool include_unormalized_float = true);

	// Ignores depth and BC textures
	ECSENGINE_API bool IsGraphicsFormatSingleChannel(ECS_GRAPHICS_FORMAT format);

	// Ignores depth and BC textures
	ECSENGINE_API bool IsGraphicsFormatDoubleChannel(ECS_GRAPHICS_FORMAT format);

	// Ignores depth and BC textures
	ECSENGINE_API bool IsGraphicsFormatTripleChannel(ECS_GRAPHICS_FORMAT format);

	// Ignores depth and BC textures
	ECSENGINE_API bool IsGraphicsFormatQuadrupleChannel(ECS_GRAPHICS_FORMAT format);

	// Returns the number of channels the format has
	ECSENGINE_API unsigned int GetGraphicsFormatChannelCount(ECS_GRAPHICS_FORMAT format);

	// For each format (it ignores BC and depth textures) it will return the equivalent format (if there is one with the
	// given count) but with a different channel count
	ECSENGINE_API ECS_GRAPHICS_FORMAT GetGraphicsFormatChannelCount(ECS_GRAPHICS_FORMAT format, unsigned int new_count);

	// It must be a depth format and returns a suitable rendering format to be used in a ResourceView
	// For formats other than depth it will return it again
	ECSENGINE_API ECS_GRAPHICS_FORMAT ConvertDepthToRenderFormat(ECS_GRAPHICS_FORMAT format);

	// It must be a depth format and return the equivalent typeless format
	// For formats other than depth it will return it again
	ECSENGINE_API ECS_GRAPHICS_FORMAT ConvertDepthToTypelessFormat(ECS_GRAPHICS_FORMAT format);

	enum ECS_REFLECT ECS_SAMPLER_FILTER_TYPE : unsigned char {
		ECS_SAMPLER_FILTER_POINT,
		ECS_SAMPLER_FILTER_LINEAR,
		ECS_SAMPLER_FILTER_ANISOTROPIC
	};

	ECSENGINE_API D3D11_FILTER GetGraphicsNativeFilter(ECS_SAMPLER_FILTER_TYPE filter);

	ECSENGINE_API ECS_SAMPLER_FILTER_TYPE GetGraphicsFilterFromNative(D3D11_FILTER filter);

	// The way wrap around is handler
	enum ECS_REFLECT ECS_SAMPLER_ADDRESS_TYPE : unsigned char {
		ECS_SAMPLER_ADDRESS_WRAP,
		ECS_SAMPLER_ADDRESS_MIRROR,
		ECS_SAMPLER_ADDRESS_CLAMP,
		ECS_SAMPLER_ADDRESS_BORDER
	};

	ECSENGINE_API D3D11_TEXTURE_ADDRESS_MODE GetGraphicsNativeAddressMode(ECS_SAMPLER_ADDRESS_TYPE address_type);

	ECSENGINE_API ECS_SAMPLER_ADDRESS_TYPE GetGraphicsAddressModeFromNative(D3D11_TEXTURE_ADDRESS_MODE address_mode);

	enum ECS_BLEND_FACTOR : unsigned char {
		ECS_BLEND_ZERO,
		ECS_BLEND_ONE,
		ECS_BLEND_SRC_COLOR,
		ECS_BLEND_INV_SRC_COLOR,
		ECS_BLEND_SRC_ALPHA,
		ECS_BLEND_INV_SRC_ALPHA,
		ECS_BLEND_DEST_COLOR,
		ECS_BLEND_INV_DEST_COLOR,
		ECS_BLEND_DEST_ALPHA,
		ECS_BLEND_INV_DEST_ALPHA
	};

	ECSENGINE_API D3D11_BLEND GetGraphicsNativeBlendFactor(ECS_BLEND_FACTOR blend_factor);

	ECSENGINE_API ECS_BLEND_FACTOR GetGraphicsBlendFactorFromNative(D3D11_BLEND blend_factor);

	enum ECS_BLEND_COLOR_CHANNEL : unsigned char {
		ECS_BLEND_COLOR_CHANNEL_RED = D3D11_COLOR_WRITE_ENABLE_RED,
		ECS_BLEND_COLOR_CHANNEL_GREEN = D3D11_COLOR_WRITE_ENABLE_GREEN,
		ECS_BLEND_COLOR_CHANNEL_BLUE = D3D11_COLOR_WRITE_ENABLE_BLUE,
		ECS_BLEND_COLOR_CHANNEL_ALPHA = D3D11_COLOR_WRITE_ENABLE_ALPHA,
		ECS_BLEND_COLOR_CHANNEL_ALL = D3D11_COLOR_WRITE_ENABLE_ALL
	};

	ECS_INLINE unsigned char GetGraphicsNativeBlendColorChannel(ECS_BLEND_COLOR_CHANNEL channel) {
		return channel;
	}

	ECS_INLINE ECS_BLEND_COLOR_CHANNEL GetGraphicsBlendColorChannelFromNative(unsigned char channel) {
		return (ECS_BLEND_COLOR_CHANNEL)channel;
	}

	enum ECS_BLEND_OP : unsigned char {
		ECS_BLEND_OP_ADD,
		ECS_BLEND_OP_SUBTRACT,			// Subtract source 1 from source 2
		ECS_BLEND_OP_INVERTED_SUBTRACT, // Subtracts source 2 from source 1
		ECS_BLEND_OP_MIN,
		ECS_BLEND_OP_MAX
	};

	ECSENGINE_API D3D11_BLEND_OP GetGraphicsNativeBlendOp(ECS_BLEND_OP blend_op);

	ECSENGINE_API ECS_BLEND_OP GetGraphicsBlendOpFromNative(D3D11_BLEND_OP blend_op);

	enum ECS_COMPARISON_OP : unsigned char {
		ECS_COMPARISON_NEVER = D3D11_COMPARISON_NEVER,
		ECS_COMPARISON_LESS = D3D11_COMPARISON_LESS,
		ECS_COMPARISON_EQUAL = D3D11_COMPARISON_EQUAL,
		ECS_COMPARISON_LESS_EQUAL = D3D11_COMPARISON_LESS_EQUAL,
		ECS_COMPARISON_GREATER = D3D11_COMPARISON_GREATER,
		ECS_COMPARISON_NOT_EQUAL = D3D11_COMPARISON_NOT_EQUAL,
		ECS_COMPARISON_GREATER_EQUAL = D3D11_COMPARISON_GREATER_EQUAL,
		ECS_COMPARISON_ALWAYS = D3D11_COMPARISON_ALWAYS
	};

	ECS_INLINE D3D11_COMPARISON_FUNC GetGraphicsNativeComparisonOp(ECS_COMPARISON_OP op) {
		return (D3D11_COMPARISON_FUNC)op;
	}

	ECS_INLINE ECS_COMPARISON_OP GetGraphicsComparisonOpFromNative(D3D11_COMPARISON_FUNC op) {
		return (ECS_COMPARISON_OP)op;
	}

	enum ECS_STENCIL_OP : unsigned char {
		ECS_STENCIL_OP_KEEP = D3D11_STENCIL_OP_KEEP,
		ECS_STENCIL_OP_ZERO = D3D11_STENCIL_OP_ZERO,
		ECS_STENCIL_OP_REPLACE = D3D11_STENCIL_OP_REPLACE,
		ECS_STENCIL_OP_INCR_SAT = D3D11_STENCIL_OP_INCR_SAT,
		ECS_STENCIL_OP_DECR_SAT = D3D11_STENCIL_OP_DECR_SAT,
		ECS_STENCIL_OP_INVERT = D3D11_STENCIL_OP_INVERT,
		ECS_STENCIL_OP_INCR = D3D11_STENCIL_OP_INCR,
		ECS_STENCIL_OP_DECR = D3D11_STENCIL_OP_DECR
	};

	ECS_INLINE D3D11_STENCIL_OP GetGraphicsNativeStencilOp(ECS_STENCIL_OP op) {
		return (D3D11_STENCIL_OP)op;
	}

	ECS_INLINE ECS_STENCIL_OP GetGraphicsStencilOpFromNative(D3D11_STENCIL_OP op) {
		return (ECS_STENCIL_OP)op;
	}

	enum ECS_CULL_MODE : unsigned char {
		ECS_CULL_NONE = D3D11_CULL_NONE,
		ECS_CULL_FRONT = D3D11_CULL_FRONT,
		ECS_CULL_BACK = D3D11_CULL_BACK
	};

	ECS_INLINE D3D11_CULL_MODE GetGraphicsNativeCullMode(ECS_CULL_MODE mode) {
		return (D3D11_CULL_MODE)mode;
	}

	ECS_INLINE ECS_CULL_MODE GetGraphicsCullModeFromNative(D3D11_CULL_MODE mode) {
		return (ECS_CULL_MODE)mode;
	}

	struct ECS_REFLECT ShaderMacro {
		ECS_INLINE bool Compare(ShaderMacro other) const {
			return name == other.name && definition == other.definition;
		}

		ECS_INLINE ShaderMacro Copy(AllocatorPolymorphic allocator) const {
			return { StringCopy(allocator, name), StringCopy(allocator, definition) };
		}

		Stream<char> name;
		Stream<char> definition;
	};

	enum ECS_SHADER_TARGET : unsigned char {
		ECS_SHADER_TARGET_5_0,
		ECS_SHADER_TARGET_COUNT
	};

	enum ECS_REFLECT ECS_SHADER_COMPILE_FLAGS : unsigned char {
		ECS_SHADER_COMPILE_NONE,
		ECS_SHADER_COMPILE_DEBUG,
		ECS_SHADER_COMPILE_OPTIMIZATION_LOWEST,
		ECS_SHADER_COMPILE_OPTIMIZATION_LOW,
		ECS_SHADER_COMPILE_OPTIMIZATION_HIGH,
		ECS_SHADER_COMPILE_OPTIMIZATION_HIGHEST
	};

	enum TextureCubeFace {
		ECS_TEXTURE_CUBE_X_POS,
		ECS_TEXTURE_CUBE_X_NEG,
		ECS_TEXTURE_CUBE_Y_POS,
		ECS_TEXTURE_CUBE_Y_NEG,
		ECS_TEXTURE_CUBE_Z_POS,
		ECS_TEXTURE_CUBE_Z_NEG
	};

	// Default is no macros, shader target 5 and no compile flags
	struct ShaderCompileOptions {
		Stream<ShaderMacro> macros = { nullptr, 0 };
		ECS_SHADER_TARGET target = ECS_SHADER_TARGET_5_0;
		ECS_SHADER_COMPILE_FLAGS compile_flags = ECS_SHADER_COMPILE_NONE;
	};
	
	typedef ID3D11DeviceContext GraphicsContext;
	typedef ID3D11Device GraphicsDevice;

	template<typename GraphicsStructure>
	ECS_INLINE GraphicsDevice* GetDevice(GraphicsStructure structure) {
		GraphicsDevice* device;
		auto interface_ptr = structure.Interface();
		interface_ptr->GetDevice(&device);
		device->Release();
		return device;
	}

	// Releases the target resource as well
	// This function does not take into account the graphics object bookkeeping
	template<typename View>
	ECS_INLINE void ReleaseGraphicsView(View view) {
		ID3D11Resource* resource = view.GetResource();
		view.Release();
		resource->Release();
	}

	// Row byte size is valid only for Texture2D and 3D
	// Slice byte size is valid only for 3D
	struct MappedTexture {
		void* data;
		unsigned int row_byte_size;
		unsigned int slice_byte_size;
	};

	// Default arguments all but width; initial_data can be set to fill the texture
	// The initial data is a stream of Stream<void> for each mip map data
	struct Texture1DDescriptor {
		unsigned int width;
		unsigned int array_size = 1u;
		unsigned int mip_levels = 0u;
		Stream<Stream<void>> mip_data = { nullptr, 0 };
		ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DEFAULT;
		ECS_GRAPHICS_CPU_ACCESS cpu_flag = ECS_GRAPHICS_CPU_ACCESS_NONE;
		ECS_GRAPHICS_BIND_TYPE bind_flag = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		ECS_GRAPHICS_MISC_FLAGS misc_flag = ECS_GRAPHICS_MISC_NONE;
	};

	// Size must always be initialized;
	// The initial data is a stream of Stream<void> for each mip map data
	struct Texture2DDescriptor {
		uint2 size;
		unsigned int array_size = 1u;
		unsigned int mip_levels = 0u;
		unsigned int sample_count = 1u;
		unsigned int sampler_quality = 0u;

		Stream<Stream<void>> mip_data = { nullptr, 0 };
		ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DEFAULT;
		ECS_GRAPHICS_CPU_ACCESS cpu_flag = ECS_GRAPHICS_CPU_ACCESS_NONE;
		ECS_GRAPHICS_BIND_TYPE bind_flag = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		ECS_GRAPHICS_MISC_FLAGS misc_flag = ECS_GRAPHICS_MISC_NONE;
	};

	// Size must be set
	// The initial data is a stream of Stream<void> for each mip map data
	struct Texture3DDescriptor {
		uint3 size;
		unsigned int mip_levels = 0u;

		Stream<Stream<void>> mip_data = { nullptr, 0 };
		ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DEFAULT;
		ECS_GRAPHICS_CPU_ACCESS cpu_flag = ECS_GRAPHICS_CPU_ACCESS_NONE;
		ECS_GRAPHICS_BIND_TYPE bind_flag = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		ECS_GRAPHICS_MISC_FLAGS misc_flag = ECS_GRAPHICS_MISC_NONE;
	};

	// The initial data is a stream of Stream<void> for each mip map data
	struct TextureCubeDescriptor {
		uint2 size;
		unsigned int mip_levels = 0u;

		Stream<Stream<void>> mip_data = { nullptr, 0 };
		ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DEFAULT;
		ECS_GRAPHICS_CPU_ACCESS cpu_flag = ECS_GRAPHICS_CPU_ACCESS_NONE;
		ECS_GRAPHICS_BIND_TYPE bind_flag = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		ECS_GRAPHICS_MISC_FLAGS misc_flag = ECS_GRAPHICS_MISC_NONE;
	};

	struct SamplerDescriptor {
		ECS_INLINE void SetAddressType(ECS_SAMPLER_ADDRESS_TYPE type) {
			address_type_u = type;
			address_type_v = type;
			address_type_w = type;
		}

		ECS_INLINE unsigned int Hash() const {
			// We could theoretically cast the type into an array of unsigned ints, but for safety, let it hand roll all of its members
			return CantorVariableLength(filter_type, address_type_u, address_type_v, address_type_w, max_anisotropic_level, BitCast<unsigned int>(mip_bias), min_lod, max_lod,
				BitCast<unsigned int>(border_color.red), BitCast<unsigned int>(border_color.green), BitCast<unsigned int>(border_color.blue), BitCast<unsigned int>(border_color.alpha));
		}

		ECS_INLINE bool operator ==(const SamplerDescriptor& other) const {
			return memcmp(this, &other, sizeof(*this)) == 0;
		}

		ECS_SAMPLER_FILTER_TYPE filter_type = ECS_SAMPLER_FILTER_LINEAR;
		ECS_SAMPLER_ADDRESS_TYPE address_type_u = ECS_SAMPLER_ADDRESS_WRAP;
		ECS_SAMPLER_ADDRESS_TYPE address_type_v = ECS_SAMPLER_ADDRESS_WRAP;
		ECS_SAMPLER_ADDRESS_TYPE address_type_w = ECS_SAMPLER_ADDRESS_WRAP;

		unsigned int max_anisotropic_level = 4;

		// Adjust the mip which is being sample (i.e. if the GPU calculates mip 2 and mip_bias is 1
		// it will sample at level 3)
		float mip_bias = 0.0f;

		// Which is the biggest mip level accessible (0 is the biggest, mip_count - 1 the smallest)
		unsigned int min_lod = 0;
		// Which is the smallest mip level accessible (must be greater than min_lod)
		unsigned int max_lod = UINT_MAX;

		// Used when address_type_u/v/w is set to ECS_SAMPLER_ADDRESS_BORDER
		ColorFloat border_color;
	};

	ECSENGINE_API D3D11_SAMPLER_DESC GetGraphicsNativeSamplerDescriptor(const SamplerDescriptor& descriptor);

	ECSENGINE_API SamplerDescriptor GetGraphicsSamplerDescriptorFromNative(const D3D11_SAMPLER_DESC& descriptor);

	// At the moment, leave this as a single render target. When we will need multiple render targets, adapt then
	struct BlendDescriptor {
		ECS_INLINE unsigned int Hash() const {
			// Can cast this type as an array of unsigned int and use cantor on that
			static_assert(sizeof(BlendDescriptor) % sizeof(unsigned int) == 0);
			return Cantor(Stream<unsigned int>(this, sizeof(*this) / sizeof(unsigned int)));
		}

		ECS_INLINE bool operator ==(const BlendDescriptor& other) const {
			return memcmp(this, &other, sizeof(*this)) == 0;
		}

		bool enabled = false;
		ECS_BLEND_OP color_op = ECS_BLEND_OP_ADD;
		ECS_BLEND_FACTOR color_source_factor = ECS_BLEND_SRC_ALPHA;
		ECS_BLEND_FACTOR color_destination_factor = ECS_BLEND_INV_SRC_ALPHA;
		ECS_BLEND_OP alpha_op = ECS_BLEND_OP_ADD;
		ECS_BLEND_FACTOR alpha_source_factor = ECS_BLEND_ZERO;
		ECS_BLEND_FACTOR alpha_destination_factor = ECS_BLEND_ONE;
		ECS_BLEND_COLOR_CHANNEL write_mask = ECS_BLEND_COLOR_CHANNEL_ALL;
	};

	ECSENGINE_API D3D11_BLEND_DESC GetGraphicsNativeBlendDescriptor(const BlendDescriptor& descriptor);

	ECSENGINE_API BlendDescriptor GetGraphicsBlendDescriptorFromNative(const D3D11_BLEND_DESC& descriptor);

	struct DepthStencilDescriptor {
		ECS_INLINE unsigned int Hash() const {
			// We can use cantor adaptive for this case
			return CantorAdaptive(this);
		}

		ECS_INLINE bool operator ==(const DepthStencilDescriptor& other) const {
			return memcmp(this, &other, sizeof(*this)) == 0;
		}

		struct FaceStencilOp {
			ECS_STENCIL_OP stencil_fail = ECS_STENCIL_OP_KEEP;
			ECS_STENCIL_OP depth_fail = ECS_STENCIL_OP_KEEP;
			ECS_STENCIL_OP pass = ECS_STENCIL_OP_KEEP;
			ECS_COMPARISON_OP stencil_comparison = ECS_COMPARISON_NEVER;
		};

		bool depth_enabled = false;
		bool write_depth = false;
		ECS_COMPARISON_OP depth_op = ECS_COMPARISON_LESS;
		bool stencil_enabled = false;
		unsigned char stencil_read_mask = 0xFF;
		unsigned char stencil_write_mask = 0xFF;
		FaceStencilOp stencil_front_face = {};
		FaceStencilOp stencil_back_face = {};
	};

	ECSENGINE_API D3D11_DEPTH_STENCIL_DESC GetGraphicsNativeDepthStencilDescriptor(const DepthStencilDescriptor& descriptor);

	ECSENGINE_API DepthStencilDescriptor GetGraphicsDepthStencilDescriptorFromNative(const D3D11_DEPTH_STENCIL_DESC& descriptor);

	struct RasterizerDescriptor {
		ECS_INLINE bool operator ==(const RasterizerDescriptor& other) const {
			return memcmp(this, &other, sizeof(*this)) == 0;
		}

		ECS_INLINE unsigned int Hash() const {
			// Use a cantor on bytes for this - we don't want to return the hash as is like an unsigned int since that
			// Doesn't work well for power of two hashes
			return Cantor(Stream<unsigned char>(this, sizeof(*this)));
		}
		
		// If set to true, the shape will be solid, else wireframe
		bool solid_fill = true;
		ECS_CULL_MODE cull_mode = ECS_CULL_BACK;
		bool front_face_is_counter_clockwise = false;
		bool enable_scissor = false;
	};

	ECSENGINE_API D3D11_RASTERIZER_DESC GetGraphicsNativeRasterizerDescriptor(const RasterizerDescriptor& descriptor);

	ECSENGINE_API RasterizerDescriptor GetGraphicsRasterizerDescriptorFromNative(const D3D11_RASTERIZER_DESC& descriptor);

	struct ECSENGINE_API VertexBuffer {
		ECS_INLINE VertexBuffer() : buffer(nullptr), stride(0), size(0) {}
		ECS_INLINE VertexBuffer(unsigned int _stride, unsigned int _size, ID3D11Buffer* _buffer) : buffer(_buffer), size(_size), stride(_stride) {}

		VertexBuffer(const VertexBuffer& other) = default;
		VertexBuffer& operator = (const VertexBuffer& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Buffer* Interface() const {
			return buffer;
		}

		ECS_INLINE unsigned int Release() {
			return buffer->Release();
		}

		static VertexBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
		unsigned int stride;
		unsigned int size;
	};

	struct ECSENGINE_API IndexBuffer {
		ECS_INLINE IndexBuffer() : buffer(nullptr), count(0), int_size(0) {}

		IndexBuffer(const IndexBuffer& other) = default;
		IndexBuffer& operator = (const IndexBuffer& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Buffer* Interface() const {
			return buffer;
		}

		ECS_INLINE unsigned int Release() {
			return buffer->Release();
		}

		static IndexBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
		unsigned int count;
		unsigned int int_size;
	};


	struct ECSENGINE_API InputLayout {
		ECS_INLINE InputLayout() : layout(nullptr) {}
		ECS_INLINE InputLayout(ID3D11InputLayout* _layout) : layout(_layout) {}

		InputLayout(const InputLayout& other) = default;
		InputLayout& operator = (const InputLayout& other) = default;

		ECS_INLINE ID3D11InputLayout* Interface() const {
			return layout;
		}

		ECS_INLINE unsigned int Release() {
			return layout->Release();
		}

		static InputLayout RawCreate(GraphicsDevice* device, Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, Stream<void> byte_code);

		ID3D11InputLayout* layout;
	};

	struct ECSENGINE_API VertexShader {
		ECS_INLINE VertexShader() : shader(nullptr) {}
		ECS_INLINE VertexShader(ID3D11VertexShader* _shader) : shader(_shader) {}

		VertexShader(const VertexShader& other) = default;
		VertexShader& operator = (const VertexShader& other) = default;

		ECS_INLINE ID3D11VertexShader* Interface() const {
			return shader;
		}

		ECS_INLINE unsigned int Release() {
			return shader->Release();
		}

		static VertexShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ECS_INLINE static VertexShader FromInterface(void* interface_) {
			return VertexShader((ID3D11VertexShader*)interface_);
		}

		ECS_INLINE void From(void* interface_) {
			shader = (ID3D11VertexShader*)interface_;
		}

		ID3D11VertexShader* shader;
	};

	struct ECSENGINE_API PixelShader {
		ECS_INLINE PixelShader() : shader(nullptr) {}
		ECS_INLINE PixelShader(ID3D11PixelShader* _shader) : shader(_shader) {}

		PixelShader(const PixelShader& other) = default;
		PixelShader& operator = (const PixelShader& other) = default;

		ECS_INLINE ID3D11PixelShader* Interface() const {
			return shader;
		}

		ECS_INLINE unsigned int Release() {
			return shader->Release();
		}

		static PixelShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ECS_INLINE static PixelShader FromInterface(void* interface_) {
			return PixelShader((ID3D11PixelShader*)interface_);
		}

		ECS_INLINE void From(void* interface_) {
			shader = (ID3D11PixelShader*)interface_;
		}

		ID3D11PixelShader* shader;
	};

	struct ECSENGINE_API GeometryShader {
		ECS_INLINE GeometryShader() : shader(nullptr) {}
		ECS_INLINE GeometryShader(ID3D11GeometryShader* _shader) : shader(_shader) {}

		GeometryShader(const GeometryShader& other) = default;
		GeometryShader& operator = (const GeometryShader& other) = default;

		ECS_INLINE ID3D11GeometryShader* Interface() const {
			return shader;
		}

		ECS_INLINE unsigned int Release() {
			return shader->Release();
		}

		static GeometryShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ECS_INLINE static GeometryShader FromInterface(void* interface_) {
			return GeometryShader((ID3D11GeometryShader*)interface_);
		}

		ECS_INLINE void From(void* interface_) {
			shader = (ID3D11GeometryShader*)interface_;
		}

		ID3D11GeometryShader* shader;
	};

	struct ECSENGINE_API DomainShader {
		ECS_INLINE DomainShader() : shader(nullptr) {}
		ECS_INLINE DomainShader(ID3D11DomainShader* _shader) : shader(_shader) {}

		DomainShader(const DomainShader& other) = default;
		DomainShader& operator = (const DomainShader& other) = default;

		ECS_INLINE ID3D11DomainShader* Interface() const {
			return shader;
		}

		ECS_INLINE unsigned int Release() {
			return shader->Release();
		}

		static DomainShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ECS_INLINE static DomainShader FromInterface(void* interface_) {
			return DomainShader((ID3D11DomainShader*)interface_);
		}

		ECS_INLINE void From(void* interface_) {
			shader = (ID3D11DomainShader*)interface_;
		}

		ID3D11DomainShader* shader;
	};

	struct ECSENGINE_API HullShader {
		ECS_INLINE HullShader() : shader(nullptr) {}
		ECS_INLINE HullShader(ID3D11HullShader* _shader) : shader(_shader) {}

		HullShader(const HullShader& other) = default;
		HullShader& operator = (const HullShader& other) = default;
		
		ECS_INLINE ID3D11HullShader* Interface() const {
			return shader;
		}

		ECS_INLINE unsigned int Release() {
			return shader->Release();
		}

		static HullShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ECS_INLINE static HullShader FromInterface(void* interface_) {
			return HullShader((ID3D11HullShader*)interface_);
		}

		ECS_INLINE void From(void* interface_) {
			shader = (ID3D11HullShader*)interface_;
		}

		ID3D11HullShader* shader;
	};

	// Path is stable and null terminated
	struct ECSENGINE_API ComputeShader {
		ECS_INLINE ComputeShader() : shader(nullptr) {}
		ECS_INLINE ComputeShader(ID3D11ComputeShader* _shader) : shader(_shader) {}

		ComputeShader(const ComputeShader& other) = default;
		ComputeShader& operator = (const ComputeShader& other) = default;

		ECS_INLINE ID3D11ComputeShader* Interface() const {
			return shader;
		}

		ECS_INLINE unsigned int Release() {
			return shader->Release();
		}

		static ComputeShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);
		
		ECS_INLINE static ComputeShader FromInterface(void* interface_) {
			return ComputeShader((ID3D11ComputeShader*)interface_);
		}

		ECS_INLINE void From(void* interface_) {
			shader = (ID3D11ComputeShader*)interface_;
		}

		ID3D11ComputeShader* shader;
	};

	struct ShaderInterface {
		ECS_INLINE operator void*() const {
			return interface_;
		}

		ECS_INLINE operator VertexShader() const {
			return VertexShader::FromInterface(interface_);
		}

		ECS_INLINE operator PixelShader() const {
			return PixelShader::FromInterface(interface_);
		}

		ECS_INLINE operator GeometryShader() const {
			return GeometryShader::FromInterface(interface_);
		}

		ECS_INLINE operator DomainShader() const {
			return DomainShader::FromInterface(interface_);
		}

		ECS_INLINE operator HullShader() const {
			return HullShader::FromInterface(interface_);
		}

		ECS_INLINE operator ComputeShader() const {
			return ComputeShader::FromInterface(interface_);
		}

		void* interface_;
	};

	// All types except the vertex shader which needs byte code as well
	template<typename ShaderType>
	struct ShaderStorage {
		ShaderType shader;
		Stream<char> source_code;
	};

	struct VertexShaderStorage {
		VertexShader shader;
		Stream<char> source_code;
		Stream<void> byte_code;
	};

	struct ECSENGINE_API Topology {
		ECS_INLINE Topology() : value(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {}
		ECS_INLINE Topology(D3D11_PRIMITIVE_TOPOLOGY _topology) : value(_topology) {}

		Topology(const Topology& other) = default;
		Topology& operator = (const Topology& other) = default;

		D3D11_PRIMITIVE_TOPOLOGY value;
	};

	struct ECSENGINE_API Texture1D {
		using RawDescriptor = D3D11_TEXTURE1D_DESC;
		using RawInterface = ID3D11Texture1D;

		ECS_INLINE Texture1D() : tex(nullptr) {}
		ECS_INLINE Texture1D(ID3D11Texture1D* _tex) : tex(_tex) {}
		Texture1D(ID3D11Resource* _resource);

		Texture1D(const Texture1D& other) = default;
		Texture1D& operator = (const Texture1D& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Texture1D* Interface() const {
			return tex;
		}

		ECS_INLINE unsigned int Release() {
			return tex->Release();
		}

		static Texture1D RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture1D* tex;
	};

	struct ECSENGINE_API Texture2D {
		using RawDescriptor = D3D11_TEXTURE2D_DESC;
		using RawInterface = ID3D11Texture2D;

		ECS_INLINE Texture2D() : tex(nullptr) {}
		ECS_INLINE Texture2D(ID3D11Texture2D* _tex) : tex(_tex) {}
		Texture2D(ID3D11Resource* _resource);

		Texture2D(const Texture2D& other) = default;
		Texture2D& operator = (const Texture2D& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Texture2D* Interface() const {
			return tex;
		}

		ECS_INLINE unsigned int Release() {
			return tex->Release();
		}

		static Texture2D RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture2D* tex;
	};

	struct ECSENGINE_API Texture3D {
		using RawDescriptor = D3D11_TEXTURE3D_DESC;
		using RawInterface = ID3D11Texture3D;

		ECS_INLINE Texture3D() : tex(nullptr) {}
		ECS_INLINE Texture3D(ID3D11Texture3D* _tex) : tex(_tex) {}
		Texture3D(ID3D11Resource* _resource);

		Texture3D(const Texture3D& other) = default;
		Texture3D& operator = (const Texture3D& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Texture3D* Interface() const {
			return tex;
		}

		ECS_INLINE unsigned int Release() {
			return tex->Release();
		}

		static Texture3D RawCreate(GraphicsDevice* graphics, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture3D* tex;
	};

	struct ECSENGINE_API TextureCube {
		using RawDescriptor = D3D11_TEXTURE2D_DESC;
		using RawInterface = ID3D11Texture2D;

		ECS_INLINE TextureCube() : tex(nullptr) {}
		ECS_INLINE TextureCube(ID3D11Texture2D* _tex) : tex(_tex) {}
		TextureCube(ID3D11Resource* _resource);

		TextureCube(const TextureCube& other) = default;
		TextureCube& operator = (const TextureCube& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Texture2D* Interface() const {
			return tex;
		}

		ECS_INLINE unsigned int Release() {
			return tex->Release();
		}

		static TextureCube RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture2D* tex;
	};

	struct ECSENGINE_API ResourceView {
		ECS_INLINE ResourceView() : view(nullptr) {}
		ECS_INLINE ResourceView(ID3D11ShaderResourceView* _view) : view(_view) {}

		ResourceView(const ResourceView& other) = default;
		ResourceView& operator = (const ResourceView& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11ShaderResourceView* Interface() const {
			return view;
		}

		ECS_INLINE unsigned int Release() {
			return view->Release();
		}

		ECS_INLINE Texture1D AsTexture1D() const {
			return Texture1D(GetResource());
		}

		ECS_INLINE Texture2D AsTexture2D() const {
			return Texture2D(GetResource());
		}

		ECS_INLINE Texture3D AsTexture3D() const {
			return Texture3D(GetResource());
		}

		ECS_INLINE void FromUntyped(void* pointer) {
			view = (ID3D11ShaderResourceView*)pointer;
		}

		static ResourceView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_SHADER_RESOURCE_VIEW_DESC* descriptor);

		// It will copy using the descriptor from the view
		static ResourceView RawCopy(GraphicsDevice* device, ID3D11Resource* resource, ResourceView view);

		ID3D11ShaderResourceView* view;
	};

	struct ECSENGINE_API RenderTargetView {
		ECS_INLINE RenderTargetView() : view(nullptr) {}
		ECS_INLINE RenderTargetView(ID3D11RenderTargetView* _target) : view(_target) {}

		RenderTargetView(const RenderTargetView& other) = default;
		RenderTargetView& operator = (const RenderTargetView& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE Texture2D AsTexture2D() const {
			return GetResource();
		}

		ECS_INLINE ID3D11RenderTargetView* Interface() const {
			return view;
		}

		ECS_INLINE unsigned int Release() {
			return view->Release();
		}

		static RenderTargetView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_RENDER_TARGET_VIEW_DESC* descriptor);

		// It will copy using the descriptor from the view
		static RenderTargetView RawCopy(GraphicsDevice* device, ID3D11Resource* resource, RenderTargetView view);

		ID3D11RenderTargetView* view;
	};

	struct ECSENGINE_API DepthStencilView {
		ECS_INLINE DepthStencilView() : view(nullptr) {}
		ECS_INLINE DepthStencilView(ID3D11DepthStencilView* _view) : view(_view) {}

		DepthStencilView(const DepthStencilView& other) = default;
		DepthStencilView& operator = (const DepthStencilView& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11DepthStencilView* Interface() const {
			return view;
		}

		ECS_INLINE unsigned int Release() {
			return view->Release();
		}

		static DepthStencilView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_DEPTH_STENCIL_VIEW_DESC* descriptor);

		// It will copy using the descriptor from the view
		static DepthStencilView RawCopy(GraphicsDevice* device, ID3D11Resource* resource, DepthStencilView view);

		ID3D11DepthStencilView* view;
	};

	struct ECSENGINE_API UAView {
		ECS_INLINE UAView() : view(nullptr) {}
		ECS_INLINE UAView(ID3D11UnorderedAccessView* _view) : view(_view) {}

		UAView(const UAView& other) = default;
		UAView& operator = (const UAView& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11UnorderedAccessView* Interface() const {
			return view;
		}

		ECS_INLINE unsigned int Release() {
			return view->Release();
		}

		static UAView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* descriptor);

		// It will copy using the descriptor from the view
		static UAView RawCopy(GraphicsDevice* device, ID3D11Resource* resource, UAView view);

		ID3D11UnorderedAccessView* view;
	};

	struct ECSENGINE_API ConstantBuffer {
		ECS_INLINE ConstantBuffer() : buffer(nullptr) {}
		ECS_INLINE ConstantBuffer(ID3D11Buffer* buffer) : buffer(buffer) {}

		ConstantBuffer(const ConstantBuffer& other) = default;
		ConstantBuffer& operator = (const ConstantBuffer& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Buffer* Interface() const {
			return buffer;
		}

		ECS_INLINE unsigned int Release() {
			return buffer->Release();
		}

		static ConstantBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API StandardBuffer {
		ECS_INLINE StandardBuffer() : buffer(nullptr) {}
		ECS_INLINE StandardBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		StandardBuffer(const StandardBuffer& other) = default;
		StandardBuffer& operator = (const StandardBuffer& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Buffer* Interface() const {
			return buffer;
		}

		ECS_INLINE unsigned int Release() {
			return buffer->Release();
		}

		static StandardBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
		size_t count;
	};

	struct ECSENGINE_API StructuredBuffer {
		ECS_INLINE StructuredBuffer() : buffer(nullptr) {}
		ECS_INLINE StructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		StructuredBuffer(const StructuredBuffer& other) = default;
		StructuredBuffer& operator = (const StructuredBuffer& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Buffer* Interface() const {
			return buffer;
		}

		ECS_INLINE unsigned int Release() {
			return buffer->Release();
		}

		static StructuredBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API IndirectBuffer {
		ECS_INLINE IndirectBuffer() : buffer(nullptr) {}
		ECS_INLINE IndirectBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		IndirectBuffer(const IndirectBuffer& other) = default;
		IndirectBuffer& operator = (const IndirectBuffer& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Buffer* Interface() const {
			return buffer;
		}

		ECS_INLINE unsigned int Release() {
			return buffer->Release();
		}

		static IndirectBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API UABuffer {
		ECS_INLINE UABuffer() : buffer(nullptr), element_count(0) {}
		ECS_INLINE UABuffer(ID3D11Buffer* _buffer) : buffer(_buffer), element_count(0) {}

		UABuffer(const UABuffer& other) = default;
		UABuffer& operator = (const UABuffer& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Buffer* Interface() const {
			return buffer;
		}

		ECS_INLINE unsigned int Release() {
			return buffer->Release();
		}

		static UABuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
		size_t element_count;
	};

	struct ECSENGINE_API AppendStructuredBuffer {
		ECS_INLINE AppendStructuredBuffer() : buffer(nullptr), element_count(0) {}
		ECS_INLINE AppendStructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer), element_count(0) {}

		AppendStructuredBuffer(const AppendStructuredBuffer& other) = default;
		AppendStructuredBuffer& operator = (const AppendStructuredBuffer& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Buffer* Interface() const {
			return buffer;
		}

		ECS_INLINE unsigned int Release() {
			return buffer->Release();
		}

		static AppendStructuredBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
		size_t element_count;
	};

	struct ECSENGINE_API ConsumeStructuredBuffer {
		ECS_INLINE ConsumeStructuredBuffer() : buffer(nullptr), element_count(0) {}
		ECS_INLINE ConsumeStructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer), element_count(0) {}

		ConsumeStructuredBuffer(const ConsumeStructuredBuffer& other) = default;
		ConsumeStructuredBuffer& operator = (const ConsumeStructuredBuffer& other) = default;

		ID3D11Resource* GetResource() const;

		ECS_INLINE ID3D11Buffer* Interface() const {
			return buffer;
		}

		ECS_INLINE unsigned int Release() {
			return buffer->Release();
		}

		static ConsumeStructuredBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
		size_t element_count;
	};

	struct ECSENGINE_API SamplerState {
		ECS_INLINE SamplerState() : sampler(nullptr) {}
		ECS_INLINE SamplerState(ID3D11SamplerState* _sampler) : sampler(_sampler) {}

		SamplerState(const SamplerState& other) = default;
		SamplerState& operator = (const SamplerState& other) = default;

		ECS_INLINE ID3D11SamplerState* Interface() const {
			return sampler;
		}

		ECS_INLINE unsigned int Release() {
			return sampler->Release();
		}

		ECS_INLINE SamplerDescriptor GetDescriptor() const {
			D3D11_SAMPLER_DESC desc;
			sampler->GetDesc(&desc);
			return GetGraphicsSamplerDescriptorFromNative(desc);
		}

		static SamplerState RawCreate(GraphicsDevice* device, const D3D11_SAMPLER_DESC* descriptor);

		ID3D11SamplerState* sampler;
	};

	struct ECSENGINE_API BlendState {
		ECS_INLINE BlendState() : state(nullptr) {}
		ECS_INLINE BlendState(ID3D11BlendState* _state) : state(_state) {}

		BlendState(const BlendState& other) = default;
		BlendState& operator = (const BlendState& other) = default;

		ECS_INLINE ID3D11BlendState* Interface() const {
			return state;
		}

		ECS_INLINE unsigned int Release() {
			return state->Release();
		}

		ECS_INLINE BlendDescriptor GetDescriptor() const {
			D3D11_BLEND_DESC desc;
			state->GetDesc(&desc);
			return GetGraphicsBlendDescriptorFromNative(desc);
		}

		ID3D11BlendState* state;
	};

	struct ECSENGINE_API RasterizerState {
		ECS_INLINE RasterizerState() : state(nullptr) {}
		ECS_INLINE RasterizerState(ID3D11RasterizerState* _state) : state(_state) {}

		RasterizerState(const RasterizerState& other) = default;
		RasterizerState& operator = (const RasterizerState& other) = default;

		ECS_INLINE ID3D11RasterizerState* Interface() const {
			return state;
		}

		ECS_INLINE unsigned int Release() {
			return state->Release();
		}

		ECS_INLINE RasterizerDescriptor GetDescriptor() const {
			D3D11_RASTERIZER_DESC desc;
			state->GetDesc(&desc);
			return GetGraphicsRasterizerDescriptorFromNative(desc);
		}

		ID3D11RasterizerState* state;
	};

	struct ECSENGINE_API DepthStencilState {
		ECS_INLINE DepthStencilState() : state(nullptr) {}
		ECS_INLINE DepthStencilState(ID3D11DepthStencilState* _state) : state(_state) {}

		DepthStencilState(const DepthStencilState& other) = default;
		DepthStencilState& operator = (const DepthStencilState& other) = default;

		ECS_INLINE ID3D11DepthStencilState* Interface() const {
			return state;
		}

		ECS_INLINE unsigned int Release() {
			return state->Release();
		}

		ECS_INLINE DepthStencilDescriptor GetDescriptor() const {
			D3D11_DEPTH_STENCIL_DESC desc;
			state->GetDesc(&desc);
			return GetGraphicsDepthStencilDescriptorFromNative(desc);
		}

		ID3D11DepthStencilState* state;
	};

	struct ECSENGINE_API CommandList {
		ECS_INLINE CommandList() : list(nullptr) {}
		ECS_INLINE CommandList(ID3D11CommandList* _list) : list(_list) {}

		CommandList(const CommandList& other) = default;
		CommandList& operator = (const CommandList& other) = default;

		ECS_INLINE ID3D11CommandList* Interface() const {
			return list;
		}

		ECS_INLINE unsigned int Release() {
			return list->Release();
		}

		ID3D11CommandList* list;
	};

	struct GPUQuery {
		ECS_INLINE GPUQuery() : query(nullptr) {}
		ECS_INLINE GPUQuery(ID3D11Query* _query) : query(_query) {}

		GPUQuery(const GPUQuery& other) = default;
		GPUQuery& operator = (const GPUQuery& other) = default;

		ECS_INLINE ID3D11Query* Interface() const {
			return query;
		}

		ECS_INLINE unsigned int Release() {
			return query->Release();
		}

		ID3D11Query* query;
	};

	struct GraphicsViewport {
		float width;
		float height;
		float top_left_x = 0.0f;
		float top_left_y = 0.0f;
		float min_depth = 0.0f;
		float max_depth = 1.0f;
	};

	struct RenderDestination {
		// Does not take into account the graphics object bookkeeping
		ECS_INLINE void Release() {
			ReleaseGraphicsView(render_view);
			ReleaseGraphicsView(depth_view);
			// The resource view interface needs to be released solo
			// because the underlying texture will be released with the previous
			// render view call
			output_view.Release();
			if (render_ua_view.Interface() != nullptr) {
				render_ua_view.Release();
			}
		}

		// This can be used to use the render as input for other phases
		ResourceView output_view;
		RenderTargetView render_view;
		DepthStencilView depth_view;
		UAView render_ua_view;
	};

	struct GraphicsPipelineBlendState {
		BlendState blend_state;
		float blend_factors[4];
		unsigned int sample_mask;
	};

	struct GraphicsPipelineDepthStencilState {
		DepthStencilState depth_stencil_state;
		unsigned int stencil_ref;
	};

	struct GraphicsPipelineRasterizerState {
		RasterizerState rasterizer_state;
	};

	struct GraphicsPipelineShaders {
		InputLayout layout;
		VertexShader vertex_shader;
		PixelShader pixel_shader;
		DomainShader domain_shader;
		HullShader hull_shader;
		GeometryShader geometry_shader;
		ComputeShader compute_shader;
	};

	struct GraphicsPipelineRenderState {
		GraphicsPipelineBlendState blend_state;
		GraphicsPipelineDepthStencilState depth_stencil_state;
		GraphicsPipelineRasterizerState rasterizer_state;
		GraphicsPipelineShaders shaders;
	};

	struct GraphicsBoundViews {
		RenderTargetView target;
		DepthStencilView depth_stencil;
	};

	struct GraphicsBoundTarget {
		RenderTargetView target;
		DepthStencilView depth_stencil;
		GraphicsViewport viewport;
	};

	struct GraphicsPipelineState {
		GraphicsPipelineRenderState render_state;
		GraphicsBoundTarget target;
	};

	struct ECS_REFLECT OrientedPoint {
		// Sets the position and the rotation to 0.0f
		ECS_INLINE void ToOrigin() {
			position = { 0.0f, 0.0f, 0.0f };
			rotation = { 0.0f, 0.0f, 0.0f };
		}

		float3 position;
		float3 rotation;
	};

	struct ECSENGINE_API Mesh {
		ECS_INLINE Mesh() : name(nullptr, 0), mapping_count(0) {}

		Mesh(const Mesh& other) = default;
		Mesh& operator = (const Mesh& other) = default;

		// Returns a nullptr vertex buffer if there is no vertex buffer for that mapping
		VertexBuffer GetBuffer(ECS_MESH_INDEX vertex_mapping) const {
			for (unsigned char index = 0; index < mapping_count; index++) {
				if (mapping[index] == vertex_mapping) {
					return vertex_buffers[index];
				}
			}
			return {};
		}

		void SetBuffer(ECS_MESH_INDEX vertex_mapping, VertexBuffer buffer) {
			for (size_t index = 0; index < mapping_count; index++) {
				if (mapping[index] == vertex_mapping) {
					vertex_buffers[index] = buffer;
					return;
				}
			}
		}

		// Retrieves all the GPU resources in use by this mesh
		void GetGPUResources(AdditionStream<void*> resources) const {
			resources.Add(index_buffer.Interface());
			for (size_t index = 0; index < (size_t)mapping_count; index++) {
				resources.Add(vertex_buffers[index].Interface());
			}
		}

		IndexBuffer index_buffer;
		VertexBuffer vertex_buffers[ECS_MESH_BUFFER_COUNT];
		ECS_MESH_INDEX mapping[ECS_MESH_BUFFER_COUNT];
		unsigned char mapping_count;
		Stream<char> name;
		AABBScalar bounds;
	};

	struct ECSENGINE_API Submesh {
		ECS_INLINE Submesh() : name(nullptr, 0), index_buffer_offset(0), vertex_buffer_offset(0), index_count(0), vertex_count(0) {}
		ECS_INLINE Submesh(unsigned int _index_buffer_offset, unsigned int _vertex_buffer_offset, unsigned int _index_count, unsigned int _vertex_count) 
			: name(nullptr, 0), index_buffer_offset(_index_buffer_offset), vertex_buffer_offset(_vertex_buffer_offset), index_count(_index_count), 
		vertex_count(_vertex_count) {}

		Submesh(const Submesh& other) = default;
		Submesh& operator = (const Submesh& other) = default;

		ECS_INLINE size_t CopySize() const {
			return name.CopySize();
		}

		ECS_INLINE Submesh Copy(AllocatorPolymorphic allocator) const {
			void* allocation = Allocate(allocator, CopySize());
			uintptr_t ptr = (uintptr_t)allocation;
			return CopyTo(ptr);
		}

		ECS_INLINE Submesh CopyTo(uintptr_t& ptr) const {
			Submesh submesh;
			memcpy(&submesh, this, sizeof(submesh));
			submesh.name = name.CopyTo(ptr);
			return submesh;
		}

		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
			if (name.size > 0) {
				ECSEngine::Deallocate(allocator, name.buffer);
			}
		}

		Stream<char> name;
		unsigned int index_buffer_offset;
		unsigned int vertex_buffer_offset;
		unsigned int index_count;
		unsigned int vertex_count;
		AABBScalar bounds;
	};

	ECS_INLINE AABBScalar GetSubmeshesBoundingBox(Stream<Submesh> submeshes) {
		AABBScalar combined = ReverseInfiniteAABBScalar();
		for (size_t index = 0; index < submeshes.size; index++) {
			combined = GetCombinedAABB(combined, submeshes[index].bounds);
		}
		return combined;
	}

	// Contains the actual pipeline objects that can be bound to the graphics context
	struct ECSENGINE_API Material {
		ECS_INLINE Material() : vertex_buffer_mapping_count(0), v_buffer_count(0), p_buffer_count(0), v_texture_count(0), p_texture_count(0),
			unordered_view_count(0) {}

		Material(const Material& other) = default;
		Material& operator = (const Material& other) = default;

	private:
		// Returns the index where it was written
		unsigned char AddTag(
			Stream<char> tag, 
			unsigned short byte_offset, 
			unsigned char buffer_index, 
			unsigned char* tag_count, 
			uchar2* tags, 
			unsigned short* byte_offsets,
			unsigned char* buffer_indices,
			unsigned char max_count
		);

		// Returns the index where it was written
		unsigned char AddConstantBuffer(
			ConstantBuffer buffer,
			unsigned char slot,
			unsigned char* current_count,
			ConstantBuffer* buffers,
			unsigned char* slots,
			unsigned char max_count
		);

		// Returns the index where it was written
		unsigned char AddSampler(
			SamplerState sampler,
			unsigned char slot,
			unsigned char* current_count,
			SamplerState* samplers,
			unsigned char* slots,
			unsigned char max_count
		);

		// Returns the index where it was written
		unsigned char AddResourceView(
			ResourceView resource_view,
			unsigned char slot,
			unsigned char* current_count,
			ResourceView* resource_views,
			unsigned char* slots,
			unsigned char max_count
		);

	public:
		// Returns the index where it was written
		ECS_INLINE unsigned char AddVertexTag(Stream<char> tag, unsigned short byte_offset, unsigned char buffer_index) {
			return AddTag(tag, byte_offset, buffer_index, &v_tag_count, v_tags, v_tag_byte_offset, v_tag_buffer_index, ECS_MATERIAL_VERTEX_TAG_COUNT);
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddPixelTag(Stream<char> tag, unsigned short byte_offset, unsigned char buffer_index) {
			return AddTag(tag, byte_offset, buffer_index, &p_tag_count, p_tags, p_tag_byte_offset, p_tag_buffer_index, ECS_MATERIAL_PIXEL_TAG_COUNT);
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddTag(Stream<char> tag, unsigned short byte_offset, unsigned char buffer_index, bool is_vertex) {
			if (is_vertex) {
				return AddVertexTag(tag, byte_offset, buffer_index);
			}
			else {
				return AddPixelTag(tag, byte_offset, buffer_index);
			}
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddConstantBuffer(ConstantBuffer buffer, unsigned char slot, bool is_vertex) {
			if (is_vertex) {
				return AddConstantBufferVertex(buffer, slot);
			}
			else {
				return AddConstantBufferPixel(buffer, slot);
			}
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddConstantBufferVertex(ConstantBuffer buffer, unsigned char slot) {
			return AddConstantBuffer(buffer, slot, &v_buffer_count, v_buffers, v_buffer_slot, ECS_MATERIAL_VERTEX_CONSTANT_BUFFER_COUNT);
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddConstantBufferPixel(ConstantBuffer buffer, unsigned char slot) {
			return AddConstantBuffer(buffer, slot, &p_buffer_count, p_buffers, p_buffer_slot, ECS_MATERIAL_PIXEL_CONSTANT_BUFFER_COUNT);
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddSampler(SamplerState sampler, unsigned char slot, bool is_vertex) {
			if (is_vertex) {
				return AddSamplerVertex(sampler, slot);
			}
			else {
				return AddSamplerPixel(sampler, slot);
			}
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddSamplerVertex(SamplerState sampler, unsigned char slot) {
			return AddSampler(sampler, slot, &v_sampler_count, v_samplers, v_sampler_slot, ECS_MATERIAL_VERTEX_TEXTURES_COUNT);
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddSamplerPixel(SamplerState sampler, unsigned char slot) {
			return AddSampler(sampler, slot, &p_sampler_count, p_samplers, p_sampler_slot, ECS_MATERIAL_PIXEL_TEXTURES_COUNT);
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddResourceView(ResourceView resource_view, unsigned char slot, bool is_vertex) {
			if (is_vertex) {
				return AddResourceViewVertex(resource_view, slot);
			}
			else {
				return AddResourceViewPixel(resource_view, slot);
			}
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddResourceViewVertex(ResourceView resource_view, unsigned char slot) {
			return AddResourceView(resource_view, slot, &v_texture_count, v_textures, v_texture_slot, ECS_MATERIAL_VERTEX_TEXTURES_COUNT);
		}

		// Returns the index where it was written
		ECS_INLINE unsigned char AddResourceViewPixel(ResourceView resource_view, unsigned char slot) {
			return AddResourceView(resource_view, slot, &p_texture_count, p_textures, p_texture_slot, ECS_MATERIAL_PIXEL_TEXTURES_COUNT);
		}

		bool ContainsTexture(ResourceView texture) const;

		ECS_INLINE bool ContainsShader(VertexShader _vertex_shader) const {
			return vertex_shader.Interface() == _vertex_shader.Interface();
		}

		ECS_INLINE bool ContainsShader(PixelShader _pixel_shader) const {
			return pixel_shader.Interface() == _pixel_shader.Interface();
		}

		bool ContainsSampler(SamplerState sampler_state) const;

		// Copies the values that pertain to the vertex shader
		void CopyVertexShader(const Material* other);

		// Copies the values that pertain to the pixel shader
		void CopyPixelShader(const Material* other);

		// Copies the values that pertain to textures
		void CopyTextures(const Material* other);

		// Copies the values that pertain to unordered views
		void CopyUnorderedViews(const Material* other);

		// Copies the values that pertain to constant buffers
		void CopyConstantBuffers(const Material* other);

		// Copies the values that pertain to samplers
		void CopySamplers(const Material* other);

		// Copies all the fields
		void Copy(const Material* other);

		// The x component contains the index of the buffer and the y component the byte offset inside that buffer
		// If it doesn't find the tag, it returns { -1, -1 }
		uint2 FindTag(Stream<char> tag, bool vertex_shader) const;

		ECS_INLINE Stream<char> GetTag(unsigned char index, const uchar2* tags) const {
			return { tag_storage + tags[index].x, tags[index].y };
		}

		ECS_INLINE Stream<char> GetVertexTag(unsigned char index) const {
			return GetTag(index, v_tags);
		}

		ECS_INLINE Stream<char> GetPixelTag(unsigned char index) const {
			return GetTag(index, p_tags);
		}
		
		ECS_INLINE unsigned short GetVertexTagByteOffset(unsigned char index) const {
			return v_tag_byte_offset[index];
		}

		ECS_INLINE unsigned short GetPixelTagByteOffset(unsigned char index) const {
			return p_tag_byte_offset[index];
		}

		ECS_INLINE void ResetVertexShader() {
			layout = {};
			vertex_shader = {};
			vertex_buffer_mapping_count = 0;
		}

		ECS_INLINE void ResetPixelShader() {
			pixel_shader = {};
		}

		ECS_INLINE void ResetTextures() {
			v_texture_count = 0;
			p_texture_count = 0;
		}

		ECS_INLINE void ResetSamplers() {
			v_sampler_count = 0;
			p_sampler_count = 0;
		}

		ECS_INLINE void ResetConstantBuffers() {
			v_buffer_count = 0;
			p_buffer_count = 0;
			v_tag_count = 0;
			p_tag_count = 0;
			tag_storage_size = 0;
		}

		ECS_INLINE void ResetUnorderedViews() {
			unordered_view_count = 0;
		}

		// Retrieves all the GPU resources in use by this material. Useful for protecting/unprotecting the resources.
		void GetGPUResources(AdditionStream<void*> resources) const;

		InputLayout layout;
		VertexShader vertex_shader;
		PixelShader pixel_shader;
		ConstantBuffer v_buffers[ECS_MATERIAL_VERTEX_CONSTANT_BUFFER_COUNT];
		ConstantBuffer p_buffers[ECS_MATERIAL_PIXEL_CONSTANT_BUFFER_COUNT];
		ResourceView v_textures[ECS_MATERIAL_VERTEX_TEXTURES_COUNT];
		ResourceView p_textures[ECS_MATERIAL_PIXEL_TEXTURES_COUNT];
		UAView unordered_views[ECS_MATERIAL_UAVIEW_COUNT];
		SamplerState v_samplers[ECS_MATERIAL_VERTEX_TEXTURES_COUNT];
		SamplerState p_samplers[ECS_MATERIAL_PIXEL_TEXTURES_COUNT];
		ECS_MESH_INDEX vertex_buffer_mappings[ECS_MESH_BUFFER_COUNT];
		unsigned char vertex_buffer_mapping_count = 0;
		unsigned char unordered_view_count = 0;
		unsigned char v_buffer_count = 0;
		unsigned char p_buffer_count = 0;
		unsigned char v_texture_count = 0;
		unsigned char p_texture_count = 0;
		unsigned char v_sampler_count = 0;
		unsigned char p_sampler_count = 0;
		unsigned char v_tag_count = 0;
		unsigned char p_tag_count = 0;
		unsigned char tag_storage_size = 0;
		unsigned char v_buffer_slot[ECS_MATERIAL_VERTEX_CONSTANT_BUFFER_COUNT];
		unsigned char p_buffer_slot[ECS_MATERIAL_PIXEL_CONSTANT_BUFFER_COUNT];
		unsigned char v_texture_slot[ECS_MATERIAL_VERTEX_TEXTURES_COUNT];
		unsigned char p_texture_slot[ECS_MATERIAL_PIXEL_TEXTURES_COUNT];
		unsigned char v_sampler_slot[ECS_MATERIAL_VERTEX_TEXTURES_COUNT];
		unsigned char p_sampler_slot[ECS_MATERIAL_PIXEL_TEXTURES_COUNT];
		char tag_storage[ECS_MATERIAL_TAG_STORAGE_CAPACITY];
		uchar2 v_tags[ECS_MATERIAL_VERTEX_TAG_COUNT];
		uchar2 p_tags[ECS_MATERIAL_PIXEL_TAG_COUNT];
		unsigned short v_tag_byte_offset[ECS_MATERIAL_VERTEX_TAG_COUNT];
		unsigned short p_tag_byte_offset[ECS_MATERIAL_PIXEL_TAG_COUNT];
		unsigned char v_tag_buffer_index[ECS_MATERIAL_VERTEX_TAG_COUNT];
		unsigned char p_tag_buffer_index[ECS_MATERIAL_PIXEL_TAG_COUNT];
	};

	struct ECSENGINE_API PBRMaterial {
		// Returns the start of the textures as paths, can use indices to index into them
		ECS_INLINE Stream<wchar_t>* TextureStart() {
			return &color_texture;
		}

		bool HasTextures() const;

		Stream<char> name;
		float metallic_factor;
		float roughness_factor;
		Color tint;
		float3 emissive_factor;
		Stream<wchar_t> color_texture;
		Stream<wchar_t> normal_texture;
		Stream<wchar_t> metallic_texture;
		Stream<wchar_t> roughness_texture;
		Stream<wchar_t> occlusion_texture;
		Stream<wchar_t> emissive_texture;
	};

	enum ECS_TEXTURE_COMPRESSION_EX : unsigned char;

	struct ECSENGINE_API UserMaterialTexture {
		// Fills in the suffix based on the settings to uniquely identify the texture
		void GenerateSettingsSuffix(CapacityStream<void>& suffix) const;

		// The name is used only to generate a settings suffix.
		// Otherwise it serves no use
		Stream<char> name = { nullptr, 0 };

		Stream<wchar_t> filename;
		ECS_SHADER_TYPE shader_type;
		unsigned char slot;
		bool srgb = false;
		bool generate_mips = true;
		ECS_TEXTURE_COMPRESSION_EX compression = (ECS_TEXTURE_COMPRESSION_EX)0;
	};

	struct UserMaterialBufferTag {
		Stream<char> string;
		unsigned short byte_offset;
	};

	// The data size needs to be know in order to allocate correctly the buffer. If the
	// data buffer is nullptr then there will be no copy of the data (so it cannot be static)
	struct UserMaterialBuffer {
		Stream<void> data;
		unsigned char slot;
		bool dynamic;
		ECS_SHADER_TYPE shader_type;
		Stream<UserMaterialBufferTag> tags = { nullptr, 0 };
	};

	struct UserMaterialSampler {
		SamplerState state;
		ECS_SHADER_TYPE shader_type;
		unsigned char slot;
	};

	// Generates an identifier that can be used to uniquely identify a shader based on its options
	// Useful for the resource manager.
	ECSENGINE_API void GenerateShaderCompileOptionsSuffix(ShaderCompileOptions compile_options, CapacityStream<void>& suffix, Stream<void> optional_addition = { nullptr, 0 });

	// A user material which is based only on textures and constant buffers
	// If a texture filename is not filled in then it will bind a nullptr texture
	// If a buffer data is not specified it will bind a nullptr buffer
	struct UserMaterial {
		Stream<UserMaterialTexture> textures;
		Stream<UserMaterialBuffer> buffers;
		Stream<UserMaterialSampler> samplers;
		Stream<wchar_t> vertex_shader;
		Stream<wchar_t> pixel_shader;
		ShaderCompileOptions vertex_compile_options;
		ShaderCompileOptions pixel_compile_options;

		// If set, it will generate a suffix for each texture
		// based upon the settings such that it won't return a texture
		// that already exists on the same target but with different settings
		bool generate_unique_name_from_setting;

		// These are optional. Only needed when generating unique identifiers from settings
		// Can optionally tag the setting such that a new shader is created even tho there is one
		// with the same settings
		Stream<char> vertex_shader_name = { nullptr, 0 };
		Stream<char> pixel_shader_name = { nullptr, 0 };
	};

	enum PBRMaterialTextureIndex : unsigned char {
		ECS_PBR_MATERIAL_COLOR,
		ECS_PBR_MATERIAL_NORMAL,
		ECS_PBR_MATERIAL_METALLIC,
		ECS_PBR_MATERIAL_ROUGHNESS,
		ECS_PBR_MATERIAL_OCCLUSION,
		ECS_PBR_MATERIAL_EMISSIVE,
		ECS_PBR_MATERIAL_MAPPING_COUNT
	};

	struct PBRMaterialMapping {
		Stream<wchar_t> texture;
		PBRMaterialTextureIndex index;
	};

	struct CoalescedMesh {
		Mesh mesh;
		Stream<Submesh> submeshes;
	};

	// Each submesh has associated a material
	struct PBRMesh {
		CoalescedMesh mesh;
		PBRMaterial* materials;
	};

	ECSENGINE_API void SetPBRMaterialTexture(PBRMaterial* material, uintptr_t& memory, Stream<wchar_t> texture, PBRMaterialTextureIndex texture_index);

	ECSENGINE_API void AllocatePBRMaterial(
		PBRMaterial& material, 
		Stream<char> name, 
		Stream<PBRMaterialMapping> mappings,
		AllocatorPolymorphic allocator
	);

	// Releases the memory used for the texture names and the material name - it's coalesced
	// in the name buffer
	ECSENGINE_API void FreePBRMaterial(const PBRMaterial& material, AllocatorPolymorphic allocator);

	struct CreatePBRMaterialFromNameOptions {
		Stream<PBRMaterialTextureIndex>* texture_mask = nullptr;
		// If you set this flag to true, then all paths will be relatived to search directory
		bool search_directory_is_mount_point = false;
	};

	// It will search every directory in order to find each texture - they can be situated
	// in different folders; the texture mask can be used to tell the function which textures
	// to look for, by default it will search for all
	// Might change the texture mask 
	ECSENGINE_API PBRMaterial CreatePBRMaterialFromName(
		Stream<char> material_name,
		Stream<char> texture_base_name, 
		Stream<wchar_t> search_directory, 
		AllocatorPolymorphic allocator,
		CreatePBRMaterialFromNameOptions options = {}
	);

	// It will search every directory in order to find each texture - they can be situated
	// in different folders; the texture mask can be used to tell the function which textures
	// to look for, by default it will search for all
	// Might change the texture mask 
	ECSENGINE_API PBRMaterial CreatePBRMaterialFromName(
		Stream<char> material_name,
		Stream<wchar_t> texture_base_name,
		Stream<wchar_t> search_directory,
		AllocatorPolymorphic allocator,
		CreatePBRMaterialFromNameOptions options = {}
	);

}