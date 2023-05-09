#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"
#include "ShaderReflection.h"
#include "../Allocators/MemoryManager.h"
#include "../Containers/AtomicStream.h"

#define ECS_PIXEL_SHADER_SOURCE(name) L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Pixel\\" TEXT(STRING(name.hlsl))
#define ECS_VERTEX_SHADER_SOURCE(name) L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Vertex\\" TEXT(STRING(name.hlsl))
#define ECS_COMPUTE_SHADER_SOURCE(name) L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Compute\\" TEXT(STRING(name.hlsl))
#define ECS_SHADER_DIRECTORY L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders"

namespace ECSEngine {

#ifdef ECSENGINE_PLATFORM_WINDOWS

#ifdef ECSENGINE_DIRECTX11

#define ECS_GRAPHICS_MAX_RENDER_TARGETS_BIND 4
#define ECS_GRAPHICS_INTERNAL_RESOURCE_GROW_FACTOR 1.5f

	// If creating a swap chain the window size needs to be specified
	// If not then it can be { 0, 0 }, in that case not creating a render target and a depth target
	// The window size needs to be specified in pixels
	struct GraphicsDescriptor {
		HWND hWnd;
		uint2 window_size;
		MemoryManager* allocator;
		bool gamma_corrected = true;
		bool discrete_gpu = false;
		bool create_swap_chain = true;
	};

	// Default arguments all but width; initial_data can be set to fill the texture
	// The initial data is a stream of Stream<void> for each mip map data
	struct GraphicsTexture1DDescriptor {
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
	struct GraphicsTexture2DDescriptor {
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
	struct GraphicsTexture3DDescriptor {
		uint3 size;
		unsigned int array_size = 1u;
		unsigned int mip_levels = 0u;

		Stream<Stream<void>> mip_data = { nullptr, 0 };
		ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DEFAULT;
		ECS_GRAPHICS_CPU_ACCESS cpu_flag = ECS_GRAPHICS_CPU_ACCESS_NONE;
		ECS_GRAPHICS_BIND_TYPE bind_flag = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		ECS_GRAPHICS_MISC_FLAGS misc_flag = ECS_GRAPHICS_MISC_NONE;
	};

	// The initial data is a stream of Stream<void> for each mip map data
	struct GraphicsTextureCubeDescriptor {
		uint2 size;
		unsigned int mip_levels = 0u;

		Stream<Stream<void>> mip_data = { nullptr, 0 };
		ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DEFAULT;
		ECS_GRAPHICS_CPU_ACCESS cpu_flag = ECS_GRAPHICS_CPU_ACCESS_NONE;
		ECS_GRAPHICS_BIND_TYPE bind_flag = ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		ECS_GRAPHICS_MISC_FLAGS misc_flag = ECS_GRAPHICS_MISC_NONE;
	};

	struct GraphicsRenderDestinationOptions {
		bool gamma_corrected = true;
		bool no_stencil = false;
		ECS_GRAPHICS_MISC_FLAGS render_misc = ECS_GRAPHICS_MISC_NONE;
		ECS_GRAPHICS_MISC_FLAGS depth_misc = ECS_GRAPHICS_MISC_NONE;
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

	struct GraphicsPipelineRenderState {
		GraphicsPipelineBlendState blend_state;
		GraphicsPipelineDepthStencilState depth_stencil_state;
		GraphicsPipelineRasterizerState rasterizer_state;
	};

	struct GraphicsBoundViews {
		RenderTargetView target;
		DepthStencilView depth_stencil;
	};

	struct GraphicsPipelineState {
		GraphicsPipelineRenderState render_state;
		GraphicsBoundViews views;
		GraphicsViewport viewport;
	};

	enum GraphicsShaderHelpers : unsigned char {
		ECS_GRAPHICS_SHADER_HELPER_CREATE_TEXTURE_CUBE,
		ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_TEXTURE_CUBE,
		ECS_GRAPHICS_SHADER_HELPER_CREATE_DIFFUSE_ENVIRONMENT,
		ECS_GRAPHICS_SHADER_HELPER_CREATE_SPECULAR_ENVIRONEMNT,
		ECS_GRAPHICS_SHADER_HELPER_BRDF_INTEGRATION,
		ECS_GRAPHICS_SHADER_HELPER_GLTF_THUMBNAIL,
		ECS_GRAPHICS_SHADER_HELPER_COUNT
	};

	struct GraphicsShaderHelper {
		VertexShader vertex;
		PixelShader pixel;
		InputLayout input_layout;
		SamplerState pixel_sampler;
	};

	enum ECS_GRAPHICS_DEVICE_ERROR : unsigned char {
		ECS_GRAPHICS_DEVICE_ERROR_NONE,
		ECS_GRAPHICS_DEVICE_ERROR_HUNG,
		ECS_GRAPHICS_DEVICE_ERROR_REMOVED,
		ECS_GRAPHICS_DEVICE_ERROR_RESET,
		ECS_GRAPHICS_DEVICE_ERROR_DRIVER_INTERNAL_ERROR,
		ECS_GRAPHICS_DEVICE_ERROR_INVALID_CALL
	};

	// Must be String matched with ECS_GRAPHICS_RESOURCE_TYPE_STRING
	enum ECS_GRAPHICS_RESOURCE_TYPE : unsigned char {
		ECS_GRAPHICS_RESOURCE_VERTEX_SHADER,
		ECS_GRAPHICS_RESOURCE_PIXEL_SHADER,
		ECS_GRAPHICS_RESOURCE_DOMAIN_SHADER,
		ECS_GRAPHICS_RESOURCE_HULL_SHADER,
		ECS_GRAPHICS_RESOURCE_GEOMETRY_SHADER,
		ECS_GRAPHICS_RESOURCE_COMPUTE_SHADER,
		ECS_GRAPHICS_RESOURCE_INDEX_BUFFER,
		ECS_GRAPHICS_RESOURCE_INPUT_LAYOUT,
		ECS_GRAPHICS_RESOURCE_CONSTANT_BUFFER,
		ECS_GRAPHICS_RESOURCE_VERTEX_BUFFER,
		ECS_GRAPHICS_RESOURCE_RESOURCE_VIEW,
		ECS_GRAPHICS_RESOURCE_SAMPLER_STATE,
		ECS_GRAPHICS_RESOURCE_UA_VIEW,
		ECS_GRAPHICS_RESOURCE_GRAPHICS_CONTEXT,
		ECS_GRAPHICS_RESOURCE_RENDER_TARGET_VIEW,
		ECS_GRAPHICS_RESOURCE_DEPTH_STENCIL_VIEW,
		ECS_GRAPHICS_RESOURCE_STANDARD_BUFFER,
		ECS_GRAPHICS_RESOURCE_STRUCTURED_BUFFER,
		ECS_GRAPHICS_RESOURCE_UA_BUFFER,
		ECS_GRAPHICS_RESOURCE_INDIRECT_BUFFER,
		ECS_GRAPHICS_RESOURCE_APPEND_BUFFER,
		ECS_GRAPHICS_RESOURCE_CONSUME_BUFFER,
		ECS_GRAPHICS_RESOURCE_TEXTURE_1D,
		ECS_GRAPHICS_RESOURCE_TEXTURE_2D,
		ECS_GRAPHICS_RESOURCE_TEXTURE_3D,
		ECS_GRAPHICS_RESOURCE_TEXTURE_CUBE,
		ECS_GRAPHICS_RESOURCE_BLEND_STATE,
		ECS_GRAPHICS_RESOURCE_DEPTH_STENCIL_STATE,
		ECS_GRAPHICS_RESOURCE_RASTERIZER_STATE,
		ECS_GRAPHICS_RESOURCE_COMMAND_LIST,
		ECS_GRAPHICS_RESOURCE_DX_RESOURCE_INTERFACE,
		ECS_GRAPHICS_RESOURCE_TYPE_COUNT
	};

	extern const char* ECS_GRAPHICS_RESOURCE_TYPE_STRING[];

	ECS_INLINE Stream<char> GraphicsResourceTypeString(ECS_GRAPHICS_RESOURCE_TYPE type) {
		return ECS_GRAPHICS_RESOURCE_TYPE_STRING[type];
	}

	struct GraphicsInternalResource {
		inline GraphicsInternalResource& operator = (const GraphicsInternalResource& other) {
			interface_pointer = other.interface_pointer;
			type = other.type;
			debug_info = other.debug_info;
			is_deleted.store(other.is_deleted.load(std::memory_order_acquire), std::memory_order_release);

			return *this;
		}

		void* interface_pointer;
		DebugInfo debug_info;
		ECS_GRAPHICS_RESOURCE_TYPE type;
		std::atomic<bool> is_deleted;
	};

	struct GraphicsResourceSnapshot {
		ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) const {
			// This is maintained as a coallesced buffer
			ECSEngine::Deallocate(allocator, interface_pointers.buffer);
		}

		// These are maintained as SoA buffers
		Stream<void*> interface_pointers;
		Stream<ECS_GRAPHICS_RESOURCE_TYPE> types;
		Stream<DebugInfo> debug_infos;
	};

#define ECS_GRAPHICS_ASSERT_GRAPHICS_RESOURCE static_assert(std::is_same_v<Resource, VertexBuffer> || std::is_same_v<Resource, IndexBuffer> || std::is_same_v<Resource, VertexShader> \
	|| std::is_same_v<Resource, InputLayout> || std::is_same_v<Resource, PixelShader> || std::is_same_v<Resource, GeometryShader> \
		|| std::is_same_v<Resource, DomainShader> || std::is_same_v<Resource, HullShader> || std::is_same_v<Resource, ComputeShader> \
		|| std::is_same_v<Resource, ConstantBuffer> || std::is_same_v<Resource, ResourceView> || std::is_same_v<Resource, SamplerState> \
		|| std::is_same_v<Resource, Texture1D> || std::is_same_v<Resource, Texture2D> || std::is_same_v<Resource, Texture3D> \
		|| std::is_same_v<Resource, TextureCube> || std::is_same_v<Resource, RenderTargetView> || std::is_same_v<Resource, DepthStencilView> \
		|| std::is_same_v<Resource, StandardBuffer> || std::is_same_v<Resource, StructuredBuffer> || std::is_same_v<Resource, IndirectBuffer> \
		|| std::is_same_v<Resource, UABuffer> || std::is_same_v<Resource, AppendStructuredBuffer> || std::is_same_v<Resource, ConsumeStructuredBuffer> \
		|| std::is_same_v<Resource, UAView> || std::is_same_v<Resource, BlendState> || std::is_same_v<Resource, RasterizerState> \
		|| std::is_same_v<Resource, DepthStencilState> || std::is_same_v<Resource, CommandList>);

	template<typename Resource>
	ECS_GRAPHICS_RESOURCE_TYPE GetGraphicsResourceType(Resource resource) {
		if constexpr (std::is_same_v<Resource, BlendState>) {
			return ECS_GRAPHICS_RESOURCE_BLEND_STATE;
		}
		else if constexpr (std::is_same_v<Resource, RasterizerState>) {
			return ECS_GRAPHICS_RESOURCE_RASTERIZER_STATE;
		}
		else if constexpr (std::is_same_v<Resource, DepthStencilState>) {
			return ECS_GRAPHICS_RESOURCE_DEPTH_STENCIL_STATE;
		}
		else if constexpr (std::is_same_v<Resource, CommandList>) {
			return ECS_GRAPHICS_RESOURCE_COMMAND_LIST;
		}
		else if constexpr (std::is_same_v<Resource, GraphicsContext*>) {
			return ECS_GRAPHICS_RESOURCE_GRAPHICS_CONTEXT;
		}
		else if constexpr (std::is_same_v<Resource, VertexShader>) {
			return ECS_GRAPHICS_RESOURCE_VERTEX_SHADER;
		}
		else if constexpr (std::is_same_v<Resource, PixelShader>) {
			return ECS_GRAPHICS_RESOURCE_COMPUTE_SHADER;
		}
		else if constexpr (std::is_same_v<Resource, GeometryShader>) {
			return ECS_GRAPHICS_RESOURCE_GEOMETRY_SHADER;
		}
		else if constexpr (std::is_same_v<Resource, HullShader>) {
			return ECS_GRAPHICS_RESOURCE_HULL_SHADER;
		}
		else if constexpr (std::is_same_v<Resource, DomainShader>) {
			return ECS_GRAPHICS_RESOURCE_DOMAIN_SHADER;
		}
		else if constexpr (std::is_same_v<Resource, ComputeShader>) {
			return ECS_GRAPHICS_RESOURCE_COMPUTE_SHADER;
		}
		else if constexpr (std::is_same_v<Resource, IndexBuffer>) {
			return ECS_GRAPHICS_RESOURCE_INDEX_BUFFER;
		}
		else if constexpr (std::is_same_v<Resource, VertexBuffer>) {
			return ECS_GRAPHICS_RESOURCE_VERTEX_BUFFER;
		}
		else if constexpr (std::is_same_v<Resource, ConstantBuffer>) {
			return ECS_GRAPHICS_RESOURCE_CONSTANT_BUFFER;
		}
		else if constexpr (std::is_same_v<Resource, StandardBuffer>) {
			return ECS_GRAPHICS_RESOURCE_STANDARD_BUFFER;
		}
		else if constexpr (std::is_same_v<Resource, StructuredBuffer>) {
			return ECS_GRAPHICS_RESOURCE_STRUCTURED_BUFFER;
		}
		else if constexpr (std::is_same_v<Resource, UABuffer>) {
			return ECS_GRAPHICS_RESOURCE_UA_BUFFER;
		}
		else if constexpr (std::is_same_v<Resource, IndirectBuffer>) {
			return ECS_GRAPHICS_RESOURCE_INDIRECT_BUFFER;
		}
		else if constexpr (std::is_same_v<Resource, AppendStructuredBuffer>) {
			return ECS_GRAPHICS_RESOURCE_APPEND_BUFFER;
		}
		else if constexpr (std::is_same_v<Resource, ConsumeStructuredBuffer>) {
			return ECS_GRAPHICS_RESOURCE_CONSUME_BUFFER;
		}
		else if constexpr (std::is_same_v<Resource, InputLayout>) {
			return ECS_GRAPHICS_RESOURCE_INPUT_LAYOUT;
		}
		else if constexpr (std::is_same_v<Resource, SamplerState>) {
			return ECS_GRAPHICS_RESOURCE_SAMPLER_STATE;
		}
		else if constexpr (std::is_same_v<Resource, UAView>) {
			return ECS_GRAPHICS_RESOURCE_UA_VIEW;
		}
		else if constexpr (std::is_same_v<Resource, RenderTargetView>) {
			return ECS_GRAPHICS_RESOURCE_RENDER_TARGET_VIEW;
		}
		else if constexpr (std::is_same_v<Resource, ResourceView>) {
			return ECS_GRAPHICS_RESOURCE_RESOURCE_VIEW;
		}
		else if constexpr (std::is_same_v<Resource, DepthStencilView>) {
			return ECS_GRAPHICS_RESOURCE_DEPTH_STENCIL_VIEW;
		}
		else if constexpr (std::is_same_v<Resource, Texture1D>) {
			return ECS_GRAPHICS_RESOURCE_TEXTURE_1D;
		}
		else if constexpr (std::is_same_v<Resource, Texture2D>) {
			return ECS_GRAPHICS_RESOURCE_TEXTURE_2D;
		}
		else if constexpr (std::is_same_v<Resource, Texture3D>) {
			return ECS_GRAPHICS_RESOURCE_TEXTURE_3D;
		}
		else if constexpr (std::is_same_v<Resource, TextureCube>) {
			return ECS_GRAPHICS_RESOURCE_TEXTURE_CUBE;
		}
		else if constexpr (std::is_same_v<Resource, ID3D11Resource*>) {
			return ECS_GRAPHICS_RESOURCE_DX_RESOURCE_INTERFACE;
		}
		else {
			static_assert(false, "Invalid resource type when adding internal graphics resource");
		}
	}

	template<typename Resource>
	void* GetGraphicsResourceInterface(Resource resource) {
		if constexpr (std::is_same_v<Resource, GraphicsContext*>) {
			return resource;
		}
		else if constexpr (std::is_same_v<Resource, ID3D11Resource*>) {
			return resource;
		}
		else {
			return resource.Interface();
		}
	}

	ECSENGINE_API void FreeGraphicsResourceInterface(void* interface_pointer, ECS_GRAPHICS_RESOURCE_TYPE type);

	ECSENGINE_API MemoryManager DefaultGraphicsAllocator(GlobalMemoryManager* manager);

	// Returns how much memory it allocates in its initial allocation
	ECSENGINE_API size_t DefaultGraphicsAllocatorSize();

	/* It has an immediate and a deferred context. The deferred context can be used to generate CommandLists */
	struct ECSENGINE_API Graphics
	{
	public:
		Graphics(const GraphicsDescriptor* descriptor);

		Graphics(const Graphics& other);
		Graphics& operator = (const Graphics& other);

#pragma region Context Bindings

		// ----------------------------------------------- Context Bindings ------------------------------------------------------------------

		void BindIndexBuffer(IndexBuffer index_buffer);

		void BindInputLayout(InputLayout layout);

		void BindVertexShader(VertexShader shader);

		void BindPixelShader(PixelShader shader);

		void BindDomainShader(DomainShader shader);

		void BindHullShader(HullShader shader);

		void BindGeometryShader(GeometryShader shader);

		void BindComputeShader(ComputeShader shader);

		void BindVertexConstantBuffer(ConstantBuffer buffer, unsigned int slot = 0u);

		void BindVertexConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot = 0u);

		void BindPixelConstantBuffer(ConstantBuffer buffer, unsigned int slot = 0u);

		void BindPixelConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot = 0u);

		void BindDomainConstantBuffer(ConstantBuffer buffer, unsigned int slot = 0u);

		void BindDomainConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot = 0u);

		void BindHullConstantBuffer(ConstantBuffer buffer, unsigned int slot = 0u);

		void BindHullConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot = 0u);

		void BindGeometryConstantBuffer(ConstantBuffer buffer, unsigned int slot = 0u);

		void BindGeometryConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot = 0u);

		void BindComputeConstantBuffer(ConstantBuffer buffer, unsigned int slot = 0u);

		void BindComputeConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot = 0u);

		void BindVertexBuffer(VertexBuffer buffer, unsigned int slot = 0u);

		void BindVertexBuffers(Stream<VertexBuffer> buffers, unsigned int start_slot = 0u);

		void BindTopology(Topology topology);

		void BindPixelResourceView(ResourceView component, unsigned int slot = 0u);

		void BindPixelResourceViews(Stream<ResourceView> component, unsigned int start_slot = 0u);

		void BindVertexResourceView(ResourceView component, unsigned int slot = 0u);

		void BindVertexResourceViews(Stream<ResourceView> component, unsigned int start_slot = 0u);

		void BindDomainResourceView(ResourceView component, unsigned int slot = 0u);

		void BindDomainResourceViews(Stream<ResourceView> component, unsigned int start_slot = 0u);

		void BindHullResourceView(ResourceView component, unsigned int slot = 0u);

		void BindHullResourceViews(Stream<ResourceView> component, unsigned int start_slot = 0u);

		void BindGeometryResourceView(ResourceView component, unsigned int slot = 0u);

		void BindGeometryResourceViews(Stream<ResourceView> component, unsigned int start_slot = 0u);

		void BindComputeResourceView(ResourceView component, unsigned int slot = 0u);

		void BindComputeResourceViews(Stream<ResourceView> component, unsigned int start_slot = 0u);

		void BindSamplerState(SamplerState sampler, unsigned int slot = 0u);

		void BindSamplerStates(Stream<SamplerState> samplers, unsigned int start_slot = 0u);

		void BindPixelUAView(UAView view, unsigned int start_slot = 0u);

		void BindPixelUAViews(Stream<UAView> views, unsigned int start_slot = 0u);

		void BindComputeUAView(UAView view, unsigned int start_slot = 0u);

		void BindComputeUAViews(Stream<UAView> views, unsigned int start_slot = 0u);

		void BindRasterizerState(RasterizerState state);

		void BindDepthStencilState(DepthStencilState state, unsigned int stencil_ref = 0);

		void BindBlendState(BlendState state);

		void BindRenderTargetViewFromInitialViews();

		void BindRenderTargetViewFromInitialViews(GraphicsContext* context);

		void BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view);

		void BindViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth);

		void BindViewport(GraphicsViewport viewport);

		void BindDefaultViewport();

		void BindDefaultViewport(GraphicsContext* context);

		void BindMesh(const Mesh& mesh);

		void BindMesh(const Mesh& mesh, Stream<ECS_MESH_INDEX> mapping);

		void BindMaterial(const Material& material);

		void BindHelperShader(GraphicsShaderHelpers helper);

#pragma endregion

#pragma region Component Creation

		// ----------------------------------------------- Component Creation ----------------------------------------------------

		// It will create an empty index buffer - must be populated afterwards
		IndexBuffer CreateIndexBuffer(
			size_t int_size,
			size_t element_count,
			bool temporary = false, 
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DEFAULT, 
			ECS_GRAPHICS_CPU_ACCESS cpu_access = ECS_GRAPHICS_CPU_ACCESS_NONE, 
			ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		IndexBuffer CreateIndexBuffer(
			size_t int_size,
			size_t element_count,
			const void* data,
			bool temporary = false, 
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_IMMUTABLE,
			ECS_GRAPHICS_CPU_ACCESS cpu_access = ECS_GRAPHICS_CPU_ACCESS_NONE,
			ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		IndexBuffer CreateIndexBuffer(Stream<unsigned char> indices, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		IndexBuffer CreateIndexBuffer(Stream<unsigned short> indices, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		IndexBuffer CreateIndexBuffer(Stream<unsigned int> indices, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns a nullptr shader if it fails
		PixelShader CreatePixelShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns a nullptr shader if it fails
		PixelShader CreatePixelShaderFromSource(
			Stream<char> source_code, 
			ID3DInclude* include_policy,
			ShaderCompileOptions options = {}, 
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// Returns a nullptr shader if it fails
		VertexShader CreateVertexShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns a nullptr shader if it fails
		// The byte code is allocated from the graphics allocator. The reason for that pointer
		// is such that you can create an input layout from it.
		// It is automatically deallocated when creating an input layout.
		VertexShader CreateVertexShaderFromSource(
			Stream<char> source_code, 
			ID3DInclude* include_policy, 
			ShaderCompileOptions options = {},
			Stream<void>* byte_code = nullptr,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// Returns a nullptr shader if it fails
		DomainShader CreateDomainShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns a nullptr shader if it fails
		DomainShader CreateDomainShaderFromSource(
			Stream<char> source_code,
			ID3DInclude* include_policy, 
			ShaderCompileOptions options = {},
			bool temporary = false, 
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// Returns a nullptr shader if it fails
		HullShader CreateHullShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns a nullptr shader if it fails
		HullShader CreateHullShaderFromSource(
			Stream<char> source_code, 
			ID3DInclude* include_policy,
			ShaderCompileOptions options = {},
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// Returns a nullptr shader if it fails
		GeometryShader CreateGeometryShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns a nullptr shader if it fails
		GeometryShader CreateGeometryShaderFromSource(
			Stream<char> source_code, 
			ID3DInclude* include_policy, 
			ShaderCompileOptions options = {},
			bool temporary = false, 
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// Returns a nullptr shader if it fails
		ComputeShader CreateComputeShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns a nullptr shader if it fails
		ComputeShader CreateComputeShaderFromSource(
			Stream<char> source_code,
			ID3DInclude* include_policy, 
			ShaderCompileOptions options = {},
			bool temporary = false, 
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// Returns a nullptr shader if it fails
		// Returns only the interface
		void* CreateShader(Stream<void> byte_code, ECS_SHADER_TYPE type, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns a nullptr shader if it fails
		// Returns only the interface
		// The byte code is relevant only when the shader is a vertex shader
		void* CreateShaderFromSource(
			Stream<char> source_code,
			ECS_SHADER_TYPE type, 
			ID3DInclude* include_policy,
			ShaderCompileOptions options = {},
			Stream<void>* byte_code = nullptr,
			bool temporary = false, 
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// Compiles the shader source into byte code
		// If the allocator is nullptr, it will allocate from the internal allocator
		Stream<void> CompileShaderToByteCode(
			Stream<char> source_code,
			ECS_SHADER_TYPE type,
			ID3DInclude* include_policy,
			AllocatorPolymorphic allocator = { nullptr },
			ShaderCompileOptions options = {}
		);

		InputLayout CreateInputLayout(
			Stream<D3D11_INPUT_ELEMENT_DESC> descriptor,
			Stream<void> vertex_shader_byte_code, 
			bool deallocate_byte_code = true,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		VertexBuffer CreateVertexBuffer(
			size_t element_size, 
			size_t element_count, 
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DYNAMIC, 
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_WRITE, 
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		VertexBuffer CreateVertexBuffer(
			size_t element_size,
			size_t element_count,
			const void* buffer,
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_IMMUTABLE,
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_NONE,
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);
		
		ConstantBuffer CreateConstantBuffer(
			size_t byte_size,
			const void* buffer,
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DYNAMIC,
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_WRITE,
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ConstantBuffer CreateConstantBuffer(
			size_t byte_size,
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DYNAMIC,
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_WRITE,
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		StandardBuffer CreateStandardBuffer(
			size_t element_size,
			size_t element_count,
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DYNAMIC,
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_WRITE,
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		StandardBuffer CreateStandardBuffer(
			size_t element_size,
			size_t element_count,
			const void* data,
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_IMMUTABLE,
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_NONE,
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		StructuredBuffer CreateStructuredBuffer(
			size_t element_size,
			size_t element_count,
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DYNAMIC,
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_WRITE,
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		StructuredBuffer CreateStructuredBuffer(
			size_t element_size,
			size_t element_count,
			const void* data,
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_IMMUTABLE,
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_NONE,
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		UABuffer CreateUABuffer(
			size_t element_size,
			size_t element_count,
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_DYNAMIC,
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_WRITE,
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		UABuffer CreateUABuffer(
			size_t element_size,
			size_t element_count,
			const void* data,
			bool temporary = false,
			ECS_GRAPHICS_USAGE usage = ECS_GRAPHICS_USAGE_IMMUTABLE,
			ECS_GRAPHICS_CPU_ACCESS cpuFlags = ECS_GRAPHICS_CPU_ACCESS_NONE,
			ECS_GRAPHICS_MISC_FLAGS miscFlags = ECS_GRAPHICS_MISC_NONE,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		IndirectBuffer CreateIndirectBuffer(bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		AppendStructuredBuffer CreateAppendStructuredBuffer(size_t element_size, size_t element_count, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		ConsumeStructuredBuffer CreateConsumeStructuredBuffer(size_t element_size, size_t element_count, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		SamplerState CreateSamplerState(const SamplerDescriptor& descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		Texture1D CreateTexture(const GraphicsTexture1DDescriptor* descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		Texture2D CreateTexture(const GraphicsTexture2DDescriptor* descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Must be called in a single threaded context because it uses the immediate context
		// Cannot create mips for BC formats directly
		ResourceView CreateTextureWithMips(Stream<void> first_mip, ECS_GRAPHICS_FORMAT format, uint2 size, bool temporary = false, DebugInfo debug_infp = ECS_DEBUG_INFO);

		Texture3D CreateTexture(const GraphicsTexture3DDescriptor* descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		TextureCube CreateTexture(const GraphicsTextureCubeDescriptor* descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ECS_GRAPHICS_FORMAT_UNKNOWN means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			Texture1D texture,
			ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_UNKNOWN,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ResourceView CreateTextureShaderViewResource(Texture1D texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ECS_GRAPHICS_FORMAT_UNKNOWN means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			Texture2D texture,
			ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_UNKNOWN,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ResourceView CreateTextureShaderViewResource(Texture2D texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ECS_GRAPHICS_FORMAT_UNKNOWN means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			Texture3D texture,
			ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_UNKNOWN,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ResourceView CreateTextureShaderViewResource(Texture3D texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// ECS_GRAPHICS_FORMAT_UNKNOWN means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			TextureCube texture, 
			ECS_GRAPHICS_FORMAT format = ECS_GRAPHICS_FORMAT_UNKNOWN,
			unsigned int most_detailed_mip = 0u, 
			unsigned int mip_levels = -1,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ResourceView CreateTextureShaderViewResource(TextureCube texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		RenderTargetView CreateRenderTargetView(Texture2D texture, unsigned int mip_level = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		RenderTargetView CreateRenderTargetView(TextureCube cube, TextureCubeFace face, unsigned int mip_level = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		DepthStencilView CreateDepthStencilView(Texture2D texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		ResourceView CreateBufferView(StandardBuffer buffer, ECS_GRAPHICS_FORMAT format, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		ResourceView CreateBufferView(StructuredBuffer buffer, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(UABuffer buffer, ECS_GRAPHICS_FORMAT format, unsigned int first_element = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(AppendStructuredBuffer buffer, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(ConsumeStructuredBuffer buffer, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(IndirectBuffer buffer, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(Texture1D texture, unsigned int mip_slice = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(Texture2D texture, unsigned int mip_slice = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(Texture3D texture, unsigned int mip_slice = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		RasterizerState CreateRasterizerState(const D3D11_RASTERIZER_DESC& descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		RasterizerState CreateRasterizerStateDefault(bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		DepthStencilState CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		DepthStencilState CreateDepthStencilStateDefault(bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		BlendState CreateBlendState(const D3D11_BLEND_DESC& descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		BlendState CreateBlendStateDefault(bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

#pragma endregion

#pragma region Resource release

		// Releases the graphics resources and the name if it has one
		void FreeMesh(const Mesh& mesh);

#pragma endregion

#pragma region Pipeline State Changes

		// ------------------------------------------- Pipeline State Changes ------------------------------------

		// Uses the immediate context. Do not rely on this call generating exact bit patterns since it is using floating point values
		void ClearRenderTarget(RenderTargetView target, ColorFloat color);

		// Uses the immediate context.
		void ClearDepthStencil(DepthStencilView depth_stencil, float depth = 1.0f, unsigned char stencil = 0);

		void ClearBackBuffer(float red, float green, float blue);

		void DisableAlphaBlending();

		void DisableAlphaBlending(GraphicsContext* context);

		void DisableDepth();

		void DisableDepth(GraphicsContext* context);

		void DisableCulling(bool wireframe = false);

		void DisableCulling(GraphicsContext* context, bool wireframe = false);

		void Dispatch(uint3 dispatch_size);
		
		void DispatchIndirect(IndirectBuffer indirect_buffer);

		void Draw(unsigned int vertex_count, unsigned int vertex_offset = 0u);

		void DrawIndexed(unsigned int index_count, unsigned int start_index = 0u, unsigned int base_vertex_location = 0);

		void DrawInstanced(unsigned int vertex_count, unsigned int instance_count, unsigned int vertex_buffer_offset = 0u, unsigned int instance_offset = 0u);

		void DrawIndexedInstanced(unsigned int index_count, unsigned int instance_count, unsigned int index_buffer_offset = 0u, unsigned int vertex_buffer_offset = 0u, unsigned int instance_offset = 0u);

		void DrawIndexedInstancedIndirect(IndirectBuffer buffer);

		void DrawInstancedIndirect(IndirectBuffer buffer);

		// Single mesh drawing - very inefficient, just use for small number of draws
		void DrawMesh(const Mesh& mesh, const Material& material);

		// Single mesh drawing - very inefficient, just use for small number of draws
		void DrawMesh(const CoallescedMesh& mesh, unsigned int submesh_index, const Material& material);

		// This will issue only a DrawIndexed command, does not bind any mesh/material
		void DrawSubmeshCommand(Submesh submesh);

		void EnableAlphaBlending();

		void EnableAlphaBlending(GraphicsContext* context);

		void EnableDepth();

		void EnableDepth(GraphicsContext* context);

		void EnableCulling(bool wireframe = false);

		void EnableCulling(GraphicsContext* context, bool wireframe = false);

		CommandList FinishCommandList(bool restore_state = false, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		void GenerateMips(ResourceView view);

		RenderTargetView GetBoundRenderTarget() const;

		DepthStencilView GetBoundDepthStencil() const;

		GraphicsViewport GetBoundViewport() const;

		void* MapBuffer(ID3D11Buffer* buffer, ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture(Texture1D texture, ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture(Texture2D texture, ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture(Texture3D texture, ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void UpdateBuffer(ID3D11Buffer* buffer, const void* data, size_t data_size, ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD, unsigned int map_flags = 0u, unsigned int subresource_index = 0);

		void UnmapBuffer(ID3D11Buffer* buffer, unsigned int subresource_index = 0);

		void UnmapTexture(Texture1D texture, unsigned int subresource_index = 0);

		void UnmapTexture(Texture2D texture, unsigned int subresource_index = 0);

		void UnmapTexture(Texture3D texture, unsigned int subresource_index = 0);

#pragma endregion

#pragma region Shader Reflection

		// ------------------------------------------------- Shader Reflection --------------------------------------------------

		// Returns ECS_SHADER_TYPE_COUNT if it couldn't determine the shader type for a given shader
		ECS_SHADER_TYPE DetermineShaderType(Stream<char> source_code) const;

		ECS_INLINE const ShaderReflection* GetShaderReflection() const {
			return m_shader_reflection;
		}

		// The vertex byte code needs to be given from the compilation of the vertex shader
		InputLayout ReflectVertexShaderInput(
			Stream<char> source_code, 
			Stream<void> vertex_byte_code,
			bool deallocate_byte_code = true,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		bool ReflectShaderBuffers(Stream<char> source_code, CapacityStream<ShaderReflectedBuffer>& buffers, AllocatorPolymorphic allocator);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		bool ReflectShaderTextures(Stream<char> source_code, CapacityStream<ShaderReflectedTexture>& textures, AllocatorPolymorphic allocator);

		bool ReflectShaderMacros(Stream<char> source_code, CapacityStream<Stream<char>>* defined_macros, CapacityStream<Stream<char>>* conditional_macros, AllocatorPolymorphic allocator);
		
		// No memory needs to be allocated
		bool ReflectVertexBufferMapping(Stream<char> source_code, CapacityStream<ECS_MESH_INDEX>& mapping);

		// The memory for the names will be allocated from the given allocator
		bool ReflectShaderSamplers(Stream<char> source_code, CapacityStream<ShaderReflectedSampler>& samplers, AllocatorPolymorphic allocator);

#pragma endregion

#pragma region Getters and other operations
		
		// -------------------------------------------- Getters and other operations --------------------------------------------

		// Can optionally bind the render target and the depth stencil
		void ChangeInitialRenderTarget(RenderTargetView render_target, DepthStencilView depth_stencil, bool bind = true);

		// It does not acquire the lock - call this if inside a lock
		// Can be called only from a single thread
		void CommitInternalResourcesToBeFreed();

		void FreeTrackedResources();

		template<typename Resource>
		void AddInternalResource(Resource resource, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ECS_GRAPHICS_RESOURCE_TYPE resource_type = GetGraphicsResourceType(resource);
			void* interface_ptr = GetGraphicsResourceInterface(resource);

			GraphicsInternalResource internal_res = { 
				interface_ptr,
				debug_info,
				resource_type,
				false
			};

			unsigned int write_index = m_internal_resources.RequestInt(1);
			if (write_index >= m_internal_resources.capacity) {
				bool do_resizing = m_internal_resources_lock.try_lock();
				// The first one to acquire the lock - do the resizing or the flush of removals
				if (do_resizing) {
					// Spin wait while there are still readers
					SpinWait<'>'>(m_internal_resources_reader_count, (unsigned int)0);

					// Commit the removals
					CommitInternalResourcesToBeFreed();

					// Do the resizing if no resources were deleted from the main array
					if (m_internal_resources.size.load(ECS_RELAXED) >= m_internal_resources.capacity) {
						void* allocation = m_allocator->Allocate(sizeof(GraphicsInternalResource) * (m_internal_resources.capacity * ECS_GRAPHICS_INTERNAL_RESOURCE_GROW_FACTOR));
						memcpy(allocation, m_internal_resources.buffer, sizeof(GraphicsInternalResource) * m_internal_resources.capacity);
						m_allocator->Deallocate(m_internal_resources.buffer);
						m_internal_resources.InitializeFromBuffer(allocation, m_internal_resources.capacity, m_internal_resources.capacity * ECS_GRAPHICS_INTERNAL_RESOURCE_GROW_FACTOR);
					}
					write_index = m_internal_resources.RequestInt(1);
					m_internal_resources[write_index] = internal_res;
					m_internal_resources.FinishRequest(1);

					m_internal_resources_lock.unlock();
				}
				// Other thread got to do the resizing or freeing of the resources - stall until it finishes
				else {
					m_internal_resources_lock.wait_locked();
					// Rerequest the position
					write_index = m_internal_resources.RequestInt(1);
					if (write_index >= m_internal_resources.capacity) {
						// Retry again
						AddInternalResource(resource, debug_info);
						return;
					}
					else {
						m_internal_resources[write_index] = internal_res;
						m_internal_resources.FinishRequest(1);
					}
				}
			}
			// Write into the internal resources
			else {
				m_internal_resources[write_index] = internal_res;
				m_internal_resources.FinishRequest(1);
			}
		}

		template<typename Resource>
		void AddInternalResource(Resource resource, bool temporary, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (!temporary) {
				AddInternalResource(resource, debug_info);
			}
		}

		AllocatorPolymorphic Allocator() const;

		// Only for basic resources
		template<typename Resource>
		void FreeResource(Resource resource) {
			void* interface_pointer = GetGraphicsResourceInterface(resource);
			RemoveResourceFromTracking(interface_pointer);

			unsigned int count = -1;
			if constexpr (std::is_same_v<Resource, GraphicsContext*>) {
				count = resource->Release();
			}
			else if constexpr (std::is_same_v<Resource, ID3D11Resource*>) {
				count = resource->Release();
			}
			else {
				count = resource.Release();
			}
		}

		template<typename View>
		void FreeView(View view) {
			ID3D11Resource* resource = view.GetResource();
			FreeResource(view);
			// It doesn't matter what the type is - only the interface
			FreeResource(Texture2D(resource));
		}

		void FreeRenderDestination(RenderDestination destination);

		// It will assert FALSE if it doesn't exist
		void RemoveResourceFromTracking(void* resource);

		// If it doesn't find the resource, it does nothing
		void RemovePossibleResourceFromTracking(void* resource);

		void CreateRenderTargetViewFromSwapChain(bool gamma_corrected);
		
		void CreateWindowRenderTargetView(bool gamma_corrected);

		RenderDestination CreateRenderDestination(uint2 texture_size, GraphicsRenderDestinationOptions options = {});

		void CreateWindowDepthStencilView();

		GraphicsContext* CreateDeferredContext(unsigned int context_flags = 0u, DebugInfo debug_info = ECS_DEBUG_INFO);

		void FreeAllocatedBuffer(const void* buffer);

		void FreeAllocatedBufferTs(const void* buffer);

		void* GetAllocatorBuffer(size_t size);

		void* GetAllocatorBufferTs(size_t size);

		ECS_GRAPHICS_DEVICE_ERROR GetDeviceError();

		ECS_INLINE GraphicsDevice* GetDevice() {
			return m_device;
		}

		ECS_INLINE GraphicsContext* GetContext() {
			return m_context;
		}

		ECS_INLINE GraphicsContext* GetDeferredContext() {
			return m_deferred_context;
		}

		GraphicsPipelineBlendState GetBlendState() const;

		GraphicsPipelineDepthStencilState GetDepthStencilState() const;

		GraphicsPipelineRasterizerState GetRasterizerState() const;

		GraphicsPipelineRenderState GetPipelineRenderState() const;

		GraphicsBoundViews GetCurrentViews() const;

		GraphicsPipelineState GetPipelineState() const;

		GraphicsResourceSnapshot GetResourceSnapshot(AllocatorPolymorphic allocator) const;

		void GetWindowSize(unsigned int& width, unsigned int& height) const;

		uint2 GetWindowSize() const;

		void RestoreBlendState(GraphicsPipelineBlendState state);

		void RestoreDepthStencilState(GraphicsPipelineDepthStencilState state);

		void RestoreRasterizerState(GraphicsPipelineRasterizerState state);

		void RestorePipelineRenderState(GraphicsPipelineRenderState state);

		void RestoreBoundViews(GraphicsBoundViews views);

		void RestorePipelineState(const GraphicsPipelineState* state);

		// It will remove the resources which have been added in between the snapshot and the current state.
		// Returns true if all the resources from the previous snapshot are still valid else it returns false. 
		// Can optionally give a string to be built with the mismatches that are between the two states
		// It doesn't deallocate the snapshot (it must be done outside)!
		bool RestoreResourceSnapshot(GraphicsResourceSnapshot snapshot, CapacityStream<char>* mismatch_string = nullptr);

		void ResizeSwapChainSize(HWND hWnd, float width, float height);

		void ResizeViewport(float top_left_x, float top_left_y, float new_width, float new_height);

		// Returns true if the device was removed
		bool SwapBuffers(unsigned int sync_interval);

		void SetNewSize(HWND hWnd, unsigned int width, unsigned int height);

		// Transfers a shared GPU texture/buffer from a Graphics instance to another - should only create
		// another runtime DX11 reference to that texture, there should be no memory blit or copy
		// It does not affect samplers, input layouts, shaders, other pipeline objects (rasterizer/blend/depth states)
		// Use this function only for textures and buffers since these are the only ones who actually need transfering
		template<typename Resource>
		Resource TransferGPUResource(Resource resource, bool temporary = false);

		// Transfers the deep target buffer or texture and then creates a new texture view that points to it
		// Only views should be given
		template<typename View>
		View TransferGPUView(View view, bool temporary = false);

		// It will register the newly created resources if not temporary
		Mesh TransferMesh(const Mesh* mesh, bool temporary = false);

		// Will keep the same submeshes stream, only the mesh portion gets actually transferred
		// It will register the newly created resources if not temporary
		CoallescedMesh TransferCoallescedMesh(const CoallescedMesh* mesh, bool temporary = false);

		// Will keep the same material buffer and submesh stream, only the mesh portion gets actually transferred
		// It will register the newly created resources if not temporary
		PBRMesh TransferPBRMesh(const PBRMesh* mesh, bool temporary = false);

		// It will register the newly created resources if not temporary
		Material TransferMaterial(const Material* material, bool temporary = false);

#pragma endregion

	//private:
		uint2 m_window_size;
		GraphicsDevice* m_device;
		IDXGISwapChain* m_swap_chain;
		GraphicsContext* m_context;
		GraphicsContext* m_deferred_context;
		RenderTargetView m_target_view;
		RenderTargetView m_bound_render_targets[ECS_GRAPHICS_MAX_RENDER_TARGETS_BIND];
		size_t m_bound_render_target_count;
		DepthStencilView m_depth_stencil_view;
		DepthStencilView m_current_depth_stencil;

		ShaderReflection* m_shader_reflection;
		MemoryManager* m_allocator;
		CapacityStream<GraphicsShaderHelper> m_shader_helpers;
		// Keep a track of the created resources, for leaks and for winking out the device
		// For some reason DX11 does not provide a winking method for the device!!!
		AtomicStream<GraphicsInternalResource> m_internal_resources;
	private:
		char padding_1[ECS_CACHE_LINE_SIZE - sizeof(std::atomic<unsigned int>)];
	public:
		std::atomic<unsigned int> m_internal_resources_reader_count;
	private:
		char padding_2[ECS_CACHE_LINE_SIZE - sizeof(SpinLock)];
	public:
		SpinLock m_internal_resources_lock;
	};

	// Does not deallocate the entire allocator - care must be taken to make sure that if this graphics is used with a world and
	// the allocator comes from another allocator than the world's when destroying the world the memory used by graphics might not
	// be all deallocated
	ECSENGINE_API void DestroyGraphics(Graphics* graphics);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void BindVertexBuffer(VertexBuffer buffer, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindVertexBuffers(Stream<VertexBuffer> buffers, GraphicsContext* context, unsigned int start_slot = 0u);

	ECSENGINE_API void BindIndexBuffer(IndexBuffer index_buffer, GraphicsContext* context);

	ECSENGINE_API void BindInputLayout(InputLayout layout, GraphicsContext* context);

	ECSENGINE_API void BindVertexShader(VertexShader shader, GraphicsContext* context);

	ECSENGINE_API void BindPixelShader(PixelShader shader, GraphicsContext* context);

	ECSENGINE_API void BindDomainShader(DomainShader shader, GraphicsContext* context);

	ECSENGINE_API void BindHullShader(HullShader shader, GraphicsContext* context);

	ECSENGINE_API void BindGeometryShader(GeometryShader shader, GraphicsContext* context);

	ECSENGINE_API void BindComputeShader(ComputeShader shader, GraphicsContext* context);

	ECSENGINE_API void BindVertexConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindVertexConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		unsigned int start_slot = 0u
	);

	ECSENGINE_API void BindPixelConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindPixelConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		unsigned int start_slot = 0u
	);

	ECSENGINE_API void BindComputeConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindComputeConstantBuffers(Stream<ConstantBuffer> buffers, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindDomainConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindDomainConstantBuffers(Stream<ConstantBuffer> buffers, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindHullConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindHullConstantBuffers(Stream<ConstantBuffer> buffers, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindGeometryConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindGeometryConstantBuffers(Stream<ConstantBuffer> buffers, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindTopology(Topology topology, GraphicsContext* context);

	ECSENGINE_API void BindPixelResourceView(ResourceView view, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindPixelResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int start_slot = 0u);

	ECSENGINE_API void BindVertexResourceView(ResourceView view, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindVertexResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindDomainResourceView(ResourceView view, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindDomainResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindHullResourceView(ResourceView view, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindHullResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindGeometryResourceView(ResourceView view, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindGeometryResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindComputeResourceView(ResourceView view, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindComputeResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int slot = 0u);

	ECSENGINE_API void BindSamplerState(SamplerState sampler, GraphicsContext* context, unsigned int slot = 0u, ECS_SHADER_TYPE type = ECS_SHADER_PIXEL);

	ECSENGINE_API void BindSamplerStates(Stream<SamplerState> samplers, GraphicsContext* context, unsigned int start_slot = 0u, ECS_SHADER_TYPE type = ECS_SHADER_PIXEL);

	ECSENGINE_API void BindPixelUAView(UAView view, GraphicsContext* context, unsigned int start_slot = 0u);

	ECSENGINE_API void BindPixelUAViews(Stream<UAView> views, GraphicsContext* context, unsigned int start_slot = 0u);

	ECSENGINE_API void BindComputeUAView(UAView view, GraphicsContext* context, unsigned int start_slot = 0u);

	ECSENGINE_API void BindComputeUAViews(Stream<UAView> views, GraphicsContext* context, unsigned int start_slot = 0u);

	ECSENGINE_API void BindRasterizerState(RasterizerState state, GraphicsContext* context);

	ECSENGINE_API void BindDepthStencilState(DepthStencilState state, GraphicsContext* context, unsigned int stencil_ref = 0);

	ECSENGINE_API void BindBlendState(BlendState state, GraphicsContext* context);

	ECSENGINE_API void BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view, GraphicsContext* context);

	ECSENGINE_API void BindViewport(float top_left_x, float top_left_y, float new_width, float new_height, float min_depth, float max_depth, GraphicsContext* context);

	ECSENGINE_API void BindViewport(GraphicsViewport viewport, GraphicsContext* context);

	ECSENGINE_API void BindMesh(const Mesh& mesh, GraphicsContext* context);

	ECSENGINE_API void BindMesh(const Mesh& mesh, GraphicsContext* context, Stream<ECS_MESH_INDEX> mapping);

	ECSENGINE_API void BindMaterial(const Material& material, GraphicsContext* context);

	// Only works on the immediate context
	ECSENGINE_API void ClearRenderTarget(RenderTargetView view, GraphicsContext* context, ColorFloat color);

	// Only works on the immediate context
	ECSENGINE_API void ClearDepthStencil(DepthStencilView view, GraphicsContext* context, float depth = 1.0f, unsigned char stencil = 0);

	template<typename Resource>
	ECSENGINE_API void CopyGraphicsResource(Resource destination, Resource source, GraphicsContext* context);

	// It will copy the whole texture into the destination face
	ECSENGINE_API void CopyGraphicsResource(TextureCube destination, Texture2D source, TextureCubeFace face, GraphicsContext* context, unsigned int mip_level = 0);

	// Available types - IndexBuffer, VertexBuffer, ConstantBuffer, StandardBuffer, StructuredBuffer 
	// and UABuffer
	// For vertex buffers and index buffers offsets and sizes are in element quantities
	// For constant buffers, standard buffers and structured buffers offsets and sizes are in byte
	// quantities
	template<typename Buffer>
	ECSENGINE_API void CopyBufferSubresource(
		Buffer destination,
		unsigned int destination_offset,
		Buffer source,
		unsigned int source_offset,
		unsigned int source_size,
		GraphicsContext* context
	);
	
	// Subresource index can be used to change the mip level
	// Most of the time it will be 0
	ECSENGINE_API void CopyTextureSubresource(
		Texture1D destination,
		unsigned int destination_offset,
		unsigned int destination_subresource_index,
		Texture1D source,
		unsigned int source_offset,
		unsigned int source_size,
		unsigned int source_subresource_index,
		GraphicsContext* context
	);

	// Subresource index can be used to change the mip level
	// Most of the time it will be 0
	ECSENGINE_API void CopyTextureSubresource(
		Texture2D destination,
		uint2 destination_offset,
		unsigned int destination_subreosurce_index,
		Texture2D source,
		uint2 source_offset,
		uint2 source_size,
		unsigned int source_subresource_index,
		GraphicsContext* context
	);

	// Subresource index can be used to change the mip level
	// Most of the time it will be 0
	ECSENGINE_API void CopyTextureSubresource(
		Texture3D destination,
		uint3 destination_offset,
		unsigned int destination_subresource_index,
		Texture3D source,
		uint3 source_offset,
		uint3 source_size,
		unsigned int source_subresource_index,
		GraphicsContext* context
	);

	ECSENGINE_API void CopyTextureSubresource(
		TextureCube texture,
		uint2 destination_offset,
		unsigned int mip_level,
		TextureCubeFace face,
		Texture2D source,
		uint2 source_offset,
		uint2 source_size,
		unsigned int source_subresource_index,
		GraphicsContext* context
	);

	ECSENGINE_API void Dispatch(uint3 dispatch_size, GraphicsContext* context);

	ECSENGINE_API void DispatchIndirect(IndirectBuffer buffer, GraphicsContext* context);

	ECSENGINE_API void Draw(unsigned int vertex_count, GraphicsContext* context, unsigned int vertex_offset = 0u);

	ECSENGINE_API void DrawIndexed(unsigned int index_count, GraphicsContext* context, unsigned int start_index = 0u, unsigned int base_vertex_location = 0);

	ECSENGINE_API void DrawInstanced(
		unsigned int vertex_count, 
		unsigned int instance_count,
		GraphicsContext* context, 
		unsigned int vertex_offset = 0u, 
		unsigned int instance_offset = 0u
	);

	ECSENGINE_API void DrawIndexedInstanced(
		unsigned int index_count,
		unsigned int instance_count,
		GraphicsContext* context,
		unsigned int index_offset = 0u,
		unsigned int vertex_offset = 0u,
		unsigned int instance_offset = 0u
	);

	ECSENGINE_API void DrawIndexedInstancedIndirect(IndirectBuffer buffer, GraphicsContext* context);

	ECSENGINE_API void DrawInstancedIndirect(IndirectBuffer buffer, GraphicsContext* context);

	ECSENGINE_API void DrawMesh(const Mesh& mesh, const Material& material, GraphicsContext* context);

	ECSENGINE_API void DrawMesh(const CoallescedMesh& coallesced_mesh, unsigned int submesh_index, const Material& material, GraphicsContext* context);

	// It will only issue the draw command, it will not bind material/mesh
	ECSENGINE_API void DrawSubmeshCommand(Submesh submesh, GraphicsContext* context);

	ECSENGINE_API GraphicsPipelineBlendState GetBlendState(GraphicsContext* context);

	ECSENGINE_API GraphicsPipelineDepthStencilState GetDepthStencilState(GraphicsContext* context);

	ECSENGINE_API GraphicsPipelineRasterizerState GetRasterizerState(GraphicsContext* context);

	ECSENGINE_API GraphicsPipelineRenderState GetPipelineRenderState(GraphicsContext* context);

	ECSENGINE_API GraphicsBoundViews GetBoundViews(GraphicsContext* context);

	ECSENGINE_API GraphicsPipelineState GetPipelineState(GraphicsContext* context);

	ECSENGINE_API GraphicsViewport GetViewport(GraphicsContext* context);

	// It must be unmapped manually
	ECSENGINE_API void* MapBuffer(
		ID3D11Buffer* buffer,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD,
		unsigned int subresource_index = 0,
		unsigned int map_flags = 0
	);

	// It must be unmapped manually
	ECSENGINE_API D3D11_MAPPED_SUBRESOURCE MapBufferEx(
		ID3D11Buffer* buffer,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD,
		unsigned int subresource_index = 0,
		unsigned int map_flags = 0
	);

	// It must be unmapped manually
	template<typename Texture>
	ECSENGINE_API void* MapTexture(
		Texture texture,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD,
		unsigned int subresource_index = 0,
		unsigned int map_flags = 0
	);

	// It must be unmapped manually
	template<typename Texture>
	ECSENGINE_API D3D11_MAPPED_SUBRESOURCE MapTextureEx(
		Texture texture,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD,
		unsigned int subresource_index = 0,
		unsigned int map_flags = 0
	);

	ECSENGINE_API void RestoreBlendState(GraphicsContext* context, GraphicsPipelineBlendState blend_state);

	ECSENGINE_API void RestoreDepthStencilState(GraphicsContext* context, GraphicsPipelineDepthStencilState depth_stencil_state);

	ECSENGINE_API void RestoreRasterizerState(GraphicsContext* context, GraphicsPipelineRasterizerState rasterizer_state);

	ECSENGINE_API void RestorePipelineRenderState(GraphicsContext* context, GraphicsPipelineRenderState render_state);

	ECSENGINE_API void RestoreBoundViews(GraphicsContext* context, GraphicsBoundViews views);

	ECSENGINE_API void RestorePipelineState(GraphicsContext* context, const GraphicsPipelineState* state);

	// Transfers a shared GPU texture/buffer from a Graphics instance to another - should only create
	// another runtime DX11 reference to that texture, there should be no memory blit or copy
	// It does not affect samplers, input layouts, shaders, other pipeline objects (rasterizer/blend/depth states)
	// Use this function only for textures and buffers since these are the only ones who actually need transfering
	template<typename Resource>
	ECSENGINE_API Resource TransferGPUResource(Resource resource, GraphicsDevice* device);

	// Transfers the deep target buffer or texture and then creates a new texture view that points to it
	// Only views should be given
	template<typename View>
	ECSENGINE_API View TransferGPUView(View view, GraphicsDevice* device);

	template<typename Texture>
	ECSENGINE_API void UpdateTexture(
		Texture texture,
		const void* data,
		size_t data_size,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD,
		unsigned int map_flags = 0u,
		unsigned int subresource_index = 0
	);

	template<typename Texture>
	ECSENGINE_API void UpdateTextureResource(Texture texture, const void* data, size_t data_size, GraphicsContext* context, unsigned int subresource_index = 0);

	ECSENGINE_API void UpdateBuffer(
		ID3D11Buffer* buffer,
		const void* data,
		size_t data_size,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type = ECS_GRAPHICS_MAP_WRITE_DISCARD,
		unsigned int map_flags = 0u,
		unsigned int subresource_index = 0
	);

	ECSENGINE_API void UpdateBufferResource(
		ID3D11Buffer* buffer,
		const void* data,
		size_t data_size,
		GraphicsContext* context,
		unsigned int subresource_index = 0
	);

	ECSENGINE_API void UnmapBuffer(ID3D11Buffer* buffer, GraphicsContext* context, unsigned int subresource_index = 0);

	template<typename Texture>
	ECSENGINE_API void UnmapTexture(Texture texture, GraphicsContext* context, unsigned int subresource_index = 0);

	// Releases the graphics resources of this material
	ECSENGINE_API void FreeMaterial(Graphics* graphics, const Material* material);

	// Releases the mesh GPU resources and the names of the submeshes if any
	ECSENGINE_API void FreeCoallescedMesh(Graphics* graphics, CoallescedMesh* mesh, bool coallesced_allocation, AllocatorPolymorphic allocator);

	// SINGLE THREADED - It uses the CopyResource which requires the immediate context
	// Merges the vertex buffers and the index buffers into a single resource that can reduce 
	// the bind calls by moving the offsets into the draw call; it returns the aggregate mesh
	// and the submeshes - the offsets into the buffers; it will release the initial mesh 
	// buffers; The meshes must have the same index buffer int size
	// The mesh will have no name associated with it
	// It will release the graphics resources of the meshes
	// The submeshes will inherit the mesh name if it has one
	ECSENGINE_API Mesh MeshesToSubmeshes(Graphics* graphics, Stream<Mesh> meshes, Submesh* submeshes, ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE);

	// SINGLE THREADED - It uses the CopyResource which requires the immediate context
	// Same as the non mask variant - the difference is that it will only convert the meshes specified
	// in the mesh mask
	// The mesh will have no name associated with it
	// It will release the graphics resources of the meshes
	// The submeshes will inherit the mesh name if it has one
	ECSENGINE_API Mesh MeshesToSubmeshes(
		Graphics* graphics, 
		Stream<Mesh> meshes, 
		Submesh* submeshes,
		Stream<unsigned int> mesh_mask, 
		ECS_GRAPHICS_MISC_FLAGS misc_flags = ECS_GRAPHICS_MISC_NONE
	);

#endif // ECSENGINE_DIRECTX11


#endif // ECSENGINE_PLATFORM_WINDOWS

}

