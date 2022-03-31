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

	struct GraphicsDescriptor {
		uint2 window_size;
		MemoryManager* allocator;
		DXGI_USAGE target_usage = 0;
		bool gamma_corrected = true;
	};

	// Default arguments all but width; initial_data can be set to fill the texture
	// The initial data is a stream of Stream<void> for each mip map data
	struct GraphicsTexture1DDescriptor {
		unsigned int width;
		Stream<Stream<void>> mip_data = { nullptr, 0 };
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		unsigned int array_size = 1u;
		unsigned int mip_levels = 0u;
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		D3D11_CPU_ACCESS_FLAG cpu_flag = static_cast<D3D11_CPU_ACCESS_FLAG>(0);
		D3D11_BIND_FLAG bind_flag = D3D11_BIND_SHADER_RESOURCE;
		unsigned int misc_flag = 0u;
	};

	// Size must always be initialized;
	// The initial data is a stream of Stream<void> for each mip map data
	struct GraphicsTexture2DDescriptor {
		uint2 size;
		Stream<Stream<void>> mip_data = { nullptr, 0 };
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		unsigned int array_size = 1u;
		unsigned int mip_levels = 0u;
		unsigned int usage = D3D11_USAGE_DEFAULT;
		unsigned int cpu_flag = 0;
		unsigned int bind_flag = D3D11_BIND_SHADER_RESOURCE;
		unsigned int misc_flag = 0u;
		unsigned int sample_count = 1u;
		unsigned int sampler_quality = 0u;
	};

	// Size must be set
	// The initial data is a stream of Stream<void> for each mip map data
	struct GraphicsTexture3DDescriptor {
		uint3 size;
		Stream<Stream<void>> mip_data = { nullptr, 0 };
		DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT;
		unsigned int array_size = 1u;
		unsigned int mip_levels = 0u;
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		D3D11_CPU_ACCESS_FLAG cpu_flag = static_cast<D3D11_CPU_ACCESS_FLAG>(0);
		D3D11_BIND_FLAG bind_flag = D3D11_BIND_SHADER_RESOURCE;
		unsigned int misc_flag = 0u;
	};

	// The initial data is a stream of Stream<void> for each mip map data
	struct GraphicsTextureCubeDescriptor {
		uint2 size;
		Stream<Stream<void>> mip_data = { nullptr, 0 };
		DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT;
		unsigned int mip_levels = 0u;
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		D3D11_CPU_ACCESS_FLAG cpu_flag = static_cast<D3D11_CPU_ACCESS_FLAG>(0);
		D3D11_BIND_FLAG bind_flag = D3D11_BIND_SHADER_RESOURCE;
		unsigned int misc_flag = 0u;
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

	struct GraphicsViewport {
		float top_left_x;
		float top_left_y;
		float width;
		float height;
		float min_depth;
		float max_depth;
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
		ECS_GRAPHICS_RESOURCE_COMMAND_LIST
	};

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
		else {
			static_assert(false);
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

	ECSENGINE_API MemoryManager DefaultGraphicsAllocator(GlobalMemoryManager* manager);

	/* It has an immediate and a deferred context. The deferred context can be used to generate CommandLists */
	struct ECSENGINE_API Graphics
	{
	public:
		Graphics(HWND hWnd, const GraphicsDescriptor* descriptor);
		Graphics(const Graphics* graphics_to_copy, RenderTargetView new_render_target = { nullptr }, DepthStencilView new_depth_view = { nullptr }, MemoryManager* new_allocator = nullptr);

		Graphics& operator = (const Graphics& other) = default;

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

		void BindVertexConstantBuffer(ConstantBuffer buffer, UINT slot = 0u);

		void BindVertexConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindPixelConstantBuffer(ConstantBuffer buffer, UINT slot = 0u);

		void BindPixelConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindDomainConstantBuffer(ConstantBuffer buffer, UINT slot = 0u);

		void BindDomainConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindHullConstantBuffer(ConstantBuffer buffer, UINT slot = 0u);

		void BindHullConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindGeometryConstantBuffer(ConstantBuffer buffer, UINT slot = 0u);

		void BindGeometryConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindComputeConstantBuffer(ConstantBuffer buffer, UINT slot = 0u);

		void BindComputeConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindVertexBuffer(VertexBuffer buffer, UINT slot = 0u);

		void BindVertexBuffers(Stream<VertexBuffer> buffers, UINT start_slot = 0u);

		void BindTopology(Topology topology);

		void BindPixelResourceView(ResourceView component, UINT slot = 0u);

		void BindPixelResourceViews(Stream<ResourceView> component, UINT start_slot = 0u);

		void BindVertexResourceView(ResourceView component, UINT slot = 0u);

		void BindVertexResourceViews(Stream<ResourceView> component, UINT start_slot = 0u);

		void BindDomainResourceView(ResourceView component, UINT slot = 0u);

		void BindDomainResourceViews(Stream<ResourceView> component, UINT start_slot = 0u);

		void BindHullResourceView(ResourceView component, UINT slot = 0u);

		void BindHullResourceViews(Stream<ResourceView> component, UINT start_slot = 0u);

		void BindGeometryResourceView(ResourceView component, UINT slot = 0u);

		void BindGeometryResourceViews(Stream<ResourceView> component, UINT start_slot = 0u);

		void BindComputeResourceView(ResourceView component, UINT slot = 0u);

		void BindComputeResourceViews(Stream<ResourceView> component, UINT start_slot = 0u);

		void BindSamplerState(SamplerState sampler, UINT slot = 0u);

		void BindSamplerStates(Stream<SamplerState> samplers, UINT start_slot = 0u);

		void BindPixelUAView(UAView view, UINT start_slot = 0u);

		void BindPixelUAViews(Stream<UAView> views, UINT start_slot = 0u);

		void BindComputeUAView(UAView view, UINT start_slot = 0u);

		void BindComputeUAViews(Stream<UAView> views, UINT start_slot = 0u);

		void BindRasterizerState(RasterizerState state);

		void BindDepthStencilState(DepthStencilState state, UINT stencil_ref = 0);

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
			D3D11_USAGE usage = D3D11_USAGE_DEFAULT, 
			unsigned int cpu_access = 0, 
			unsigned int misc_flags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		IndexBuffer CreateIndexBuffer(
			size_t int_size,
			size_t element_count,
			const void* data,
			bool temporary = false, 
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE, 
			unsigned int cpu_access = 0,
			unsigned int misc_flags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		IndexBuffer CreateIndexBuffer(Stream<unsigned char> indices, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		IndexBuffer CreateIndexBuffer(Stream<unsigned short> indices, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		IndexBuffer CreateIndexBuffer(Stream<unsigned int> indices, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// No source code path will be assigned - so no reflection can be done on it
		PixelShader CreatePixelShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		PixelShader CreatePixelShaderFromSource(
			Stream<char> source_code, 
			ID3DInclude* include_policy,
			bool temporary = false,
			ShaderCompileOptions options = {}, 
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// No source code path will be assigned - so no reflection can be done on it
		VertexShader CreateVertexShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		VertexShader CreateVertexShaderFromSource(
			Stream<char> source_code, 
			ID3DInclude* include_policy, 
			bool temporary = false,
			ShaderCompileOptions options = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// No source code path will be assigned - so no reflection can be done on it
		DomainShader CreateDomainShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		DomainShader CreateDomainShaderFromSource(
			Stream<char> source_code,
			ID3DInclude* include_policy, 
			bool temporary = false, 
			ShaderCompileOptions options = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// No source code path will be assigned - so no reflection can be done on it
		HullShader CreateHullShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		HullShader CreateHullShaderFromSource(
			Stream<char> source_code, 
			ID3DInclude* include_policy,
			bool temporary = false,
			ShaderCompileOptions options = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// No source code path will be assigned - so no reflection can be done on it
		GeometryShader CreateGeometryShader(Stream<void> byte_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		GeometryShader CreateGeometryShaderFromSource(
			Stream<char> source_code, 
			ID3DInclude* include_policy, 
			bool temporary = false, 
			ShaderCompileOptions options = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		// No source code path will be assigned - so no reflection can be done on it
		ComputeShader CreateComputeShader(Stream<void> path, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		ComputeShader CreateComputeShaderFromSource(
			Stream<char> source_code,
			ID3DInclude* include_policy, 
			bool temporary = false, 
			ShaderCompileOptions options = {},
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		InputLayout CreateInputLayout(Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, VertexShader vertex_shader, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		VertexBuffer CreateVertexBuffer(
			size_t element_size, 
			size_t element_count, 
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC, 
			unsigned int cpuFlags = D3D11_CPU_ACCESS_WRITE, 
			unsigned int miscFlags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		VertexBuffer CreateVertexBuffer(
			size_t element_size,
			size_t element_count,
			const void* buffer,
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE,
			unsigned int cpuFlags = 0,
			unsigned int miscFlags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);
		
		ConstantBuffer CreateConstantBuffer(
			size_t byte_size,
			const void* buffer,
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ConstantBuffer CreateConstantBuffer(
			size_t byte_size,
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		StandardBuffer CreateStandardBuffer(
			size_t element_size,
			size_t element_count,
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		StandardBuffer CreateStandardBuffer(
			size_t element_size,
			size_t element_count,
			const void* data,
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE,
			unsigned int cpuAccessFlags = 0,
			unsigned int miscFlags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		StructuredBuffer CreateStructuredBuffer(
			size_t element_size,
			size_t element_count,
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		StructuredBuffer CreateStructuredBuffer(
			size_t element_size,
			size_t element_count,
			const void* data,
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE,
			unsigned int cpuAccessFlags = 0,
			unsigned int miscFlags = 0, 
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		UABuffer CreateUABuffer(
			size_t element_size,
			size_t element_count,
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		UABuffer CreateUABuffer(
			size_t element_size,
			size_t element_count,
			const void* data,
			bool temporary = false,
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE,
			unsigned int cpuAccessFlags = 0,
			unsigned int miscFlags = 0,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		IndirectBuffer CreateIndirectBuffer(bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		AppendStructuredBuffer CreateAppendStructuredBuffer(size_t element_size, size_t element_count, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		ConsumeStructuredBuffer CreateConsumeStructuredBuffer(size_t element_size, size_t element_count, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		SamplerState CreateSamplerState(const D3D11_SAMPLER_DESC& descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		Texture1D CreateTexture(const GraphicsTexture1DDescriptor* descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		Texture2D CreateTexture(const GraphicsTexture2DDescriptor* descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		Texture3D CreateTexture(const GraphicsTexture3DDescriptor* descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		TextureCube CreateTexture(const GraphicsTextureCubeDescriptor* descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			Texture1D texture,
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ResourceView CreateTextureShaderViewResource(Texture1D texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			Texture2D texture,
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ResourceView CreateTextureShaderViewResource(Texture2D texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			Texture3D texture,
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ResourceView CreateTextureShaderViewResource(Texture3D texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			TextureCube texture, 
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT, 
			unsigned int most_detailed_mip = 0u, 
			unsigned int mip_levels = -1,
			bool temporary = false,
			DebugInfo debug_info = ECS_DEBUG_INFO
		);

		ResourceView CreateTextureShaderViewResource(TextureCube texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		RenderTargetView CreateRenderTargetView(Texture2D texture, unsigned int mip_level = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		RenderTargetView CreateRenderTargetView(TextureCube cube, TextureCubeFace face, unsigned int mip_level = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		DepthStencilView CreateDepthStencilView(Texture2D texture, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		ResourceView CreateBufferView(StandardBuffer buffer, DXGI_FORMAT format, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		ResourceView CreateBufferView(StructuredBuffer buffer, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(UABuffer buffer, DXGI_FORMAT format, unsigned int first_element = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(AppendStructuredBuffer buffer, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(ConsumeStructuredBuffer buffer, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(IndirectBuffer buffer, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(Texture1D texture, unsigned int mip_slice = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(Texture2D texture, unsigned int mip_slice = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		UAView CreateUAView(Texture3D texture, unsigned int mip_slice = 0, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		RasterizerState CreateRasterizerState(const D3D11_RASTERIZER_DESC& descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		DepthStencilState CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		BlendState CreateBlendState(const D3D11_BLEND_DESC& descriptor, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

#pragma endregion

#pragma region Resource release

		// Releases the graphics resources and the name if it has one
		void FreeMesh(const Mesh& mesh);

		// Releases the name if it has one
		void FreeSubmesh(const Submesh& submesh);

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

		void* MapBuffer(ID3D11Buffer* buffer, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture(Texture1D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture(Texture2D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture(Texture3D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void UpdateBuffer(ID3D11Buffer* buffer, const void* data, size_t data_size, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int map_flags = 0u, unsigned int subresource_index = 0);

		void UnmapBuffer(ID3D11Buffer* buffer, unsigned int subresource_index = 0);

		void UnmapTexture(Texture1D texture, unsigned int subresource_index = 0);

		void UnmapTexture(Texture2D texture, unsigned int subresource_index = 0);

		void UnmapTexture(Texture3D texture, unsigned int subresource_index = 0);

#pragma endregion

#pragma region Shader Reflection

		// ------------------------------------------------- Shader Reflection --------------------------------------------------

		// Path nullptr means take the path from the shader
		InputLayout ReflectVertexShaderInput(VertexShader shader, Stream<char> source_code, bool temporary = false, DebugInfo debug_info = ECS_DEBUG_INFO);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		bool ReflectShaderBuffers(Stream<char> source_code, CapacityStream<ShaderReflectedBuffer>& buffers);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		bool ReflectShaderTextures(Stream<char> source_code, CapacityStream<ShaderReflectedTexture>& textures);

		// No memory needs to be allocated
		bool ReflectVertexBufferMapping(Stream<char> source_code, CapacityStream<ECS_MESH_INDEX>& mapping);

#pragma endregion

#pragma region Getters and other operations
		
		// -------------------------------------------- Getters and other operations --------------------------------------------

		// It does not acquire the lock - call this if inside a lock
		// Can be called only from a single thread
		void CommitInternalResourcesToBeFreed();

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
				// The first one to acquire the semaphore - do the resizing or the flush of removals
				if (do_resizing) {
					// Spin wait while there are still readers
					unsigned int read_count = m_internal_resources_reader_count.load(ECS_RELAXED);
					while (read_count > 0) {
						read_count = m_internal_resources_reader_count.load(ECS_RELAXED);
						_mm_pause();
					}

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

					m_internal_resources_lock.unlock();
				}
				// Other thread got to do the resizing or freeing of the resources - stall until it finishes
				else {
					while (m_internal_resources_lock.is_locked()) {
						_mm_pause();
					}
					// Rerequest the position
					write_index = m_internal_resources.RequestInt(1);
					m_internal_resources[write_index] = internal_res;
				}
			}
			// Write into the internal resources
			else {
				m_internal_resources[write_index] = internal_res;
			}
		}

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
				//ECS_GRAPHICS_ASSERT_GRAPHICS_RESOURCE;
				count = resource.Release();
			}

			ECS_ASSERT(count < 100);
		}

		// It will assert FALSE if it doesn't exist
		void RemoveResourceFromTracking(void* resource);

		// If it doesn't find the resource, it does nothing
		void RemovePossibleResourceFromTracking(void* resource);
		
		void FreeResourceView(ResourceView view);

		void FreeUAView(UAView view);

		void FreeRenderView(RenderTargetView view);

		void CreateInitialRenderTargetView(bool gamma_corrected);

		void CreateInitialDepthStencilView();

		GraphicsContext* CreateDeferredContext(UINT context_flags = 0u, DebugInfo debug_info = ECS_DEBUG_INFO);

		void FreeAllocatedBuffer(const void* buffer);

		void FreeAllocatedBufferTs(const void* buffer);

		void* GetAllocatorBuffer(size_t size);

		void* GetAllocatorBufferTs(size_t size);

		GraphicsDevice* GetDevice();

		GraphicsContext* GetContext();

		GraphicsContext* GetDeferredContext();

		GraphicsPipelineBlendState GetBlendState() const;

		GraphicsPipelineDepthStencilState GetDepthStencilState() const;

		GraphicsPipelineRasterizerState GetRasterizerState() const;

		GraphicsPipelineRenderState GetPipelineRenderState() const;

		GraphicsBoundViews GetCurrentViews() const;

		GraphicsPipelineState GetPipelineState() const;

		void GetWindowSize(unsigned int& width, unsigned int& height) const;

		uint2 GetWindowSize() const;

		void RestoreBlendState(GraphicsPipelineBlendState state);

		void RestoreDepthStencilState(GraphicsPipelineDepthStencilState state);

		void RestoreRasterizerState(GraphicsPipelineRasterizerState state);

		void RestorePipelineRenderState(GraphicsPipelineRenderState state);

		void RestoreBoundViews(GraphicsBoundViews views);

		void RestorePipelineState(const GraphicsPipelineState* state);

		void ResizeSwapChainSize(HWND hWnd, float width, float height);

		void ResizeViewport(float top_left_x, float top_left_y, float new_width, float new_height);

		void SwapBuffers(unsigned int sync_interval);

		void SetNewSize(HWND hWnd, unsigned int width, unsigned int height);

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
		BlendState m_blend_disabled;
		BlendState m_blend_enabled;
		ShaderReflection* m_shader_reflection;
		MemoryManager* m_allocator;
		CapacityStream<GraphicsShaderHelper> m_shader_helpers;
		// Keep a track of the created resources, for leaks and for winking out the device
		// For some reason DX11 does not provide a winking method for the device!!!!!!!!
		AtomicStream<GraphicsInternalResource> m_internal_resources;
	private:
		char padding_1[ECS_CACHE_LINE_SIZE - sizeof(std::atomic<unsigned int>)];
	public:
		std::atomic<unsigned int> m_internal_resources_reader_count;
	private:
		char padding_2[ECS_CACHE_LINE_SIZE - sizeof(SpinLock)];
	public:
		SpinLock m_internal_resources_lock;
		bool m_copied_graphics;
	};

	// Does not deallocate the entire allocator - care must be taken to make sure that if this graphics is used with a world and
	// the allocator comes from another allocator than the world's when destroying the world the memory used by graphics might not
	// be all deallocated
	ECSENGINE_API void DestroyGraphics(Graphics* graphics);

	// -------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void BindVertexBuffer(VertexBuffer buffer, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindVertexBuffers(Stream<VertexBuffer> buffers, GraphicsContext* context, UINT start_slot = 0u);

	ECSENGINE_API void BindIndexBuffer(IndexBuffer index_buffer, GraphicsContext* context);

	ECSENGINE_API void BindInputLayout(InputLayout layout, GraphicsContext* context);

	ECSENGINE_API void BindVertexShader(VertexShader shader, GraphicsContext* context);

	ECSENGINE_API void BindPixelShader(PixelShader shader, GraphicsContext* context);

	ECSENGINE_API void BindDomainShader(DomainShader shader, GraphicsContext* context);

	ECSENGINE_API void BindHullShader(HullShader shader, GraphicsContext* context);

	ECSENGINE_API void BindGeometryShader(GeometryShader shader, GraphicsContext* context);

	ECSENGINE_API void BindComputeShader(ComputeShader shader, GraphicsContext* context);

	ECSENGINE_API void BindVertexConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindVertexConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		UINT start_slot = 0u
	);

	ECSENGINE_API void BindPixelConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindPixelConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		UINT start_slot = 0u
	);

	ECSENGINE_API void BindComputeConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindComputeConstantBuffers(Stream<ConstantBuffer> buffers, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindDomainConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindDomainConstantBuffers(Stream<ConstantBuffer> buffers, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindHullConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindHullConstantBuffers(Stream<ConstantBuffer> buffers, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindGeometryConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindGeometryConstantBuffers(Stream<ConstantBuffer> buffers, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindTopology(Topology topology, GraphicsContext* context);

	ECSENGINE_API void BindPixelResourceView(ResourceView view, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindPixelResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT start_slot = 0u);

	ECSENGINE_API void BindVertexResourceView(ResourceView view, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindVertexResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindDomainResourceView(ResourceView view, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindDomainResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindHullResourceView(ResourceView view, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindHullResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindGeometryResourceView(ResourceView view, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindGeometryResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindComputeResourceView(ResourceView view, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindComputeResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindSamplerState(SamplerState sampler, GraphicsContext* context, UINT slot = 0u);

	ECSENGINE_API void BindSamplerStates(Stream<SamplerState> samplers, GraphicsContext* context, UINT start_slot = 0u);

	ECSENGINE_API void BindPixelUAView(UAView view, GraphicsContext* context, UINT start_slot = 0u);

	ECSENGINE_API void BindPixelUAViews(Stream<UAView> views, GraphicsContext* context, UINT start_slot = 0u);

	ECSENGINE_API void BindComputeUAView(UAView view, GraphicsContext* context, UINT start_slot = 0u);

	ECSENGINE_API void BindComputeUAViews(Stream<UAView> views, GraphicsContext* context, UINT start_slot = 0u);

	ECSENGINE_API void BindRasterizerState(RasterizerState state, GraphicsContext* context);

	ECSENGINE_API void BindDepthStencilState(DepthStencilState state, GraphicsContext* context, UINT stencil_ref = 0);

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
		D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD,
		unsigned int subresource_index = 0,
		unsigned int map_flags = 0
	);

	// It must be unmapped manually
	ECSENGINE_API D3D11_MAPPED_SUBRESOURCE MapBufferEx(
		ID3D11Buffer* buffer,
		GraphicsContext* context,
		D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD,
		unsigned int subresource_index = 0,
		unsigned int map_flags = 0
	);

	// It must be unmapped manually
	template<typename Texture>
	ECSENGINE_API void* MapTexture(
		Texture texture,
		GraphicsContext* context,
		D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD,
		unsigned int subresource_index = 0,
		unsigned int map_flags = 0
	);

	// It must be unmapped manually
	template<typename Texture>
	ECSENGINE_API D3D11_MAPPED_SUBRESOURCE MapTextureEx(
		Texture texture,
		GraphicsContext* context,
		D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD,
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
	template<typename Resource>
	ECSENGINE_API Resource TransferGPUResource(Resource resource, GraphicsDevice* device);

	template<typename Texture>
	ECSENGINE_API void UpdateTexture(
		Texture texture,
		const void* data,
		size_t data_size,
		GraphicsContext* context,
		D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD,
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
		D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD,
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
	ECSENGINE_API void FreeCoallescedMesh(Graphics* graphics, const CoallescedMesh* mesh);

	// Merges the vertex buffers and the index buffers into a single resource that can reduce 
	// the bind calls by moving the offsets into the draw call; it returns the aggregate mesh
	// and the submeshes - the offsets into the buffers; it will release the initial mesh 
	// buffers; The meshes must have the same index buffer int size
	// The mesh will have no name associated with it
	// It will release the graphics resources of the meshes
	// The submeshes will inherit the mesh name if it has one
	ECSENGINE_API Mesh MeshesToSubmeshes(Graphics* graphics, Stream<Mesh> meshes, Submesh* submeshes, unsigned int misc_flags = 0);

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
		unsigned int misc_flags = 0
	);

#endif // ECSENGINE_DIRECTX11


#endif // ECSENGINE_PLATFORM_WINDOWS

}

