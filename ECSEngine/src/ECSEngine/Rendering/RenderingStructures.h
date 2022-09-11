// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Containers/Stream.h"
#include "../Math/Matrix.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Utilities/Reflection/ReflectionMacros.h"

#define ECS_GRAPHICS_BUFFERS(function) /* Useful for macro expansion */ function(VertexBuffer); \
function(IndexBuffer); \
function(ConstantBuffer); \
function(StandardBuffer); \
function(StructuredBuffer); \
function(UABuffer); 

#define ECS_GRAPHICS_TEXTURES(function) /* Useful for macro expansion */ function(Texture1D); \
function(Texture2D); \
function(Texture3D); 

#define ECS_GRAPHICS_RESOURCES(function) /* Useful for macro expansion */ ECS_GRAPHICS_TEXTURES(function); \
ECS_GRAPHICS_BUFFERS(function); 

#define ECS_MATERIAL_VERTEX_CONSTANT_BUFFER_COUNT 4
#define ECS_MATERIAL_PIXEL_CONSTANT_BUFFER_COUNT 4
#define ECS_MATERIAL_HULL_CONSTANT_BUFFER_COUNT 1
#define ECS_MATERIAL_DOMAIN_CONSTANT_BUFFER_COUNT 1
#define ECS_MATERIAL_GEOMETRY_CONSTANT_BUFFER_COUNT 1
#define ECS_MATERIAL_VERTEX_TEXTURES_COUNT 2
#define ECS_MATERIAL_PIXEL_TEXTURES_COUNT 8
#define ECS_MATERIAL_DOMAIN_TEXTURES_COUNT 1
#define ECS_MATERIAL_HULL_TEXTURES_COUNT 1
#define ECS_MATERIAL_GEOMETRY_TEXTURES_COUNT 1
#define ECS_MATERIAL_UAVIEW_COUNT 4

namespace ECSEngine {

	enum ECS_MESH_INDEX {
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
		ECS_GRAPHICS_FORMAT_R32_TYPELESS = DXGI_FORMAT_R32_TYPELESS,
		ECS_GRAPHICS_FORMAT_RG32_TYPELESS = DXGI_FORMAT_R32G32_TYPELESS,
		ECS_GRAPHICS_FORMAT_RGB32_TYPELESS = DXGI_FORMAT_R32G32B32_TYPELESS,
		ECS_GRAPHICS_FORMAT_RGBA32_TYPELESS = DXGI_FORMAT_R32G32B32A32_TYPELESS
		
		// ----------- Typeless formats -------------------
	};

	// At the moment map the values directly for "fast" retrieval
	enum ECS_GRAPHICS_BIND_TYPE {
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

	inline DXGI_FORMAT GetGraphicsNativeFormat(ECS_GRAPHICS_FORMAT format) {
		// Can return as is for the moment
		return (DXGI_FORMAT)format;
	}

	inline ECS_GRAPHICS_FORMAT GetGraphicsFormatFromNative(DXGI_FORMAT format) {
		return (ECS_GRAPHICS_FORMAT)format;
	}

	inline D3D11_BIND_FLAG GetGraphicsNativeBind(ECS_GRAPHICS_BIND_TYPE bind_type) {
		// Can return as is for the moment
		return (D3D11_BIND_FLAG)bind_type;
	}

	inline ECS_GRAPHICS_BIND_TYPE GetGraphicsBindFromNative(D3D11_BIND_FLAG bind_type) {
		return (ECS_GRAPHICS_BIND_TYPE)bind_type;
	}

	inline ECS_GRAPHICS_BIND_TYPE GetGraphicsBindFromNative(int bind_type) {
		return (ECS_GRAPHICS_BIND_TYPE)bind_type;
	}

	inline D3D11_CPU_ACCESS_FLAG GetGraphicsNativeCPUAccess(ECS_GRAPHICS_CPU_ACCESS cpu_access) {
		return (D3D11_CPU_ACCESS_FLAG)cpu_access;
	}

	inline ECS_GRAPHICS_CPU_ACCESS GetGraphicsCPUAccessFromNative(unsigned int cpu_flag) {
		return (ECS_GRAPHICS_CPU_ACCESS)cpu_flag;
	}

	inline D3D11_USAGE GetGraphicsNativeUsage(ECS_GRAPHICS_USAGE usage) {
		return (D3D11_USAGE)usage;
	}

	inline ECS_GRAPHICS_USAGE GetGraphicsUsageFromNative(D3D11_USAGE usage) {
		return (ECS_GRAPHICS_USAGE)usage;
	}

	inline D3D11_MAP GetGraphicsNativeMapType(ECS_GRAPHICS_MAP_TYPE map_type) {
		return (D3D11_MAP)map_type;
	}

	inline ECS_GRAPHICS_MAP_TYPE GetGraphicsMapTypeFromNative(D3D11_MAP map_type) {
		return (ECS_GRAPHICS_MAP_TYPE)map_type;
	}

	inline unsigned int GetGraphicsNativeMiscFlags(ECS_GRAPHICS_MISC_FLAGS flags) {
		return (unsigned int)flags;
	}

	inline ECS_GRAPHICS_MISC_FLAGS GetGraphicsMiscFlagsFromNative(unsigned int flags) {
		return (ECS_GRAPHICS_MISC_FLAGS)flags;
	}

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

	struct ECS_REFLECT ShaderMacro {
		const char* name;
		const char* definition;
	};

	enum ShaderTarget : unsigned char {
		ECS_SHADER_TARGET_5_0,
		ECS_SHADER_TARGET_5_1,
		ECS_SHADER_TARGET_COUNT
	};

	enum ShaderCompileFlags : unsigned char {
		ECS_SHADER_COMPILE_NONE = 0,
		ECS_SHADER_COMPILE_DEBUG = 1 << 0,
		ECS_SHADER_COMPILE_OPTIMIZATION_LOWEST = 1 << 1,
		ECS_SHADER_COMPILE_OPTIMIZATION_LOW = 1 << 2,
		ECS_SHADER_COMPILE_OPTIMIZATION_HIGH = 1 << 3,
		ECS_SHADER_COMPILE_OPTIMIZATION_HIGHEST = 1 << 4
	};

	// Default is no macros, shader target 5 and no compile flags
	struct ShaderCompileOptions {
		Stream<ShaderMacro> macros = { nullptr, 0 };
		ShaderTarget target = ECS_SHADER_TARGET_5_0;
		ShaderCompileFlags compile_flags = ECS_SHADER_COMPILE_NONE;
	};
	
	struct ECSENGINE_API ColorFloat;

	using GraphicsContext = ID3D11DeviceContext;
	using GraphicsDevice = ID3D11Device;

	struct ECSENGINE_API Color {
		Color();
		Color(unsigned char red);

		Color(unsigned char red, unsigned char green);

		Color(unsigned char red, unsigned char green, unsigned char blue);

		Color(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

		// normalized values
		Color(float red, float green, float blue, float alpha);

		Color(ColorFloat color);

		Color(const unsigned char* values);

		Color(const float* values);

		Color(const Color& other) = default;
		Color& operator = (const Color& other) = default;

		bool operator == (const Color& other) const;
		bool operator != (const Color& other) const;

		// these operators do not apply for alpha
		Color operator + (const Color& other) const;
		Color operator * (const Color& other) const;
		Color operator * (float percentage) const;

		static constexpr float GetRange() {
			return (float)255;
		}

		void Normalize(float* values) const;

		union {
			struct {
				unsigned char red;
				unsigned char green;
				unsigned char blue;
			};
			struct {
				unsigned char hue;
				unsigned char saturation;
				unsigned char value;
			};
		};
		unsigned char alpha;
	};

	struct ECSENGINE_API ColorFloat {
		ColorFloat();

		ColorFloat(float red);

		ColorFloat(float red, float green);

		ColorFloat(float red, float green, float blue);

		ColorFloat(float red, float green, float blue, float alpha);

		ColorFloat(Color color);

		ColorFloat(const float* values);

		ColorFloat(const ColorFloat& other) = default;
		ColorFloat& operator = (const ColorFloat& other) = default;

		ColorFloat operator * (const ColorFloat& other) const;
		ColorFloat operator * (float percentage) const;
		ColorFloat operator + (const ColorFloat& other) const;
		ColorFloat operator - (const ColorFloat& other) const;

		static constexpr float GetRange() {
			return 1.0f;
		}
		void Normalize(float* values) const;

		union {
			struct {
				float red;
				float green;
				float blue;
			};
			struct {
				float hue;
				float saturation;
				float value;
			};
		};
		float alpha;
	};

	struct ECSENGINE_API VertexBuffer {
		VertexBuffer();
		VertexBuffer(UINT _stride, UINT _size, ID3D11Buffer* _buffer);

		VertexBuffer(const VertexBuffer& other) = default;
		VertexBuffer& operator = (const VertexBuffer& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Buffer* Interface() const {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static VertexBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		unsigned int stride;
		unsigned int size;
		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API IndexBuffer {
		IndexBuffer();

		IndexBuffer(const IndexBuffer& other) = default;
		IndexBuffer& operator = (const IndexBuffer& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Buffer* Interface() const {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static IndexBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		unsigned int count;
		unsigned int int_size;
		ID3D11Buffer* buffer;
	};

	// The byte code is mostly needed to check the input layout
	// against the shader's signature; Path is stable and null terminated
	// The byte code can be released after the input layout has been created
	struct ECSENGINE_API VertexShader {
		VertexShader();

		VertexShader(const VertexShader& other) = default;
		VertexShader& operator = (const VertexShader& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11VertexShader* Interface() const {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static VertexShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		Stream<void> byte_code;
		ID3D11VertexShader* shader;
	};

	struct ECSENGINE_API InputLayout {
		InputLayout();

		InputLayout(const InputLayout& other) = default;
		InputLayout& operator = (const InputLayout& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			layout->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11InputLayout* Interface() const {
			return layout;
		}

		inline unsigned int Release() {
			return layout->Release();
		}

		static InputLayout RawCreate(GraphicsDevice* device, Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, Stream<void> byte_code);

		ID3D11InputLayout* layout;
	};

	// Path is stable and null terminated
	struct ECSENGINE_API PixelShader {
		PixelShader();
		PixelShader(ID3D11PixelShader* _shader) : shader(_shader) {}

		PixelShader(const PixelShader& other) = default;
		PixelShader& operator = (const PixelShader& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11PixelShader* Interface() const {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static PixelShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ID3D11PixelShader* shader;
	};

	struct ECSENGINE_API GeometryShader {
		GeometryShader() : shader(nullptr) {}
		GeometryShader(ID3D11GeometryShader* _shader) : shader(_shader) {}

		GeometryShader(const GeometryShader& other) = default;
		GeometryShader& operator = (const GeometryShader& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11GeometryShader* Interface() const {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static GeometryShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ID3D11GeometryShader* shader;
	};

	struct ECSENGINE_API DomainShader {
		DomainShader() : shader(nullptr) {}
		DomainShader(ID3D11DomainShader* _shader) : shader(_shader) {}

		DomainShader(const DomainShader& other) = default;
		DomainShader& operator = (const DomainShader& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11DomainShader* Interface() const {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static DomainShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ID3D11DomainShader* shader;
	};

	struct ECSENGINE_API HullShader {
		HullShader() : shader(nullptr) {}
		HullShader(ID3D11HullShader* _shader) : shader(_shader) {}

		HullShader(const HullShader& other) = default;
		HullShader& operator = (const HullShader& other) = default;
		
		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}
		
		inline ID3D11HullShader* Interface() const {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static HullShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ID3D11HullShader* shader;
	};

	// Path is stable and null terminated
	struct ECSENGINE_API ComputeShader {
		ComputeShader() : shader(nullptr) {}

		ComputeShader(const ComputeShader& other) = default;
		ComputeShader& operator = (const ComputeShader& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11ComputeShader* Interface() const {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static ComputeShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ID3D11ComputeShader* shader;
	};

	struct ECSENGINE_API Topology {
		Topology();
		Topology(D3D11_PRIMITIVE_TOPOLOGY _topology);

		Topology(const Topology& other) = default;
		Topology& operator = (const Topology& other) = default;

		D3D11_PRIMITIVE_TOPOLOGY value;
	};

	struct ECSENGINE_API Texture1D {
		using RawDescriptor = D3D11_TEXTURE1D_DESC;
		using RawInterface = ID3D11Texture1D;

		Texture1D();
		Texture1D(ID3D11Resource* _resource);
		Texture1D(ID3D11Texture1D* _tex);

		Texture1D(const Texture1D& other) = default;
		Texture1D& operator = (const Texture1D& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Texture1D* Interface() const {
			return tex;
		}

		inline unsigned int Release() {
			return tex->Release();
		}

		static Texture1D RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture1D* tex;
	};

	struct ECSENGINE_API Texture2D {
		using RawDescriptor = D3D11_TEXTURE2D_DESC;
		using RawInterface = ID3D11Texture2D;

		Texture2D();
		Texture2D(ID3D11Resource* _resource);
		Texture2D(ID3D11Texture2D* _tex);

		Texture2D(const Texture2D& other) = default;
		Texture2D& operator = (const Texture2D& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Texture2D* Interface() const {
			return tex;
		}

		inline unsigned int Release() {
			return tex->Release();
		}

		static Texture2D RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture2D* tex;
	};

	struct ECSENGINE_API Texture3D {
		using RawDescriptor = D3D11_TEXTURE3D_DESC;
		using RawInterface = ID3D11Texture3D;

		Texture3D();
		Texture3D(ID3D11Resource* _resource);
		Texture3D(ID3D11Texture3D* _tex);
		//Texture3D(Microsoft::WRL::ComPtr<ID3D11Texture3D> _tex);

		Texture3D(const Texture3D& other) = default;
		Texture3D& operator = (const Texture3D& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Texture3D* Interface() const {
			return tex;
		}

		inline unsigned int Release() {
			return tex->Release();
		}

		static Texture3D RawCreate(GraphicsDevice* graphics, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture3D* tex;
	};

	struct ECSENGINE_API TextureCube {
		using RawDescriptor = D3D11_TEXTURE2D_DESC;
		using RawInterface = ID3D11Texture2D;

		TextureCube();
		TextureCube(ID3D11Resource* _resource);
		TextureCube(ID3D11Texture2D* _text);

		TextureCube(const TextureCube& other) = default;
		TextureCube& operator = (const TextureCube& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Texture2D* Interface() const {
			return tex;
		}

		inline unsigned int Release() {
			return tex->Release();
		}

		static TextureCube RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture2D* tex;
	};

	enum TextureCubeFace {
		ECS_TEXTURE_CUBE_X_POS,
		ECS_TEXTURE_CUBE_X_NEG,
		ECS_TEXTURE_CUBE_Y_POS,
		ECS_TEXTURE_CUBE_Y_NEG,
		ECS_TEXTURE_CUBE_Z_POS,
		ECS_TEXTURE_CUBE_Z_NEG
	};

	struct ECSENGINE_API ResourceView {
		ResourceView();
		ResourceView(ID3D11ShaderResourceView* _view);

		ResourceView(const ResourceView& other) = default;
		ResourceView& operator = (const ResourceView& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			view->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11ShaderResourceView* Interface() const {
			return view;
		}

		inline unsigned int Release() {
			return view->Release();
		}

		static ResourceView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_SHADER_RESOURCE_VIEW_DESC* descriptor);

		// It will copy using the descriptor from the view
		static ResourceView RawCopy(GraphicsDevice* device, ID3D11Resource* resource, ResourceView view);

		ID3D11ShaderResourceView* view;
	};

	struct ECSENGINE_API RenderTargetView {
		RenderTargetView() : view(nullptr) {}
		RenderTargetView(ID3D11RenderTargetView* _target) : view(_target) {}

		RenderTargetView(const RenderTargetView& other) = default;
		RenderTargetView& operator = (const RenderTargetView& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			view->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11RenderTargetView* Interface() const {
			return view;
		}

		inline unsigned int Release() {
			return view->Release();
		}

		static RenderTargetView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_RENDER_TARGET_VIEW_DESC* descriptor);

		// It will copy using the descriptor from the view
		static RenderTargetView RawCopy(GraphicsDevice* device, ID3D11Resource* resource, RenderTargetView view);

		ID3D11RenderTargetView* view;
	};

	struct ECSENGINE_API DepthStencilView {
		DepthStencilView() : view(nullptr) {}
		DepthStencilView(ID3D11DepthStencilView* _view) : view(_view) {}

		DepthStencilView(const DepthStencilView& other) = default;
		DepthStencilView& operator = (const DepthStencilView& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			view->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11DepthStencilView* Interface() const {
			return view;
		}

		inline unsigned int Release() {
			return view->Release();
		}

		static DepthStencilView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_DEPTH_STENCIL_VIEW_DESC* descriptor);

		// It will copy using the descriptor from the view
		static DepthStencilView RawCopy(GraphicsDevice* device, ID3D11Resource* resource, DepthStencilView view);

		ID3D11DepthStencilView* view;
	};

	struct ECSENGINE_API UAView {
		UAView() : view(nullptr) {}
		UAView(ID3D11UnorderedAccessView* _view) : view(_view) {}

		UAView(const UAView& other) = default;
		UAView& operator = (const UAView& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			view->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11UnorderedAccessView* Interface() const {
			return view;
		}

		inline unsigned int Release() {
			return view->Release();
		}

		static UAView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* descriptor);

		// It will copy using the descriptor from the view
		static UAView RawCopy(GraphicsDevice* device, ID3D11Resource* resource, UAView view);

		ID3D11UnorderedAccessView* view;
	};

	struct ECSENGINE_API ConstantBuffer {
		ConstantBuffer();
		ConstantBuffer(ID3D11Buffer* buffer);

		ConstantBuffer(const ConstantBuffer& other) = default;
		ConstantBuffer& operator = (const ConstantBuffer& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Buffer* Interface() const {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static ConstantBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API StandardBuffer {
		StandardBuffer() : buffer(nullptr) {}
		StandardBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		StandardBuffer(const StandardBuffer& other) = default;
		StandardBuffer& operator = (const StandardBuffer& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Buffer* Interface() const {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static StandardBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
		size_t count;
	};

	struct ECSENGINE_API StructuredBuffer {
		StructuredBuffer() : buffer(nullptr) {}
		StructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		StructuredBuffer(const StructuredBuffer& other) = default;
		StructuredBuffer& operator = (const StructuredBuffer& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Buffer* Interface() const {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static StructuredBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API IndirectBuffer {
		IndirectBuffer() : buffer(nullptr) {}
		IndirectBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		IndirectBuffer(const IndirectBuffer& other) = default;
		IndirectBuffer& operator = (const IndirectBuffer& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Buffer* Interface() const {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static IndirectBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API UABuffer {
		UABuffer() : buffer(nullptr), element_count(0) {}
		UABuffer(ID3D11Buffer* _buffer) : buffer(_buffer), element_count(0) {}

		UABuffer(const UABuffer& other) = default;
		UABuffer& operator = (const UABuffer& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Buffer* Interface() const {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static UABuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		size_t element_count;
		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API AppendStructuredBuffer {
		AppendStructuredBuffer() : buffer(nullptr), element_count(0) {}
		AppendStructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer), element_count(0) {}

		AppendStructuredBuffer(const AppendStructuredBuffer& other) = default;
		AppendStructuredBuffer& operator = (const AppendStructuredBuffer& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Buffer* Interface() const {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static AppendStructuredBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		size_t element_count;
		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API ConsumeStructuredBuffer {
		ConsumeStructuredBuffer() : buffer(nullptr), element_count(0) {}
		ConsumeStructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer), element_count(0) {}

		ConsumeStructuredBuffer(const ConsumeStructuredBuffer& other) = default;
		ConsumeStructuredBuffer& operator = (const ConsumeStructuredBuffer& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		ID3D11Resource* GetResource() const;

		inline ID3D11Buffer* Interface() const {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static ConsumeStructuredBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		size_t element_count;
		ID3D11Buffer* buffer;
	};

	struct SamplerDescriptor {
		inline void SetAddressType(ECS_SAMPLER_ADDRESS_TYPE type) {
			address_type_u = type;
			address_type_v = type;
			address_type_w = type;
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

	struct ECSENGINE_API SamplerState {
		SamplerState();
		SamplerState(ID3D11SamplerState* _sampler);

		SamplerState(const SamplerState& other) = default;
		SamplerState& operator = (const SamplerState& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			sampler->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11SamplerState* Interface() const {
			return sampler;
		}

		inline unsigned int Release() {
			return sampler->Release();
		}

		static SamplerState RawCreate(GraphicsDevice* device, const D3D11_SAMPLER_DESC* descriptor);

		ID3D11SamplerState* sampler;
	};

	struct ECSENGINE_API BlendState {
		BlendState() : state(nullptr) {}
		BlendState(ID3D11BlendState* _state) : state(_state) {}

		BlendState(const BlendState& other) = default;
		BlendState& operator = (const BlendState& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			state->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11BlendState* Interface() const {
			return state;
		}

		inline unsigned int Release() {
			return state->Release();
		}

		ID3D11BlendState* state;
	};

	struct ECSENGINE_API RasterizerState {
		RasterizerState() : state(nullptr) {}
		RasterizerState(ID3D11RasterizerState* _state) : state(_state) {}

		RasterizerState(const RasterizerState& other) = default;
		RasterizerState& operator = (const RasterizerState& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			state->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11RasterizerState* Interface() const {
			return state;
		}

		inline unsigned int Release() {
			return state->Release();
		}

		ID3D11RasterizerState* state;
	};

	struct ECSENGINE_API DepthStencilState {
		DepthStencilState() : state(nullptr) {}
		DepthStencilState(ID3D11DepthStencilState* _state) : state(_state) {}

		DepthStencilState(const DepthStencilState& other) = default;
		DepthStencilState& operator = (const DepthStencilState& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			state->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11DepthStencilState* Interface() const {
			return state;
		}

		inline unsigned int Release() {
			return state->Release();
		}

		ID3D11DepthStencilState* state;
	};

	struct ECSENGINE_API CommandList {
		CommandList() : list(nullptr) {}
		CommandList(ID3D11CommandList* _list) : list(_list) {}

		CommandList(const CommandList& other) = default;
		CommandList& operator = (const CommandList& other) = default;

		inline GraphicsDevice* GetDevice() const {
			GraphicsDevice* device;
			list->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11CommandList* Interface() const {
			return list;
		}

		inline unsigned int Release() {
			return list->Release();
		}

		ID3D11CommandList* list;
	};

	struct ECSENGINE_API Camera {
		Camera();
		Camera(float3 translation, float3 rotation);
		Camera(Matrix projection, float3 translation, float3 rotation);

		Camera(const Camera& other) = default;
		Camera& operator = (const Camera& other) = default;

		void SetPerspectiveProjection(float width, float height, float near_z, float far_z);

		void SetPerspectiveProjectionFOV(float angle_y, float aspect_ratio, float near_z, float far_z);

		void SetOrthographicProjection(float width, float height, float near_z, float far_z);

		Matrix GetViewProjectionMatrix() const;

		Matrix projection;
		float3 translation;
		float3 rotation;
	};

	struct ECSENGINE_API Mesh {
		Mesh() : name(nullptr), mapping_count(0) {}

		Mesh(const Mesh& other) = default;
		Mesh& operator = (const Mesh& other) = default;

		const char* name;
		IndexBuffer index_buffer;
		VertexBuffer vertex_buffers[ECS_MESH_BUFFER_COUNT];
		ECS_MESH_INDEX mapping[ECS_MESH_BUFFER_COUNT];
		unsigned char mapping_count;
	};

	struct ECSENGINE_API Submesh {
		Submesh() : name(nullptr), index_buffer_offset(0), vertex_buffer_offset(0), index_count(0), vertex_count(0) {}
		Submesh(unsigned int _index_buffer_offset, unsigned int _vertex_buffer_offset, unsigned int _index_count, unsigned int _vertex_count) 
			: name(nullptr), index_buffer_offset(_index_buffer_offset), vertex_buffer_offset(_vertex_buffer_offset), index_count(_index_count), 
		vertex_count(_vertex_count) {}

		Submesh(const Submesh& other) = default;
		Submesh& operator = (const Submesh& other) = default;

		const char* name;
		unsigned int index_buffer_offset;
		unsigned int vertex_buffer_offset;
		unsigned int index_count;
		unsigned int vertex_count;
	};

	// Contains the actual pipeline objects that can be bound to the graphics context
	struct ECSENGINE_API Material {
		Material();

		Material(const Material& other) = default;
		Material& operator = (const Material& other) = default;

		const char* name;
		InputLayout layout;
		VertexShader vertex_shader;
		PixelShader pixel_shader;
		DomainShader domain_shader;
		HullShader hull_shader;
		GeometryShader geometry_shader;
		ConstantBuffer vc_buffers[ECS_MATERIAL_VERTEX_CONSTANT_BUFFER_COUNT];
		ConstantBuffer pc_buffers[ECS_MATERIAL_PIXEL_CONSTANT_BUFFER_COUNT];
		ConstantBuffer dc_buffers[ECS_MATERIAL_DOMAIN_CONSTANT_BUFFER_COUNT];
		ConstantBuffer hc_buffers[ECS_MATERIAL_HULL_CONSTANT_BUFFER_COUNT];
		ConstantBuffer gc_buffers[ECS_MATERIAL_GEOMETRY_CONSTANT_BUFFER_COUNT];
		ResourceView vertex_textures[ECS_MATERIAL_VERTEX_TEXTURES_COUNT];
		ResourceView pixel_textures[ECS_MATERIAL_PIXEL_TEXTURES_COUNT];
		ResourceView domain_textures[ECS_MATERIAL_DOMAIN_TEXTURES_COUNT];
		ResourceView hull_textures[ECS_MATERIAL_HULL_TEXTURES_COUNT];
		ResourceView geometry_textures[ECS_MATERIAL_GEOMETRY_TEXTURES_COUNT];
		UAView unordered_views[ECS_MATERIAL_UAVIEW_COUNT];
		ECS_MESH_INDEX vertex_buffer_mappings[ECS_MESH_BUFFER_COUNT];
		unsigned char vertex_buffer_mapping_count;
		unsigned char vc_buffer_count;
		unsigned char pc_buffer_count;
		unsigned char dc_buffer_count;
		unsigned char hc_buffer_count;
		unsigned char gc_buffer_count;
		unsigned char vertex_texture_count;
		unsigned char pixel_texture_count;
		unsigned char domain_texture_count;
		unsigned char hull_texture_count;
		unsigned char geometry_texture_count;
		unsigned char unordered_view_count;
	};

	struct PBRMaterial {
		const char* name;
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

	struct CoallescedMesh {
		Mesh mesh;
		Stream<Submesh> submeshes;
	};

	// Each submesh has associated a material
	struct PBRMesh {
		CoallescedMesh mesh;
		PBRMaterial* materials;
	};

	ECSENGINE_API void SetPBRMaterialTexture(PBRMaterial* material, uintptr_t& memory, Stream<wchar_t> texture, PBRMaterialTextureIndex texture_index);

	ECSENGINE_API void AllocatePBRMaterial(
		PBRMaterial& material, 
		Stream<char> name, 
		Stream<PBRMaterialMapping> mappings,
		AllocatorPolymorphic allocator
	);

	// Releases the memory used for the texture names and the material name - it's coallesced
	// in the name buffer
	ECSENGINE_API void FreePBRMaterial(const PBRMaterial& material, AllocatorPolymorphic allocator);

	// It will search every directory in order to find each texture - they can be situated
	// in different folders; the texture mask can be used to tell the function which textures
	// to look for, by default it will search for all
	// Might change the texture mask 
	ECSENGINE_API PBRMaterial CreatePBRMaterialFromName(
		Stream<char> material_name,
		Stream<char> texture_base_name, 
		Stream<wchar_t> search_directory, 
		AllocatorPolymorphic allocator,
		Stream<PBRMaterialTextureIndex>* texture_mask = nullptr
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
		Stream<PBRMaterialTextureIndex>* texture_mask = nullptr
	);

	ECSENGINE_API VertexBuffer GetMeshVertexBuffer(const Mesh& mesh, ECS_MESH_INDEX buffer_type);

	// It does not release the vertex buffer that is being replaced
	ECSENGINE_API void SetMeshVertexBuffer(Mesh& mesh, ECS_MESH_INDEX buffer_type, VertexBuffer buffer);

}