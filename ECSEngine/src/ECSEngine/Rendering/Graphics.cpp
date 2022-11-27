#include "ecspch.h"
#include "Graphics.h"
#include "../Utilities/FunctionInterfaces.h"
#include "GraphicsHelpers.h"
#include "../Utilities/Function.h"
#include "../Utilities/File.h"
#include "../Utilities/Path.h"
#include "ShaderInclude.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Utilities/Crash.h"

// The library won't compile with this macro, microsoft quality btw
#define FAR 
#include <dxgi1_6.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "dxgi.lib")

namespace ECSEngine {
	
	const char* SHADER_ENTRY_POINT = "main";

#ifdef ECSENGINE_PLATFORM_WINDOWS

#ifdef ECSENGINE_DIRECTX11

	const char* SHADER_COMPILE_TARGET[] = {
		"vs_5_0",
		"vs_5_1",
		"ps_5_0",
		"ps_5_1",
		"ds_5_0",
		"ds_5_1",
		"hs_5_0",
		"hs_5_1",
		"gs_5_0",
		"gs_5_1",
		"cs_5_0",
		"cs_5_1"
	};

	const wchar_t* SHADER_HELPERS_VERTEX[] = {
		ECS_VERTEX_SHADER_SOURCE(EquirectangleToCube),
		ECS_VERTEX_SHADER_SOURCE(Skybox),
		ECS_VERTEX_SHADER_SOURCE(ConvoluteDiffuseEnvironment),
		ECS_VERTEX_SHADER_SOURCE(ConvoluteSpecularEnvironment),
		ECS_VERTEX_SHADER_SOURCE(BRDFIntegration),
		ECS_VERTEX_SHADER_SOURCE(GLTFThumbnail)
	};

	const wchar_t* SHADER_HELPERS_PIXEL[] = {
		ECS_PIXEL_SHADER_SOURCE(EquirectangleToCube),
		ECS_PIXEL_SHADER_SOURCE(Skybox),
		ECS_PIXEL_SHADER_SOURCE(ConvoluteDiffuseEnvironment),
		ECS_PIXEL_SHADER_SOURCE(ConvoluteSpecularEnvironment),
		ECS_PIXEL_SHADER_SOURCE(BRDFIntegration),
		ECS_PIXEL_SHADER_SOURCE(GLTFThumbnail)
	};

#define GRAPHICS_INTERNAL_RESOURCE_STARTING_COUNT 1024

	void InitializeGraphicsHelpers(Graphics* graphics) {
		graphics->m_shader_helpers.Initialize(graphics->m_allocator, ECS_GRAPHICS_SHADER_HELPER_COUNT, ECS_GRAPHICS_SHADER_HELPER_COUNT);

		auto load_source_code = [&](const wchar_t* path) {
			return ReadWholeFileText(path, GetAllocatorPolymorphic(graphics->m_allocator));
		};

		Stream<wchar_t> include_directory = ECS_SHADER_DIRECTORY;
		ShaderIncludeFiles include(graphics->m_allocator, { &include_directory, 1 });
		for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
			Stream<char> vertex_source = load_source_code(SHADER_HELPERS_VERTEX[index]);
			ECS_ASSERT(vertex_source.buffer != nullptr);

			Stream<void> vertex_byte_code;
			graphics->m_shader_helpers[index].vertex = graphics->CreateVertexShaderFromSource(vertex_source, &include, {}, &vertex_byte_code, true);

			Stream<char> pixel_source = load_source_code(SHADER_HELPERS_PIXEL[index]);
			ECS_ASSERT(pixel_source.buffer != nullptr);

			graphics->m_shader_helpers[index].pixel = graphics->CreatePixelShaderFromSource(pixel_source, &include, {}, true);
			graphics->m_shader_helpers[index].input_layout = graphics->ReflectVertexShaderInput(vertex_source, vertex_byte_code, true, true);

			graphics->m_allocator->Deallocate(vertex_source.buffer);
			graphics->m_allocator->Deallocate(pixel_source.buffer);
		}

		SamplerDescriptor sampler_descriptor;
		sampler_descriptor.address_type_u = ECS_SAMPLER_ADDRESS_CLAMP;
		sampler_descriptor.address_type_v = ECS_SAMPLER_ADDRESS_CLAMP;
		sampler_descriptor.address_type_w = ECS_SAMPLER_ADDRESS_CLAMP;
		for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
			graphics->m_shader_helpers[index].pixel_sampler = graphics->CreateSamplerState(sampler_descriptor, true);
		}
	}

	void EnumGPU(CapacityStream<IDXGIAdapter1*>& adapters, DXGI_GPU_PREFERENCE preference) {
		IDXGIAdapter1* adapter;
		IDXGIFactory6* factory = NULL;

		// Create a DXGIFactory object.
		if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory6), (void**)&factory))) {
			return;
		}

		for (unsigned int index = 0; factory->EnumAdapterByGpuPreference(index, preference, __uuidof(*adapter), (void**)&adapter) != DXGI_ERROR_NOT_FOUND; index++) {
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
				continue;
			}

			adapters.AddSafe(adapter);
		}

		if (factory != nullptr) {
			factory->Release();
		}
	}
	
	void EnumerateDiscreteGPU(CapacityStream<IDXGIAdapter1*>& adapters) {
		EnumGPU(adapters, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE);
	}

	void EnumerateIntegratedGPU(CapacityStream<IDXGIAdapter1*>& adapters) {
		EnumGPU(adapters, DXGI_GPU_PREFERENCE_MINIMUM_POWER);
	}

	Graphics::Graphics(const GraphicsDescriptor* descriptor) 
		: m_target_view(nullptr), m_device(nullptr), m_context(nullptr), m_swap_chain(nullptr), m_allocator(descriptor->allocator),
		m_bound_render_target_count(1)
	{
		// The internal resources
		m_internal_resources.Initialize(descriptor->allocator, GRAPHICS_INTERNAL_RESOURCE_STARTING_COUNT);
		m_internal_resources_lock.unlock();

		unsigned int flags = 0;
//#ifdef ECSENGINE_DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
//#endif

		ECS_STACK_CAPACITY_STREAM(IDXGIAdapter1*, adapters, 64);
		if (descriptor->discrete_gpu) {
			EnumerateDiscreteGPU(adapters);
		}
		else {
			EnumerateIntegratedGPU(adapters);
		}

		ECS_ASSERT(adapters.size > 0, "There are no GPUs available");

		HRESULT result = S_OK;
		if (descriptor->create_swap_chain) {
			const float resize_factor = 1.0f;
			m_window_size.x = descriptor->window_size.x * resize_factor;
			m_window_size.y = descriptor->window_size.y * resize_factor;

			DXGI_SWAP_CHAIN_DESC swap_chain_descriptor = {};
			swap_chain_descriptor.BufferDesc.Width = m_window_size.x;
			swap_chain_descriptor.BufferDesc.Height = m_window_size.y;
			swap_chain_descriptor.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swap_chain_descriptor.BufferDesc.RefreshRate.Numerator = 0;
			swap_chain_descriptor.BufferDesc.RefreshRate.Denominator = 0;
			swap_chain_descriptor.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			swap_chain_descriptor.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			swap_chain_descriptor.SampleDesc.Count = 1;
			swap_chain_descriptor.SampleDesc.Quality = 0;
			swap_chain_descriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
			swap_chain_descriptor.BufferCount = 2;
			swap_chain_descriptor.OutputWindow = descriptor->hWnd;
			swap_chain_descriptor.Windowed = TRUE;
			swap_chain_descriptor.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swap_chain_descriptor.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

			// create device, front and back buffers, swap chain and rendering context
			result = D3D11CreateDeviceAndSwapChain(
				adapters[0],
				D3D_DRIVER_TYPE_UNKNOWN,
				nullptr,
				flags,
				nullptr,
				0,
				D3D11_SDK_VERSION,
				&swap_chain_descriptor,
				&m_swap_chain,
				&m_device,
				nullptr,
				&m_context
			);
		}
		else {
			m_swap_chain = nullptr;

			// Create a new device and context
			result = D3D11CreateDevice(
				adapters[0],
				D3D_DRIVER_TYPE_UNKNOWN,
				nullptr,
				flags,
				nullptr,
				0,
				D3D11_SDK_VERSION,
				&m_device,
				nullptr,
				&m_context
			);
		}

		ECS_CRASH_RETURN(SUCCEEDED(result), "Initializing device and swap chain failed!");

		result = m_device->CreateDeferredContext(0, &m_deferred_context);

		ECS_CRASH_RETURN(SUCCEEDED(result), "Creating deferred context failed!");

		if (descriptor->create_swap_chain) {
			CreateRenderTargetViewFromSwapChain(descriptor->gamma_corrected);
			CreateWindowDepthStencilView();
			BindRenderTargetView(m_target_view, m_depth_stencil_view);
		}
		else {
			if (descriptor->window_size.x != 0 && descriptor->window_size.y != 0) {
				CreateWindowDepthStencilView();
				CreateWindowRenderTargetView(descriptor->gamma_corrected);
				BindRenderTargetView(m_target_view, m_depth_stencil_view);
			}
		}

		DepthStencilState depth_stencil_state = CreateDepthStencilStateDefault(true);
		RasterizerState rasterizer_state = CreateRasterizerStateDefault(true);
		BlendState blend_state = CreateBlendStateDefault(true);

		BindDepthStencilState(depth_stencil_state);
		BindRasterizerState(rasterizer_state);
		BindBlendState(blend_state);

		depth_stencil_state.Release();
		rasterizer_state.Release();
		blend_state.Release();

		BindViewport(0, 0, m_window_size.x, m_window_size.y, 0.0f, 1.0f);

		// Shader Reflection
		size_t memory_size = m_shader_reflection->MemoryOf();
		void* allocation = m_allocator->Allocate(memory_size);
		m_shader_reflection = new ShaderReflection(allocation);

		InitializeGraphicsHelpers(this);
	}

	Graphics::Graphics(const Graphics& other) {
		memcpy(this, &other, sizeof(other));
	}
	
	Graphics& Graphics::operator = (const Graphics& other) {
		memcpy(this, &other, sizeof(other));
		return *this;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexBuffer(VertexBuffer buffer, unsigned int slot) {
		ECSEngine::BindVertexBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexBuffers(Stream<VertexBuffer> buffers, unsigned int start_slot) {
		ECSEngine::BindVertexBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindIndexBuffer(IndexBuffer index_buffer) {
		ECSEngine::BindIndexBuffer(index_buffer, m_context);
	}
	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindInputLayout(InputLayout input_layout) {
		ECSEngine::BindInputLayout(input_layout, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexShader(VertexShader shader) {
		ECSEngine::BindVertexShader(shader, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelShader(PixelShader shader) {
		ECSEngine::BindPixelShader(shader, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainShader(DomainShader shader) {
		ECSEngine::BindDomainShader(shader, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullShader(HullShader shader) {
		ECSEngine::BindHullShader(shader, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryShader(GeometryShader shader) {
		ECSEngine::BindGeometryShader(shader, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeShader(ComputeShader shader) {
		ECSEngine::BindComputeShader(shader, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelConstantBuffer(ConstantBuffer buffer, unsigned int slot) {
		ECSEngine::BindPixelConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot) {
		ECSEngine::BindPixelConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexConstantBuffer(ConstantBuffer buffer, unsigned int slot) {
		ECSEngine::BindVertexConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot) {
		ECSEngine::BindVertexConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainConstantBuffer(ConstantBuffer buffer, unsigned int slot) {
		ECSEngine::BindDomainConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot) {
		ECSEngine::BindDomainConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullConstantBuffer(ConstantBuffer buffer, unsigned int slot) {
		ECSEngine::BindHullConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot) {
		ECSEngine::BindHullConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryConstantBuffer(ConstantBuffer buffer, unsigned int slot) {
		ECSEngine::BindGeometryConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot) {
		ECSEngine::BindGeometryConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeConstantBuffer(ConstantBuffer buffer, unsigned int slot) {
		ECSEngine::BindComputeConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeConstantBuffers(Stream<ConstantBuffer> buffers, unsigned int start_slot) {
		ECSEngine::BindComputeConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindTopology(Topology topology)
	{
		ECSEngine::BindTopology(topology, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelResourceView(ResourceView component, unsigned int slot) {
		ECSEngine::BindPixelResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelResourceViews(Stream<ResourceView> views, unsigned int start_slot) {
		ECSEngine::BindPixelResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexResourceView(ResourceView component, unsigned int slot) {
		ECSEngine::BindVertexResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexResourceViews(Stream<ResourceView> views, unsigned int start_slot) {
		ECSEngine::BindVertexResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainResourceView(ResourceView component, unsigned int slot) {
		ECSEngine::BindDomainResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainResourceViews(Stream<ResourceView> views, unsigned int start_slot) {
		ECSEngine::BindDomainResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullResourceView(ResourceView component, unsigned int slot) {
		ECSEngine::BindHullResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullResourceViews(Stream<ResourceView> views, unsigned int start_slot) {
		ECSEngine::BindHullResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryResourceView(ResourceView component, unsigned int slot) {
		ECSEngine::BindGeometryResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryResourceViews(Stream<ResourceView> views, unsigned int start_slot) {
		ECSEngine::BindGeometryResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeResourceView(ResourceView component, unsigned int slot) {
		ECSEngine::BindComputeResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeResourceViews(Stream<ResourceView> views, unsigned int start_slot) {
		ECSEngine::BindComputeResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindSamplerState(SamplerState sampler, unsigned int slot)
	{
		ECSEngine::BindSamplerState(sampler, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindSamplerStates(Stream<SamplerState> samplers, unsigned int start_slot)
	{
		ECSEngine::BindSamplerStates(samplers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelUAView(UAView view, unsigned int start_slot)
	{
		m_context->OMSetRenderTargetsAndUnorderedAccessViews(
			D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, 
			nullptr, 
			nullptr,
			m_bound_render_target_count,
			1u,
			&view.view, 
			nullptr
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeUAView(UAView view, unsigned int start_slot)
	{
		ECSEngine::BindComputeUAView(view, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelUAViews(Stream<UAView> views, unsigned int start_slot) {
		m_context->OMSetRenderTargetsAndUnorderedAccessViews(
			D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, 
			nullptr, 
			nullptr,
			m_bound_render_target_count, 
			views.size, 
			(ID3D11UnorderedAccessView**)views.buffer, 
			nullptr
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeUAViews(Stream<UAView> views, unsigned int start_slot) {
		ECSEngine::BindComputeUAViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRasterizerState(RasterizerState state)
	{
		ECSEngine::BindRasterizerState(state, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDepthStencilState(DepthStencilState state, unsigned int stencil_ref)
	{
		ECSEngine::BindDepthStencilState(state, m_context, stencil_ref);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindBlendState(BlendState state)
	{
		ECSEngine::BindBlendState(state, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRenderTargetViewFromInitialViews()
	{
		m_context->OMSetRenderTargets(1u, &m_target_view.view, m_depth_stencil_view.view);
		m_bound_render_targets[0] = m_target_view;
		m_bound_render_target_count = 1;
		m_current_depth_stencil = m_depth_stencil_view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRenderTargetViewFromInitialViews(GraphicsContext* context)
	{
		context->OMSetRenderTargets(1u, &m_target_view.view, m_depth_stencil_view.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view)
	{
		ECSEngine::BindRenderTargetView(render_view, depth_stencil_view, m_context);
		m_bound_render_targets[0] = render_view;
		m_bound_render_target_count = 1;
		m_current_depth_stencil = depth_stencil_view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth)
	{
		ECSEngine::BindViewport(top_left_x, top_left_y, width, height, min_depth, max_depth, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindViewport(GraphicsViewport viewport)
	{
		BindViewport(viewport.top_left_x, viewport.top_left_y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDefaultViewport() {
		BindViewport(0.0f, 0.0f, m_window_size.x, m_window_size.y, 0.0f, 1.0f);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDefaultViewport(GraphicsContext* context) {
		ECSEngine::BindViewport(0.0f, 0.0f, m_window_size.x, m_window_size.y, 0.0f, 1.0f, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindMesh(const Mesh& mesh)
	{
		ECSEngine::BindMesh(mesh, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindMesh(const Mesh& mesh, Stream<ECS_MESH_INDEX> mapping)
	{
		ECSEngine::BindMesh(mesh, m_context, mapping);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindMaterial(const Material& material) {
		ECSEngine::BindMaterial(material, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHelperShader(GraphicsShaderHelpers helper)
	{
		BindVertexShader(m_shader_helpers[helper].vertex);
		BindPixelShader(m_shader_helpers[helper].pixel);
		BindInputLayout(m_shader_helpers[helper].input_layout);
		BindSamplerState(m_shader_helpers[helper].pixel_sampler);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(Stream<unsigned char> indices, bool temporary, DebugInfo debug_info)
	{ 
		return CreateIndexBuffer(
			sizeof(unsigned char), 
			indices.size, 
			indices.buffer, 
			temporary,
			ECS_GRAPHICS_USAGE_IMMUTABLE, 
			ECS_GRAPHICS_CPU_ACCESS_NONE,
			ECS_GRAPHICS_MISC_NONE,
			debug_info
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(Stream<unsigned short> indices, bool temporary, DebugInfo debug_info)
	{
		return CreateIndexBuffer(
			sizeof(unsigned short), 
			indices.size,
			indices.buffer, 
			temporary, 
			ECS_GRAPHICS_USAGE_IMMUTABLE,
			ECS_GRAPHICS_CPU_ACCESS_NONE,
			ECS_GRAPHICS_MISC_NONE,
			debug_info
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(Stream<unsigned int> indices, bool temporary, DebugInfo debug_info)
	{
		return CreateIndexBuffer(
			sizeof(unsigned int),
			indices.size, 
			indices.buffer,
			temporary,
			ECS_GRAPHICS_USAGE_IMMUTABLE, 
			ECS_GRAPHICS_CPU_ACCESS_NONE, 
			ECS_GRAPHICS_MISC_NONE, 
			debug_info
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(
		size_t int_size, 
		size_t element_count, 
		bool temporary, 
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access, 
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		IndexBuffer buffer;
		buffer.count = element_count;
		buffer.int_size = int_size;

		HRESULT result;
		D3D11_BUFFER_DESC index_buffer_descriptor = {};
		index_buffer_descriptor.ByteWidth = unsigned int(buffer.count * int_size);
		index_buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		index_buffer_descriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
		index_buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		index_buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		index_buffer_descriptor.StructureByteStride = int_size;

		result = m_device->CreateBuffer(&index_buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating index buffer failed");

		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(
		size_t int_size, 
		size_t element_count, 
		const void* data, 
		bool temporary, 
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		IndexBuffer buffer;
		buffer.count = element_count;
		buffer.int_size = int_size;

		HRESULT result;
		D3D11_BUFFER_DESC index_buffer_descriptor = {};
		index_buffer_descriptor.ByteWidth = unsigned int(buffer.count * int_size);
		index_buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		index_buffer_descriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
		index_buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		index_buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		index_buffer_descriptor.StructureByteStride = int_size;

		D3D11_SUBRESOURCE_DATA subresource_data = {};
		subresource_data.pSysMem = data;

		result = m_device->CreateBuffer(&index_buffer_descriptor, &subresource_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating index buffer failed");
		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ID3DBlob* ShaderByteCode(GraphicsDevice* device, Stream<char> source_code, ShaderCompileOptions options, ID3DInclude* include_policy, ECS_SHADER_TYPE type) {
		D3D_SHADER_MACRO macros[64];
		ECS_ASSERT(options.macros.size <= 64 - 2);
		memcpy(macros, options.macros.buffer, sizeof(const char*) * 2 * options.macros.size);
		macros[options.macros.size] = { NULL, NULL };

		const char* target = SHADER_COMPILE_TARGET[type * ECS_SHADER_TARGET_COUNT + options.target];

		unsigned int compile_flags = 0;
		switch (options.compile_flags) {
		case ECS_SHADER_COMPILE_DEBUG:
			compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
			break;
		case ECS_SHADER_COMPILE_OPTIMIZATION_LOWEST:
			compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL0;
			break;
		case ECS_SHADER_COMPILE_OPTIMIZATION_LOW:
			compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL1;
			break;
		case ECS_SHADER_COMPILE_OPTIMIZATION_HIGH:
			compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL2;
			break;
		case ECS_SHADER_COMPILE_OPTIMIZATION_HIGHEST:
			compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
			break;
		}

		ID3DBlob* blob;
		ID3DBlob* error_message_blob = nullptr;
		HRESULT result = D3DCompile(source_code.buffer, source_code.size, nullptr, macros, include_policy, SHADER_ENTRY_POINT, target, compile_flags, 0, &blob, &error_message_blob);

		const char* error_message;
		if (error_message_blob != nullptr) {
			error_message = (char*)error_message_blob->GetBufferPointer();
			error_message_blob->Release();
		}

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), blob, "Compiling a shader failed.");
		return blob;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	PixelShader Graphics::CreatePixelShader(Stream<void> byte_code, bool temporary, DebugInfo debug_info)
	{
		PixelShader shader;

		HRESULT result;
		result = m_device->CreatePixelShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Pixel shader failed.");
		AddInternalResource(shader, temporary, debug_info);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	PixelShader Graphics::CreatePixelShaderFromSource(Stream<char> source_code, ID3DInclude* include_policy, ShaderCompileOptions options, bool temporary, DebugInfo debug_info)
	{
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_PIXEL);
		
		if (byte_code == nullptr) {
			return { nullptr };
		}

		PixelShader shader = CreatePixelShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary, debug_info);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	// The vertex shader must not have the blob released
	VertexShader Graphics::CreateVertexShader(Stream<void> byte_code, bool temporary, DebugInfo debug_info) {
		VertexShader shader;

		HRESULT result;
		result = m_device->CreateVertexShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Vertex shader failed.");
		
		AddInternalResource(shader, temporary, debug_info);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexShader Graphics::CreateVertexShaderFromSource(
		Stream<char> source_code, 
		ID3DInclude* include_policy, 
		ShaderCompileOptions options,
		Stream<void>* vertex_byte_code,
		bool temporary,
		DebugInfo debug_info
	) {
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_VERTEX);
		if (byte_code == nullptr) {
			return { nullptr };
		}

		if (vertex_byte_code != nullptr) {
			void* allocation = m_allocator->Allocate_ts(byte_code->GetBufferSize());
			vertex_byte_code->buffer = allocation;
			memcpy(allocation, byte_code->GetBufferPointer(), byte_code->GetBufferSize());
			vertex_byte_code->size = byte_code->GetBufferSize();
		}

		VertexShader shader = CreateVertexShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary, debug_info);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader Graphics::CreateDomainShader(Stream<void> byte_code, bool temporary, DebugInfo debug_info)
	{
		DomainShader shader;

		HRESULT result; 
		result = m_device->CreateDomainShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Domain shader failed."); 
		AddInternalResource(shader, temporary, debug_info);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader Graphics::CreateDomainShaderFromSource(
		Stream<char> source_code,
		ID3DInclude* include_policy, 
		ShaderCompileOptions options,
		bool temporary, 
		DebugInfo debug_info
	)
	{
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_DOMAIN);
		if (byte_code == nullptr) {
			return { nullptr };
		}

		DomainShader shader = CreateDomainShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary, debug_info);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HullShader Graphics::CreateHullShader(Stream<void> byte_code, bool temporary, DebugInfo debug_info) {
		HullShader shader;

		HRESULT result;
		result = m_device->CreateHullShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Hull shader failed.");
		
		AddInternalResource(shader, temporary, debug_info);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HullShader Graphics::CreateHullShaderFromSource(
		Stream<char> source_code, 
		ID3DInclude* include_policy,
		ShaderCompileOptions options,
		bool temporary, 
		DebugInfo debug_info
	) {
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_HULL);
		if (byte_code == nullptr) {
			return { nullptr };
		}

		HullShader shader = CreateHullShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary, debug_info);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GeometryShader Graphics::CreateGeometryShader(Stream<void> byte_code, bool temporary, DebugInfo debug_info) {
		GeometryShader shader;

		HRESULT result;
		result = m_device->CreateGeometryShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Geometry shader failed.");
		AddInternalResource(shader, temporary, debug_info);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GeometryShader Graphics::CreateGeometryShaderFromSource(
		Stream<char> source_code, 
		ID3DInclude* include_policy,
		ShaderCompileOptions options,
		bool temporary,
		DebugInfo debug_info
	) {
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_GEOMETRY);
		if (byte_code == nullptr) {
			return { nullptr };
		}

		GeometryShader shader = CreateGeometryShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary, debug_info);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ComputeShader Graphics::CreateComputeShader(Stream<void> byte_code, bool temporary, DebugInfo debug_info) {
		ComputeShader shader;

		HRESULT result;
		result = m_device->CreateComputeShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Compute shader failed.");
		AddInternalResource(shader, temporary, debug_info);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ComputeShader Graphics::CreateComputeShaderFromSource(
		Stream<char> source_code, 
		ID3DInclude* include_policy,
		ShaderCompileOptions options,
		bool temporary, 
		DebugInfo debug_info
	) {
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_COMPUTE);
		if (byte_code == nullptr) {
			return { nullptr };
		}

		ComputeShader shader = CreateComputeShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary, debug_info);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::CreateShader(Stream<void> path, ECS_SHADER_TYPE type, bool temporary, DebugInfo debug_info)
	{
		switch (type)
		{
		case ECS_SHADER_VERTEX:
			return CreateVertexShader(path, temporary, debug_info).shader;
		case ECS_SHADER_PIXEL:
			return CreatePixelShader(path, temporary, debug_info).shader;
		case ECS_SHADER_DOMAIN:
			return CreateDomainShader(path, temporary, debug_info).shader;
		case ECS_SHADER_HULL:
			return CreateHullShader(path, temporary, debug_info).shader;
		case ECS_SHADER_GEOMETRY:
			return CreateGeometryShader(path, temporary, debug_info).shader;
		case ECS_SHADER_COMPUTE:
			return CreateComputeShader(path, temporary, debug_info).shader;
		default:
			ECS_ASSERT(false);
		}

		return nullptr;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::CreateShaderFromSource(
		Stream<char> source_code, 
		ECS_SHADER_TYPE type, 
		ID3DInclude* include_policy,
		ShaderCompileOptions options,
		Stream<void>* byte_code,
		bool temporary, 
		DebugInfo debug_info
	)
	{
		switch (type)
		{
		case ECS_SHADER_VERTEX:
			return CreateVertexShaderFromSource(source_code, include_policy, options, byte_code, temporary, debug_info).shader;
		case ECS_SHADER_PIXEL:
			return CreatePixelShaderFromSource(source_code, include_policy, options, temporary, debug_info).shader;
		case ECS_SHADER_DOMAIN:
			return CreateDomainShaderFromSource(source_code, include_policy, options, temporary, debug_info).shader;
		case ECS_SHADER_HULL:
			return CreateHullShaderFromSource(source_code, include_policy, options, temporary, debug_info).shader;
		case ECS_SHADER_GEOMETRY:
			return CreateGeometryShaderFromSource(source_code, include_policy, options, temporary, debug_info).shader;
		case ECS_SHADER_COMPUTE:
			return CreateComputeShaderFromSource(source_code, include_policy, options, temporary, debug_info).shader;
		default:
			ECS_ASSERT(false);
		}

		return nullptr;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Stream<void> Graphics::CompileShaderToByteCode(Stream<char> source_code, ECS_SHADER_TYPE type, ID3DInclude* include_policy, AllocatorPolymorphic allocator, ShaderCompileOptions options)
	{
		if (allocator.allocator == nullptr) {
			allocator = GetAllocatorPolymorphic(m_allocator, ECS_ALLOCATION_MULTI);
		}

		ID3DBlob* blob = ShaderByteCode(GetDevice(), source_code, options, include_policy, type);
		if (blob == nullptr) {
			return { nullptr, 0 };
		}

		void* allocation = Allocate(allocator, blob->GetBufferSize());
		memcpy(allocation, blob->GetBufferPointer(), blob->GetBufferSize());
		return { allocation, blob->GetBufferSize() };
	}

	// ------------------------------------------------------------------------------------------------------------------------

	InputLayout Graphics::CreateInputLayout(
		Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, 
		Stream<void> vertex_byte_code,
		bool deallocate_byte_code,
		bool temporary,
		DebugInfo debug_info
	)
	{
		InputLayout layout;
		HRESULT result;
		
		result = m_device->CreateInputLayout(
			descriptor.buffer, 
			descriptor.size, 
			vertex_byte_code.buffer, 
			vertex_byte_code.size,
			&layout.layout
		);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), layout, "Creating input layout failed");
		
		if (deallocate_byte_code) {
			m_allocator->Deallocate_ts(vertex_byte_code.buffer);
		}
		AddInternalResource(layout, temporary, debug_info);
		return layout;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexBuffer Graphics::CreateVertexBuffer(
		size_t element_size,
		size_t element_count,
		bool temporary,
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		VertexBuffer buffer;
		buffer.stride = element_size;
		buffer.size = element_count;
		D3D11_BUFFER_DESC vertex_buffer_descriptor = {};
		vertex_buffer_descriptor.ByteWidth = element_size * element_count;
		vertex_buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		vertex_buffer_descriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertex_buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		vertex_buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		vertex_buffer_descriptor.StructureByteStride = buffer.stride;

		HRESULT result;
		result = m_device->CreateBuffer(&vertex_buffer_descriptor, nullptr, &buffer.buffer);

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating vertex buffer failed");
		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexBuffer Graphics::CreateVertexBuffer(
		size_t element_size,
		size_t element_count, 
		const void* buffer_data, 
		bool temporary,
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		VertexBuffer buffer;
		buffer.stride = element_size;
		buffer.size = element_count;

		D3D11_BUFFER_DESC vertex_buffer_descriptor = {};
		vertex_buffer_descriptor.ByteWidth = unsigned int(element_size * element_count);
		vertex_buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		vertex_buffer_descriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertex_buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		vertex_buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		vertex_buffer_descriptor.StructureByteStride = buffer.stride;

		D3D11_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = buffer_data;
		initial_data.SysMemPitch = vertex_buffer_descriptor.ByteWidth;

		HRESULT result;
		result = m_device->CreateBuffer(&vertex_buffer_descriptor, &initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating vertex buffer failed");
		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer Graphics::CreateConstantBuffer(
		size_t byte_size, 
		const void* buffer_data, 
		bool temporary,
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		ConstantBuffer buffer;
		HRESULT result;
		D3D11_BUFFER_DESC constant_buffer_descriptor = {};
		// Byte Width must be a multiple of 16, so padd the byte_size
		constant_buffer_descriptor.ByteWidth = function::AlignPointer(byte_size, 16);
		constant_buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		constant_buffer_descriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constant_buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		constant_buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		constant_buffer_descriptor.StructureByteStride = 0u;

		D3D11_SUBRESOURCE_DATA initial_data_constant = {};
		initial_data_constant.pSysMem = buffer_data;

		result = m_device->CreateBuffer(&constant_buffer_descriptor, &initial_data_constant, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating pixel constant buffer failed.");
		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer Graphics::CreateConstantBuffer(
		size_t byte_size,
		bool temporary, 
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		ConstantBuffer buffer;
		HRESULT result;
		D3D11_BUFFER_DESC constant_buffer_descriptor = {};
		// Byte Width must be a multiple of 16, so padd the byte_size
		constant_buffer_descriptor.ByteWidth = function::AlignPointer(byte_size, 16);
		constant_buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		constant_buffer_descriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constant_buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		constant_buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		constant_buffer_descriptor.StructureByteStride = 0u;

		result = m_device->CreateBuffer(&constant_buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating pixel constant buffer failed.");
		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StandardBuffer Graphics::CreateStandardBuffer(
		size_t element_size,
		size_t element_count,
		bool temporary,
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		StandardBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		buffer_descriptor.StructureByteStride = 0u;

		result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating standard buffer failed.");
		buffer.count = element_count;
		
		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StandardBuffer Graphics::CreateStandardBuffer(
		size_t element_size,
		size_t element_count,
		const void* data, 
		bool temporary,
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		StandardBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		buffer_descriptor.StructureByteStride = 0u;

		D3D11_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = data;

		result = m_device->CreateBuffer(&buffer_descriptor, &initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating standard buffer failed.");
		buffer.count = element_count;

		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StructuredBuffer Graphics::CreateStructuredBuffer(
		size_t element_size,
		size_t element_count,
		bool temporary,
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		StructuredBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags) | D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_descriptor.StructureByteStride = element_size;

		result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating structred buffer failed.");

		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StructuredBuffer Graphics::CreateStructuredBuffer(
		size_t element_size,
		size_t element_count, 
		const void* data, 
		bool temporary,
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		StructuredBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags) | D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_descriptor.StructureByteStride = element_size;

		D3D11_SUBRESOURCE_DATA initial_data;
		initial_data.pSysMem = data;

		result = m_device->CreateBuffer(&buffer_descriptor, &initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating Structred Buffer failed.");

		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UABuffer Graphics::CreateUABuffer(
		size_t element_size, 
		size_t element_count, 
		bool temporary,
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		UABuffer buffer;

		buffer.element_count = element_count;
		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};
		
		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		buffer_descriptor.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		buffer_descriptor.StructureByteStride = 0u;

		result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating UA Buffer failed.");

		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UABuffer Graphics::CreateUABuffer(
		size_t element_size, 
		size_t element_count,
		const void* data, 
		bool temporary,
		ECS_GRAPHICS_USAGE usage,
		ECS_GRAPHICS_CPU_ACCESS cpu_access,
		ECS_GRAPHICS_MISC_FLAGS misc_flags,
		DebugInfo debug_info
	)
	{
		UABuffer buffer;

		buffer.element_count = element_count;
		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		buffer_descriptor.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		buffer_descriptor.StructureByteStride = 0u;

		D3D11_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = data;

		result = m_device->CreateBuffer(&buffer_descriptor, &initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating UA Buffer failed.");

		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndirectBuffer Graphics::CreateIndirectBuffer(bool temporary, DebugInfo debug_info)
	{
		IndirectBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		// For draw indexed instaced we need:
		// 
		// unsigned int IndexCountPerInstance,
		// unsigned int InstanceCount,
	    // unsigned int StartIndexLocation,
		// INT  BaseVertexLocation,
		// unsigned int StartInstanceLocation

		// and for draw instanced:
		//  unsigned int VertexCountPerInstance,
		//	unsigned int InstanceCount,
		// unsigned int StartVertexLocation,
		// unsigned int StartInstanceLocation
		
		// So a total of max 20 bytes needed

		buffer_descriptor.ByteWidth = 20;
		buffer_descriptor.Usage = D3D11_USAGE_DEFAULT;
		buffer_descriptor.CPUAccessFlags = 0;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		buffer_descriptor.StructureByteStride = 0;

		result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating Indirect Buffer failed.");
		
		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	AppendStructuredBuffer Graphics::CreateAppendStructuredBuffer(size_t element_size, size_t element_count, bool temporary, DebugInfo debug_info) {
		AppendStructuredBuffer buffer;

		buffer.element_count = element_count;
		D3D11_BUFFER_DESC buffer_descriptor = {};
		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = D3D11_USAGE_DEFAULT;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_descriptor.CPUAccessFlags = 0;
		buffer_descriptor.StructureByteStride = element_size;

		HRESULT result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating Append Buffer failed.");

		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConsumeStructuredBuffer Graphics::CreateConsumeStructuredBuffer(size_t element_size, size_t element_count, bool temporary, DebugInfo debug_info) {
		ConsumeStructuredBuffer buffer;

		buffer.element_count = element_count;
		D3D11_BUFFER_DESC buffer_descriptor = {};
		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = D3D11_USAGE_DEFAULT;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_descriptor.CPUAccessFlags = 0;
		buffer_descriptor.StructureByteStride = element_size;

		HRESULT result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating Consume Buffer failed.");

		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	SamplerState Graphics::CreateSamplerState(const SamplerDescriptor& descriptor, bool temporary, DebugInfo debug_info)
	{
		D3D11_SAMPLER_DESC sampler_desc;
		sampler_desc.MaxAnisotropy = descriptor.max_anisotropic_level;
		sampler_desc.Filter = GetGraphicsNativeFilter(descriptor.filter_type);
		sampler_desc.AddressU = GetGraphicsNativeAddressMode(descriptor.address_type_u);
		sampler_desc.AddressV = GetGraphicsNativeAddressMode(descriptor.address_type_v);
		sampler_desc.AddressW = GetGraphicsNativeAddressMode(descriptor.address_type_w);

		sampler_desc.MaxLOD = descriptor.max_lod;
		sampler_desc.MinLOD = descriptor.min_lod;
		sampler_desc.MipLODBias = descriptor.mip_bias;

		memcpy(sampler_desc.BorderColor, &descriptor.border_color, sizeof(descriptor.border_color));

		SamplerState state;
		HRESULT result = m_device->CreateSamplerState(&sampler_desc, &state.sampler);

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), state, "Constructing sampler state failed!");
		AddInternalResource(state, temporary, debug_info);
		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<bool assert>
	void RemoveResourceFromTrackingImplementation(Graphics* graphics, void* resource) {
		// Spin lock while the resizing is finished
		graphics->m_internal_resources_lock.wait_locked();
		graphics->m_internal_resources_reader_count.fetch_add(1, ECS_RELAXED);

		// If the resource_count and the write count are different, then wait until they get equal
		graphics->m_internal_resources.SpinWaitWrites();
		unsigned int resource_count = graphics->m_internal_resources.size.load(ECS_RELAXED);

		for (unsigned int index = 0; index < resource_count && resource != nullptr; index++) {
			if (graphics->m_internal_resources[index].interface_pointer == resource) {
				bool expected = false;
				if (graphics->m_internal_resources[index].is_deleted.compare_exchange_weak(expected, true)) {
					// Exit by changing the interface to nullptr
					resource = nullptr;
				}
			}
		}

		if constexpr (assert) {
			ECS_ASSERT(resource == nullptr);
		}
		graphics->m_internal_resources_reader_count.fetch_sub(1, ECS_RELAXED);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic Graphics::Allocator() const
	{
		return GetAllocatorPolymorphic(m_allocator);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RemoveResourceFromTracking(void* resource)
	{
		RemoveResourceFromTrackingImplementation<true>(this, resource);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RemovePossibleResourceFromTracking(void* resource)
	{
		RemoveResourceFromTrackingImplementation<false>(this, resource);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeResourceView(ResourceView view)
	{
		ID3D11Resource* resource = view.GetResource();
		FreeResource(view);
		// It doesn't matter what the type is - only the interface
		FreeResource(Texture2D(resource));
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeUAView(UAView view)
	{
		ID3D11Resource* resource = view.GetResource();
		FreeResource(view);
		FreeResource(Texture2D(resource));
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeRenderView(RenderTargetView view)
	{
		ID3D11Resource* resource = view.GetResource();
		FreeResource(view);
		FreeResource(Texture2D(resource));
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CreateWindowRenderTargetView(bool gamma_corrected)
	{
		// gain access to the texture subresource of the back buffer
		GraphicsTexture2DDescriptor descriptor;
		descriptor.format = gamma_corrected ? ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB : ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		descriptor.bind_flag |= ECS_GRAPHICS_BIND_RENDER_TARGET;
		descriptor.size = m_window_size;

		Texture2D texture = CreateTexture(&descriptor);
		m_target_view = CreateRenderTargetView(texture);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CreateRenderTargetViewFromSwapChain(bool gamma_corrected)
	{
		// gain access to the texture subresource of the back buffer
		HRESULT result;
		Microsoft::WRL::ComPtr<ID3D11Resource> back_buffer;
		result = m_swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), &back_buffer);

		ECS_CRASH_RETURN(SUCCEEDED(result), "Acquiring buffer resource failed");

		if (gamma_corrected) {
			D3D11_RENDER_TARGET_VIEW_DESC descriptor;
			descriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			descriptor.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			descriptor.Texture2D.MipSlice = 0;
			result = m_device->CreateRenderTargetView(back_buffer.Get(), &descriptor, &m_target_view.view);
		}
		else {
			result = m_device->CreateRenderTargetView(back_buffer.Get(), nullptr, &m_target_view.view);
		}

		ECS_CRASH_RETURN(SUCCEEDED(result), "Creating render target view failed");
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CreateWindowDepthStencilView()
	{
		HRESULT result;
		// creating depth texture
		ID3D11Texture2D* depth_texture;
		D3D11_TEXTURE2D_DESC depth_texture_descriptor = {};
		depth_texture_descriptor.Height = m_window_size.y;
		depth_texture_descriptor.Width = m_window_size.x;
		depth_texture_descriptor.MipLevels = 1u;
		depth_texture_descriptor.ArraySize = 1u;
		depth_texture_descriptor.SampleDesc.Count = 1u;
		depth_texture_descriptor.SampleDesc.Quality = 0u;
		depth_texture_descriptor.Format = DXGI_FORMAT_D32_FLOAT;
		depth_texture_descriptor.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depth_texture_descriptor.Usage = D3D11_USAGE_DEFAULT;

		result = m_device->CreateTexture2D(&depth_texture_descriptor, nullptr, &depth_texture);

		ECS_CRASH_RETURN(SUCCEEDED(result), "Creating depth texture failed");

		// creating depth stencil texture view
		D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_descriptor = {};
		depth_view_descriptor.Format = DXGI_FORMAT_D32_FLOAT;
		depth_view_descriptor.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depth_view_descriptor.Texture2D.MipSlice = 0u;

		result = m_device->CreateDepthStencilView(depth_texture, &depth_view_descriptor, &m_depth_stencil_view.view);

		ECS_CRASH_RETURN(SUCCEEDED(result), "Creating depth stencil view failed");
		depth_texture->Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsContext* Graphics::CreateDeferredContext(unsigned int context_flags, DebugInfo debug_info)
	{
		GraphicsContext* context = nullptr;
		HRESULT result = m_device->CreateDeferredContext(0, &context);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), context, "Failed to create deferred context.");

		AddInternalResource(context, debug_info);
		return context;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture1D Graphics::CreateTexture(const GraphicsTexture1DDescriptor* ecs_descriptor, bool temporary, DebugInfo debug_info)
	{
		Texture1D resource;
		D3D11_TEXTURE1D_DESC descriptor = { 0 };
		descriptor.Format = GetGraphicsNativeFormat(ecs_descriptor->format);
		descriptor.ArraySize = ecs_descriptor->array_size;
		descriptor.Width = ecs_descriptor->width;
		descriptor.MipLevels = ecs_descriptor->mip_levels;
		descriptor.Usage = GetGraphicsNativeUsage(ecs_descriptor->usage);
		descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(ecs_descriptor->cpu_flag);
		descriptor.BindFlags = GetGraphicsNativeBind(ecs_descriptor->bind_flag);
		descriptor.MiscFlags = GetGraphicsNativeMiscFlags(ecs_descriptor->misc_flag);

		HRESULT result;

		if (ecs_descriptor->mip_data.buffer == nullptr) {
			result = m_device->CreateTexture1D(&descriptor, nullptr, &resource.tex);
		}
		else {
			D3D11_SUBRESOURCE_DATA subresource_data[32];
			memset(subresource_data, 0, sizeof(D3D11_SUBRESOURCE_DATA) * 32);
			for (size_t index = 0; index < ecs_descriptor->mip_data.size; index++) {
				subresource_data[index].pSysMem = ecs_descriptor->mip_data[index].buffer;
			}
			result = m_device->CreateTexture1D(&descriptor, subresource_data, &resource.tex);
		}

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), resource, "Creating Texture 1D failed!");
		AddInternalResource(resource, temporary, debug_info);
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture2D Graphics::CreateTexture(const GraphicsTexture2DDescriptor* ecs_descriptor, bool temporary, DebugInfo debug_info)
	{
		Texture2D resource;
		D3D11_TEXTURE2D_DESC descriptor = { 0 };
		descriptor.Format = GetGraphicsNativeFormat(ecs_descriptor->format);
		descriptor.Width = ecs_descriptor->size.x;
		descriptor.Height = ecs_descriptor->size.y;
		descriptor.MipLevels = ecs_descriptor->mip_levels;
		descriptor.ArraySize = ecs_descriptor->array_size;
		descriptor.BindFlags = GetGraphicsNativeBind(ecs_descriptor->bind_flag);
		descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(ecs_descriptor->cpu_flag);
		descriptor.MiscFlags = GetGraphicsNativeMiscFlags(ecs_descriptor->misc_flag);
		descriptor.SampleDesc.Count = ecs_descriptor->sample_count;
		descriptor.SampleDesc.Quality = ecs_descriptor->sampler_quality;
		descriptor.Usage = GetGraphicsNativeUsage(ecs_descriptor->usage);

		HRESULT result;

		if (ecs_descriptor->mip_data.buffer == nullptr) {
			result = m_device->CreateTexture2D(&descriptor, nullptr, &resource.tex);
		}
		else {
			D3D11_SUBRESOURCE_DATA subresource_data[32];
			memset(subresource_data, 0, sizeof(D3D11_SUBRESOURCE_DATA) * 32);
			unsigned int height = ecs_descriptor->size.y;

			unsigned int pitch_multiplier = 1;
			if (IsGraphicsFormatBC(ecs_descriptor->format)) {
				pitch_multiplier = 4;
			}

			for (size_t index = 0; index < ecs_descriptor->mip_data.size; index++) {
				subresource_data[index].pSysMem = ecs_descriptor->mip_data[index].buffer;
				subresource_data[index].SysMemPitch = ecs_descriptor->mip_data[index].size / height * pitch_multiplier;
				subresource_data[index].SysMemSlicePitch = 0;

				height = height == 1 ? 1 : height >> 1;
			}
			result = m_device->CreateTexture2D(&descriptor, subresource_data, &resource.tex);
		}

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), resource, "Creating Texture 2D failed!");
		AddInternalResource(resource, temporary, debug_info);
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureWithMips(Stream<void> first_mip, ECS_GRAPHICS_FORMAT format, uint2 size, bool temporary, DebugInfo debug_info)
	{
		D3D11_TEXTURE2D_DESC dx_descriptor;
		dx_descriptor.SampleDesc.Count = 1;
		dx_descriptor.SampleDesc.Quality = 0;

		dx_descriptor.Format = GetGraphicsNativeFormat(format);
		dx_descriptor.ArraySize = 1;
		dx_descriptor.BindFlags = GetGraphicsNativeBind(ECS_GRAPHICS_BIND_RENDER_TARGET | ECS_GRAPHICS_BIND_SHADER_RESOURCE);
		dx_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(ECS_GRAPHICS_CPU_ACCESS_NONE);

		dx_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(/*ECS_GRAPHICS_MISC_GENERATE_MIPS | */ECS_GRAPHICS_MISC_SHARED);
		dx_descriptor.Usage = GetGraphicsNativeUsage(ECS_GRAPHICS_USAGE_DEFAULT);
		dx_descriptor.Width = size.x;
		dx_descriptor.Height = size.y;
		dx_descriptor.MipLevels = 1;

		ID3D11Texture2D* texture_interface;
		ID3D11ShaderResourceView* texture_view = nullptr;

		HRESULT result = GetDevice()->CreateTexture2D(&dx_descriptor, nullptr, &texture_interface);
		ECS_ASSERT(!FAILED(result), "Creating texture from DXTex failed.");

		D3D11_BOX box;
		box.left = 0;
		box.right = dx_descriptor.Width;
		box.top = 0;
		box.bottom = dx_descriptor.Height;
		box.front = 0;
		box.back = 1;

		size_t row_pitch, slice_pitch;
		row_pitch = first_mip.size / size.y;
		slice_pitch = first_mip.size;
		GetContext()->UpdateSubresource(texture_interface, 0, &box, first_mip.buffer, row_pitch, slice_pitch);

		result = GetDevice()->CreateShaderResourceView(texture_interface, nullptr, &texture_view);
		ECS_ASSERT(!FAILED(result), "Creating texture view from DXTex failed.");

		// Generate mips
		//GetContext()->GenerateMips(texture_view);

		if (!temporary) {
			AddInternalResource(Texture2D(texture_interface), debug_info);
			AddInternalResource(ResourceView(texture_view), debug_info);
		}

		return texture_view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture3D Graphics::CreateTexture(const GraphicsTexture3DDescriptor* ecs_descriptor, bool temporary, DebugInfo debug_info)
	{
		Texture3D resource;
		D3D11_TEXTURE3D_DESC descriptor = { 0 };
		descriptor.Format = GetGraphicsNativeFormat(ecs_descriptor->format);
		descriptor.Width = ecs_descriptor->size.x;
		descriptor.Height = ecs_descriptor->size.y;
		descriptor.Depth = ecs_descriptor->size.z;
		descriptor.MipLevels = ecs_descriptor->mip_levels;
		descriptor.Usage = GetGraphicsNativeUsage(ecs_descriptor->usage);
		descriptor.BindFlags = GetGraphicsNativeBind(ecs_descriptor->bind_flag);
		descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(ecs_descriptor->cpu_flag);
		descriptor.MiscFlags = GetGraphicsNativeMiscFlags(ecs_descriptor->misc_flag);

		HRESULT result;

		if (ecs_descriptor->mip_data.buffer == nullptr) {
			result = m_device->CreateTexture3D(&descriptor, nullptr, &resource.tex);
		}
		else {
			D3D11_SUBRESOURCE_DATA subresource_data[32];
			memset(subresource_data, 0, sizeof(D3D11_SUBRESOURCE_DATA) * 32);

			unsigned int height = ecs_descriptor->size.y;
			unsigned int depth = ecs_descriptor->size.z;
			for (size_t index = 0; index < ecs_descriptor->mip_data.size; index++) {
				subresource_data[index].pSysMem = ecs_descriptor->mip_data[index].buffer;
				subresource_data[index].SysMemPitch = ecs_descriptor->mip_data[index].size / (height * depth);
				subresource_data[index].SysMemSlicePitch = ecs_descriptor->mip_data[index].size;

				height = height == 1 ? 1 : height >> 1;
				depth = depth == 1 ? 1 : depth >> 1;
			}
			result = m_device->CreateTexture3D(&descriptor, subresource_data, &resource.tex);
		}

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), resource, "Creating Texture 3D failed!");
		AddInternalResource(resource, temporary, debug_info);
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	TextureCube Graphics::CreateTexture(const GraphicsTextureCubeDescriptor* ecs_descriptor, bool temporary, DebugInfo debug_info) {
		TextureCube resource;
		D3D11_TEXTURE2D_DESC descriptor = { 0 };
		descriptor.Format = GetGraphicsNativeFormat(ecs_descriptor->format);
		descriptor.Width = ecs_descriptor->size.x;
		descriptor.Height = ecs_descriptor->size.y;
		descriptor.MipLevels = ecs_descriptor->mip_levels;
		descriptor.ArraySize = 6;
		descriptor.BindFlags = GetGraphicsNativeBind(ecs_descriptor->bind_flag);
		descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(ecs_descriptor->cpu_flag);
		descriptor.MiscFlags = GetGraphicsNativeMiscFlags(ecs_descriptor->misc_flag) | D3D11_RESOURCE_MISC_TEXTURECUBE;
		descriptor.SampleDesc.Count = 1;
		descriptor.SampleDesc.Quality = 0;
		descriptor.Usage = GetGraphicsNativeUsage(ecs_descriptor->usage);

		HRESULT result;

		if (ecs_descriptor->mip_data.buffer == nullptr) {
			result = m_device->CreateTexture2D(&descriptor, nullptr, &resource.tex);
		}
		else {
			D3D11_SUBRESOURCE_DATA subresource_data[128];
			memset(subresource_data, 0, sizeof(D3D11_SUBRESOURCE_DATA) * 128);
			for (size_t index = 0; index < ecs_descriptor->mip_data.size; index++) {
				subresource_data[index].pSysMem = ecs_descriptor->mip_data[index].buffer;
				subresource_data[index].SysMemPitch = ecs_descriptor->mip_data[index].size;
			}

			result = m_device->CreateTexture2D(&descriptor, subresource_data, &resource.tex);
		}

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), resource, "Creating Texture Cube failed!");
		AddInternalResource(resource, temporary, debug_info);
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	ResourceView Graphics::CreateTextureShaderView(
		Texture1D texture,
		ECS_GRAPHICS_FORMAT format, 
		unsigned int most_detailed_mip,
		unsigned int mip_levels,
		bool temporary,
		DebugInfo debug_info
	)
	{
		if (format == ECS_GRAPHICS_FORMAT_UNKNOWN) {
			D3D11_TEXTURE1D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = (ECS_GRAPHICS_FORMAT)descriptor.Format;
		}

		ResourceView view;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = GetGraphicsNativeFormat(format);
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		descriptor.Texture1D.MipLevels = mip_levels;
		descriptor.Texture1D.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating Texture 1D Shader View failed.");
		
		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture1D texture, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating Texture 1D Shader Entire View failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderView(
		Texture2D texture, 
		ECS_GRAPHICS_FORMAT format, 
		unsigned int most_detailed_mip, 
		unsigned int mip_levels,
		bool temporary,
		DebugInfo debug_info
	)
	{
		if (format == ECS_GRAPHICS_FORMAT_UNKNOWN) {
			D3D11_TEXTURE2D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = (ECS_GRAPHICS_FORMAT)descriptor.Format;
		}

		ResourceView view;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = GetGraphicsNativeFormat(format);
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipLevels = mip_levels;
		descriptor.Texture2D.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating Texture 2D Shader View.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture2D texture, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating Texture 2D Shader Entire View failed.");
		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderView(
		Texture3D texture,
		ECS_GRAPHICS_FORMAT format,
		unsigned int most_detailed_mip,
		unsigned int mip_levels,
		bool temporary,
		DebugInfo debug_info
	)
	{
		if (format == ECS_GRAPHICS_FORMAT_UNKNOWN) {
			D3D11_TEXTURE3D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = (ECS_GRAPHICS_FORMAT)descriptor.Format;
		}

		ResourceView view;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = GetGraphicsNativeFormat(format);
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		descriptor.Texture3D.MipLevels = mip_levels;
		descriptor.Texture3D.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating Texture 3D Shader View failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture3D texture, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating Texture 3D Shader Entire View failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderView(
		TextureCube texture,
		ECS_GRAPHICS_FORMAT format,
		unsigned int most_detailed_mip, 
		unsigned int mip_levels,
		bool temporary,
		DebugInfo debug_info
	)
	{
		ResourceView view;

		if (format == ECS_GRAPHICS_FORMAT_UNKNOWN) {
			D3D11_TEXTURE2D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = (ECS_GRAPHICS_FORMAT)descriptor.Format;
		}

		ResourceView component;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = GetGraphicsNativeFormat(format);
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		descriptor.TextureCube.MipLevels = mip_levels;
		descriptor.TextureCube.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &component.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating Texture Cube Shader View.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(TextureCube texture, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating Texture Cube Shader Entire View failed.");
		
		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateBufferView(StandardBuffer buffer, ECS_GRAPHICS_FORMAT format, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor;
		descriptor.Format = GetGraphicsNativeFormat(format);
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		descriptor.Buffer.ElementOffset = 0;
		descriptor.Buffer.ElementWidth = buffer.count;
		HRESULT result = m_device->CreateShaderResourceView(buffer.buffer, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating standard buffer view failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateBufferView(StructuredBuffer buffer, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(buffer.buffer, nullptr, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating structured buffer view failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RenderTargetView Graphics::CreateRenderTargetView(Texture2D texture, unsigned int mip_level, bool temporary, DebugInfo debug_info)
	{
		HRESULT result;

		D3D11_TEXTURE2D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		RenderTargetView view;
		D3D11_RENDER_TARGET_VIEW_DESC descriptor;
		descriptor.Format = texture_desc.Format;
		descriptor.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipSlice = mip_level;
		result = m_device->CreateRenderTargetView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating render target view failed!");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RenderTargetView Graphics::CreateRenderTargetView(TextureCube cube, TextureCubeFace face, unsigned int mip_level, bool temporary, DebugInfo debug_info)
	{
		HRESULT result;

		D3D11_TEXTURE2D_DESC descriptor;
		cube.tex->GetDesc(&descriptor);

		RenderTargetView view;
		D3D11_RENDER_TARGET_VIEW_DESC view_descriptor;
		view_descriptor.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		view_descriptor.Format = descriptor.Format;
		view_descriptor.Texture2DArray.ArraySize = 1;
		view_descriptor.Texture2DArray.FirstArraySlice = face;
		view_descriptor.Texture2DArray.MipSlice = mip_level;
		result = m_device->CreateRenderTargetView(cube.tex, &view_descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating render target view failed!");

		if (!temporary) {
			AddInternalResource(view, debug_info);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DepthStencilView Graphics::CreateDepthStencilView(Texture2D texture, bool temporary, DebugInfo debug_info)
	{
		HRESULT result;

		DepthStencilView view;
		result = m_device->CreateDepthStencilView(texture.tex, nullptr, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating render target view failed!");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(UABuffer buffer, ECS_GRAPHICS_FORMAT format, unsigned int first_element, bool temporary, DebugInfo debug_info)
	{
		UAView view;

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor;
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = first_element;
		descriptor.Buffer.NumElements = buffer.element_count - first_element;
		descriptor.Buffer.Flags = 0;
		descriptor.Format = GetGraphicsNativeFormat(format);

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating UAView from UABuffer failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(IndirectBuffer buffer, bool temporary, DebugInfo debug_info) {
		UAView view;

		// Max 5 uints can be written
		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor;
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = 0;
		descriptor.Buffer.NumElements = 5;
		descriptor.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		descriptor.Format = DXGI_FORMAT_R32_TYPELESS;

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating UAView from Indirect Buffer failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(AppendStructuredBuffer buffer, bool temporary, DebugInfo debug_info) {
		UAView view;

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor;
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = 0;
		descriptor.Buffer.NumElements = buffer.element_count;
		descriptor.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		descriptor.Format = DXGI_FORMAT_UNKNOWN;

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating UAView from Append Buffer failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(ConsumeStructuredBuffer buffer, bool temporary, DebugInfo debug_info) {
		UAView view;

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = 0;
		descriptor.Buffer.NumElements = buffer.element_count;
		descriptor.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		descriptor.Format = DXGI_FORMAT_UNKNOWN;

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating UAView from Consume Buffer failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture1D texture, unsigned int mip_slice, bool temporary, DebugInfo debug_info) {
		UAView view;

		D3D11_TEXTURE1D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		descriptor.Texture1D.MipSlice = mip_slice;
		descriptor.Format = texture_desc.Format;

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating UAView from Texture 1D failed.");
		
		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture2D texture, unsigned int mip_slice, bool temporary, DebugInfo debug_info) {
		UAView view;

		D3D11_TEXTURE2D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipSlice = mip_slice;
		descriptor.Format = texture_desc.Format;

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating UAView from Texture 2D failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture3D texture, unsigned int mip_slice, bool temporary, DebugInfo debug_info) {
		UAView view;

		D3D11_TEXTURE3D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		descriptor.Texture3D.MipSlice = mip_slice;
		descriptor.Format = texture_desc.Format;

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating UAView from Texture 3D failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RasterizerState Graphics::CreateRasterizerState(const D3D11_RASTERIZER_DESC& descriptor, bool temporary, DebugInfo debug_info)
	{
		RasterizerState state;

		HRESULT result = m_device->CreateRasterizerState(&descriptor, &state.state);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), state, "Creating Rasterizer state failed.");

		AddInternalResource(state, temporary, debug_info);
		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RasterizerState Graphics::CreateRasterizerStateDefault(bool temporary, DebugInfo debug_info)
	{
		D3D11_RASTERIZER_DESC descriptor = {};
		descriptor.AntialiasedLineEnable = TRUE;
		descriptor.CullMode = D3D11_CULL_BACK;
		descriptor.FillMode = D3D11_FILL_SOLID;
		
		return CreateRasterizerState(descriptor, temporary, debug_info);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DepthStencilState Graphics::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& descriptor, bool temporary, DebugInfo debug_info)
	{
		DepthStencilState state;

		HRESULT result = m_device->CreateDepthStencilState(&descriptor, &state.state);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), state, "Creating Rasterizer state failed.");

		AddInternalResource(state, temporary, debug_info);
		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DepthStencilState Graphics::CreateDepthStencilStateDefault(bool temporary, DebugInfo debug_info)
	{
		D3D11_DEPTH_STENCIL_DESC descriptor = {};
		descriptor.DepthEnable = TRUE;
		descriptor.DepthFunc = D3D11_COMPARISON_LESS;
		descriptor.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		descriptor.StencilEnable = FALSE;

		return CreateDepthStencilState(descriptor, temporary, debug_info);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	BlendState Graphics::CreateBlendState(const D3D11_BLEND_DESC& descriptor, bool temporary, DebugInfo debug_info)
	{
		BlendState state;

		HRESULT result = m_device->CreateBlendState(&descriptor, &state.state);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), state, "Creating Rasterizer state failed.");

		AddInternalResource(state, temporary, debug_info);
		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	BlendState Graphics::CreateBlendStateDefault(bool temporary, DebugInfo debug_info)
	{
		D3D11_BLEND_DESC descriptor = {};
		descriptor.AlphaToCoverageEnable = FALSE;
		descriptor.IndependentBlendEnable = FALSE;
		
		D3D11_RENDER_TARGET_BLEND_DESC render_target_descriptor = {};
		render_target_descriptor.BlendEnable = FALSE;
		render_target_descriptor.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		render_target_descriptor.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		render_target_descriptor.BlendOp = D3D11_BLEND_OP_ADD;
		render_target_descriptor.SrcBlendAlpha = D3D11_BLEND_ZERO;
		render_target_descriptor.DestBlendAlpha = D3D11_BLEND_ZERO;
		render_target_descriptor.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		render_target_descriptor.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		descriptor.RenderTarget[0] = render_target_descriptor;
		return CreateBlendState(descriptor, temporary, debug_info);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeMesh(const Mesh& mesh)
	{
		if (mesh.name.buffer != nullptr) {
			m_allocator->Deallocate(mesh.name.buffer);
		}
		for (size_t index = 0; index < mesh.mapping_count; index++) {
			if (mesh.vertex_buffers[index].buffer != nullptr) {
				FreeResource(mesh.vertex_buffers[index]);
			}
		}
		if (mesh.index_buffer.buffer != nullptr) {
			FreeResource(mesh.index_buffer);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeSubmesh(const Submesh& submesh) {
		if (submesh.name.buffer != nullptr) {
			m_allocator->Deallocate(submesh.name.buffer);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ClearRenderTarget(RenderTargetView target, ColorFloat color)
	{
		ECSEngine::ClearRenderTarget(target, GetContext(), color);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ClearDepthStencil(DepthStencilView depth_stencil, float depth, unsigned char stencil)
	{
		ECSEngine::ClearDepthStencil(depth_stencil, GetContext(), depth, stencil);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ClearBackBuffer(float red, float green, float blue) {
		const float color[] = { red, green, blue, 1.0f };
		m_context->ClearRenderTargetView(m_target_view.view, color);
		m_context->ClearDepthStencilView(m_depth_stencil_view.view, D3D11_CLEAR_DEPTH, 1.0f, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableAlphaBlending() {
		DisableAlphaBlending(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableAlphaBlending(GraphicsContext* context) {
		GraphicsPipelineBlendState state = GetBlendState();
		// The call increases the reference count - must balance it out
		state.blend_state.Release();

		D3D11_BLEND_DESC blend_desc = {};
		state.blend_state.state->GetDesc(&blend_desc);
		blend_desc.RenderTarget[0].BlendEnable = FALSE;

		/*unsigned int count = state.blend_state.Release();
		RemovePossibleResourceFromTracking(state.blend_state.Interface());*/

		BlendState new_state = CreateBlendState(blend_desc, true);
		BindBlendState(new_state);
		// This will be kept alive by the binding
		unsigned int count = new_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableDepth()
	{
		DisableDepth(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableCulling(bool wireframe)
	{
		DisableCulling(m_context, wireframe);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableDepth(GraphicsContext* context)
	{
		D3D11_DEPTH_STENCIL_DESC depth_desc = {};
		GraphicsPipelineDepthStencilState depth_stencil_state = GetDepthStencilState();
		// The call increases the reference count - must balance it out
		depth_stencil_state.depth_stencil_state.Release();

		depth_stencil_state.depth_stencil_state.state->GetDesc(&depth_desc);
		depth_desc.DepthEnable = FALSE;

		/*unsigned int count = depth_stencil_state.depth_stencil_state.Release();
		RemovePossibleResourceFromTracking(depth_stencil_state.depth_stencil_state.Interface());*/

		DepthStencilState state = CreateDepthStencilState(depth_desc, true);
		BindDepthStencilState(state);
		// This will be kept alive by the binding
		unsigned int count = state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableCulling(GraphicsContext* context, bool wireframe)
	{
		D3D11_RASTERIZER_DESC rasterizer_desc = {};

		GraphicsPipelineRasterizerState raster_state = GetRasterizerState();
		// The call increases the reference count - must balance it out
		raster_state.rasterizer_state.Release();
		raster_state.rasterizer_state.state->GetDesc(&rasterizer_desc);
		
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FillMode = wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		
		/*unsigned int count = raster_state.rasterizer_state.Release();
		RemovePossibleResourceFromTracking(raster_state.rasterizer_state.Interface());*/

		RasterizerState new_state = CreateRasterizerState(rasterizer_desc, true);
		BindRasterizerState(new_state);
		// The binding will keep the state alive
		unsigned int count = new_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::Dispatch(uint3 dispatch_size)
	{
		ECSEngine::Dispatch(dispatch_size, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DispatchIndirect(IndirectBuffer buffer)
	{
		ECSEngine::DispatchIndirect(buffer, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::Draw(unsigned int vertex_count, unsigned int vertex_offset)
	{
		ECSEngine::Draw(vertex_count, m_context, vertex_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawIndexed(unsigned int count, unsigned int start_index, unsigned int base_vertex_location) {
		ECSEngine::DrawIndexed(count, m_context, start_index, base_vertex_location);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawInstanced(
		unsigned int vertex_count, 
		unsigned int instance_count, 
		unsigned int vertex_buffer_offset,
		unsigned int instance_offset
	) {
		ECSEngine::DrawInstanced(vertex_count, instance_count, m_context, vertex_buffer_offset, instance_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawIndexedInstanced(
		unsigned int index_count, 
		unsigned int instance_count, 
		unsigned int index_buffer_offset,
		unsigned int vertex_buffer_offset, 
		unsigned int instance_offset
	) {
		ECSEngine::DrawIndexedInstanced(index_count, instance_count, m_context, index_buffer_offset, vertex_buffer_offset, instance_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawIndexedInstancedIndirect(IndirectBuffer buffer) {
		ECSEngine::DrawIndexedInstancedIndirect(buffer, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawInstancedIndirect(IndirectBuffer buffer) {
		ECSEngine::DrawInstancedIndirect(buffer, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableAlphaBlending() {
		EnableAlphaBlending(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableAlphaBlending(GraphicsContext* context) {
		GraphicsPipelineBlendState state = GetBlendState();
		// The call increases the reference count - must balance it out
		unsigned int ref_count = state.blend_state.Release();

		D3D11_BLEND_DESC blend_desc = {};
		state.blend_state.state->GetDesc(&blend_desc);
		
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		/*unsigned int count = state.blend_state.Release();
		RemovePossibleResourceFromTracking(state.blend_state.Interface());*/

		BlendState new_state = CreateBlendState(blend_desc, true);
		BindBlendState(new_state);
		// The binding will keep the state alive
		unsigned int count = new_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableDepth()
	{
		EnableDepth(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableDepth(GraphicsContext* context)
	{
		D3D11_DEPTH_STENCIL_DESC depth_desc = {};
		GraphicsPipelineDepthStencilState depth_stencil_state = GetDepthStencilState();
		// The call increases the reference count - must balance it out
		unsigned int ref_count = depth_stencil_state.depth_stencil_state.Release();

		depth_stencil_state.depth_stencil_state.state->GetDesc(&depth_desc);
		depth_desc.DepthEnable = TRUE;
		depth_desc.DepthFunc = D3D11_COMPARISON_LESS;
		depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

		/*unsigned int count = depth_stencil_state.depth_stencil_state.Release();
		RemovePossibleResourceFromTracking(depth_stencil_state.depth_stencil_state.Interface());*/

		DepthStencilState state = CreateDepthStencilState(depth_desc, true);
		BindDepthStencilState(state);
		// The binding will keep the resource alive
		unsigned int count = state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableCulling(bool wireframe)
	{
		EnableCulling(m_context, wireframe);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableCulling(GraphicsContext* context, bool wireframe)
	{
		D3D11_RASTERIZER_DESC rasterizer_desc = {};

		GraphicsPipelineRasterizerState raster_state = GetRasterizerState();
		// The call increases the reference count - must balance it out
		raster_state.rasterizer_state.Release();
		
		raster_state.rasterizer_state.state->GetDesc(&rasterizer_desc);
		rasterizer_desc.CullMode = D3D11_CULL_BACK;
		rasterizer_desc.FillMode = wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		
		/*unsigned int count = raster_state.rasterizer_state.Release();
		RemovePossibleResourceFromTracking(raster_state.rasterizer_state.Interface());*/

		RasterizerState new_state = CreateRasterizerState(rasterizer_desc, true);
		BindRasterizerState(new_state);
		// The binding will keep the resource alive
		unsigned int count = new_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	CommandList Graphics::FinishCommandList(bool restore_state, bool temporary, DebugInfo debug_info) {
		CommandList list = nullptr;
		HRESULT result;
		result = m_context->FinishCommandList(restore_state, &list.list);

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), list, "Creating command list failed!");
		if (!temporary) {
			AddInternalResource(list, debug_info);
		}
		return list;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapBuffer(ID3D11Buffer* buffer, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapBuffer(buffer, m_context, map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapTexture(Texture1D texture, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, m_context, map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapTexture(Texture2D texture, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, m_context, map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapTexture(Texture3D texture, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, m_context, map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CommitInternalResourcesToBeFreed()
	{
		int32_t last_index = m_internal_resources.size.load(ECS_RELAXED);
		if (last_index < m_internal_resources.capacity) {
			m_internal_resources.SpinWaitWrites();
		}

		last_index = std::min(last_index, (int32_t)m_internal_resources.capacity);
		for (int32_t index = 0; index < last_index; index++) {
			if (m_internal_resources[index].is_deleted.load(ECS_RELAXED)) {
				// The remove swap back must be done manually - the size might be incremented by other threads trying to write
				last_index--;
				m_internal_resources[index] = m_internal_resources[last_index];
				index--;
			}
		}

		// Commit the new size to the other threads
		m_internal_resources.SetSize(last_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::GenerateMips(ResourceView view)
	{
		m_context->GenerateMips(view.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::GetWindowSize(unsigned int& width, unsigned int& height) const {
		width = m_window_size.x;
		height = m_window_size.y;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	uint2 Graphics::GetWindowSize() const
	{
		return m_window_size;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineBlendState Graphics::GetBlendState() const
	{
		return ECSEngine::GetBlendState(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeAllocatedBuffer(const void* buffer)
	{
		m_allocator->Deallocate(buffer);
	}

	void Graphics::FreeAllocatedBufferTs(const void* buffer)
	{
		m_allocator->Deallocate_ts(buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::GetAllocatorBuffer(size_t size)
	{
		return m_allocator->Allocate(size);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::GetAllocatorBufferTs(size_t size)
	{
		return m_allocator->Allocate_ts(size);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_DEVICE_ERROR Graphics::GetDeviceError() {
		HRESULT result = m_device->GetDeviceRemovedReason();
		switch (result) {
		case DXGI_ERROR_DEVICE_HUNG:
			return ECS_GRAPHICS_DEVICE_ERROR_INVALID_CALL;
		case DXGI_ERROR_DEVICE_REMOVED:
			return ECS_GRAPHICS_DEVICE_ERROR_REMOVED;
		case DXGI_ERROR_DEVICE_RESET:
			return ECS_GRAPHICS_DEVICE_ERROR_RESET;
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			return ECS_GRAPHICS_DEVICE_ERROR_DRIVER_INTERNAL_ERROR;
		case DXGI_ERROR_INVALID_CALL:
			return ECS_GRAPHICS_DEVICE_ERROR_INVALID_CALL;
		case S_OK:
			return ECS_GRAPHICS_DEVICE_ERROR_NONE;
		}

		// If no code is matched, return a removed reason
		return ECS_GRAPHICS_DEVICE_ERROR_REMOVED;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineDepthStencilState Graphics::GetDepthStencilState() const
	{
		return ECSEngine::GetDepthStencilState(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineRasterizerState Graphics::GetRasterizerState() const
	{
		return ECSEngine::GetRasterizerState(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineRenderState Graphics::GetPipelineRenderState() const
	{
		return ECSEngine::GetPipelineRenderState(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsBoundViews Graphics::GetCurrentViews() const
	{
		return { m_bound_render_targets[0], m_current_depth_stencil };
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineState Graphics::GetPipelineState() const
	{
		return { GetPipelineRenderState(), GetCurrentViews(), GetBoundViewport() };
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RenderTargetView Graphics::GetBoundRenderTarget() const
	{
		ECS_ASSERT(m_bound_render_target_count > 0);
		return m_bound_render_targets[0];
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DepthStencilView Graphics::GetBoundDepthStencil() const
	{
		return m_current_depth_stencil;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsViewport Graphics::GetBoundViewport() const
	{
		return ECSEngine::GetViewport(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ResizeSwapChainSize(HWND hWnd, float width, float height)
	{
		D3D11_RENDER_TARGET_VIEW_DESC descriptor;
		m_target_view.view->GetDesc(&descriptor);
		m_context->ClearState();
		m_target_view.view->Release();
		m_depth_stencil_view.view->Release();

		HRESULT result = m_swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

		ECS_CRASH_RETURN(SUCCEEDED(result), "Resizing swap chain failed!");

		/*DXGI_MODE_DESC descriptor;
		descriptor.Width = m_window_size.x;
		descriptor.Height = m_window_size.y;
		descriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		descriptor.RefreshRate.Numerator = 1;
		descriptor.RefreshRate.Denominator = 60;
		descriptor.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		descriptor.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		result = m_swap_chain->ResizeTarget(&descriptor);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Resizing the swap chain failed!", true);*/

		CreateRenderTargetViewFromSwapChain(descriptor.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

		ResizeViewport(0.0f, 0.0f, width, height);
		BindRenderTargetViewFromInitialViews(m_context);
		EnableDepth(m_context);

		CreateWindowDepthStencilView();

		DisableAlphaBlending(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ResizeViewport(float top_left_x, float top_left_y, float new_width, float new_height)
	{
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = top_left_x;
		viewport.TopLeftY = top_left_y;
		viewport.Width = new_width;
		viewport.Height = new_height;
		viewport.MaxDepth = 1;
		viewport.MinDepth = 0;

		m_context->RSSetViewports(1u, &viewport);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::SwapBuffers(unsigned int sync_interval) {
		HRESULT result;
		result = m_swap_chain->Present(sync_interval, 0);
		if (FAILED(result)) {
			if (result == DXGI_ERROR_DEVICE_REMOVED) {
				return true;
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	void Graphics::SetNewSize(HWND hWnd, unsigned int width, unsigned int height)
	{
		m_window_size.x = width;
		m_window_size.y = height;
		ResizeSwapChainSize(hWnd, width, height);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Mesh Graphics::TransferMesh(const Mesh* mesh, bool temporary)
	{
		Mesh new_mesh;

		// Only the index buffer and the vertex buffers need to be transferred
		new_mesh.name = mesh->name;
		new_mesh.mapping_count = mesh->mapping_count;

		new_mesh.index_buffer = TransferGPUResource(mesh->index_buffer, GetDevice());
		for (size_t index = 0; index < mesh->mapping_count; index++) {
			new_mesh.mapping[index] = mesh->mapping[index];
			new_mesh.vertex_buffers[index] = TransferGPUResource(mesh->vertex_buffers[index], GetDevice());
		}

		if (!temporary) {
			AddInternalResource(new_mesh.index_buffer);
			for (size_t index = 0; index < mesh->mapping_count; index++) {
				AddInternalResource(new_mesh.vertex_buffers[index]);
			}
		}

		return new_mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	CoallescedMesh Graphics::TransferCoallescedMesh(const CoallescedMesh* mesh, bool temporary)
	{
		CoallescedMesh new_mesh;

		new_mesh.submeshes = mesh->submeshes;
		new_mesh.mesh = TransferMesh(&mesh->mesh, temporary);

		return new_mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	PBRMesh Graphics::TransferPBRMesh(const PBRMesh* mesh, bool temporary)
	{
		PBRMesh new_mesh;

		new_mesh.materials = mesh->materials;
		new_mesh.mesh = TransferCoallescedMesh(&mesh->mesh, temporary);

		return new_mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Material Graphics::TransferMaterial(const Material* material, bool temporary)
	{
		Material new_material;

		new_material.layout = material->layout;
		new_material.pixel_shader = material->pixel_shader;
		new_material.vertex_shader = material->vertex_shader;

		new_material.vertex_buffer_mapping_count = material->vertex_buffer_mapping_count;
		memcpy(new_material.vertex_buffer_mappings, material->vertex_buffer_mappings, sizeof(ECS_MESH_INDEX) * material->vertex_buffer_mapping_count);

		new_material.p_buffer_count = material->p_buffer_count;
		new_material.p_texture_count = material->p_texture_count;

		new_material.unordered_view_count = material->unordered_view_count;

		new_material.v_buffer_count = material->v_buffer_count;
		new_material.v_texture_count = material->v_texture_count;

		auto copy_buffers = [&](size_t offset, unsigned char copy_count) {
			ConstantBuffer* old_buffers = (ConstantBuffer*)function::OffsetPointer(material, offset);
			ConstantBuffer* new_buffers = (ConstantBuffer*)function::OffsetPointer(&new_material, offset);
			
			for (unsigned char index = 0; index < copy_count; index++) {
				new_buffers[index] = TransferGPUResource(old_buffers[index], GetDevice());
				AddInternalResource(new_buffers[index], temporary);
			}
		};
		
		auto copy_textures = [&](size_t offset, unsigned char copy_count) {
			ResourceView* old_views = (ResourceView*)function::OffsetPointer(material, offset);
			ResourceView* new_views = (ResourceView*)function::OffsetPointer(&new_material, offset);

			for (unsigned char index = 0; index < copy_count; index++) {
				new_views[index] = TransferGPUView(old_views[index], GetDevice());
				AddInternalResource(new_views[index], temporary);
				AddInternalResource(new_views[index].GetResource(), temporary);
			}
		};

		copy_buffers(offsetof(Material, p_buffers), material->p_buffer_count);
		copy_buffers(offsetof(Material, v_buffers), material->v_buffer_count);

		copy_textures(offsetof(Material, v_textures), material->v_texture_count);
		copy_textures(offsetof(Material, p_textures), material->p_texture_count);;

		// The unordered views need to be handled separately
		for (unsigned char index = 0; index < material->unordered_view_count; index++) {
			new_material.unordered_views[index] = TransferGPUView(material->unordered_views[index], GetDevice());
			AddInternalResource(new_material.unordered_views[index], temporary);
			AddInternalResource(new_material.unordered_views[index].GetResource(), temporary);
		}

		return new_material;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RestoreBlendState(GraphicsPipelineBlendState state)
	{
		ECSEngine::RestoreBlendState(GetContext(), state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RestoreDepthStencilState(GraphicsPipelineDepthStencilState state)
	{
		ECSEngine::RestoreDepthStencilState(GetContext(), state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RestoreRasterizerState(GraphicsPipelineRasterizerState state)
	{
		ECSEngine::RestoreRasterizerState(GetContext(), state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RestorePipelineRenderState(GraphicsPipelineRenderState state)
	{
		ECSEngine::RestorePipelineRenderState(GetContext(), state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RestoreBoundViews(GraphicsBoundViews views)
	{
		BindRenderTargetView(views.target, views.depth_stencil);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RestorePipelineState(const GraphicsPipelineState* state)
	{
		RestorePipelineRenderState(state->render_state);
		RestoreBoundViews(state->views);
		BindViewport(state->viewport);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UpdateBuffer(ID3D11Buffer* buffer, const void* data, size_t data_size, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int map_flags, unsigned int subresource_index)
	{
		ECSEngine::UpdateBuffer(buffer, data, data_size, m_context, map_type, map_flags, subresource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UnmapBuffer(ID3D11Buffer* buffer, unsigned int resource_index)
	{
		ECSEngine::UnmapBuffer(buffer, m_context, resource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UnmapTexture(Texture1D texture, unsigned int resource_index) {
		ECSEngine::UnmapTexture(texture, m_context, resource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UnmapTexture(Texture2D texture, unsigned int resource_index) {
		ECSEngine::UnmapTexture(texture, m_context, resource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UnmapTexture(Texture3D texture, unsigned int resource_index) {
		ECSEngine::UnmapTexture(texture, m_context, resource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ECS_SHADER_TYPE Graphics::DetermineShaderType(Stream<char> source_code) const {
		return m_shader_reflection->DetermineShaderType(source_code);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	InputLayout Graphics::ReflectVertexShaderInput(
		Stream<char> source_code,
		Stream<void> vertex_byte_code,
		bool deallocate_byte_code,
		bool temporary,
		DebugInfo debug_info
	)
	{
		constexpr size_t MAX_INPUT_FIELDS = 128;
		D3D11_INPUT_ELEMENT_DESC _element_descriptors[MAX_INPUT_FIELDS];
		CapacityStream<D3D11_INPUT_ELEMENT_DESC> element_descriptors(_element_descriptors, 0, MAX_INPUT_FIELDS);

		constexpr size_t NAME_POOL_SIZE = 8192;
		ECS_STACK_LINEAR_ALLOCATOR(allocator, NAME_POOL_SIZE);
		bool success = m_shader_reflection->ReflectVertexShaderInputSource(source_code, element_descriptors, GetAllocatorPolymorphic(&allocator));
		if (!success) {
			return {};
		}

		InputLayout layout = CreateInputLayout(element_descriptors, vertex_byte_code, deallocate_byte_code, temporary, debug_info);
		return layout;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderBuffers(Stream<char> source_code, CapacityStream<ShaderReflectedBuffer>& buffers) {
		constexpr size_t NAME_POOL_SIZE = 8192;
		ECS_STACK_LINEAR_ALLOCATOR(name_allocator, NAME_POOL_SIZE);

		bool success = m_shader_reflection->ReflectShaderBuffersSource(source_code, buffers, GetAllocatorPolymorphic(&name_allocator));
		if (!success) {
			return false;
		}

		size_t total_allocation = 0;
		for (size_t index = 0; index < buffers.size; index++) {
			total_allocation += buffers[index].name.size;
		}

		void* permanent_name_allocation = m_allocator->Allocate_ts(total_allocation);
		uintptr_t buffer = (uintptr_t)permanent_name_allocation;
		for (size_t index = 0; index < buffers.size; index++) {
			buffers[index].name.CopyTo(buffer);
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderTextures(Stream<char> source_code, CapacityStream<ShaderReflectedTexture>& textures)
	{
		constexpr size_t NAME_POOL_SIZE = 8192;
		ECS_STACK_LINEAR_ALLOCATOR(name_allocator, NAME_POOL_SIZE);

		bool success = m_shader_reflection->ReflectShaderTexturesSource(source_code, textures, GetAllocatorPolymorphic(&name_allocator));
		if (!success) {
			return false;
		}

		size_t total_allocation = 0;
		for (size_t index = 0; index < textures.size; index++) {
			total_allocation += textures[index].name.size;
		}

		void* permanent_name_allocation = m_allocator->Allocate_ts(total_allocation);
		uintptr_t buffer = (uintptr_t)permanent_name_allocation;
		for (size_t index = 0; index < textures.size; index++) {
			textures[index].name.CopyTo(buffer);
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderMacros(
		Stream<char> source_code, 
		CapacityStream<Stream<char>>* defined_macros, 
		CapacityStream<Stream<char>>* conditional_macros, 
		AllocatorPolymorphic allocator
	) {
		return m_shader_reflection->ReflectShaderMacros(source_code, defined_macros, conditional_macros, allocator);
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	bool Graphics::ReflectVertexBufferMapping(Stream<char> source_code, CapacityStream<ECS_MESH_INDEX>& mapping) {
		return m_shader_reflection->ReflectVertexBufferMappingSource(source_code, mapping);
	}

	// ------------------------------------------------------------------------------------------------------------------------

#pragma region Free functions

	// ------------------------------------------------------------------------------------------------------------------------

	MemoryManager DefaultGraphicsAllocator(GlobalMemoryManager* manager)
	{
		return MemoryManager(100'000, 1024, 100'000, manager);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	size_t DefaultGraphicsAllocatorSize()
	{
		return 100'000;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DestroyGraphics(Graphics* graphics)
	{
		// Walk through the list of resources that the graphic stores and free them
		// Use a switch case to handle the pointer identification
		
		// Unbind the resource bound to the pipeline
		graphics->m_context->ClearState();

		// First commit the releases
		graphics->CommitInternalResourcesToBeFreed();

		unsigned int resource_count = graphics->m_internal_resources.size.load(ECS_RELAXED);

#define CASE(type, pointer_type) case type: { pointer_type* resource = (pointer_type*)pointer; ref_count = resource->Release(); } break;
#define CASE2(type0, type1, pointer_type) case type0: case type1: { pointer_type* resource = (pointer_type*)pointer; ref_count = resource->Release(); } break;
#define CASE3(type0, type1, type2, pointer_type) case type0: case type1: case type2: { pointer_type* resource = (pointer_type*)pointer; ref_count = resource->Release(); } break;

		unsigned int ref_count = 0;
		for (unsigned int index = 0; index < resource_count; index++) {
			void* pointer = graphics->m_internal_resources[index].interface_pointer;
			ECS_GRAPHICS_RESOURCE_TYPE type = (ECS_GRAPHICS_RESOURCE_TYPE)graphics->m_internal_resources[index].type;

			const char* file = graphics->m_internal_resources[index].debug_info.file;
			const char* function = graphics->m_internal_resources[index].debug_info.function;
			unsigned int line = graphics->m_internal_resources[index].debug_info.line;

			switch (type) {
				CASE(ECS_GRAPHICS_RESOURCE_VERTEX_SHADER, ID3D11VertexShader);
				CASE(ECS_GRAPHICS_RESOURCE_PIXEL_SHADER, ID3D11PixelShader);
				CASE(ECS_GRAPHICS_RESOURCE_HULL_SHADER, ID3D11HullShader);
				CASE(ECS_GRAPHICS_RESOURCE_GEOMETRY_SHADER, ID3D11GeometryShader);
				CASE(ECS_GRAPHICS_RESOURCE_DOMAIN_SHADER, ID3D11DomainShader);
				CASE(ECS_GRAPHICS_RESOURCE_COMPUTE_SHADER, ID3D11ComputeShader);
				CASE3(ECS_GRAPHICS_RESOURCE_INDEX_BUFFER, ECS_GRAPHICS_RESOURCE_CONSTANT_BUFFER, ECS_GRAPHICS_RESOURCE_VERTEX_BUFFER, ID3D11Buffer);
				CASE3(ECS_GRAPHICS_RESOURCE_STANDARD_BUFFER, ECS_GRAPHICS_RESOURCE_STRUCTURED_BUFFER, ECS_GRAPHICS_RESOURCE_UA_BUFFER, ID3D11Buffer);
				CASE3(ECS_GRAPHICS_RESOURCE_INDIRECT_BUFFER, ECS_GRAPHICS_RESOURCE_APPEND_BUFFER, ECS_GRAPHICS_RESOURCE_CONSUME_BUFFER, ID3D11Buffer);
				CASE(ECS_GRAPHICS_RESOURCE_INPUT_LAYOUT, ID3D11InputLayout);
				CASE(ECS_GRAPHICS_RESOURCE_RESOURCE_VIEW, ID3D11ShaderResourceView);
				CASE(ECS_GRAPHICS_RESOURCE_SAMPLER_STATE, ID3D11SamplerState);
				CASE(ECS_GRAPHICS_RESOURCE_UA_VIEW, ID3D11UnorderedAccessView);
				CASE(ECS_GRAPHICS_RESOURCE_GRAPHICS_CONTEXT, ID3D11DeviceContext);
				CASE(ECS_GRAPHICS_RESOURCE_RENDER_TARGET_VIEW, ID3D11RenderTargetView);
				CASE(ECS_GRAPHICS_RESOURCE_DEPTH_STENCIL_VIEW, ID3D11DepthStencilView);
				CASE(ECS_GRAPHICS_RESOURCE_TEXTURE_1D, ID3D11Texture1D);
				CASE(ECS_GRAPHICS_RESOURCE_TEXTURE_2D, ID3D11Texture2D);
				CASE(ECS_GRAPHICS_RESOURCE_TEXTURE_3D, ID3D11Texture3D);
				CASE(ECS_GRAPHICS_RESOURCE_TEXTURE_CUBE, ID3D11Texture2D);
				CASE(ECS_GRAPHICS_RESOURCE_BLEND_STATE, ID3D11BlendState);
				CASE(ECS_GRAPHICS_RESOURCE_DEPTH_STENCIL_STATE, ID3D11DepthStencilState);
				CASE(ECS_GRAPHICS_RESOURCE_RASTERIZER_STATE, ID3D11RasterizerState);
				CASE(ECS_GRAPHICS_RESOURCE_COMMAND_LIST, ID3D11CommandList);
			default:
				ECS_ASSERT(false);
			}
		}
		
#undef CASE
#undef CASE2
#undef CASE3

		for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
			graphics->m_shader_helpers[index].vertex.Release();
			graphics->m_shader_helpers[index].pixel.Release();
			graphics->m_shader_helpers[index].input_layout.Release();
			graphics->m_shader_helpers[index].pixel_sampler.Release();
		}

		// Deallocate the shader helper array
		graphics->m_allocator->Deallocate(graphics->m_shader_helpers.buffer);

		graphics->m_context->Flush();

		unsigned int count = graphics->m_device->Release();
		count = graphics->m_deferred_context->Release();
		count = graphics->m_context->Release();
		// Deallocate the buffer for the resource tracking
		graphics->m_allocator->Deallocate(graphics->m_internal_resources.buffer);

		if (graphics->m_depth_stencil_view.view != nullptr) {
			count = graphics->m_depth_stencil_view.Release();
		}
		if (graphics->m_target_view.view != nullptr) {
			count = graphics->m_target_view.Release();
		}
		if (graphics->m_swap_chain != nullptr) {
			count = graphics->m_swap_chain->Release();
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexBuffer(VertexBuffer buffer, GraphicsContext* context, unsigned int slot) {
		const unsigned int offset = 0u;
		context->IASetVertexBuffers(slot, 1u, &buffer.buffer, &buffer.stride, &offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexBuffers(Stream<VertexBuffer> buffers, GraphicsContext* context, unsigned int start_slot) {
		ID3D11Buffer* v_buffers[16];
		unsigned int strides[16];
		unsigned int offsets[16] = { 0 };
		for (size_t index = 0; index < buffers.size; index++) {
			v_buffers[index] = buffers[index].buffer;
			strides[index] = buffers[index].stride;
		}
		context->IASetVertexBuffers(start_slot, buffers.size, v_buffers, strides, offsets);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindIndexBuffer(IndexBuffer index_buffer, GraphicsContext* context) {
		DXGI_FORMAT format = DXGI_FORMAT_R32_UINT;
		format = index_buffer.int_size == 1 ? DXGI_FORMAT_R8_UINT : format;
		format = index_buffer.int_size == 2 ? DXGI_FORMAT_R16_UINT : format;

		context->IASetIndexBuffer(index_buffer.buffer, format, 0);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindInputLayout(InputLayout input_layout, GraphicsContext* context) {
		context->IASetInputLayout(input_layout.layout);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexShader(VertexShader shader, GraphicsContext* context) {
		context->VSSetShader(shader.shader, nullptr, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelShader(PixelShader shader, GraphicsContext* context) {
		context->PSSetShader(shader.shader, nullptr, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindHullShader(HullShader shader, GraphicsContext* context) {
		context->HSSetShader(shader.shader, nullptr, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDomainShader(DomainShader shader, GraphicsContext* context) {
		context->DSSetShader(shader.shader, nullptr, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	void BindGeometryShader(GeometryShader shader, GraphicsContext* context) {
		context->GSSetShader(shader.shader, nullptr, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeShader(ComputeShader shader, GraphicsContext* context) {
		context->CSSetShader(shader.shader, nullptr, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot) {
		context->PSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		unsigned int start_slot
	) {
		context->PSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot) {
		context->VSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		unsigned int start_slot
	) {
		ID3D11Buffer* d_buffers[16];
		for (size_t index = 0; index < buffers.size; index++) {
			d_buffers[index] = buffers[index].buffer;
		}
		context->VSSetConstantBuffers(start_slot, buffers.size, d_buffers);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDomainConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot) {
		context->DSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDomainConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		unsigned int start_slot
	) {
		context->DSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindHullConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot) {
		context->HSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindHullConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		unsigned int start_slot
	) {
		context->HSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindGeometryConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot) {
		context->GSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindGeometryConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		unsigned int start_slot
	) {
		context->GSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, unsigned int slot) {
		context->CSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		unsigned int start_slot
	) {
		context->CSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindTopology(Topology topology, GraphicsContext* context)
	{
		context->IASetPrimitiveTopology(topology.value);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelResourceView(ResourceView component, GraphicsContext* context, unsigned int slot) {
		context->PSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int start_slot) {
		context->PSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexResourceView(ResourceView component, GraphicsContext* context, unsigned int slot) {
		context->VSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int start_slot) {
		context->VSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDomainResourceView(ResourceView component, GraphicsContext* context, unsigned int slot) {
		context->DSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDomainResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int start_slot) {
		context->DSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindHullResourceView(ResourceView component, GraphicsContext* context, unsigned int slot) {
		context->HSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindHullResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int start_slot) {
		context->HSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindGeometryResourceView(ResourceView component, GraphicsContext* context, unsigned int slot) {
		context->GSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindGeometryResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int start_slot) {
		context->GSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeResourceView(ResourceView component, GraphicsContext* context, unsigned int slot) {
		context->CSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeResourceViews(Stream<ResourceView> views, GraphicsContext* context, unsigned int start_slot) {
		context->CSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindSamplerStates(Stream<SamplerState> samplers, GraphicsContext* context, unsigned int start_slot, ECS_SHADER_TYPE type)
	{
		switch (type) {
		case ECS_SHADER_PIXEL:
			context->PSSetSamplers(start_slot, 1u, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_VERTEX:
			context->VSSetSamplers(start_slot, 1u, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_COMPUTE:
			context->CSSetSamplers(start_slot, 1u, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_HULL:
			context->HSSetSamplers(start_slot, 1u, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_DOMAIN:
			context->DSSetSamplers(start_slot, 1u, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_GEOMETRY:
			context->GSSetSamplers(start_slot, 1u, (ID3D11SamplerState**)samplers.buffer);
			break;
		}

		context->PSSetSamplers(start_slot, samplers.size, (ID3D11SamplerState**)samplers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindSamplerState(SamplerState sampler, GraphicsContext* context, unsigned int slot, ECS_SHADER_TYPE type)
	{
		BindSamplerStates({ &sampler, 1 }, context, slot, type);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelUAView(UAView view, GraphicsContext* context, unsigned int start_slot)
	{
		context->OMSetRenderTargetsAndUnorderedAccessViews(
			D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL,
			nullptr,
			nullptr, 
			1u,
			1u, 
			&view.view, 
			nullptr
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelUAViews(Stream<UAView> views, GraphicsContext* context, unsigned int start_slot)
	{
		context->OMSetRenderTargetsAndUnorderedAccessViews(
			D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, 
			nullptr, 
			nullptr, 
			1u, 
			views.size, 
			(ID3D11UnorderedAccessView**)views.buffer, 
			nullptr
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeUAView(UAView view, GraphicsContext* context, unsigned int start_slot)
	{
		context->CSSetUnorderedAccessViews(start_slot, 1u, &view.view, nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeUAViews(Stream<UAView> views, GraphicsContext* context, unsigned int start_slot)
	{
		context->CSSetUnorderedAccessViews(start_slot, views.size, (ID3D11UnorderedAccessView**)views.buffer,  nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindRasterizerState(RasterizerState state, GraphicsContext* context)
	{
		context->RSSetState(state.state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDepthStencilState(DepthStencilState state, GraphicsContext* context, unsigned int stencil_ref)
	{
		context->OMSetDepthStencilState(state.state, stencil_ref);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindBlendState(BlendState state, GraphicsContext* context)
	{
		context->OMSetBlendState(state.state, nullptr, 0xffffffff);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view, GraphicsContext* context)
	{
		context->OMSetRenderTargets(1u, &render_view.view, depth_stencil_view.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth, GraphicsContext* context)
	{
		D3D11_VIEWPORT viewport = { 0 };
		viewport.TopLeftX = top_left_x;
		viewport.TopLeftY = top_left_y;
		viewport.Width = width;
		viewport.Height = height;
		viewport.MinDepth = min_depth;
		viewport.MaxDepth = max_depth;

		context->RSSetViewports(1u, &viewport);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindViewport(GraphicsViewport viewport, GraphicsContext* context)
	{
		BindViewport(viewport.top_left_x, viewport.top_left_y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindMesh(const Mesh& mesh, GraphicsContext* context) {
		// Vertex Buffers
		VertexBuffer vertex_buffers[ECS_MESH_BUFFER_COUNT];
		for (size_t index = 0; index < mesh.mapping_count; index++) {
			vertex_buffers[index] = mesh.vertex_buffers[mesh.mapping[index]];
		}

		BindVertexBuffers(Stream<VertexBuffer>(vertex_buffers, mesh.mapping_count), context);

		// Topology
		BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, context);

		// Index buffer
		BindIndexBuffer(mesh.index_buffer, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindMesh(const Mesh& mesh, GraphicsContext* context, Stream<ECS_MESH_INDEX> mapping)
	{
		// Vertex Buffers
		VertexBuffer vertex_buffers[ECS_MESH_BUFFER_COUNT];

		size_t valid_mappings = 0;
		for (size_t index = 0; index < mapping.size; index++) {
			for (size_t map_index = 0; map_index < mesh.mapping_count; map_index++) {
				if (mesh.mapping[map_index] == mapping[index]) {
					vertex_buffers[valid_mappings++] = mesh.vertex_buffers[mapping[index]];
					break;
				}
			}
		}

		ECS_ASSERT(valid_mappings == mapping.size, "The given mesh does not have the necessary vertex buffers.");
		BindVertexBuffers(Stream<VertexBuffer>(vertex_buffers, mapping.size), context);

		// Topology
		BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, context);

		// Index buffer
		BindIndexBuffer(mesh.index_buffer, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindMaterial(const Material& material, GraphicsContext* context) {
		// Bind the vertex shader
		BindVertexShader(material.vertex_shader, context);

		// Bind the pixel shader
		BindPixelShader(material.pixel_shader, context);

		// Bind the input layout
		BindInputLayout(material.layout, context);
		
		// Bind vertex constant buffers
		if (material.v_buffer_count > 0) {
			for (unsigned char index = 0; index < material.v_buffer_count; index++) {
				BindVertexConstantBuffer(material.v_buffers[index], context, material.v_buffer_slot[index]);
			}
		}

		// Bind pixel constant buffers
		if (material.p_buffer_count > 0) {
			for (unsigned char index = 0; index < material.p_buffer_count; index++) {
				BindPixelConstantBuffer(material.p_buffers[index], context, material.p_buffer_slot[index]);
			}
		}

		// Bind vertex textures
		if (material.v_texture_count > 0) {
			for (unsigned char index = 0; index < material.v_texture_count; index++) {
				BindVertexResourceView(material.v_textures[index], context, material.v_texture_slot[index]);
			}
		}

		// Bind pixel textures
		if (material.p_texture_count > 0) {
			for (unsigned char index = 0; index < material.p_texture_count; index++) {
				BindPixelResourceView(material.p_textures[index], context, material.p_texture_slot[index]);
			}
		}

		// Unordered views
		if (material.unordered_view_count > 0) {
			BindPixelUAViews(Stream<UAView>(material.unordered_views, material.unordered_view_count), context);
		}

		// The samplers
		if (material.v_sampler_count > 0) {
			for (unsigned char index = 0; index < material.v_sampler_count; index++) {
				BindSamplerState(material.v_samplers[index], context, material.v_sampler_slot[index], ECS_SHADER_VERTEX);
			}
		}

		if (material.p_sampler_count > 0) {
			for (unsigned char index = 0; index < material.p_sampler_count; index++) {
				BindSamplerState(material.p_samplers[index], context, material.p_sampler_slot[index], ECS_SHADER_PIXEL);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void ClearRenderTarget(RenderTargetView view, GraphicsContext* context, ColorFloat color)
	{
		context->ClearRenderTargetView(view.view, (float*)&color);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void ClearDepthStencil(DepthStencilView view, GraphicsContext* context, float depth, unsigned char stencil)
	{
		context->ClearDepthStencilView(view.view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Resource>
	void CopyGraphicsResource(Resource destination, Resource source, GraphicsContext* context) {
		context->CopyResource(destination.Interface(), source.Interface());
	}

#define EXPORT(type) template ECSENGINE_API void CopyGraphicsResource(type, type, GraphicsContext*);

	ECS_GRAPHICS_RESOURCES(EXPORT);

#undef EXPORT

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	void CopyBufferSubresource(
		Buffer destination,
		unsigned int destination_offset,
		Buffer source,
		unsigned int source_offset,
		unsigned int source_size,
		GraphicsContext* context
	) {
		D3D11_BOX box;

		box.top = box.bottom = box.back = box.front = 0;
		if constexpr (std::is_same_v<Buffer, VertexBuffer>) {
			box.left = source_offset * source.stride;
			box.right = source_size * source.stride;

			destination_offset *= destination.stride;
		}
		else if constexpr (std::is_same_v<Buffer, IndexBuffer>) {
			box.left = source_offset * source.int_size;
			box.right = source_size * source.int_size;

			destination_offset *= destination.int_size;
		}
		else {
			box.left = source_offset;
			box.right = source_size;
		}
		box.right += box.left;
		box.back = 1;
		box.bottom = 1;

		context->CopySubresourceRegion(destination.buffer, 0, destination_offset, 0, 0, source.buffer, 0, &box);
	}

#define EXPORT_COPY_GRAPHICS_SUBRESOURCE(type) template ECSENGINE_API void CopyBufferSubresource(type, unsigned int, type, unsigned int, unsigned int, GraphicsContext*);

	EXPORT_COPY_GRAPHICS_SUBRESOURCE(VertexBuffer);
	EXPORT_COPY_GRAPHICS_SUBRESOURCE(IndexBuffer);
	EXPORT_COPY_GRAPHICS_SUBRESOURCE(StandardBuffer);
	EXPORT_COPY_GRAPHICS_SUBRESOURCE(StructuredBuffer);
	EXPORT_COPY_GRAPHICS_SUBRESOURCE(ConstantBuffer);
	EXPORT_COPY_GRAPHICS_SUBRESOURCE(UABuffer);

#undef EXPORT_COPY_GRAPHICS_SUBRESOURCE

	// ------------------------------------------------------------------------------------------------------------------------

	void CopyGraphicsResource(TextureCube destination, Texture2D source, TextureCubeFace face, GraphicsContext* context, unsigned int mip_level)
	{
		D3D11_TEXTURE2D_DESC descriptor;
		destination.tex->GetDesc(&descriptor);
		// Clamp the width and height to 1
		descriptor.Width = function::ClampMin<unsigned int>(descriptor.Width >> mip_level, 1);
		descriptor.Height = function::ClampMin<unsigned int>(descriptor.Height >> mip_level, 1);
		CopyTextureSubresource(Texture2D(destination.tex), { 0,0 }, mip_level + descriptor.MipLevels * face, source, { 0, 0 }, { descriptor.Width, descriptor.Height }, mip_level, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void CopyTextureSubresource(
		Texture1D destination,
		unsigned int destination_offset,
		unsigned int destination_subresource_index,
		Texture1D source,
		unsigned int source_offset,
		unsigned int source_size,
		unsigned int source_subresource_index,
		GraphicsContext* context
	) {
		D3D11_BOX box;

		box.left = source_offset;
		box.right = source_offset + source_size;
		box.top = box.front = 0;
		box.back = box.bottom = 1;
		context->CopySubresourceRegion(destination.tex, destination_subresource_index, destination_offset, 0, 0, source.tex, source_subresource_index, &box);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void CopyTextureSubresource(
		Texture2D destination,
		uint2 destination_offset,
		unsigned int destination_subresource_index,
		Texture2D source,
		uint2 source_offset,
		uint2 source_size,
		unsigned int source_subresource_index,
		GraphicsContext* context
	) {
		D3D11_BOX box;

		box.left = source_offset.x;
		box.right = box.left + source_size.x;
		box.top = source_offset.y;
		box.bottom = box.top + source_size.y;
		box.front = 0;
		box.back = 1;
		context->CopySubresourceRegion(
			destination.tex, 
			destination_subresource_index, 
			destination_offset.x,
			destination_offset.y, 
			0, 
			source.tex, 
			source_subresource_index, 
			&box
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void CopyTextureSubresource(
		Texture3D destination,
		uint3 destination_offset,
		unsigned int destination_subresource_index,
		Texture3D source,
		uint3 source_offset,
		uint3 source_size,
		unsigned int source_subresource_index,
		GraphicsContext* context
	) {
		D3D11_BOX box;

		box.left = source_offset.x;
		box.right = box.left + source_size.x;
		box.top = source_offset.y;
		box.bottom = box.top + source_size.y;
		box.front = source_offset.z;
		box.back = box.front + source_size.z;
		context->CopySubresourceRegion(
			destination.tex,
			destination_subresource_index,
			destination_offset.x,
			destination_offset.y,
			destination_offset.z,
			source.tex,
			source_subresource_index,
			&box
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void CopyTextureSubresource(
		TextureCube texture,
		uint2 destination_offset,
		unsigned int mip_level,
		TextureCubeFace face,
		Texture2D source,
		uint2 source_offset,
		uint2 source_size,
		unsigned int source_subresource_index,
		GraphicsContext* context
	) {
		D3D11_TEXTURE2D_DESC descriptor;
		texture.tex->GetDesc(&descriptor);
		CopyTextureSubresource(Texture2D(texture.tex), destination_offset, mip_level + descriptor.MipLevels * face, source, source_offset, source_size, source_subresource_index, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Dispatch(uint3 dispatch_size, GraphicsContext* context)
	{
		context->Dispatch(dispatch_size.x, dispatch_size.y, dispatch_size.z);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DispatchIndirect(IndirectBuffer buffer, GraphicsContext* context)
	{
		context->DispatchIndirect(buffer.buffer, 0);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Draw(unsigned int vertex_count, GraphicsContext* context, unsigned int vertex_offset)
	{
		context->Draw(vertex_count, vertex_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawIndexed(unsigned int count, GraphicsContext* context, unsigned int start_index, unsigned int base_vertex_location) {
		context->DrawIndexed(count, start_index, base_vertex_location);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawInstanced(
		unsigned int vertex_count,
		unsigned int instance_count,
		GraphicsContext* context,
		unsigned int vertex_offset,
		unsigned int instance_offset
	) 
	{
		context->DrawInstanced(vertex_count, instance_count, vertex_offset, instance_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawIndexedInstanced(
		unsigned int index_count,
		unsigned int instance_count,
		GraphicsContext* context,
		unsigned int index_offset,
		unsigned int vertex_offset,
		unsigned int instance_offset
	) {
		context->DrawIndexedInstanced(index_count, instance_count, index_offset, vertex_offset, instance_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawIndexedInstancedIndirect(IndirectBuffer buffer, GraphicsContext* context) {
		context->DrawIndexedInstancedIndirect(buffer.buffer, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawInstancedIndirect(IndirectBuffer buffer, GraphicsContext* context) {
		context->DrawInstancedIndirect(buffer.buffer, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineRenderState GetPipelineRenderState(GraphicsContext* context)
	{
		GraphicsPipelineRenderState state;

		state.rasterizer_state = GetRasterizerState(context);
		state.blend_state = GetBlendState(context);
		state.depth_stencil_state = GetDepthStencilState(context);

		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsBoundViews GetBoundViews(GraphicsContext* context)
	{
		ID3D11RenderTargetView* render_target;
		ID3D11DepthStencilView* depth_stencil_view;
		context->OMGetRenderTargets(1, &render_target, &depth_stencil_view);
		return { render_target, depth_stencil_view };
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineState GetPipelineState(GraphicsContext* context)
	{
		return { GetPipelineRenderState(context), GetBoundViews(context), GetViewport(context) };
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsViewport GetViewport(GraphicsContext* context)
	{
		D3D11_VIEWPORT viewport;
		unsigned int viewport_count = 1;
		context->RSGetViewports(&viewport_count, &viewport);

		return { viewport.TopLeftX, viewport.TopLeftY, viewport.Width, viewport.Height, viewport.MinDepth, viewport.MaxDepth };
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineRasterizerState GetRasterizerState(GraphicsContext* context)
	{
		GraphicsPipelineRasterizerState state;

		context->RSGetState(&state.rasterizer_state.state);
		//unsigned int count = state.rasterizer_state.Release();

		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineDepthStencilState GetDepthStencilState(GraphicsContext* context)
	{
		GraphicsPipelineDepthStencilState state;

		context->OMGetDepthStencilState(&state.depth_stencil_state.state, &state.stencil_ref);
		//unsigned int count = state.depth_stencil_state.Release();
		
		// ECS_ASSERT(ref_count > 0);

		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineBlendState GetBlendState(GraphicsContext* context)
	{
		GraphicsPipelineBlendState state;

		context->OMGetBlendState(&state.blend_state.state, state.blend_factors, &state.sample_mask);
		//unsigned int count = state.blend_state.Release();

		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Resource>
	D3D11_MAPPED_SUBRESOURCE MapResourceInternal(Resource resource, GraphicsContext* context, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int subresource_index, unsigned int map_flags, const char* error_string) {
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		if constexpr (std::is_same_v<Resource, Texture1D> || std::is_same_v<Resource, Texture2D> || std::is_same_v<Resource, Texture3D>) {
			result = context->Map(resource.tex, subresource_index, GetGraphicsNativeMapType(map_type), map_flags, &mapped_subresource);
		}
		else {
			result = context->Map(resource, subresource_index, GetGraphicsNativeMapType(map_type), map_flags, &mapped_subresource);
		}
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), mapped_subresource, error_string);
		return mapped_subresource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* MapBuffer(
		ID3D11Buffer* buffer,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	)
	{
		return MapResourceInternal(buffer, context, map_type, subresource_index, map_flags, "Mapping a buffer failed.").pData;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	D3D11_MAPPED_SUBRESOURCE MapBufferEx(
		ID3D11Buffer* buffer,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	) {
		return MapResourceInternal(buffer, context, map_type, subresource_index, map_flags, "Mapping a buffer failed.");
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	void* MapTexture(
		Texture texture,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	)
	{
		return MapResourceInternal(texture, context, map_type, subresource_index, map_flags, "Mapping a texture failed.").pData;
	}

	// Cringe bug from intellisense that makes all the file full of errors when in reality everything is fine; instantiations must
	// be unrolled manually
	ECS_TEMPLATE_FUNCTION(void*, MapTexture, Texture1D, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(void*, MapTexture, Texture2D, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(void*, MapTexture, Texture3D, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);

	// ------------------------------------------------------------------------------------------------------------------------

	// It must be unmapped manually
	template<typename Texture>
	D3D11_MAPPED_SUBRESOURCE MapTextureEx(
		Texture texture,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	) {
		return MapResourceInternal(texture, context, map_type, subresource_index, map_flags, "Mapping a texture failed.");
	}

	// Cringe bug from intellisense that makes all the file full of errors when in reality everything is fine; instantiations must
	// be unrolled manually
	ECS_TEMPLATE_FUNCTION(D3D11_MAPPED_SUBRESOURCE, MapTextureEx, Texture1D, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(D3D11_MAPPED_SUBRESOURCE, MapTextureEx, Texture2D, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(D3D11_MAPPED_SUBRESOURCE, MapTextureEx, Texture3D, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);

	// ------------------------------------------------------------------------------------------------------------------------

	void RestorePipelineRenderState(GraphicsContext* context, GraphicsPipelineRenderState render_state)
	{
		RestoreBlendState(context, render_state.blend_state);
		RestoreDepthStencilState(context, render_state.depth_stencil_state);
		RestoreRasterizerState(context, render_state.rasterizer_state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreBoundViews(GraphicsContext* context, GraphicsBoundViews views)
	{
		BindRenderTargetView(views.target, views.depth_stencil, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestorePipelineState(GraphicsContext* context, const GraphicsPipelineState* state)
	{
		RestorePipelineRenderState(context, state->render_state);
		RestoreBoundViews(context, state->views);
		BindViewport(state->viewport, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreBlendState(GraphicsContext* context, GraphicsPipelineBlendState blend_state)
	{
		context->OMSetBlendState(blend_state.blend_state.state, blend_state.blend_factors, blend_state.sample_mask);
		blend_state.blend_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreRasterizerState(GraphicsContext* context, GraphicsPipelineRasterizerState rasterizer_state)
	{
		BindRasterizerState(rasterizer_state.rasterizer_state, context);
		rasterizer_state.rasterizer_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreDepthStencilState(GraphicsContext* context, GraphicsPipelineDepthStencilState depth_stencil_state)
	{
		BindDepthStencilState(depth_stencil_state.depth_stencil_state, context, depth_stencil_state.stencil_ref);
		depth_stencil_state.depth_stencil_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* TransferGPUResourceImpl(ID3D11Resource* dx_resource, GraphicsDevice* device) {
		// Acquire the DXGIResource interface from the DX resource
		ID3D11Resource* new_dx_resource = nullptr;

		IDXGIResource* dxgi_resource;
		HRESULT result = dx_resource->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgi_resource);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), new_dx_resource, "Getting the DXGI resource from shared resource failed.");

		// Query interface updates the reference count, release it to maintain invariance
		unsigned int count = dxgi_resource->Release();

		HANDLE handle;
		result = dxgi_resource->GetSharedHandle(&handle);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), new_dx_resource, "Acquiring a handle for a shared resource failed.");

		result = device->OpenSharedResource(handle, __uuidof(ID3D11Resource), (void**)&new_dx_resource);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), new_dx_resource, "Obtaining shared resource from handle failed.");

		return new_dx_resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Resource>
	Resource TransferGPUResource(Resource resource, GraphicsDevice* device)
	{
		ID3D11Resource* dx_resource = resource.GetResource();
		ID3D11Resource* new_dx_resource = TransferGPUResourceImpl(dx_resource, device);

		// In order to construct the new type, use constexpr if to properly construct the type
		auto get_buffer_resource = [](ID3D11Resource* resource) {
			ID3D11Buffer* buffer = nullptr;
			HRESULT result = resource->QueryInterface(__uuidof(ID3D11Buffer), (void**)&buffer);
			ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Converting a shared resource to buffer failed.");

			buffer->Release();
			return buffer;
		};

		Resource new_resource;
		memcpy(&new_resource, &resource, sizeof(Resource));
		if constexpr (std::is_same_v<Resource, VertexBuffer>) {
			new_resource.buffer = get_buffer_resource(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, IndexBuffer>) {
			new_resource.buffer = get_buffer_resource(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, ConstantBuffer>) {
			new_resource.buffer = get_buffer_resource(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, StandardBuffer>) {
			new_resource.buffer = get_buffer_resource(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, StructuredBuffer>) {
			new_resource.buffer = get_buffer_resource(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, UABuffer>) {
			new_resource.buffer = get_buffer_resource(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, IndirectBuffer>) {
			new_resource.buffer = get_buffer_resource(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, Texture1D>) {
			new_resource = Texture1D(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, Texture2D>) {
			new_resource = Texture2D(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, Texture3D>) {
			new_resource = Texture3D(new_dx_resource);
		}
		else if constexpr (std::is_same_v<Resource, TextureCube>) {
			new_resource = TextureCube(new_dx_resource);
		}
		else {
			ECS_ASSERT(false);
		}

		return new_resource;
	}

#define EXPORT_TRANSFER_GPU(resource) template ECSENGINE_API resource TransferGPUResource(resource, GraphicsDevice*);

	ECS_GRAPHICS_RESOURCES(EXPORT_TRANSFER_GPU);

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename View>
	View TransferGPUView(View view, GraphicsDevice* device) {
		ID3D11Resource* resource = view.GetResource();

		ID3D11Resource* copied_resource = TransferGPUResourceImpl(resource, device);
		return View::RawCopy(device, copied_resource, view);
	}

	EXPORT_TRANSFER_GPU(RenderTargetView);
	EXPORT_TRANSFER_GPU(DepthStencilView);
	EXPORT_TRANSFER_GPU(ResourceView);
	EXPORT_TRANSFER_GPU(UAView);

#undef EXPORT_TRANSFER_GPU

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	void UpdateTexture(
		Texture texture,
		const void* data,
		size_t data_size,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type,
		unsigned int map_flags,
		unsigned int subresource_index
	) {
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(texture.Interface(), subresource_index, GetGraphicsNativeMapType(map_type), map_flags, &mapped_subresource);
		ECS_CRASH_RETURN(SUCCEEDED(result), "Updating texture failed.");

		memcpy(mapped_subresource.pData, data, data_size);
		context->Unmap(texture.Interface(), subresource_index);
	}

	// Cringe bug from intellisense that makes all the file full of errors when in reality everything is fine; instantiations must
	// be unrolled manually
	ECS_TEMPLATE_FUNCTION(void, UpdateTexture, Texture1D, const void*, size_t, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(void, UpdateTexture, Texture2D, const void*, size_t, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(void, UpdateTexture, Texture3D, const void*, size_t, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	void UpdateTextureResource(Texture texture, const void* data, size_t data_size, GraphicsContext* context, unsigned int subresource_index) {
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(texture.Interface(), subresource_index, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
		ECS_CRASH_RETURN(SUCCEEDED(result), "Updating texture failed.");

		memcpy(mapped_subresource.pData, data, data_size);
		context->Unmap(texture.Interface(), subresource_index);
	}

	// Cringe bug from intellisense that makes all the file full of errors when in reality everything is fine; instantiations must
	// be unrolled manually
	ECS_TEMPLATE_FUNCTION(void, UpdateTextureResource, Texture1D, const void*, size_t, GraphicsContext*, unsigned int);
	ECS_TEMPLATE_FUNCTION(void, UpdateTextureResource, Texture2D, const void*, size_t, GraphicsContext*, unsigned int);
	ECS_TEMPLATE_FUNCTION(void, UpdateTextureResource, Texture3D, const void*, size_t, GraphicsContext*, unsigned int);

	// ------------------------------------------------------------------------------------------------------------------------

	void UpdateBuffer(
		ID3D11Buffer* buffer,
		const void* data,
		size_t data_size,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type,
		unsigned int map_flags,
		unsigned int subresource_index
	)
	{
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(buffer, subresource_index, GetGraphicsNativeMapType(map_type), map_flags, &mapped_subresource);
		ECS_CRASH_RETURN(SUCCEEDED(result), "Updating buffer failed.");

		memcpy(mapped_subresource.pData, data, data_size);
		context->Unmap(buffer, subresource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void UpdateBufferResource(
		ID3D11Buffer* buffer,
		const void* data,
		size_t data_size,
		GraphicsContext* context,
		unsigned int subresource_index
	) {
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(buffer, subresource_index, D3D11_MAP_WRITE_DISCARD, 0u, &mapped_subresource);
		ECS_CRASH_RETURN(SUCCEEDED(result), "Updating buffer resource failed.");

		memcpy(mapped_subresource.pData, data, data_size);
		context->Unmap(buffer, subresource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void UnmapBuffer(ID3D11Buffer* buffer, GraphicsContext* context, unsigned int resource_index)
	{
		context->Unmap(buffer, resource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	void UnmapTexture(Texture texture, GraphicsContext* context, unsigned int resource_index)
	{
		context->Unmap(texture.tex, resource_index);
	}

	// Cringe bug from intellisense that makes all the file full of errors when in reality everything is fine; instantiations must
	// be unrolled manually
	ECS_TEMPLATE_FUNCTION(void, UnmapTexture, Texture1D, GraphicsContext*, unsigned int);
	ECS_TEMPLATE_FUNCTION(void, UnmapTexture, Texture2D, GraphicsContext*, unsigned int);
	ECS_TEMPLATE_FUNCTION(void, UnmapTexture, Texture3D, GraphicsContext*, unsigned int);

	// ------------------------------------------------------------------------------------------------------------------------

	void FreeMaterial(Graphics* graphics, const Material* material) {
		// Release the input layout
		graphics->FreeResource(material->layout);

		// Release the shaders
		graphics->FreeResource(material->vertex_shader);
		graphics->FreeResource(material->pixel_shader);

		// Release the constant buffers - each one needs to be released
		// When they were assigned, their reference count was incremented
		// in order to avoid checking aliased buffers
		for (size_t index = 0; index < material->v_buffer_count; index++) {
			graphics->FreeResource(material->v_buffers[index]);
		}

		for (size_t index = 0; index < material->p_buffer_count; index++) {
			graphics->FreeResource(material->p_buffers[index]);
		}

		// Release the resource views - each one needs to be released
		// Same as constant buffers, their reference count was incremented upon
		// assignment alongside the resource that they view
		for (size_t index = 0; index < material->v_texture_count; index++) {
			graphics->FreeResourceView(material->v_textures[index]);
		}

		for (size_t index = 0; index < material->p_texture_count; index++) {
			graphics->FreeResourceView(material->p_textures[index]);
		}

		// Release the UAVs
		for (size_t index = 0; index < material->unordered_view_count; index++) {
			graphics->FreeUAView(material->unordered_views[index]);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void FreeCoallescedMesh(Graphics* graphics, const CoallescedMesh* mesh)
	{
		graphics->FreeMesh(mesh->mesh);
		for (size_t index = 0; index < mesh->submeshes.size; index++) {
			graphics->FreeSubmesh(mesh->submeshes[index]);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Mesh MeshesToSubmeshes(Graphics* graphics, Stream<Mesh> meshes, Submesh* submeshes, ECS_GRAPHICS_MISC_FLAGS misc_flags)
	{
		unsigned int* mask = (unsigned int*)ECS_STACK_ALLOC(meshes.size * sizeof(unsigned int));
		Stream<unsigned int> sequence(mask, meshes.size);
		function::MakeSequence(Stream<unsigned int>(mask, meshes.size));

		Mesh mesh = MeshesToSubmeshes(graphics, meshes, submeshes, sequence, misc_flags);
		return mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Mesh MeshesToSubmeshes(Graphics* graphics, Stream<Mesh> meshes, Submesh* submeshes, Stream<unsigned int> mesh_mask, ECS_GRAPHICS_MISC_FLAGS misc_flags) {
		Mesh result;

		// Walk through the meshes and determine the maximum amount of buffers. The meshes that are missing some buffers will have them
		// zero'ed in the final mesh

		// Accumulate the total vertex buffer size and index buffer size
		size_t vertex_buffer_size = 0;
		size_t index_buffer_size = 0;
		size_t mapping_count = meshes[0].mapping_count;

		ECS_MESH_INDEX mappings[ECS_MESH_BUFFER_COUNT];
		memcpy(mappings, meshes[0].mapping, sizeof(ECS_MESH_INDEX) * mapping_count);

		for (size_t index = 0; index < mesh_mask.size; index++) {
			vertex_buffer_size += meshes[mesh_mask[index]].vertex_buffers[0].size;
			index_buffer_size += meshes[mesh_mask[index]].index_buffer.count;
			if (mapping_count < meshes[mesh_mask[index]].mapping_count) {
				mapping_count = meshes[mesh_mask[index]].mapping_count;
				// Update the mapping order
				memcpy(mappings, meshes[mesh_mask[index]].mapping, sizeof(ECS_MESH_INDEX) * mapping_count);
			}
		}

		// Create a temporary zero vertex buffer for those cases when some mappings are missing
		// Allocate using calloc since it will yield better results
		void* zero_memory = calloc(vertex_buffer_size, sizeof(float4));
		VertexBuffer zero_vertex_buffer = graphics->CreateVertexBuffer(sizeof(float4), vertex_buffer_size, true);
		free(zero_memory);

		// Create the new vertex buffers and the index buffer
		VertexBuffer new_vertex_buffers[ECS_MESH_BUFFER_COUNT];
		for (size_t index = 0; index < mapping_count; index++) {
			new_vertex_buffers[index] = graphics->CreateVertexBuffer(
				meshes[0].vertex_buffers[index].stride, 
				vertex_buffer_size, 
				false, 
				ECS_GRAPHICS_USAGE_DEFAULT,
				ECS_GRAPHICS_CPU_ACCESS_NONE,
				misc_flags
			);
		}

		IndexBuffer new_index_buffer = graphics->CreateIndexBuffer(meshes[0].index_buffer.int_size, index_buffer_size, false, ECS_GRAPHICS_USAGE_DEFAULT, ECS_GRAPHICS_CPU_ACCESS_NONE, misc_flags);

		// all vertex buffers must have the same size - so a single offset suffices
		unsigned int vertex_buffer_offset = 0;
		unsigned int index_buffer_offset = 0;

		// Copy the vertex buffer and index buffer submeshes
		for (size_t index = 0; index < mesh_mask.size; index++) {
			Mesh* current_mesh = &meshes[mesh_mask[index]];
			// If the mesh has a name, simply repoint the submesh pointer - do not deallocate and reallocate again
			if (current_mesh->name.buffer != nullptr) {
				submeshes[index].name = current_mesh->name;
			}
			else {
				submeshes[index].name = { nullptr, 0 };
			}

			submeshes[index].index_buffer_offset = index_buffer_offset;
			submeshes[index].vertex_buffer_offset = vertex_buffer_offset;
			submeshes[index].index_count = current_mesh->index_buffer.count;
			submeshes[index].vertex_buffer_offset = vertex_buffer_offset;
			submeshes[index].vertex_count = current_mesh->vertex_buffers[0].size;

			bool buffers_comitted[ECS_MESH_BUFFER_COUNT] = { false };

			size_t mesh_vertex_buffer_size = current_mesh->vertex_buffers[0].size;
			// Vertex buffers
			for (size_t buffer_index = 0; buffer_index < current_mesh->mapping_count; buffer_index++) {
				size_t new_vertex_index = 0;
				// Determine the vertex buffer position from the global vertex buffer
				for (size_t subindex = 0; subindex < mapping_count; subindex++) {
					if (mappings[subindex] == current_mesh->mapping[buffer_index]) {
						new_vertex_index = subindex;
						buffers_comitted[subindex] = true;
						// Exit the loop
						subindex = mapping_count;
					}
				}

				CopyBufferSubresource(
					new_vertex_buffers[new_vertex_index],
					vertex_buffer_offset,
					current_mesh->vertex_buffers[buffer_index],
					0,
					mesh_vertex_buffer_size,
					graphics->GetContext()
				);

				// Release all the old buffers
				graphics->FreeResource(current_mesh->vertex_buffers[buffer_index]);
				current_mesh->vertex_buffers[buffer_index].buffer = nullptr;
			}

			// For all the buffers which are missing, zero the data with a global zero vector
			for (size_t buffer_index = 0; buffer_index < mapping_count; buffer_index++) {
				if (!buffers_comitted[buffer_index]) {
					// Temporarly set the zero_vertex_buffer.stride to the stride of the new vertex buffer such that the copy is done correctly,
					// else it will overcopy for types less than float4
					zero_vertex_buffer.stride = new_vertex_buffers[buffer_index].stride;
					CopyBufferSubresource(new_vertex_buffers[buffer_index], vertex_buffer_offset, zero_vertex_buffer, 0, mesh_vertex_buffer_size, graphics->GetContext());
				}
			}

			// Index buffer - a new copy must be created which has the indices offset by the correct amount
			// Use a staging resource for that
			IndexBuffer staging_buffer = BufferToStaging(graphics, current_mesh->index_buffer);

			// Map the staging buffer 
			unsigned int* staging_data = (unsigned int*)graphics->MapBuffer(staging_buffer.buffer, ECS_GRAPHICS_MAP_READ_WRITE);

			// Use simd to increase the offsets
			unsigned int index_buffer_count = current_mesh->index_buffer.count;
			unsigned int simd_count = index_buffer_count & (-Vec8ui::size());

			Vec8ui simd_offset(vertex_buffer_offset);
			for (size_t simd_index = 0; simd_index < simd_count; simd_index += Vec8ui::size()) {
				Vec8ui data;
				data.load(staging_data + simd_index);
				data += simd_offset;
				data.store(staging_data + simd_index);
			}
			for (size_t simd_index = simd_count; simd_index < index_buffer_count; simd_index++) {
				staging_data[simd_index] += vertex_buffer_offset;
			}
			graphics->UnmapBuffer(staging_buffer.buffer);
			CopyBufferSubresource(new_index_buffer, index_buffer_offset, staging_buffer, 0, index_buffer_count, graphics->GetContext());
			index_buffer_offset += index_buffer_count;

			// all vertex buffers must have the same size
			vertex_buffer_offset += current_mesh->vertex_buffers[0].size;

			// Release the staging buffer and the normal buffer
			unsigned int staging_count = staging_buffer.Release();

			graphics->FreeResource(current_mesh->index_buffer);
			current_mesh->index_buffer.buffer = nullptr;
			current_mesh->mapping_count = 0;
		}

		result.index_buffer = new_index_buffer;
		for (size_t index = 0; index < mapping_count; index++) {
			result.vertex_buffers[index] = new_vertex_buffers[index];
			result.mapping[index] = meshes[0].mapping[index];
		}
		result.mapping_count = mapping_count;

		// Release the zero vertex buffer
		zero_vertex_buffer.Release();

		return result;
	}

	// ------------------------------------------------------------------------------------------------------------------------

#pragma endregion



#endif // ECSENGINE_DIRECTX11

#endif // ECSENGINE_PLATFORM_WINDOWS
}