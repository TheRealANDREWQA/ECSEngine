#include "ecspch.h"
#include "Graphics.h"
#include "../Utilities/FunctionInterfaces.h"
#include "GraphicsHelpers.h"
#include "../Utilities/Function.h"
#include "../Utilities/File.h"
#include "../Utilities/Path.h"
#include "ShaderInclude.h"
#include "../Allocators/AllocatorPolymorphic.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")

namespace ECSEngine {

	const char* SHADER_ENTRY_POINT = "main";


#ifdef ECSENGINE_PLATFORM_WINDOWS

#ifdef ECSENGINE_DIRECTX11

	enum SHADER_ORDER {
		ECS_SHADER_VERTEX,
		ECS_SHADER_PIXEL,
		ECS_SHADER_DOMAIN,
		ECS_SHADER_HULL,
		ECS_SHADER_GEOMETRY,
		ECS_SHADER_COMPUTE
	};

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

	Graphics::Graphics(HWND hWnd, const GraphicsDescriptor* descriptor) 
		: m_target_view(nullptr), m_device(nullptr), m_context(nullptr), m_swap_chain(nullptr), m_allocator(descriptor->allocator),
		m_bound_render_target_count(1), m_copied_graphics(false)
	{
		// The internal resources
		m_internal_resources.Initialize(descriptor->allocator, GRAPHICS_INTERNAL_RESOURCE_STARTING_COUNT);
		m_internal_resources_lock.unlock();

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
		swap_chain_descriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | descriptor->target_usage;
		swap_chain_descriptor.BufferCount = 2;
		swap_chain_descriptor.OutputWindow = hWnd;
		swap_chain_descriptor.Windowed = TRUE;
		swap_chain_descriptor.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_descriptor.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		UINT flags = 0;
//#ifdef ECSENGINE_DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
//#endif

		// create device, front and back buffers, swap chain and rendering context
		HRESULT result = D3D11CreateDeviceAndSwapChain(
			nullptr, 
			D3D_DRIVER_TYPE_HARDWARE,
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

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Initializing device and swap chain failed!", true);

		result = m_device->CreateDeferredContext(0, &m_deferred_context);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating deferred context failed!", true);

		CreateInitialRenderTargetView(descriptor->gamma_corrected);

		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_state = {};
		// creating depth stencil state
		D3D11_DEPTH_STENCIL_DESC depth_stencil_descriptor = {};
		depth_stencil_descriptor.DepthEnable = TRUE;
		depth_stencil_descriptor.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depth_stencil_descriptor.DepthFunc = D3D11_COMPARISON_LESS;

		result = m_device->CreateDepthStencilState(&depth_stencil_descriptor, &depth_stencil_state);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating depth stencil state failed", true);

		CreateInitialDepthStencilView();

		// binding depth stencil state
		m_context->OMSetDepthStencilState(depth_stencil_state.Get(), 1u);

		m_context->OMSetRenderTargets(1u, &m_target_view.target, m_depth_stencil_view.view);
		m_bound_render_targets[0] = m_target_view;
		m_current_depth_stencil = m_depth_stencil_view;

		D3D11_RENDER_TARGET_BLEND_DESC render_target_descriptor = {};
		render_target_descriptor.BlendEnable = TRUE;
		render_target_descriptor.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		render_target_descriptor.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		render_target_descriptor.BlendOp = D3D11_BLEND_OP_ADD;
		render_target_descriptor.SrcBlendAlpha = D3D11_BLEND_ZERO;
		render_target_descriptor.DestBlendAlpha = D3D11_BLEND_ZERO;
		render_target_descriptor.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		render_target_descriptor.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		D3D11_BLEND_DESC blend_descriptor = {};
		blend_descriptor.AlphaToCoverageEnable = FALSE;
		blend_descriptor.RenderTarget[0] = render_target_descriptor;

		result = m_device->CreateBlendState(&blend_descriptor, &m_blend_enabled.state);
		
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating blend state failed!", true);

		blend_descriptor.RenderTarget[0].BlendEnable = FALSE;

		result = m_device->CreateBlendState(&blend_descriptor, &m_blend_disabled.state);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating blend state failed!", true);

		m_context->OMSetBlendState(m_blend_disabled.state, nullptr, 0xFFFFFFFF);

		// configure viewport
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = m_window_size.x;
		viewport.Height = m_window_size.y;
		viewport.MaxDepth = 1;
		viewport.MinDepth = 0;

		m_context->RSSetViewports(1u, &viewport);

		// Shader Reflection
		size_t memory_size = m_shader_reflection->MemoryOf();
		void* allocation = m_allocator->Allocate(memory_size);
		m_shader_reflection = new ShaderReflection(allocation);

		D3D11_RASTERIZER_DESC rasterizer_state = { };
		rasterizer_state.AntialiasedLineEnable = TRUE;
		rasterizer_state.CullMode = D3D11_CULL_BACK;
		rasterizer_state.FillMode = D3D11_FILL_SOLID;
		rasterizer_state.DepthClipEnable = TRUE;
		rasterizer_state.FrontCounterClockwise = FALSE;

		ID3D11RasterizerState* state;
		m_device->CreateRasterizerState(&rasterizer_state, &state);

		m_context->RSSetState(state);
		state->Release();

		m_shader_helpers.Initialize(m_allocator, ECS_GRAPHICS_SHADER_HELPER_COUNT, ECS_GRAPHICS_SHADER_HELPER_COUNT);
		
		auto load_source_code = [&](const wchar_t* path) {
			return ReadWholeFileText(path, GetAllocatorPolymorphic(m_allocator));
		};

		ShaderIncludeFiles include(m_allocator, ToStream(ECS_SHADER_DIRECTORY));
		for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
			Stream<char> vertex_source = load_source_code(SHADER_HELPERS_VERTEX[index]);
			ECS_ASSERT(vertex_source.buffer != nullptr);

			m_shader_helpers[index].vertex = CreateVertexShaderFromSource(vertex_source, &include, true);

			Stream<char> pixel_source = load_source_code(SHADER_HELPERS_PIXEL[index]);
			ECS_ASSERT(pixel_source.buffer != nullptr);

			m_shader_helpers[index].pixel = CreatePixelShaderFromSource(pixel_source, &include, true);
			m_shader_helpers[index].input_layout = ReflectVertexShaderInput(m_shader_helpers[index].vertex, vertex_source, true);

			m_allocator->Deallocate(vertex_source.buffer);
			m_allocator->Deallocate(pixel_source.buffer);
		}
		D3D11_SAMPLER_DESC sampler_descriptor = {};
		sampler_descriptor.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_descriptor.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_descriptor.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_descriptor.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_descriptor.MinLOD = 0.0f;
		sampler_descriptor.MaxLOD = D3D11_FLOAT32_MAX;
		for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
			if (index != ECS_GRAPHICS_SHADER_HELPER_BRDF_INTEGRATION) {
				m_shader_helpers[index].pixel_sampler = CreateSamplerState(sampler_descriptor, true);
			}
		}
		sampler_descriptor.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_descriptor.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_descriptor.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		m_shader_helpers[ECS_GRAPHICS_SHADER_HELPER_BRDF_INTEGRATION].pixel_sampler = CreateSamplerState(sampler_descriptor, true);
		
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Graphics::Graphics(const Graphics* graphics_to_copy, RenderTargetView new_render_target, DepthStencilView new_depth_view, MemoryManager* new_allocator)
	{
		m_shader_helpers = graphics_to_copy->m_shader_helpers;
		m_shader_reflection = graphics_to_copy->m_shader_reflection;

		Texture2D render_texture = GetResource(new_render_target);
		D3D11_TEXTURE2D_DESC render_descriptor;
		render_texture.tex->GetDesc(&render_descriptor);
		m_window_size = { render_descriptor.Width, render_descriptor.Height };

		m_swap_chain = nullptr;

		UINT flags = 0;
		flags |= D3D11_CREATE_DEVICE_DEBUG;
		
		// Create a new device and context
		HRESULT result = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			flags,
			nullptr,
			0,
			D3D11_SDK_VERSION,
			&m_device,
			nullptr,
			&m_context
		);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Failing to create a new GPU device.", true);

		result = m_device->CreateDeferredContext(0, &m_deferred_context);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Failed to create deferred context for new GPU device.", true);

		m_target_view = new_render_target;
		m_depth_stencil_view = new_depth_view;

		m_bound_render_targets[0] = new_render_target;
		m_bound_render_target_count = 1;
		m_current_depth_stencil = new_depth_view;

		m_blend_disabled = graphics_to_copy->m_blend_disabled;
		m_blend_enabled = graphics_to_copy->m_blend_enabled;

		if (new_allocator == nullptr) {
			m_allocator = graphics_to_copy->m_allocator;
		}
		else {
			m_allocator = new_allocator;
		}

		m_internal_resources.Initialize(m_allocator, GRAPHICS_INTERNAL_RESOURCE_STARTING_COUNT);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexBuffer(VertexBuffer buffer, UINT slot) {
		ECSEngine::BindVertexBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexBuffers(Stream<VertexBuffer> buffers, UINT start_slot) {
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

	void Graphics::BindPixelConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindPixelConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindPixelConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindVertexConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindVertexConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindDomainConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindDomainConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindHullConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindHullConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindGeometryConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindGeometryConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindComputeConstantBuffer(buffer, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindComputeConstantBuffers(buffers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindTopology(Topology topology)
	{
		ECSEngine::BindTopology(topology, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindPixelResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindPixelResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindVertexResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindVertexResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindDomainResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindDomainResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindHullResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindHullResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindGeometryResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindGeometryResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindComputeResourceView(component, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindComputeResourceViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindSamplerState(SamplerState sampler, UINT slot)
	{
		ECSEngine::BindSamplerState(sampler, m_context, slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindSamplerStates(Stream<SamplerState> samplers, UINT start_slot)
	{
		ECSEngine::BindSamplerStates(samplers, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelUAView(UAView view, UINT start_slot)
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

	void Graphics::BindComputeUAView(UAView view, UINT start_slot)
	{
		ECSEngine::BindComputeUAView(view, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelUAViews(Stream<UAView> views, UINT start_slot) {
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

	void Graphics::BindComputeUAViews(Stream<UAView> views, UINT start_slot) {
		ECSEngine::BindComputeUAViews(views, m_context, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRasterizerState(RasterizerState state)
	{
		ECSEngine::BindRasterizerState(state, m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDepthStencilState(DepthStencilState state, UINT stencil_ref)
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
		m_context->OMSetRenderTargets(1u, &m_target_view.target, m_depth_stencil_view.view);
		m_bound_render_targets[0] = m_target_view;
		m_bound_render_target_count = 1;
		m_current_depth_stencil = m_depth_stencil_view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRenderTargetViewFromInitialViews(GraphicsContext* context)
	{
		context->OMSetRenderTargets(1u, &m_target_view.target, m_depth_stencil_view.view);
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

	IndexBuffer Graphics::CreateIndexBuffer(Stream<unsigned char> indices, bool temporary)
	{ 
		return CreateIndexBuffer(sizeof(unsigned char), indices.size, indices.buffer, temporary);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(Stream<unsigned short> indices, bool temporary)
	{
		return CreateIndexBuffer(sizeof(unsigned short), indices.size, indices.buffer, temporary);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(Stream<unsigned int> indices, bool temporary)
	{
		return CreateIndexBuffer(sizeof(unsigned int), indices.size, indices.buffer, temporary);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(size_t int_size, size_t element_count, bool temporary, D3D11_USAGE usage, unsigned int cpu_access, unsigned int misc_flags)
	{
		IndexBuffer buffer;
		buffer.count = element_count;
		buffer.int_size = int_size;

		HRESULT result;
		D3D11_BUFFER_DESC index_buffer_descriptor = {};
		index_buffer_descriptor.ByteWidth = UINT(buffer.count * int_size);
		index_buffer_descriptor.Usage = usage;
		index_buffer_descriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
		index_buffer_descriptor.CPUAccessFlags = cpu_access;
		index_buffer_descriptor.MiscFlags = misc_flags;
		index_buffer_descriptor.StructureByteStride = int_size;

		result = m_device->CreateBuffer(&index_buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating index buffer failed", true);

		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(size_t int_size, size_t element_count, const void* data, bool temporary, D3D11_USAGE usage, unsigned int cpu_access, unsigned int misc_flags)
	{
		IndexBuffer buffer;
		buffer.count = element_count;
		buffer.int_size = int_size;

		HRESULT result;
		D3D11_BUFFER_DESC index_buffer_descriptor = {};
		index_buffer_descriptor.ByteWidth = UINT(buffer.count * int_size);
		index_buffer_descriptor.Usage = usage;
		index_buffer_descriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
		index_buffer_descriptor.CPUAccessFlags = cpu_access;
		index_buffer_descriptor.MiscFlags = misc_flags;
		index_buffer_descriptor.StructureByteStride = int_size;

		D3D11_SUBRESOURCE_DATA subresource_data = {};
		subresource_data.pSysMem = data;

		result = m_device->CreateBuffer(&index_buffer_descriptor, &subresource_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating index buffer failed", true);
		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ID3DBlob* ShaderByteCode(GraphicsDevice* device, Stream<char> source_code, ShaderCompileOptions options, ID3DInclude* include_policy, SHADER_ORDER order) {
		D3D_SHADER_MACRO macros[64];
		ECS_ASSERT(options.macros.size <= 64 - 2);
		memcpy(macros, options.macros.buffer, sizeof(const char*) * 2 * options.macros.size);
		macros[options.macros.size] = { NULL, NULL };

		const char* target = SHADER_COMPILE_TARGET[order * ECS_SHADER_TARGET_COUNT + options.target];

		unsigned int compile_flags = 0;
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_DEBUG), D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0);
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_LOWEST), D3DCOMPILE_OPTIMIZATION_LEVEL0, 0);
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_LOW), D3DCOMPILE_OPTIMIZATION_LEVEL1, 0);
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_HIGH), D3DCOMPILE_OPTIMIZATION_LEVEL2, 0);
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_HIGHEST), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0);

		ID3DBlob* blob;
		ID3DBlob* error_message_blob = nullptr;
		HRESULT result = D3DCompile(source_code.buffer, source_code.size, nullptr, macros, include_policy, SHADER_ENTRY_POINT, target, compile_flags, 0, &blob, &error_message_blob);

		const char* error_message;
		if (error_message_blob != nullptr) {
			error_message = (char*)error_message_blob->GetBufferPointer();
			error_message_blob->Release();
		}

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Compiling a shader failed.", true);	
		return blob;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	PixelShader Graphics::CreatePixelShader(Stream<void> byte_code, bool temporary)
	{
		PixelShader shader;

		HRESULT result;
		result = m_device->CreatePixelShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Domain shader failed.", true);
		if (!temporary) {
			AddInternalResource(shader);
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	PixelShader Graphics::CreatePixelShaderFromSource(Stream<char> source_code, ID3DInclude* include_policy, bool temporary, ShaderCompileOptions options)
	{
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_PIXEL);
		PixelShader shader = CreatePixelShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	// The vertex shader must not have the blob released
	VertexShader Graphics::CreateVertexShader(Stream<void> byte_code, bool temporary) {
		VertexShader shader;

		HRESULT result;
		result = m_device->CreateVertexShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Domain shader failed.", true);
		
		shader.byte_code = { m_allocator->Allocate_ts(byte_code.size), byte_code.size };
		memcpy(shader.byte_code.buffer, byte_code.buffer, byte_code.size);

		if (!temporary) {
			AddInternalResource(shader);
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexShader Graphics::CreateVertexShaderFromSource(Stream<char> source_code, ID3DInclude* include_policy, bool temporary, ShaderCompileOptions options) {
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_VERTEX);
		VertexShader shader = CreateVertexShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader Graphics::CreateDomainShader(Stream<void> byte_code, bool temporary)
	{
		DomainShader shader;

		HRESULT result; 
		result = m_device->CreateDomainShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Domain shader failed.", true); 
		if (!temporary) {
			AddInternalResource(shader);
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader Graphics::CreateDomainShaderFromSource(Stream<char> source_code, ID3DInclude* include_policy, bool temporary, ShaderCompileOptions options)
	{
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_DOMAIN);
		DomainShader shader = CreateDomainShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HullShader Graphics::CreateHullShader(Stream<void> byte_code, bool temporary) {
		HullShader shader;

		HRESULT result;
		result = m_device->CreateHullShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Hull shader failed.", true);
		
		if (!temporary) {
			AddInternalResource(shader);
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HullShader Graphics::CreateHullShaderFromSource(Stream<char> source_code, ID3DInclude* include_policy, bool temporary, ShaderCompileOptions options) {
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_HULL);
		HullShader shader = CreateHullShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GeometryShader Graphics::CreateGeometryShader(Stream<void> byte_code, bool temporary) {
		GeometryShader shader;

		HRESULT result;
		result = m_device->CreateGeometryShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Geometry shader failed.", true);
		if (!temporary) {
			AddInternalResource(shader);
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GeometryShader Graphics::CreateGeometryShaderFromSource(Stream<char> source_code, ID3DInclude* include_policy, bool temporary, ShaderCompileOptions options) {
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_GEOMETRY);
		GeometryShader shader = CreateGeometryShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ComputeShader Graphics::CreateComputeShader(Stream<void> byte_code, bool temporary) {
		ComputeShader shader;

		HRESULT result;
		result = m_device->CreateComputeShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Geometry shader failed.", true);
		if (!temporary) {
			AddInternalResource(shader);
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ComputeShader Graphics::CreateComputeShaderFromSource(Stream<char> source_code, ID3DInclude* include_policy, bool temporary, ShaderCompileOptions options) {
		ID3DBlob* byte_code = ShaderByteCode(m_device, source_code, options, include_policy, ECS_SHADER_COMPUTE);
		ComputeShader shader = CreateComputeShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary);
		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	InputLayout Graphics::CreateInputLayout(
		Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, 
		VertexShader vertex_shader,
		bool temporary
	)
	{
		InputLayout layout;
		HRESULT result;
		
		result = m_device->CreateInputLayout(
			descriptor.buffer, 
			descriptor.size, 
			vertex_shader.byte_code.buffer, 
			vertex_shader.byte_code.size,
			&layout.layout
		);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating input layout failed", true);
		
		m_allocator->Deallocate_ts(vertex_shader.byte_code.buffer);
		vertex_shader.byte_code = { nullptr, 0 };
		if (!temporary) {
			AddInternalResource(layout);
		}
		return layout;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexBuffer Graphics::CreateVertexBuffer(size_t element_size, size_t element_count, bool temporary, D3D11_USAGE usage, unsigned int cpuFlags, unsigned int miscFlags)
	{
		VertexBuffer buffer;
		buffer.stride = element_size;
		buffer.size = element_count;
		D3D11_BUFFER_DESC vertex_buffer_descriptor = {};
		vertex_buffer_descriptor.ByteWidth = element_size * element_count;
		vertex_buffer_descriptor.Usage = usage;
		vertex_buffer_descriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertex_buffer_descriptor.CPUAccessFlags = cpuFlags;
		vertex_buffer_descriptor.MiscFlags = miscFlags;
		vertex_buffer_descriptor.StructureByteStride = buffer.stride;

		HRESULT result;
		result = m_device->CreateBuffer(&vertex_buffer_descriptor, nullptr, &buffer.buffer);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating vertex buffer failed", true);
		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexBuffer Graphics::CreateVertexBuffer(
		size_t element_size,
		size_t element_count, 
		const void* buffer_data, 
		bool temporary,
		D3D11_USAGE usage, 
		unsigned int cpuFlags,
		unsigned int miscFlags
	)
	{
		VertexBuffer buffer;
		buffer.stride = element_size;
		buffer.size = element_count;

		D3D11_BUFFER_DESC vertex_buffer_descriptor = {};
		vertex_buffer_descriptor.ByteWidth = UINT(element_size * element_count);
		vertex_buffer_descriptor.Usage = usage;
		vertex_buffer_descriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertex_buffer_descriptor.CPUAccessFlags = cpuFlags;
		vertex_buffer_descriptor.MiscFlags = miscFlags;
		vertex_buffer_descriptor.StructureByteStride = buffer.stride;

		D3D11_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = buffer_data;
		initial_data.SysMemPitch = vertex_buffer_descriptor.ByteWidth;

		HRESULT result;
		result = m_device->CreateBuffer(&vertex_buffer_descriptor, &initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating vertex buffer failed", true);
		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer Graphics::CreateConstantBuffer(
		size_t byte_size, 
		const void* buffer_data, 
		bool temporary,
		D3D11_USAGE usage, 
		unsigned int cpuAccessFlags, 
		unsigned int miscFlags
	)
	{
		ConstantBuffer buffer;
		HRESULT result;
		D3D11_BUFFER_DESC constant_buffer_descriptor = {};
		// Byte Width must be a multiple of 16, so padd the byte_size
		constant_buffer_descriptor.ByteWidth = function::align_pointer(byte_size, 16);
		constant_buffer_descriptor.Usage = usage;
		constant_buffer_descriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constant_buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		constant_buffer_descriptor.MiscFlags = miscFlags;
		constant_buffer_descriptor.StructureByteStride = 0u;

		D3D11_SUBRESOURCE_DATA initial_data_constant = {};
		initial_data_constant.pSysMem = buffer_data;

		result = m_device->CreateBuffer(&constant_buffer_descriptor, &initial_data_constant, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating pixel constant buffer failed", true);
		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer Graphics::CreateConstantBuffer(size_t byte_size, bool temporary, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
	{
		ConstantBuffer buffer;
		HRESULT result;
		D3D11_BUFFER_DESC constant_buffer_descriptor = {};
		// Byte Width must be a multiple of 16, so padd the byte_size
		constant_buffer_descriptor.ByteWidth = function::align_pointer(byte_size, 16);
		constant_buffer_descriptor.Usage = usage;
		constant_buffer_descriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constant_buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		constant_buffer_descriptor.MiscFlags = miscFlags;
		constant_buffer_descriptor.StructureByteStride = 0u;

		result = m_device->CreateBuffer(&constant_buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating pixel constant buffer failed", true);
		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StandardBuffer Graphics::CreateStandardBuffer(
		size_t element_size,
		size_t element_count,
		bool temporary,
		D3D11_USAGE usage, 
		unsigned int cpuAccessFlags,
		unsigned int miscFlags
	)
	{
		StandardBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		buffer_descriptor.MiscFlags = miscFlags;
		buffer_descriptor.StructureByteStride = 0u;
		buffer_descriptor.Usage = usage;

		result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);
		buffer.count = element_count;
		
		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StandardBuffer Graphics::CreateStandardBuffer(
		size_t element_size,
		size_t element_count,
		const void* data, 
		bool temporary,
		D3D11_USAGE usage,
		unsigned int cpuAccessFlags,
		unsigned int miscFlags
	)
	{
		StandardBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		buffer_descriptor.MiscFlags = miscFlags;
		buffer_descriptor.StructureByteStride = 0u;
		buffer_descriptor.Usage = usage;

		D3D11_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = data;

		result = m_device->CreateBuffer(&buffer_descriptor, &initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);
		buffer.count = element_count;

		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StructuredBuffer Graphics::CreateStructuredBuffer(
		size_t element_size,
		size_t element_count,
		bool temporary,
		D3D11_USAGE usage,
		unsigned int cpuAccessFlags, 
		unsigned int miscFlags
	)
	{
		StructuredBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		buffer_descriptor.MiscFlags = miscFlags | D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_descriptor.StructureByteStride = element_size;
		buffer_descriptor.Usage = usage;

		result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating structred buffer failed.", true);

		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StructuredBuffer Graphics::CreateStructuredBuffer(
		size_t element_size,
		size_t element_count, 
		const void* data, 
		bool temporary,
		D3D11_USAGE usage, 
		unsigned int cpuAccessFlags, 
		unsigned int miscFlags
	)
	{
		StructuredBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		buffer_descriptor.MiscFlags = miscFlags | D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_descriptor.StructureByteStride = element_size;
		buffer_descriptor.Usage = usage;

		D3D11_SUBRESOURCE_DATA initial_data;
		initial_data.pSysMem = data;

		result = m_device->CreateBuffer(&buffer_descriptor, &initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Structred Buffer failed.", true);

		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------------------------------

	UABuffer Graphics::CreateUABuffer(
		size_t element_size, 
		size_t element_count, 
		bool temporary,
		D3D11_USAGE usage, 
		unsigned int cpuAccessFlags,
		unsigned int miscFlags
	)
	{
		UABuffer buffer;

		buffer.element_count = element_count;
		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};
		
		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = usage;
		buffer_descriptor.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		buffer_descriptor.MiscFlags = miscFlags;
		buffer_descriptor.StructureByteStride = 0u;

		result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UA Buffer failed.", true);

		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UABuffer Graphics::CreateUABuffer(
		size_t element_size, 
		size_t element_count,
		const void* data, 
		bool temporary,
		D3D11_USAGE usage, 
		unsigned int cpuAccessFlags,
		unsigned int miscFlags
	)
	{
		UABuffer buffer;

		buffer.element_count = element_count;
		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = element_size * element_count;
		buffer_descriptor.Usage = usage;
		buffer_descriptor.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		buffer_descriptor.MiscFlags = miscFlags;
		buffer_descriptor.StructureByteStride = 0u;

		D3D11_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = data;

		result = m_device->CreateBuffer(&buffer_descriptor, &initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UA Buffer failed.", true);

		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndirectBuffer Graphics::CreateIndirectBuffer(bool temporary)
	{
		IndirectBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		// For draw indexed instaced we need:
		// 
		// UINT IndexCountPerInstance,
		// UINT InstanceCount,
	    // UINT StartIndexLocation,
		// INT  BaseVertexLocation,
		// UINT StartInstanceLocation

		// and for draw instanced:
		//  UINT VertexCountPerInstance,
		//	UINT InstanceCount,
		// UINT StartVertexLocation,
		// UINT StartInstanceLocation
		
		// So a total of max 20 bytes needed

		buffer_descriptor.ByteWidth = 20;
		buffer_descriptor.Usage = D3D11_USAGE_DEFAULT;
		buffer_descriptor.CPUAccessFlags = 0;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		buffer_descriptor.StructureByteStride = 0;

		result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Indirect Buffer failed.", true);
		
		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	AppendStructuredBuffer Graphics::CreateAppendStructuredBuffer(size_t element_size, size_t element_count, bool temporary) {
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
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Append Buffer failed.", true);

		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConsumeStructuredBuffer Graphics::CreateConsumeStructuredBuffer(size_t element_size, size_t element_count, bool temporary) {
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
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Consume Buffer failed.", true);

		if (!temporary) {
			AddInternalResource(buffer);
		}
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	SamplerState Graphics::CreateSamplerState(const D3D11_SAMPLER_DESC& descriptor, bool temporary)
	{
		SamplerState state;
		HRESULT result = m_device->CreateSamplerState(&descriptor, &state.sampler);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Constructing sampler state failed!", true);
		if (!temporary) {
			AddInternalResource(state);
		}
		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<bool assert>
	void RemoveResourceFromTrackingImplementation(Graphics* graphics, void* resource) {
		// Spin lock while the resizing is finished
		while (graphics->m_internal_resources_lock.is_locked()) {
			_mm_pause();
		}

		graphics->m_internal_resources_reader_count.fetch_add(1, ECS_RELAXED);

		unsigned int resource_count = graphics->m_internal_resources.size.load(ECS_ACQUIRE);
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
		ID3D11Resource* resource = GetResource(view);
		FreeResource(view);
		// It doesn't matter what the type is - only the interface
		FreeResource(Texture2D(resource));
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeUAView(UAView view)
	{
		ID3D11Resource* resource = GetResource(view);
		FreeResource(view);
		FreeResource(Texture2D(resource));
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeRenderView(RenderTargetView view)
	{
		ID3D11Resource* resource = GetResource(view);
		FreeResource(view);
		FreeResource(Texture2D(resource));
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CreateInitialRenderTargetView(bool gamma_corrected)
	{
		// gain access to the texture subresource of the back buffer
		HRESULT result;
		Microsoft::WRL::ComPtr<ID3D11Resource> back_buffer;
		result = m_swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), &back_buffer);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Acquiring buffer resource failed", true);

		if (gamma_corrected) {
			D3D11_RENDER_TARGET_VIEW_DESC descriptor;
			descriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			descriptor.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			descriptor.Texture2D.MipSlice = 0;
			result = m_device->CreateRenderTargetView(back_buffer.Get(), &descriptor, &m_target_view.target);
		}
		else {
			result = m_device->CreateRenderTargetView(back_buffer.Get(), nullptr, &m_target_view.target);
		}

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed", true);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CreateInitialDepthStencilView()
	{
		HRESULT result;
		// creating depth texture
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_texture;
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

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating depth texture failed", true);

		// creating depth stencil texture view
		D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_descriptor = {};
		depth_view_descriptor.Format = DXGI_FORMAT_D32_FLOAT;
		depth_view_descriptor.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depth_view_descriptor.Texture2D.MipSlice = 0u;

		result = m_device->CreateDepthStencilView(depth_texture.Get(), &depth_view_descriptor, &m_depth_stencil_view.view);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating depth stencil view failed", true);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsContext* Graphics::CreateDeferredContext(UINT context_flags)
	{
		GraphicsContext* context;
		HRESULT result = m_device->CreateDeferredContext(0, &context);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Failed to create deferred context.", true);

		AddInternalResource(context);
		return context;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture1D Graphics::CreateTexture(const GraphicsTexture1DDescriptor* ecs_descriptor, bool temporary)
	{
		Texture1D resource;
		D3D11_TEXTURE1D_DESC descriptor = { 0 };
		descriptor.Format = ecs_descriptor->format;
		descriptor.ArraySize = ecs_descriptor->array_size;
		descriptor.Width = ecs_descriptor->width;
		descriptor.MipLevels = ecs_descriptor->mip_levels;
		descriptor.Usage = ecs_descriptor->usage;
		descriptor.CPUAccessFlags = ecs_descriptor->cpu_flag;
		descriptor.BindFlags = ecs_descriptor->bind_flag;
		descriptor.MiscFlags = ecs_descriptor->misc_flag;

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

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 1D failed!", true);
		if (!temporary) {
			AddInternalResource(resource);
		}
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture2D Graphics::CreateTexture(const GraphicsTexture2DDescriptor* ecs_descriptor, bool temporary)
	{
		Texture2D resource;
		D3D11_TEXTURE2D_DESC descriptor = { 0 };
		descriptor.Format = ecs_descriptor->format;
		descriptor.Width = ecs_descriptor->size.x;
		descriptor.Height = ecs_descriptor->size.y;
		descriptor.MipLevels = ecs_descriptor->mip_levels;
		descriptor.ArraySize = ecs_descriptor->array_size;
		descriptor.BindFlags = ecs_descriptor->bind_flag;
		descriptor.CPUAccessFlags = ecs_descriptor->cpu_flag;
		descriptor.MiscFlags = ecs_descriptor->misc_flag;
		descriptor.SampleDesc.Count = ecs_descriptor->sample_count;
		descriptor.SampleDesc.Quality = ecs_descriptor->sampler_quality;
		descriptor.Usage = (D3D11_USAGE)ecs_descriptor->usage;

		HRESULT result;

		if (ecs_descriptor->mip_data.buffer == nullptr) {
			result = m_device->CreateTexture2D(&descriptor, nullptr, &resource.tex);
		}
		else {
			D3D11_SUBRESOURCE_DATA subresource_data[32];
			memset(subresource_data, 0, sizeof(D3D11_SUBRESOURCE_DATA) * 32);
			unsigned int height = ecs_descriptor->size.y;
			for (size_t index = 0; index < ecs_descriptor->mip_data.size; index++) {
				subresource_data[index].pSysMem = ecs_descriptor->mip_data[index].buffer;
				subresource_data[index].SysMemPitch = ecs_descriptor->mip_data[index].size / height;
				subresource_data[index].SysMemSlicePitch = 0;

				height = function::Select<unsigned int>(height == 1, 1, height >> 1);
			}
			result = m_device->CreateTexture2D(&descriptor, subresource_data, &resource.tex);
		}

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 2D failed!", true);
		if (!temporary) {
			AddInternalResource(resource);
		}
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture3D Graphics::CreateTexture(const GraphicsTexture3DDescriptor* ecs_descriptor, bool temporary)
	{
		Texture3D resource;
		D3D11_TEXTURE3D_DESC descriptor = { 0 };
		descriptor.Format = ecs_descriptor->format;
		descriptor.Width = ecs_descriptor->size.x;
		descriptor.Height = ecs_descriptor->size.y;
		descriptor.Depth = ecs_descriptor->size.z;
		descriptor.MipLevels = ecs_descriptor->mip_levels;
		descriptor.Usage = ecs_descriptor->usage;
		descriptor.BindFlags = ecs_descriptor->bind_flag;
		descriptor.CPUAccessFlags = ecs_descriptor->cpu_flag;
		descriptor.MiscFlags = ecs_descriptor->misc_flag;

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

				height = function::Select<unsigned int>(height == 1, 1, height >> 1);
				depth = function::Select<unsigned int>(depth == 1, 1, depth >> 1);
			}
			result = m_device->CreateTexture3D(&descriptor, subresource_data, &resource.tex);
		}

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 3D failed!", true);
		if (!temporary) {
			AddInternalResource(resource);
		}
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	TextureCube Graphics::CreateTexture(const GraphicsTextureCubeDescriptor* ecs_descriptor, bool temporary) {
		TextureCube resource;
		D3D11_TEXTURE2D_DESC descriptor = { 0 };
		descriptor.Format = ecs_descriptor->format;
		descriptor.Width = ecs_descriptor->size.x;
		descriptor.Height = ecs_descriptor->size.y;
		descriptor.MipLevels = ecs_descriptor->mip_levels;
		descriptor.ArraySize = 6;
		descriptor.BindFlags = ecs_descriptor->bind_flag;
		descriptor.CPUAccessFlags = ecs_descriptor->cpu_flag;
		descriptor.MiscFlags = ecs_descriptor->misc_flag | D3D11_RESOURCE_MISC_TEXTURECUBE;
		descriptor.SampleDesc.Count = 1;
		descriptor.SampleDesc.Quality = 0;
		descriptor.Usage = ecs_descriptor->usage;

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

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture Cube failed!", true);
		if (!temporary) {
			AddInternalResource(resource);
		}
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	ResourceView Graphics::CreateTextureShaderView(
		Texture1D texture,
		DXGI_FORMAT format, 
		unsigned int most_detailed_mip,
		unsigned int mip_levels,
		bool temporary
	)
	{
		if (format == DXGI_FORMAT_FORCE_UINT) {
			D3D11_TEXTURE1D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = descriptor.Format;
		}

		ResourceView view;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = format;
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		descriptor.Texture1D.MipLevels = mip_levels;
		descriptor.Texture1D.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 1D Shader View failed.", true);
		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture1D texture, bool temporary)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"CReating Texture 1D Shader Entire View failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderView(
		Texture2D texture, 
		DXGI_FORMAT format, 
		unsigned int most_detailed_mip, 
		unsigned int mip_levels,
		bool temporary
	)
	{
		if (format == DXGI_FORMAT_FORCE_UINT) {
			D3D11_TEXTURE2D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = descriptor.Format;
		}

		ResourceView view;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = format;
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipLevels = mip_levels;
		descriptor.Texture2D.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 2D Shader View.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture2D texture, bool temporary)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 2D Shader Entire View failed.", true);
		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderView(
		Texture3D texture,
		DXGI_FORMAT format,
		unsigned int most_detailed_mip,
		unsigned int mip_levels,
		bool temporary
	)
	{
		if (format == DXGI_FORMAT_FORCE_UINT) {
			D3D11_TEXTURE3D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = descriptor.Format;
		}

		ResourceView view;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = format;
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		descriptor.Texture3D.MipLevels = mip_levels;
		descriptor.Texture3D.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 3D Shader View failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture3D texture, bool temporary)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 3D Shader Entire View failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderView(TextureCube texture, DXGI_FORMAT format, unsigned int most_detailed_mip, unsigned int mip_levels, bool temporary)
	{
		ResourceView view;

		if (format == DXGI_FORMAT_FORCE_UINT) {
			D3D11_TEXTURE2D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = descriptor.Format;
		}

		ResourceView component;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = format;
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		descriptor.TextureCube.MipLevels = mip_levels;
		descriptor.TextureCube.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &component.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture Cube Shader View.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(TextureCube texture, bool temporary)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture Cube Shader Entire View failed.", true);
		
		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateBufferView(StandardBuffer buffer, DXGI_FORMAT format, bool temporary)
	{
		ResourceView view;

		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor;
		descriptor.Format = format;
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		descriptor.Buffer.ElementOffset = 0;
		descriptor.Buffer.ElementWidth = buffer.count;
		HRESULT result = m_device->CreateShaderResourceView(buffer.buffer, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer view failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateBufferView(StructuredBuffer buffer, bool temporary)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(buffer.buffer, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating structured buffer view failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RenderTargetView Graphics::CreateRenderTargetView(Texture2D texture, unsigned int mip_level, bool temporary)
	{
		HRESULT result;

		D3D11_TEXTURE2D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		RenderTargetView view;
		D3D11_RENDER_TARGET_VIEW_DESC descriptor;
		descriptor.Format = texture_desc.Format;
		descriptor.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipSlice = mip_level;
		result = m_device->CreateRenderTargetView(texture.tex, &descriptor, &view.target);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed!", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RenderTargetView Graphics::CreateRenderTargetView(TextureCube cube, TextureCubeFace face, unsigned int mip_level, bool temporary)
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
		result = m_device->CreateRenderTargetView(cube.tex, &view_descriptor, &view.target);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed!", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DepthStencilView Graphics::CreateDepthStencilView(Texture2D texture, bool temporary)
	{
		HRESULT result;

		DepthStencilView view;
		result = m_device->CreateDepthStencilView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed!", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(UABuffer buffer, DXGI_FORMAT format, unsigned int first_element, bool temporary)
	{
		UAView view;

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor;
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = first_element;
		descriptor.Buffer.NumElements = buffer.element_count - first_element;
		descriptor.Buffer.Flags = 0;
		descriptor.Format = format;

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from UABuffer failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(IndirectBuffer buffer, bool temporary) {
		UAView view;

		// Max 5 uints can be written
		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor;
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = 0;
		descriptor.Buffer.NumElements = 5;
		descriptor.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		descriptor.Format = DXGI_FORMAT_R32_TYPELESS;

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Indirect Buffer failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(AppendStructuredBuffer buffer, bool temporary) {
		UAView view;

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor;
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = 0;
		descriptor.Buffer.NumElements = buffer.element_count;
		descriptor.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		descriptor.Format = DXGI_FORMAT_UNKNOWN;

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Append Buffer failed.", true);

		if (!temporary) {
			AddInternalResource(buffer);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(ConsumeStructuredBuffer buffer, bool temporary) {
		UAView view;

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = 0;
		descriptor.Buffer.NumElements = buffer.element_count;
		descriptor.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		descriptor.Format = DXGI_FORMAT_UNKNOWN;

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Consume Buffer failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture1D texture, unsigned int mip_slice, bool temporary) {
		UAView view;

		D3D11_TEXTURE1D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		descriptor.Texture1D.MipSlice = mip_slice;
		descriptor.Format = texture_desc.Format;

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Texture 1D failed.", true);
		
		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture2D texture, unsigned int mip_slice, bool temporary) {
		UAView view;

		D3D11_TEXTURE2D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipSlice = mip_slice;
		descriptor.Format = texture_desc.Format;

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Texture 2D failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture3D texture, unsigned int mip_slice, bool temporary) {
		UAView view;

		D3D11_TEXTURE3D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		descriptor.Texture3D.MipSlice = mip_slice;
		descriptor.Format = texture_desc.Format;

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Texture 3D failed.", true);

		if (!temporary) {
			AddInternalResource(view);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RasterizerState Graphics::CreateRasterizerState(const D3D11_RASTERIZER_DESC& descriptor, bool temporary)
	{
		RasterizerState state;

		HRESULT result = m_device->CreateRasterizerState(&descriptor, &state.state);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Rasterizer state failed.", true);

		if (!temporary) {
			AddInternalResource(state);
		}

		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DepthStencilState Graphics::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& descriptor, bool temporary)
	{
		DepthStencilState state;

		HRESULT result = m_device->CreateDepthStencilState(&descriptor, &state.state);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Rasterizer state failed.", true);

		if (!temporary) {
			AddInternalResource(state);
		}

		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	BlendState Graphics::CreateBlendState(const D3D11_BLEND_DESC& descriptor, bool temporary)
	{
		BlendState state;

		HRESULT result = m_device->CreateBlendState(&descriptor, &state.state);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Rasterizer state failed.", true);

		if (!temporary) {
			AddInternalResource(state);
		}
		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeMesh(const Mesh& mesh)
	{
		if (mesh.name != nullptr) {
			m_allocator->Deallocate(mesh.name);
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
		if (submesh.name != nullptr) {
			m_allocator->Deallocate(submesh.name);
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
		m_context->ClearRenderTargetView(m_target_view.target, color);
		m_context->ClearDepthStencilView(m_depth_stencil_view.view, D3D11_CLEAR_DEPTH, 1.0f, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableAlphaBlending() {
		DisableAlphaBlending(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableAlphaBlending(GraphicsContext* context) {
		context->OMSetBlendState(m_blend_disabled.state, nullptr, 0xFFFFFFFF);
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
		depth_desc.DepthEnable = FALSE;
		depth_desc.StencilEnable = FALSE;
		
		ID3D11DepthStencilState* state;
		HRESULT result = m_device->CreateDepthStencilState(&depth_desc, &state);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Failed to disable depth", true);
		
		context->OMSetDepthStencilState(state, 0);
		state->Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableCulling(GraphicsContext* context, bool wireframe)
	{
		D3D11_RASTERIZER_DESC rasterizer_desc = {};
		rasterizer_desc.AntialiasedLineEnable = TRUE;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FillMode = wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.FrontCounterClockwise = FALSE;

		ID3D11RasterizerState* state;
		HRESULT result = m_device->CreateRasterizerState(&rasterizer_desc, &state);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Failed to disable culling.", true);
		context->RSSetState(state);
		state->Release();
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
		context->OMSetBlendState(m_blend_enabled.state, nullptr, 0xFFFFFFFF);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableDepth()
	{
		EnableDepth(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableDepth(GraphicsContext* context)
	{
		D3D11_DEPTH_STENCIL_DESC depth_desc;
		depth_desc.DepthEnable = TRUE;
		depth_desc.DepthFunc = D3D11_COMPARISON_LESS;
		depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depth_desc.StencilEnable = FALSE;

		ID3D11DepthStencilState* state;
		HRESULT result = m_device->CreateDepthStencilState(&depth_desc, &state);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Failed to enable depth.", true);

		context->OMSetDepthStencilState(state, 0);
		state->Release();
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
		rasterizer_desc.AntialiasedLineEnable = TRUE;
		rasterizer_desc.CullMode = D3D11_CULL_BACK;
		rasterizer_desc.FillMode = wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.FrontCounterClockwise = FALSE;

		ID3D11RasterizerState* state;
		HRESULT result = m_device->CreateRasterizerState(&rasterizer_desc, &state);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Failed to disable culling.", true);
		context->RSSetState(state);
		state->Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	CommandList Graphics::FinishCommandList(bool restore_state) {
		return ECSEngine::FinishCommandList(m_context, restore_state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsDevice* Graphics::GetDevice() {
		return m_device;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsContext* Graphics::GetContext() {
		return m_context;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsContext* Graphics::GetDeferredContext()
	{
		return m_deferred_context;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapBuffer(ID3D11Buffer* buffer, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapBuffer(buffer, m_context, map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapTexture(Texture1D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, m_context, map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapTexture(Texture2D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, m_context, map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapTexture(Texture3D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, m_context, map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CommitInternalResourcesToBeFreed()
	{
		for (int32_t index = 0; index < (int32_t)m_internal_resources.size.load(ECS_RELAXED); index++) {
			if (m_internal_resources[index].is_deleted.load(ECS_RELAXED)) {
				m_internal_resources.RemoveSwapBack(index);
				index--;
			}
		}
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

	void* Graphics::GetAllocatorBufferTs(size_t size)
	{
		return m_allocator->Allocate_ts(size);
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
		m_target_view.target->GetDesc(&descriptor);
		m_context->ClearState();
		m_target_view.target->Release();
		m_depth_stencil_view.view->Release();

		HRESULT result = m_swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Resizing swap chain failed!", true);

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

		CreateInitialRenderTargetView(descriptor.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

		ResizeViewport(0.0f, 0.0f, width, height);
		BindRenderTargetViewFromInitialViews(m_context);
		EnableDepth(m_context);

		CreateInitialDepthStencilView();

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

	void Graphics::SwapBuffers(unsigned int sync_interval) {
		HRESULT result;
		result = m_swap_chain->Present(sync_interval, 0);
		if (FAILED(result)) {
			if (result == DXGI_ERROR_DEVICE_REMOVED) {
				ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Swapping buffers failed. Device was removed.", true);
			}
			else {
				ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Swapping buffers failed.", false);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	void Graphics::SetNewSize(HWND hWnd, unsigned int width, unsigned int height)
	{
		m_window_size.x = width;
		m_window_size.y = height;
		ResizeSwapChainSize(hWnd, width, height);
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

	void Graphics::UpdateBuffer(ID3D11Buffer* buffer, const void* data, size_t data_size, D3D11_MAP map_type, unsigned int map_flags, unsigned int subresource_index)
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

	InputLayout Graphics::ReflectVertexShaderInput(VertexShader shader, Stream<char> source_code, bool temporary)
	{
		constexpr size_t MAX_INPUT_FIELDS = 128;
		D3D11_INPUT_ELEMENT_DESC _element_descriptors[MAX_INPUT_FIELDS];
		CapacityStream<D3D11_INPUT_ELEMENT_DESC> element_descriptors(_element_descriptors, 0, MAX_INPUT_FIELDS);

		constexpr size_t NAME_POOL_SIZE = 8192;
		void* allocation = ECS_STACK_ALLOC(NAME_POOL_SIZE);
		CapacityStream<char> name_pool(allocation, 0, NAME_POOL_SIZE);
		bool success = m_shader_reflection->ReflectVertexShaderInputSource(source_code, element_descriptors, name_pool);

		if (!success) {
			return {};
		}
		InputLayout layout = CreateInputLayout(element_descriptors, shader, temporary);
		return layout;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderBuffers(Stream<char> source_code, CapacityStream<ShaderReflectedBuffer>& buffers) {
		constexpr size_t NAME_POOL_SIZE = 8192;
		char* name_allocation = (char*)ECS_STACK_ALLOC(NAME_POOL_SIZE);
		CapacityStream<char> name_pool(name_allocation, 0, NAME_POOL_SIZE);

		bool success = m_shader_reflection->ReflectShaderBuffersSource(source_code, buffers, name_pool);
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
		char* name_allocation = (char*)ECS_STACK_ALLOC(NAME_POOL_SIZE);
		CapacityStream<char> name_pool(name_allocation, 0, NAME_POOL_SIZE);

		bool success = m_shader_reflection->ReflectShaderTexturesSource(source_code, textures, name_pool);
		if (!success) {
			//_freea(name_allocation);
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
		//_freea(name_allocation);
		return true;
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

	void DestroyGraphics(Graphics* graphics)
	{
		// Walk through the list of resources that the graphic stores and free them
		// Use a switch case to handle the pointer identification
		
		// Unbind the resource bound to the pipeline
		graphics->m_context->ClearState();

		// First commit the releases
		graphics->CommitInternalResourcesToBeFreed();

		enum ECS_GRAPHICS_RESOURCE_TYPE {
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

		unsigned int resource_count = graphics->m_internal_resources.size.load(ECS_RELAXED);

#define CASE(type, pointer_type) case type: { pointer_type* resource = (pointer_type*)pointer; ref_count = resource->Release(); } break;
#define CASE2(type0, type1, pointer_type) case type0: case type1: { pointer_type* resource = (pointer_type*)pointer; ref_count = resource->Release(); } break;
#define CASE3(type0, type1, type2, pointer_type) case type0: case type1: case type2: { pointer_type* resource = (pointer_type*)pointer; ref_count = resource->Release(); } break;

		unsigned int ref_count = 0;
		for (unsigned int index = 0; index < resource_count; index++) {
			void* pointer = graphics->m_internal_resources[index].interface_pointer;
			ECS_GRAPHICS_RESOURCE_TYPE type = (ECS_GRAPHICS_RESOURCE_TYPE)graphics->m_internal_resources[index].type;

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

		if (!graphics->m_copied_graphics) {
			for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
				graphics->m_shader_helpers[index].vertex.Release();
				graphics->m_shader_helpers[index].pixel.Release();
				graphics->m_shader_helpers[index].input_layout.Release();
				graphics->m_shader_helpers[index].pixel_sampler.Release();
			}
		}

		graphics->m_context->Flush();

		unsigned int count = graphics->m_device->Release();
		count = graphics->m_deferred_context->Release();
		count = graphics->m_context->Release();
		// Deallocate the buffer for the resource tracking
		graphics->m_allocator->Deallocate(graphics->m_internal_resources.buffer);

		if (!graphics->m_copied_graphics) {
			count = graphics->m_blend_disabled.Release();
			count = graphics->m_blend_enabled.Release();
			count = graphics->m_depth_stencil_view.Release();
			count = graphics->m_target_view.Release();
			count = graphics->m_swap_chain->Release();

			// Deallocate the shader helper array
			graphics->m_allocator->Deallocate(graphics->m_shader_helpers.buffer);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexBuffer(VertexBuffer buffer, GraphicsContext* context, UINT slot) {
		const UINT offset = 0u;
		context->IASetVertexBuffers(slot, 1u, &buffer.buffer, &buffer.stride, &offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexBuffers(Stream<VertexBuffer> buffers, GraphicsContext* context, UINT start_slot) {
		ID3D11Buffer* v_buffers[16];
		UINT strides[16];
		UINT offsets[16] = { 0 };
		for (size_t index = 0; index < buffers.size; index++) {
			v_buffers[index] = buffers[index].buffer;
			strides[index] = buffers[index].stride;
		}
		context->IASetVertexBuffers(start_slot, buffers.size, v_buffers, strides, offsets);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindIndexBuffer(IndexBuffer index_buffer, GraphicsContext* context) {
		DXGI_FORMAT format = DXGI_FORMAT_R32_UINT;
		format = (DXGI_FORMAT)function::Select<unsigned int>(index_buffer.int_size == 1, DXGI_FORMAT_R8_UINT, format);
		format = (DXGI_FORMAT)function::Select<unsigned int>(index_buffer.int_size == 2, DXGI_FORMAT_R16_UINT, format);

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

	void BindPixelConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot) {
		context->PSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		UINT start_slot
	) {
		context->PSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot) {
		context->VSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		UINT start_slot
	) {
		ID3D11Buffer* d_buffers[16];
		for (size_t index = 0; index < buffers.size; index++) {
			d_buffers[index] = buffers[index].buffer;
		}
		context->VSSetConstantBuffers(start_slot, buffers.size, d_buffers);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDomainConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot) {
		context->DSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDomainConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		UINT start_slot
	) {
		context->DSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindHullConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot) {
		context->HSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindHullConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		UINT start_slot
	) {
		context->HSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindGeometryConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot) {
		context->GSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindGeometryConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		UINT start_slot
	) {
		context->GSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot) {
		context->CSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeConstantBuffers(
		Stream<ConstantBuffer> buffers,
		GraphicsContext* context,
		UINT start_slot
	) {
		context->CSSetConstantBuffers(start_slot, buffers.size, (ID3D11Buffer**)buffers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindTopology(Topology topology, GraphicsContext* context)
	{
		context->IASetPrimitiveTopology(topology.value);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelResourceView(ResourceView component, GraphicsContext* context, UINT slot) {
		context->PSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT start_slot) {
		context->PSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexResourceView(ResourceView component, GraphicsContext* context, UINT slot) {
		context->VSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindVertexResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT start_slot) {
		context->VSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDomainResourceView(ResourceView component, GraphicsContext* context, UINT slot) {
		context->DSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDomainResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT start_slot) {
		context->DSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindHullResourceView(ResourceView component, GraphicsContext* context, UINT slot) {
		context->HSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindHullResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT start_slot) {
		context->HSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindGeometryResourceView(ResourceView component, GraphicsContext* context, UINT slot) {
		context->GSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindGeometryResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT start_slot) {
		context->GSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeResourceView(ResourceView component, GraphicsContext* context, UINT slot) {
		context->CSSetShaderResources(slot, 1, &component.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT start_slot) {
		context->CSSetShaderResources(start_slot, views.size, (ID3D11ShaderResourceView**)views.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindSamplerStates(Stream<SamplerState> samplers, GraphicsContext* context, UINT start_slot)
	{
		context->PSSetSamplers(start_slot, samplers.size, (ID3D11SamplerState**)samplers.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindSamplerState(SamplerState sampler, GraphicsContext* context, UINT slot)
	{
		context->PSSetSamplers(slot, 1u, &sampler.sampler);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindPixelUAView(UAView view, GraphicsContext* context, UINT start_slot)
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

	void BindPixelUAViews(Stream<UAView> views, GraphicsContext* context, UINT start_slot)
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

	void BindComputeUAView(UAView view, GraphicsContext* context, UINT start_slot)
	{
		context->CSSetUnorderedAccessViews(start_slot, 1u, &view.view, nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindComputeUAViews(Stream<UAView> views, GraphicsContext* context, UINT start_slot)
	{
		context->CSSetUnorderedAccessViews(start_slot, views.size, (ID3D11UnorderedAccessView**)views.buffer,  nullptr);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindRasterizerState(RasterizerState state, GraphicsContext* context)
	{
		context->RSSetState(state.state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void BindDepthStencilState(DepthStencilState state, GraphicsContext* context, UINT stencil_ref)
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
		context->OMSetRenderTargets(1u, &render_view.target, depth_stencil_view.view);
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
		
		// Bind the domain shader if it is not null
		if (material.domain_shader.shader != nullptr) {
			BindDomainShader(material.domain_shader, context);
			
			// Bind domain resources
			if (material.dc_buffer_count > 0) {
				BindDomainConstantBuffers(Stream<ConstantBuffer>(material.dc_buffers, material.dc_buffer_count), context);
			}

			if (material.domain_texture_count > 0) {
				BindDomainResourceViews(Stream<ResourceView>(material.domain_textures, material.domain_texture_count), context);
			}
		}

		// Bind the hull shader if it is not null
		if (material.hull_shader.shader != nullptr) {
			BindHullShader(material.hull_shader, context);

			// Bind hull resources
			if (material.hc_buffer_count > 0) {
				BindHullConstantBuffers(Stream<ConstantBuffer>(material.hc_buffers, material.hc_buffer_count), context);
			}

			if (material.hull_texture_count > 0) {
				BindHullResourceViews(Stream<ResourceView>(material.hull_textures, material.hull_texture_count), context);
			}
		}

		// Bind the geometry shader if it not null 
		if (material.geometry_shader.shader != nullptr) {
			BindGeometryShader(material.geometry_shader, context);

			// Bind the geometry resources
			if (material.gc_buffer_count > 0) {
				BindGeometryConstantBuffers(Stream<ConstantBuffer>(material.gc_buffers, material.gc_buffer_count), context);
			}

			if (material.geometry_texture_count > 0) {
				BindGeometryResourceViews(Stream<ResourceView>(material.geometry_textures, material.geometry_texture_count), context);
			}
		}

		// Bind vertex constant buffers
		if (material.vc_buffer_count > 0) {
			BindVertexConstantBuffers(Stream<ConstantBuffer>(material.vc_buffers, material.vc_buffer_count), context);
		}

		// Bind pixel constant buffers
		if (material.pc_buffer_count > 0) {
			BindPixelConstantBuffers(Stream<ConstantBuffer>(material.pc_buffers, material.pc_buffer_count), context);
		}

		// Bind vertex textures
		if (material.vertex_texture_count > 0) {
			BindVertexResourceViews(Stream<ResourceView>(material.vertex_textures, material.vertex_texture_count), context);
		}

		// Bind pixel textures
		if (material.pixel_texture_count > 0) {
			BindPixelResourceViews(Stream<ResourceView>(material.pixel_textures, material.pixel_texture_count), context);
		}

		// Unordered views
		if (material.unordered_view_count > 0) {
			BindPixelUAViews(Stream<UAView>(material.unordered_views, material.unordered_view_count), context);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void ClearRenderTarget(RenderTargetView view, GraphicsContext* context, ColorFloat color)
	{
		context->ClearRenderTargetView(view.target, (float*)&color);
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

	CommandList FinishCommandList(GraphicsContext* context, bool restore_state) {
		ID3D11CommandList* list = nullptr;
		HRESULT result;
		result = context->FinishCommandList(restore_state, &list);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating command list failed!", true);
		return list;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineRasterizerState GetRasterizerState(GraphicsContext* context)
	{
		GraphicsPipelineRasterizerState state;

		context->RSGetState(&state.rasterizer_state.state);

		return state;
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

	GraphicsPipelineDepthStencilState GetDepthStencilState(GraphicsContext* context)
	{
		GraphicsPipelineDepthStencilState state;

		context->OMGetDepthStencilState(&state.depth_stencil_state.state, &state.stencil_ref);

		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineBlendState GetBlendState(GraphicsContext* context)
	{
		GraphicsPipelineBlendState state;

		context->OMGetBlendState(&state.blend_state.state, state.blend_factors, &state.sample_mask);

		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Resource>
	D3D11_MAPPED_SUBRESOURCE MapResourceInternal(Resource resource, GraphicsContext* context, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags, const wchar_t* error_string) {
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		if constexpr (std::is_same_v<Resource, Texture1D> || std::is_same_v<Resource, Texture2D> || std::is_same_v<Resource, Texture3D>) {
			result = context->Map(resource.tex, subresource_index, map_type, map_flags, &mapped_subresource);
		}
		else {
			result = context->Map(resource, subresource_index, map_type, map_flags, &mapped_subresource);
		}
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, error_string, true);
		return mapped_subresource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* MapBuffer(
		ID3D11Buffer* buffer,
		GraphicsContext* context,
		D3D11_MAP map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	)
	{
		return MapResourceInternal(buffer, context, map_type, subresource_index, map_flags, L"Mapping a buffer failed.").pData;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	D3D11_MAPPED_SUBRESOURCE MapBufferEx(
		ID3D11Buffer* buffer,
		GraphicsContext* context,
		D3D11_MAP map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	) {
		return MapResourceInternal(buffer, context, map_type, subresource_index, map_flags, L"Mapping a buffer failed.");
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	void* MapTexture(
		Texture texture,
		GraphicsContext* context,
		D3D11_MAP map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	)
	{
		return MapResourceInternal(texture, context, map_type, subresource_index, map_flags, L"Mapping a texture failed.").pData;
	}

	// Cringe bug from intellisense that makes all the file full of errors when in reality everything is fine; instantiations must
	// be unrolled manually
	ECS_TEMPLATE_FUNCTION(void*, MapTexture, Texture1D, GraphicsContext*, D3D11_MAP, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(void*, MapTexture, Texture2D, GraphicsContext*, D3D11_MAP, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(void*, MapTexture, Texture3D, GraphicsContext*, D3D11_MAP, unsigned int, unsigned int);

	// ------------------------------------------------------------------------------------------------------------------------

	// It must be unmapped manually
	template<typename Texture>
	D3D11_MAPPED_SUBRESOURCE MapTextureEx(
		Texture texture,
		GraphicsContext* context,
		D3D11_MAP map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	) {
		return MapResourceInternal(texture, context, map_type, subresource_index, map_flags, L"Mapping a texture failed.");
	}

	// Cringe bug from intellisense that makes all the file full of errors when in reality everything is fine; instantiations must
	// be unrolled manually
	ECS_TEMPLATE_FUNCTION(D3D11_MAPPED_SUBRESOURCE, MapTextureEx, Texture1D, GraphicsContext*, D3D11_MAP, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(D3D11_MAPPED_SUBRESOURCE, MapTextureEx, Texture2D, GraphicsContext*, D3D11_MAP, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(D3D11_MAPPED_SUBRESOURCE, MapTextureEx, Texture3D, GraphicsContext*, D3D11_MAP, unsigned int, unsigned int);

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
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreRasterizerState(GraphicsContext* context, GraphicsPipelineRasterizerState rasterizer_state)
	{
		context->RSSetState(rasterizer_state.rasterizer_state.state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreDepthStencilState(GraphicsContext* context, GraphicsPipelineDepthStencilState depth_stencil_state)
	{
		context->OMSetDepthStencilState(depth_stencil_state.depth_stencil_state.state, depth_stencil_state.stencil_ref);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Resource>
	Resource TransferGPUResource(Resource resource, GraphicsDevice* device)
	{
		ID3D11Resource* dx_resource = GetResource(resource);

		// Acquire the DXGIResource interface from the DX resource
		IDXGIResource* dxgi_resource;
		HRESULT result = dx_resource->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgi_resource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Getting the DXGI resource from shared resource failed.", true);

		// Query interface updates the reference count, release it to maintain invariance
		dxgi_resource->Release();

		HANDLE handle;
		result = dxgi_resource->GetSharedHandle(&handle);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Acquiring a handle for a shared resource failed.", true);

		ID3D11Resource* new_dx_resource;
		result = device->OpenSharedResource(handle, __uuidof(ID3D11Resource), (void**)&new_dx_resource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Obtaining shared resource from handle failed.", true);

		// In order to construct the new type, use constexpr if to properly construct the type
		auto get_buffer_resource = [](ID3D11Resource* resource) {
			ID3D11Buffer* buffer;
			HRESULT result = resource->QueryInterface(__uuidof(ID3D11Buffer), (void**)&buffer);
			ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting a shared resource to buffer failed.", true);
			
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

	template<typename Texture>
	void UpdateTexture(
		Texture texture,
		const void* data,
		size_t data_size,
		GraphicsContext* context,
		D3D11_MAP map_type,
		unsigned int map_flags,
		unsigned int subresource_index
	) {
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(texture.Interface(), subresource_index, map_type, map_flags, &mapped_subresource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Updating texture failed.", true);

		memcpy(mapped_subresource.pData, data, data_size);
		context->Unmap(texture.Interface(), subresource_index);
	}

	// Cringe bug from intellisense that makes all the file full of errors when in reality everything is fine; instantiations must
	// be unrolled manually
	ECS_TEMPLATE_FUNCTION(void, UpdateTexture, Texture1D, const void*, size_t, GraphicsContext*, D3D11_MAP, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(void, UpdateTexture, Texture2D, const void*, size_t, GraphicsContext*, D3D11_MAP, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(void, UpdateTexture, Texture3D, const void*, size_t, GraphicsContext*, D3D11_MAP, unsigned int, unsigned int);

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	void UpdateTextureResource(Texture texture, const void* data, size_t data_size, GraphicsContext* context, unsigned int subresource_index) {
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(texture.Interface(), subresource_index, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Updating texture failed.", true);

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
		D3D11_MAP map_type,
		unsigned int map_flags,
		unsigned int subresource_index
	)
	{
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(buffer, subresource_index, map_type, map_flags, &mapped_subresource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Updating buffer failed.", true);

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
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Updating buffer resource failed.", true);

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

	void FreeMaterial(Graphics* graphics, const Material& material) {
		// Release the input layout
		graphics->FreeResource(material.layout);

		// Release the mandatory shaders - pixel and vertex.
		graphics->FreeResource(material.vertex_shader);
		graphics->FreeResource(material.pixel_shader);

		// Release the mandatory shaders
		if (material.domain_shader.shader != nullptr) {
			graphics->FreeResource(material.domain_shader);
		}
		if (material.hull_shader.shader != nullptr) {
			graphics->FreeResource(material.hull_shader);
		}
		if (material.geometry_shader.shader != nullptr) {
			graphics->FreeResource(material.geometry_shader);
		}

		// Release the constant buffers - each one needs to be released
		// When they were assigned, their reference count was incremented
		// in order to avoid checking aliased buffers
		for (size_t index = 0; index < material.vc_buffer_count; index++) {
			graphics->FreeResource(material.vc_buffers[index]);
		}

		for (size_t index = 0; index < material.pc_buffer_count; index++) {
			graphics->FreeResource(material.pc_buffers[index]);
		}

		for (size_t index = 0; index < material.dc_buffer_count; index++) {
			graphics->FreeResource(material.dc_buffers[index]);
		}

		for (size_t index = 0; index < material.hc_buffer_count; index++) {
			graphics->FreeResource(material.hc_buffers[index]);
		}

		for (size_t index = 0; index < material.gc_buffer_count; index++) {
			graphics->FreeResource(material.gc_buffers[index]);
		}

		// Release the resource views - each one needs to be released
		// Same as constant buffers, their reference count was incremented upon
		// assignment alongside the resource that they view
		for (size_t index = 0; index < material.vertex_texture_count; index++) {
			graphics->FreeResourceView(material.vertex_textures[index]);
		}

		for (size_t index = 0; index < material.pixel_texture_count; index++) {
			graphics->FreeResourceView(material.pixel_textures[index]);
		}

		for (size_t index = 0; index < material.domain_texture_count; index++) {
			graphics->FreeResourceView(material.domain_textures[index]);
		}

		for (size_t index = 0; index < material.hull_texture_count; index++) {
			graphics->FreeResourceView(material.hull_textures[index]);
		}

		for (size_t index = 0; index < material.geometry_texture_count; index++) {
			graphics->FreeResourceView(material.geometry_textures[index]);
		}

		// Release the UAVs
		for (size_t index = 0; index < material.unordered_view_count; index++) {
			graphics->FreeUAView(material.unordered_views[index]);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Mesh MeshesToSubmeshes(Graphics* graphics, Stream<Mesh> meshes, Submesh* submeshes, unsigned int misc_flags)
	{
		unsigned int* mask = (unsigned int*)ECS_STACK_ALLOC(meshes.size * sizeof(unsigned int));
		Stream<unsigned int> sequence(mask, meshes.size);
		function::MakeSequence(Stream<unsigned int>(mask, meshes.size));

		Mesh mesh = MeshesToSubmeshes(graphics, meshes, submeshes, sequence, misc_flags);
		return mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Mesh MeshesToSubmeshes(Graphics* graphics, Stream<Mesh> meshes, Submesh* submeshes, Stream<unsigned int> mesh_mask, unsigned int misc_flags) {
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
				D3D11_USAGE_DEFAULT,
				0,
				misc_flags
			);
		}

		IndexBuffer new_index_buffer = graphics->CreateIndexBuffer(meshes[0].index_buffer.int_size, index_buffer_size, false, D3D11_USAGE_DEFAULT, 0, misc_flags);

		// all vertex buffers must have the same size - so a single offset suffices
		unsigned int vertex_buffer_offset = 0;
		unsigned int index_buffer_offset = 0;

		// Copy the vertex buffer and index buffer submeshes
		for (size_t index = 0; index < mesh_mask.size; index++) {
			Mesh* current_mesh = &meshes[mesh_mask[index]];
			// If the mesh has a name, simply repoint the submesh pointer - do not deallocate and reallocate again
			if (current_mesh->name != nullptr) {
				submeshes[index].name = current_mesh->name;
			}
			else {
				submeshes[index].name = nullptr;
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
			unsigned int* staging_data = (unsigned int*)graphics->MapBuffer(staging_buffer.buffer, D3D11_MAP_READ_WRITE);

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