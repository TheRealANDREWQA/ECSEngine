#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"
#include "ShaderReflection.h"
#include "../Allocators/MemoryManager.h"

#define ECS_PIXEL_SHADER_SOURCE(name) L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Pixel\\" TEXT(STRING(name.hlsl))
#define ECS_VERTEX_SHADER_SOURCE(name) L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Vertex\\" TEXT(STRING(name.hlsl))
#define ECS_COMPUTE_SHADER_SOURCE(name) L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders\\Compute\\" TEXT(STRING(name.hlsl))
#define ECS_SHADER_DIRECTORY L"C:\\Users\\Andrei\\C++\\ECSEngine\\ECSEngine\\src\\ECSEngine\\Rendering\\Shaders"

namespace ECSEngine {

	ECS_CONTAINERS;

#ifdef ECSENGINE_PLATFORM_WINDOWS

#ifdef ECSENGINE_DIRECTX11

#define ECS_GRAPHICS_MAX_RENDER_TARGETS_BIND 4

	struct ECSENGINE_API GraphicsDescriptor {
		uint2 window_size;
		MemoryManager* allocator;
		DXGI_USAGE target_usage = 0;
		bool gamma_corrected = true;
	};

	// Default arguments all but width; initial_data can be set to fill the texture
	struct GraphicsTexture1DDescriptor {
		unsigned int width;
		const void* initial_data = nullptr;
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		unsigned int array_size = 1u;
		unsigned int mip_levels = 0u;
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		D3D11_CPU_ACCESS_FLAG cpu_flag = static_cast<D3D11_CPU_ACCESS_FLAG>(0);
		D3D11_BIND_FLAG bind_flag = D3D11_BIND_SHADER_RESOURCE;
		unsigned int misc_flag = 0u;
	};

	// Size must always be initialized; for initial_data, memory_pitch and memory_slice_pitch should
	// be set; the rest are defaults
	struct GraphicsTexture2DDescriptor {
		uint2 size;
		struct {
			const void* initial_data = nullptr;
			unsigned int memory_pitch = 0;
			unsigned int memory_slice_pitch = 0;
		};
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		unsigned int array_size = 1u;
		unsigned int mip_levels = 0u;
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		D3D11_CPU_ACCESS_FLAG cpu_flag = static_cast<D3D11_CPU_ACCESS_FLAG>(0);
		D3D11_BIND_FLAG bind_flag = D3D11_BIND_SHADER_RESOURCE;
		unsigned int misc_flag = 0u;
		unsigned int sample_count = 1u;
		unsigned int sampler_quality = 0u;
	};

	// Size must be set; initial_data can be set to give initial values; the rest are defaults
	struct ECSENGINE_API GraphicsTexture3DDescriptor {
		uint3 size;
		struct {
			const void* initial_data = nullptr;
		};
		DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT;
		unsigned int array_size = 1u;
		unsigned int mip_levels = 0u;
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		D3D11_CPU_ACCESS_FLAG cpu_flag = static_cast<D3D11_CPU_ACCESS_FLAG>(0);
		D3D11_BIND_FLAG bind_flag = D3D11_BIND_SHADER_RESOURCE;
		unsigned int misc_flag = 0u;
	};

	struct ECSENGINE_API GraphicsTextureCubeDescriptor {
		uint2 size;
		struct {
			const void* initial_data = nullptr;
			unsigned int memory_pitch = 0;
			unsigned int memory_slice_pitch = 0;
		};
		DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT;
		unsigned int mip_levels = 0u;
		D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
		D3D11_CPU_ACCESS_FLAG cpu_flag = static_cast<D3D11_CPU_ACCESS_FLAG>(0);
		D3D11_BIND_FLAG bind_flag = D3D11_BIND_SHADER_RESOURCE;
		unsigned int misc_flag = 0u;
	};

	struct ECSENGINE_API GraphicsPipelineBlendState {
		ID3D11BlendState* blend_state;
		float blend_factors[4];
		unsigned int sample_mask;
	};

	struct ECSENGINE_API GraphicsPipelineDepthStencilState {
		ID3D11DepthStencilState* depth_stencil_state;
		unsigned int stencil_ref;
	};

	struct ECSENGINE_API GraphicsPipelineRasterizerState {
		ID3D11RasterizerState* rasterizer_state;
	};

	struct ECSENGINE_API GraphicsPipelineRenderState {
		GraphicsPipelineBlendState blend_state;
		GraphicsPipelineDepthStencilState depth_stencil_state;
		GraphicsPipelineRasterizerState rasterizer_state;
	};

	struct ECSENGINE_API GraphicsViewport {
		float top_left_x;
		float top_left_y; 
		float width;
		float height;
		float min_depth;
		float max_depth;
	};

	struct ECSENGINE_API ShaderMacro {
		const char* name;
		const char* definition;
	};

	enum ECSENGINE_API ShaderTarget : unsigned char {
		ECS_SHADER_TARGET_5_0,
		ECS_SHADER_TARGET_5_1,
		ECS_SHADER_TARGET_COUNT
	};

	enum ECSENGINE_API ShaderCompileFlags : unsigned char {
		ECS_SHADER_COMPILE_NONE = 0,
		ECS_SHADER_COMPILE_DEBUG = 1 << 0,
		ECS_SHADER_COMPILE_OPTIMIZATION_LOWEST = 1 << 1,
		ECS_SHADER_COMPILE_OPTIMIZATION_LOW = 1 << 2,
		ECS_SHADER_COMPILE_OPTIMIZATION_HIGH = 1 << 3,
		ECS_SHADER_COMPILE_OPTIMIZATION_HIGHEST = 1 << 4
	};

	// Default is no macros, shader target 5 and no compile flags
	struct ECSENGINE_API ShaderFromSourceOptions {
		Stream<ShaderMacro> macros = {nullptr, 0};
		ShaderTarget target = ECS_SHADER_TARGET_5_0;
		ShaderCompileFlags compile_flags = ECS_SHADER_COMPILE_NONE;
	};

	enum TextureCubeFace {
		ECS_TEXTURE_CUBE_X_POS,
		ECS_TEXTURE_CUBE_X_NEG,
		ECS_TEXTURE_CUBE_Y_POS,
		ECS_TEXTURE_CUBE_Y_NEG,
		ECS_TEXTURE_CUBE_Z_POS,
		ECS_TEXTURE_CUBE_Z_NEG
	};

	enum GraphicsShaderHelpers {
		ECS_GRAPHICS_SHADER_HELPER_CREATE_TEXTURE_CUBE,
		ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_TEXTURE_CUBE,
		ECS_GRAPHICS_SHADER_HELPER_CREATE_DIFFUSE_ENVIRONMENT,
		ECS_GRAPHICS_SHADER_HELPER_COUNT
	};

	struct GraphicsShaderHelper {
		VertexShader vertex;
		PixelShader pixel;
		InputLayout input_layout;
		SamplerState pixel_sampler;
	};

	/* It has an immediate and a deferred context. The deferred context can be used to generate CommandLists */
	class ECSENGINE_API Graphics
	{
	public:
		Graphics(HWND hWnd, const GraphicsDescriptor* descriptor);
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
		IndexBuffer CreateIndexBuffer(size_t int_size, size_t element_count, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, unsigned int cpu_access = 0);

		IndexBuffer CreateIndexBuffer(size_t int_size, size_t element_count, const void* data, D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE, unsigned int cpu_access = 0);

		IndexBuffer CreateIndexBuffer(Stream<unsigned char> indices);

		IndexBuffer CreateIndexBuffer(Stream<unsigned short> indices);

		IndexBuffer CreateIndexBuffer(Stream<unsigned int> indices);

		// No source code path will be assigned - so no reflection can be done on it
		PixelShader CreatePixelShader(Stream<wchar_t> byte_code);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		PixelShader CreatePixelShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code_path);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		PixelShader CreatePixelShaderFromSource(Stream<wchar_t> source_code_path, ShaderFromSourceOptions options = {});

		// No source code path will be assigned - so no reflection can be done on it
		VertexShader CreateVertexShader(Stream<wchar_t> byte_code);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		VertexShader CreateVertexShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code_path);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		VertexShader CreateVertexShaderFromSource(Stream<wchar_t> source_code_path, ShaderFromSourceOptions options = {});

		// No source code path will be assigned - so no reflection can be done on it
		DomainShader CreateDomainShader(Stream<wchar_t> byte_code);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		DomainShader CreateDomainShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code_path);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		DomainShader CreateDomainShaderFromSource(Stream<wchar_t> source_code_path, ShaderFromSourceOptions options = {});

		// No source code path will be assigned - so no reflection can be done on it
		HullShader CreateHullShader(Stream<wchar_t> byte_code);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		HullShader CreateHullShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code_path);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		HullShader CreateHullShaderFromSource(Stream<wchar_t> source_code_path, ShaderFromSourceOptions options = {});

		// No source code path will be assigned - so no reflection can be done on it
		GeometryShader CreateGeometryShader(Stream<wchar_t> byte_code);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		GeometryShader CreateGeometryShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		GeometryShader CreateGeometryShaderFromSource(Stream<wchar_t> source_code_path, ShaderFromSourceOptions options = {});

		// No source code path will be assigned - so no reflection can be done on it
		ComputeShader CreateComputeShader(Stream<wchar_t> path);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		ComputeShader CreateComputeShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code);

		// Source code path will be allocated from the assigned allocator;
		// Reflection works
		ComputeShader CreateComputeShaderFromSource(Stream<wchar_t> source_code_path, ShaderFromSourceOptions options = {});

		InputLayout CreateInputLayout(Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, VertexShader vertex_shader);

		VertexBuffer CreateVertexBuffer(
			size_t element_size, 
			size_t element_count, 
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC, 
			unsigned int cpuFlags = D3D11_CPU_ACCESS_WRITE, 
			unsigned int miscFlags = 0
		);

		VertexBuffer CreateVertexBuffer(
			size_t element_size,
			size_t element_count,
			const void* buffer,
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE,
			unsigned int cpuFlags = 0,
			unsigned int miscFlags = 0
		);
		
		ConstantBuffer CreateConstantBuffer(
			size_t byte_size,
			const void* buffer,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0
		);

		ConstantBuffer CreateConstantBuffer(
			size_t byte_size,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0
		);

		StandardBuffer CreateStandardBuffer(
			size_t element_size,
			size_t element_count,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0
		);

		StandardBuffer CreateStandardBuffer(
			size_t element_size,
			size_t element_count,
			const void* data,
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE,
			unsigned int cpuAccessFlags = 0,
			unsigned int miscFlags = 0
		);

		StructuredBuffer CreateStructuredBuffer(
			size_t element_size,
			size_t element_count,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0
		);

		StructuredBuffer CreateStructuredBuffer(
			size_t element_size,
			size_t element_count,
			const void* data,
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE,
			unsigned int cpuAccessFlags = 0,
			unsigned int miscFlags = 0
		);

		UABuffer CreateUABuffer(
			size_t element_size,
			size_t element_count,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0
		);

		UABuffer CreateUABuffer(
			size_t element_size,
			size_t element_count,
			const void* data,
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE,
			unsigned int cpuAccessFlags = 0,
			unsigned int miscFlags = 0
		);

		IndirectBuffer CreateIndirectBuffer();

		AppendStructuredBuffer CreateAppendStructuredBuffer(size_t element_size, size_t element_count);

		ConsumeStructuredBuffer CreateConsumeStructuredBuffer(size_t element_size, size_t element_count);

		SamplerState CreateSamplerState(const D3D11_SAMPLER_DESC& descriptor);

		Texture1D CreateTexture(const GraphicsTexture1DDescriptor* descriptor);

		Texture2D CreateTexture(const GraphicsTexture2DDescriptor* descriptor);

		Texture3D CreateTexture(const GraphicsTexture3DDescriptor* descriptor);

		TextureCube CreateTexture(const GraphicsTextureCubeDescriptor* descriptor);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			Texture1D texture,
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1
		);

		ResourceView CreateTextureShaderViewResource(Texture1D texture);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			Texture2D texture,
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1
		);

		ResourceView CreateTextureShaderViewResource(Texture2D texture);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			Texture3D texture,
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1
		);

		ResourceView CreateTextureShaderViewResource(Texture3D texture);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTextureShaderView(
			TextureCube texture, 
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT, 
			unsigned int most_detailed_mip = 0u, 
			unsigned int mip_levels = -1
		);

		ResourceView CreateTextureShaderViewResource(TextureCube texture);

		RenderTargetView CreateRenderTargetView(Texture2D texture);

		RenderTargetView CreateRenderTargetView(TextureCube cube, TextureCubeFace face, unsigned int mip_level = 0);

		DepthStencilView CreateDepthStencilView(Texture2D texture);

		ResourceView CreateBufferView(StandardBuffer buffer, DXGI_FORMAT format);

		ResourceView CreateBufferView(StructuredBuffer buffer);

		UAView CreateUAView(UABuffer buffer, DXGI_FORMAT format, unsigned int first_element = 0);

		UAView CreateUAView(AppendStructuredBuffer buffer);

		UAView CreateUAView(ConsumeStructuredBuffer buffer);

		UAView CreateUAView(IndirectBuffer buffer);

		UAView CreateUAView(Texture1D texture, unsigned int mip_slice = 0);

		UAView CreateUAView(Texture2D texture, unsigned int mip_slice = 0);

		UAView CreateUAView(Texture3D texture, unsigned int mip_slice = 0);

		// It assumes that the given shaders can be reflected
		Material CreateMaterial(VertexShader v_shader, PixelShader p_shader, DomainShader d_shader = nullptr, HullShader h_shader = nullptr, GeometryShader g_shader = nullptr);

#pragma endregion

#pragma region Resource release

		// Releases the name if it has one and the shader interface
		template<typename Shader>
		void FreeShader(Shader shader);

		// Releases the graphics resources and the name if it has one
		void FreeMesh(const Mesh& mesh);

		// Releases the name if it has one
		void FreeSubmesh(const Submesh& submesh);

#pragma endregion

#pragma region Pipeline State Changes

		// ------------------------------------------- Pipeline State Changes ------------------------------------

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

		void GenerateMips(ResourceView view);

		RenderTargetView GetBoundRenderTarget() const;

		DepthStencilView GetBoundDepthStencil() const;

		GraphicsViewport GetBoundViewport() const;

		void* MapBuffer(ID3D11Buffer* buffer, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture(Texture1D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture(Texture2D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture(Texture3D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		ID3D11CommandList* FinishCommandList(bool restore_state = false);

		void UpdateBuffer(ID3D11Buffer* buffer, const void* data, size_t data_size, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int map_flags = 0u, unsigned int subresource_index = 0);

		void UnmapBuffer(ID3D11Buffer* buffer, unsigned int subresource_index = 0);

		void UnmapTexture(Texture1D texture, unsigned int subresource_index = 0);

		void UnmapTexture(Texture2D texture, unsigned int subresource_index = 0);

		void UnmapTexture(Texture3D texture, unsigned int subresource_index = 0);

#pragma endregion

#pragma region Shader Reflection

		// ------------------------------------------------- Shader Reflection --------------------------------------------------

		// Path nullptr means take the path from the shader
		InputLayout ReflectVertexShaderInput(VertexShader shader, Stream<wchar_t> path = {nullptr, 0});

		// The memory needed for the buffer names will be allocated from the assigned allocator
		bool ReflectShaderBuffers(const wchar_t* path, CapacityStream<ShaderReflectedBuffer>& buffers);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		bool ReflectShaderBuffers(Stream<wchar_t> path, CapacityStream<ShaderReflectedBuffer>& buffers);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		bool ReflectShaderTextures(const wchar_t* path, CapacityStream<ShaderReflectedTexture>& textures);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		bool ReflectShaderTextures(Stream<wchar_t> path, CapacityStream<ShaderReflectedTexture>& textures);

		// No memory needs to be allocated
		bool ReflectVertexBufferMapping(const wchar_t* path, CapacityStream<ECS_MESH_INDEX>& mapping);

		// No memory needs to be allocated
		bool ReflectVertexBufferMapping(Stream<wchar_t> path, CapacityStream<ECS_MESH_INDEX>& mapping);

#pragma endregion

#pragma region Getters and other operations
		
		// -------------------------------------------- Getters and other operations --------------------------------------------

		void CreateInitialRenderTargetView(bool gamma_corrected);

		void CreateInitialDepthStencilView();

		GraphicsContext* CreateDeferredContext(UINT context_flags = 0u);

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

		GraphicsPipelineRenderState GetRenderState() const;

		void GetWindowSize(unsigned int& width, unsigned int& height) const;

		void GetWindowSize(uint2& size) const;

		void RestoreBlendState(GraphicsPipelineBlendState state);

		void RestoreDepthStencilState(GraphicsPipelineDepthStencilState state);

		void RestoreRasterizerState(GraphicsPipelineRasterizerState state);

		void RestoreRenderState(GraphicsPipelineRenderState state);

		void ResizeSwapChainSize(HWND hWnd, float width, float height);

		void ResizeViewport(float top_left_x, float top_left_y, float new_width, float new_height);

		void SwapBuffers(unsigned int sync_interval);

		void SetNewSize(HWND hWnd, unsigned int width, unsigned int height);

		void SetShaderDirectory(Stream<wchar_t> shader_directory);

#pragma endregion

	//private:
		uint2 m_window_size;
		Microsoft::WRL::ComPtr<GraphicsDevice> m_device;
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swap_chain;
		Microsoft::WRL::ComPtr<GraphicsContext> m_context;
		Microsoft::WRL::ComPtr<GraphicsContext> m_deferred_context;
		RenderTargetView m_target_view;
		RenderTargetView m_bound_render_targets[ECS_GRAPHICS_MAX_RENDER_TARGETS_BIND];
		Stream<RenderTargetView> m_bound_targets;
		DepthStencilView m_depth_stencil_view;
		DepthStencilView m_current_depth_stencil;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_blend_disabled;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_blend_enabled;
		ShaderReflection m_shader_reflection;
		MemoryManager* m_allocator;
		containers::Stream<wchar_t> m_shader_directory;
		containers::CapacityStream<GraphicsShaderHelper> m_shader_helpers;
	};

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

	ECSENGINE_API void BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view, GraphicsContext* context);

	ECSENGINE_API void BindViewport(float top_left_x, float top_left_y, float new_width, float new_height, float min_depth, float max_depth, GraphicsContext* context);

	ECSENGINE_API void BindMesh(const Mesh& mesh, GraphicsContext* context);

	ECSENGINE_API void BindMesh(const Mesh& mesh, GraphicsContext* context, Stream<ECS_MESH_INDEX> mapping);

	ECSENGINE_API void BindMaterial(const Material& material, GraphicsContext* context);

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

	ECSENGINE_API ID3D11CommandList* FinishCommandList(GraphicsContext* context, bool restore_state = false);

	ECSENGINE_API GraphicsPipelineBlendState GetBlendState(GraphicsContext* context);

	ECSENGINE_API GraphicsPipelineDepthStencilState GetDepthStencilState(GraphicsContext* context);

	ECSENGINE_API GraphicsPipelineRasterizerState GetRasterizerState(GraphicsContext* context);

	ECSENGINE_API GraphicsPipelineRenderState GetRenderState(GraphicsContext* context);

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

	ECSENGINE_API void RestoreRenderState(GraphicsContext* context, GraphicsPipelineRenderState render_state);

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
	ECSENGINE_API void FreeMaterial(Graphics* graphics, const Material& material);

	// Merges the vertex buffers and the index buffers into a single resource that can reduce 
	// the bind calls by moving the offsets into the draw call; it returns the aggregate mesh
	// and the submeshes - the offsets into the buffers; it will release the initial mesh 
	// buffers; The meshes must have the same index buffer int size
	// The mesh will have no name associated with it
	// It will release the graphics resources of the meshes
	// The submeshes will inherit the mesh name if it has one
	ECSENGINE_API Mesh MeshesToSubmeshes(Graphics* graphics, containers::Stream<Mesh> meshes, Submesh* submeshes);

	// Same as the non mask variant - the difference is that it will only convert the meshes specified
	// in the mesh mask
	// The mesh will have no name associated with it
	// It will release the graphics resources of the meshes
	// The submeshes will inherit the mesh name if it has one
	ECSENGINE_API Mesh MeshesToSubmeshes(Graphics* graphics, containers::Stream<Mesh> meshes, Submesh* submeshes, containers::Stream<unsigned int> mesh_mask);

#endif // ECSENGINE_DIRECTX11


#endif // ECSENGINE_PLATFORM_WINDOWS

}

