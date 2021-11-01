#include "ecspch.h"
#include "Graphics.h"
#include "../Utilities/FunctionInterfaces.h"
#include "GraphicsHelpers.h"
#include "../Utilities/Function.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")

namespace ECSEngine {

	constexpr const char* SHADER_ENTRY_POINT = "main";

	ECS_MICROSOFT_WRL;

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

	constexpr const char* SHADER_COMPILE_TARGET[] = {
		"vs_4_0",
		"vs_5_0",
		"ps_4_0",
		"ps_5_0",
		"ds_4_0",
		"ds_5_0",
		"hs_4_0",
		"hs_5_0",
		"gs_4_0",
		"gs_5_0",
		"cs_4_0",
		"cs_5_0"
	};

#define CreateShaderFromBlob(shader_type, blob) HRESULT create_result; \
	create_result = m_device->Create##shader_type##Shader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader.shader); \
	ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating " TEXT(STRING(shader_type)) L" shader failed.", true); \
\
	blob->Release(); 

#define CreateShaderByteCode(shader_type) shader_type##Shader shader; \
	shader.path = { nullptr,  0 }; \
	HRESULT result; \
	ID3DBlob* blob; \
\
	ECS_TEMP_STRING(null_terminated_path, 256); \
	null_terminated_path.Copy(byte_code); \
	null_terminated_path[byte_code.size] = L'\0'; \
\
	result = D3DReadFileToBlob(null_terminated_path.buffer, &blob); \
	ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Reading " TEXT(STRING(shader_type)) L" shader path failed.", true); \
\
	CreateShaderFromBlob(shader_type, blob)

#define CreateShaderByteCodeWithPath(shader_type, path) shader_type##Shader shader; \
	HRESULT result; \
	ID3DBlob* blob; \
\
	result = D3DReadFileToBlob(path, &blob); \
	ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Reading " TEXT(STRING(shader_type)) L" shader path failed.", true); \
\
	CreateShaderFromBlob(shader_type, blob)

#define CreateShaderByteCodeAndSourceCode(shader_type) wchar_t* allocation = (wchar_t*)m_allocator->Allocate_ts((source_code.size + 1) * sizeof(wchar_t), alignof(wchar_t)); \
	memcpy(allocation, source_code.buffer, source_code.size * sizeof(wchar_t)); \
	allocation[source_code.size] = L'\0'; \
	CreateShaderByteCodeWithPath(shader_type, allocation) \
	shader.path = { allocation, source_code.size }; 

#define CreateShaderFromSourceCode(shader_type, shader_order) shader_type##Shader shader; \
\
	wchar_t* allocation = (wchar_t*)m_allocator->Allocate_ts(sizeof(wchar_t) * (source_code.size + 1), alignof(wchar_t)); \
	memcpy(allocation, source_code.buffer, sizeof(wchar_t)* source_code.size); \
	allocation[source_code.size] = L'\0'; \
	shader.path = { allocation, source_code.size }; \
\
	D3D_SHADER_MACRO macros[32]; \
	memcpy(macros, options.macros.buffer, sizeof(const char*) * 2 * options.macros.size); \
	macros[options.macros.size] = { NULL, NULL }; \
\
	const char* target = SHADER_COMPILE_TARGET[shader_order + options.target]; \
\
	unsigned int compile_flags = 0; \
	compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_DEBUG), D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0); \
	compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_LOWEST), D3DCOMPILE_OPTIMIZATION_LEVEL0, 0); \
	compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_LOW), D3DCOMPILE_OPTIMIZATION_LEVEL1, 0); \
	compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_HIGH), D3DCOMPILE_OPTIMIZATION_LEVEL2, 0); \
	compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_HIGHEST), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0); \
\
	ID3DBlob* blob; \
	HRESULT result = D3DCompileFromFile(source_code.buffer, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, SHADER_ENTRY_POINT, target, compile_flags, 0, &blob, &error_message_blob); \
	ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Compiling " TEXT(STRING(shader_type)) L" shader failed.", true); \
	CreateShaderFromBlob(shader_type, blob); 


	Graphics::Graphics(HWND hWnd, const GraphicsDescriptor* descriptor) 
		: m_target_view(nullptr), m_device(nullptr), m_context(nullptr), m_swap_chain(nullptr), m_allocator(descriptor->allocator),
		m_bound_targets(m_bound_render_targets, 1)
	{
		constexpr float resize_factor = 1.0f;
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

		result = m_device->CreateDeferredContext(0, m_deferred_context.GetAddressOf());

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating deferred context failed!", true);

		CreateInitialRenderTargetView(descriptor->gamma_corrected);

		ComPtr<ID3D11DepthStencilState> depth_stencil_state = {};
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

		result = m_device->CreateBlendState(&blend_descriptor, &m_blend_enabled);
		
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating blend state failed!", true);

		blend_descriptor.RenderTarget[0].BlendEnable = FALSE;

		result = m_device->CreateBlendState(&blend_descriptor, &m_blend_disabled);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating blend state failed!", true);

		m_context->OMSetBlendState(m_blend_disabled.Get(), nullptr, 0xFFFFFFFF);

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
		size_t memory_size = m_shader_reflection.MemoryOf();
		void* allocation = m_allocator->Allocate(memory_size);
		m_shader_reflection = ShaderReflection(allocation);

		D3D11_RASTERIZER_DESC rasterizer_state = { };
		rasterizer_state.AntialiasedLineEnable = TRUE;
		rasterizer_state.CullMode = D3D11_CULL_BACK;
		rasterizer_state.FillMode = D3D11_FILL_SOLID;
		rasterizer_state.DepthClipEnable = TRUE;
		rasterizer_state.FrontCounterClockwise = FALSE;

		ID3D11RasterizerState* state;
		m_device->CreateRasterizerState(&rasterizer_state, &state);

		m_context->RSSetState(state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexBuffer(VertexBuffer buffer, UINT slot) {
		ECSEngine::BindVertexBuffer(buffer, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexBuffers(Stream<VertexBuffer> buffers, UINT start_slot) {
		ECSEngine::BindVertexBuffers(buffers, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindIndexBuffer(IndexBuffer index_buffer) {
		ECSEngine::BindIndexBuffer(index_buffer, m_context.Get());
	}
	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindInputLayout(InputLayout input_layout) {
		ECSEngine::BindInputLayout(input_layout, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexShader(VertexShader shader) {
		ECSEngine::BindVertexShader(shader, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelShader(PixelShader shader) {
		ECSEngine::BindPixelShader(shader, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainShader(DomainShader shader) {
		ECSEngine::BindDomainShader(shader, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullShader(HullShader shader) {
		ECSEngine::BindHullShader(shader, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryShader(GeometryShader shader) {
		ECSEngine::BindGeometryShader(shader, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeShader(ComputeShader shader) {
		ECSEngine::BindComputeShader(shader, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindPixelConstantBuffer(buffer, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindPixelConstantBuffers(buffers, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindVertexConstantBuffer(buffer, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindVertexConstantBuffers(buffers, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindDomainConstantBuffer(buffer, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindDomainConstantBuffers(buffers, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindHullConstantBuffer(buffer, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindHullConstantBuffers(buffers, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindGeometryConstantBuffer(buffer, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindGeometryConstantBuffers(buffers, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeConstantBuffer(ConstantBuffer buffer, UINT slot) {
		ECSEngine::BindComputeConstantBuffer(buffer, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		ECSEngine::BindComputeConstantBuffers(buffers, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindTopology(Topology topology)
	{
		ECSEngine::BindTopology(topology, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindPixelResourceView(component, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindPixelResourceViews(views, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindVertexResourceView(component, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindVertexResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindVertexResourceViews(views, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindDomainResourceView(component, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindDomainResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindDomainResourceViews(views, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindHullResourceView(component, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindHullResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindHullResourceViews(views, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindGeometryResourceView(component, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindGeometryResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindGeometryResourceViews(views, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeResourceView(ResourceView component, UINT slot) {
		ECSEngine::BindComputeResourceView(component, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeResourceViews(Stream<ResourceView> views, UINT start_slot) {
		ECSEngine::BindComputeResourceViews(views, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindSamplerState(SamplerState sampler, UINT slot)
	{
		ECSEngine::BindSamplerState(sampler, m_context.Get(), slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindSamplerStates(Stream<SamplerState> samplers, UINT start_slot)
	{
		ECSEngine::BindSamplerStates(samplers, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view)
	{
		ECSEngine::BindRenderTargetView(render_view, depth_stencil_view, m_context.Get());
		m_bound_targets[0] = render_view;
		m_bound_targets.size = 1;
		m_current_depth_stencil = depth_stencil_view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelUAView(UAView view, UINT start_slot)
	{
		m_context->OMSetRenderTargetsAndUnorderedAccessViews(
			D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, 
			nullptr, 
			nullptr,
			m_bound_targets.size,
			1u,
			&view.view, 
			nullptr
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeUAView(UAView view, UINT start_slot)
	{
		ECSEngine::BindComputeUAView(view, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindPixelUAViews(Stream<UAView> views, UINT start_slot) {
		m_context->OMSetRenderTargetsAndUnorderedAccessViews(
			D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, 
			nullptr, 
			nullptr,
			m_bound_targets.size, 
			views.size, 
			(ID3D11UnorderedAccessView**)views.buffer, 
			nullptr
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindComputeUAViews(Stream<UAView> views, UINT start_slot) {
		ECSEngine::BindComputeUAViews(views, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRenderTargetViewFromInitialViews()
	{
		m_context->OMSetRenderTargets(1u, &m_target_view.target, m_depth_stencil_view.view);
		m_bound_targets[0] = m_target_view;
		m_bound_targets.size = 1;
		m_current_depth_stencil = m_depth_stencil_view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRenderTargetViewFromInitialViews(GraphicsContext* context)
	{
		context->OMSetRenderTargets(1u, &m_target_view.target, m_depth_stencil_view.view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth)
	{
		ECSEngine::BindViewport(top_left_x, top_left_y, width, height, min_depth, max_depth, m_context.Get());
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
		ECSEngine::BindMesh(mesh, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindMesh(const Mesh& mesh, Stream<ECS_MESH_INDEX> mapping)
	{
		ECSEngine::BindMesh(mesh, m_context.Get(), mapping);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindMaterial(const Material& material) {
		ECSEngine::BindMaterial(material, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename IntType>
	IndexBuffer CreateIndexBufferImpl(Graphics* graphics, Stream<IntType> indices) {
		IndexBuffer component;
		component.count = indices.size;
		component.int_size = sizeof(IntType);

		HRESULT result;
		D3D11_BUFFER_DESC index_buffer_descriptor = {};
		index_buffer_descriptor.ByteWidth = UINT(component.count * sizeof(IntType));
		index_buffer_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		index_buffer_descriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
		index_buffer_descriptor.CPUAccessFlags = 0;
		index_buffer_descriptor.MiscFlags = 0;
		index_buffer_descriptor.StructureByteStride = sizeof(IntType);

		D3D11_SUBRESOURCE_DATA initial_data_index = {};
		initial_data_index.pSysMem = indices.buffer;

		result = graphics->m_device->CreateBuffer(&index_buffer_descriptor, &initial_data_index, &component.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating index buffer failed", true);
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(Stream<unsigned char> indices)
	{ 
		return CreateIndexBufferImpl(this, indices);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(Stream<unsigned short> indices)
	{
		return CreateIndexBufferImpl(this, indices);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(Stream<unsigned int> indices)
	{
		return CreateIndexBufferImpl(this, indices);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndexBuffer Graphics::CreateIndexBuffer(size_t int_size, size_t element_count, D3D11_USAGE usage, unsigned int cpu_access)
	{
		IndexBuffer component;
		component.count = element_count;
		component.int_size = int_size;

		HRESULT result;
		D3D11_BUFFER_DESC index_buffer_descriptor = {};
		index_buffer_descriptor.ByteWidth = UINT(component.count * int_size);
		index_buffer_descriptor.Usage = usage;
		index_buffer_descriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
		index_buffer_descriptor.CPUAccessFlags = cpu_access;
		index_buffer_descriptor.MiscFlags = 0;
		index_buffer_descriptor.StructureByteStride = int_size;

		result = m_device->CreateBuffer(&index_buffer_descriptor, nullptr, &component.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating index buffer failed", true);
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	PixelShader Graphics::CreatePixelShader(Stream<wchar_t> byte_code)
	{
		CreateShaderByteCode(Pixel);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	PixelShader Graphics::CreatePixelShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code)
	{
		CreateShaderByteCodeAndSourceCode(Pixel);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	PixelShader Graphics::CreatePixelShaderFromSource(Stream<wchar_t> source_code, ShaderFromSourceOptions options)
	{
		ID3DBlob* error_message_blob;
		CreateShaderFromSourceCode(Pixel, ECS_SHADER_PIXEL);
		if (error_message_blob != nullptr) {
			error_message_blob->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	// The vertex shader must not have the blob released
	VertexShader Graphics::CreateVertexShader(Stream<wchar_t> byte_code) {
		VertexShader shader;
	
		ECS_TEMP_STRING(null_terminated_path, 256);
		null_terminated_path.Copy(byte_code);
		null_terminated_path[byte_code.size] = L'\0';

		HRESULT result;

		result = D3DReadFileToBlob(null_terminated_path.buffer, &shader.byte_code);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Reading vertex shader location failed", true);

		result = m_device->CreateVertexShader(
			shader.byte_code->GetBufferPointer(),
			shader.byte_code->GetBufferSize(),
			nullptr,
			&shader.shader
		);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating vertex shader failed", true);

		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	// The vertex shader must not have its blob released
	VertexShader Graphics::CreateVertexShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code) {
		VertexShader shader;

		wchar_t* allocation = (wchar_t*)m_allocator->Allocate_ts(sizeof(wchar_t) * (source_code.size + 1), alignof(wchar_t));
		memcpy(allocation, source_code.buffer, sizeof(wchar_t) * source_code.size);
		allocation[source_code.size] = L'\0';
		shader.path = { allocation, source_code.size };

		HRESULT result;

		result = D3DReadFileToBlob(shader.path.buffer, &shader.byte_code);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Reading vertex shader location failed", true);

		result = m_device->CreateVertexShader(
			shader.byte_code->GetBufferPointer(),
			shader.byte_code->GetBufferSize(),
			nullptr,
			&shader.shader
		);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating vertex shader failed", true);

		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	// The vertex shader must not have its blob released
	VertexShader Graphics::CreateVertexShaderFromSource(Stream<wchar_t> source_code, ShaderFromSourceOptions options) {
		VertexShader shader; 
			
		wchar_t* allocation = (wchar_t*)m_allocator->Allocate_ts(sizeof(wchar_t) * (source_code.size + 1), alignof(wchar_t)); 
		memcpy(allocation, source_code.buffer, sizeof(wchar_t) * source_code.size); 
		allocation[source_code.size] = L'\0'; 
		shader.path = { allocation, source_code.size }; 
			
		D3D_SHADER_MACRO macros[32]; 
		memcpy(macros, options.macros.buffer, sizeof(const char*) * 2 * options.macros.size); 
		macros[options.macros.size] = { NULL, NULL }; 
			
		const char* target = SHADER_COMPILE_TARGET[ECS_SHADER_VERTEX + options.target]; 
			
		unsigned int compile_flags = 0; 
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_DEBUG), D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0); 
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_LOWEST), D3DCOMPILE_OPTIMIZATION_LEVEL0, 0); 
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_LOW), D3DCOMPILE_OPTIMIZATION_LEVEL1, 0); 
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_HIGH), D3DCOMPILE_OPTIMIZATION_LEVEL2, 0); 
		compile_flags |= function::Select(function::HasFlag(options.compile_flags, ECS_SHADER_COMPILE_OPTIMIZATION_HIGHEST), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0); 
			
		ID3DBlob* blob; 
		HRESULT result = D3DCompileFromFile(source_code.buffer, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, SHADER_ENTRY_POINT, target, compile_flags, 0, &blob, nullptr); 
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Compiling Vertex shader failed.", true); 
		
		result = m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Vertex shader failed.", true);

		shader.byte_code = blob;

		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader Graphics::CreateDomainShader(Stream<wchar_t> byte_code)
	{
		CreateShaderByteCode(Domain);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader Graphics::CreateDomainShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code)
	{
		CreateShaderByteCodeAndSourceCode(Domain);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader Graphics::CreateDomainShaderFromSource(Stream<wchar_t> source_code, ShaderFromSourceOptions options)
	{
		ID3DBlob* error_message_blob;
		CreateShaderFromSourceCode(Domain, ECS_SHADER_DOMAIN);
		if (error_message_blob != nullptr) {
			error_message_blob->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HullShader Graphics::CreateHullShader(Stream<wchar_t> byte_code) {
		CreateShaderByteCode(Hull);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HullShader Graphics::CreateHullShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code) {
		CreateShaderByteCodeAndSourceCode(Hull);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HullShader Graphics::CreateHullShaderFromSource(Stream<wchar_t> source_code, ShaderFromSourceOptions options) {
		ID3DBlob* error_message_blob;
		CreateShaderFromSourceCode(Hull, ECS_SHADER_HULL);
		if (error_message_blob != nullptr) {
			error_message_blob->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GeometryShader Graphics::CreateGeometryShader(Stream<wchar_t> byte_code) {
		CreateShaderByteCode(Geometry);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GeometryShader Graphics::CreateGeometryShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code) {
		CreateShaderByteCodeAndSourceCode(Geometry);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GeometryShader Graphics::CreateGeometryShaderFromSource(Stream<wchar_t> source_code, ShaderFromSourceOptions options) {
		ID3DBlob* error_message_blob;
		CreateShaderFromSourceCode(Geometry, ECS_SHADER_GEOMETRY);
		if (error_message_blob != nullptr) {
			error_message_blob->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ComputeShader Graphics::CreateComputeShader(Stream<wchar_t> byte_code) {
		CreateShaderByteCode(Compute);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ComputeShader Graphics::CreateComputeShader(Stream<wchar_t> byte_code, Stream<wchar_t> source_code) {
		CreateShaderByteCodeAndSourceCode(Compute);
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ComputeShader Graphics::CreateComputeShaderFromSource(Stream<wchar_t> source_code, ShaderFromSourceOptions options) {
		ID3DBlob* error_message_blob;
		CreateShaderFromSourceCode(Compute, ECS_SHADER_COMPUTE);
		if (error_message_blob != nullptr) {
			error_message_blob->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	InputLayout Graphics::CreateInputLayout(
		Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, 
		VertexShader vertex_shader
	)
	{
		InputLayout component;
		HRESULT result;
		auto byte_code = vertex_shader.byte_code;
		
		const void* byte_code_ptr = byte_code->GetBufferPointer();
		size_t byte_code_size = byte_code->GetBufferSize();
		
		result = m_device->CreateInputLayout(
			descriptor.buffer, 
			descriptor.size, 
			byte_code->GetBufferPointer(), 
			byte_code->GetBufferSize(),
			&component.layout
		);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating input layout failed", true);
		vertex_shader.ReleaseByteCode();
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexBuffer Graphics::CreateVertexBuffer(size_t element_size, size_t element_count, D3D11_USAGE usage, unsigned int cpuFlags, unsigned int miscFlags)
	{
		VertexBuffer component;
		component.stride = element_size;
		component.size = element_count;
		D3D11_BUFFER_DESC vertex_buffer_descriptor = {};
		vertex_buffer_descriptor.ByteWidth = element_size * element_count;
		vertex_buffer_descriptor.Usage = usage;
		vertex_buffer_descriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertex_buffer_descriptor.CPUAccessFlags = cpuFlags;
		vertex_buffer_descriptor.MiscFlags = miscFlags;
		vertex_buffer_descriptor.StructureByteStride = component.stride;

		HRESULT result;
		result = m_device->CreateBuffer(&vertex_buffer_descriptor, nullptr, &component.buffer);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating vertex buffer failed", true);
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexBuffer Graphics::CreateVertexBuffer(size_t element_size, size_t element_count, const void* buffer, D3D11_USAGE usage, unsigned int cpuFlags, unsigned int miscFlags)
	{
		VertexBuffer component;
		component.stride = element_size;
		component.size = element_count;
		D3D11_BUFFER_DESC vertex_buffer_descriptor = {};
		vertex_buffer_descriptor.ByteWidth = UINT(element_size * element_count);
		vertex_buffer_descriptor.Usage = usage;
		vertex_buffer_descriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertex_buffer_descriptor.CPUAccessFlags = cpuFlags;
		vertex_buffer_descriptor.MiscFlags = miscFlags;
		vertex_buffer_descriptor.StructureByteStride = component.stride;

		D3D11_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = buffer;

		HRESULT result;
		result = m_device->CreateBuffer(&vertex_buffer_descriptor, &initial_data, &component.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating vertex buffer failed", true);
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer Graphics::CreateConstantBuffer(size_t byte_size, const void* buffer, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
	{
		ConstantBuffer component;
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
		initial_data_constant.pSysMem = buffer;

		result = m_device->CreateBuffer(&constant_buffer_descriptor, &initial_data_constant, &component.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating pixel constant buffer failed", true);
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer Graphics::CreateConstantBuffer(size_t byte_size, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
	{
		ConstantBuffer component;
		HRESULT result;
		D3D11_BUFFER_DESC constant_buffer_descriptor = {};
		// Byte Width must be a multiple of 16, so padd the byte_size
		constant_buffer_descriptor.ByteWidth = function::align_pointer(byte_size, 16);
		constant_buffer_descriptor.Usage = usage;
		constant_buffer_descriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constant_buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		constant_buffer_descriptor.MiscFlags = miscFlags;
		constant_buffer_descriptor.StructureByteStride = 0u;

		result = m_device->CreateBuffer(&constant_buffer_descriptor, nullptr, &component.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating pixel constant buffer failed", true);
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StandardBuffer Graphics::CreateStandardBuffer(size_t byte_size, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
	{
		StandardBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = byte_size;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		buffer_descriptor.MiscFlags = miscFlags;
		buffer_descriptor.StructureByteStride = 0u;
		buffer_descriptor.Usage = usage;

		result = m_device->CreateBuffer(&buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);

		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StandardBuffer Graphics::CreateStandardBuffer(size_t byte_size, const void* data, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
	{
		StandardBuffer buffer;

		HRESULT result;
		D3D11_BUFFER_DESC buffer_descriptor = {};

		buffer_descriptor.ByteWidth = byte_size;
		buffer_descriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		buffer_descriptor.CPUAccessFlags = cpuAccessFlags;
		buffer_descriptor.MiscFlags = miscFlags;
		buffer_descriptor.StructureByteStride = 0u;
		buffer_descriptor.Usage = usage;

		D3D11_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = data;

		result = m_device->CreateBuffer(&buffer_descriptor, &initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);

		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StructuredBuffer Graphics::CreateStructuredBuffer(size_t element_size, size_t element_count, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
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

		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	StructuredBuffer Graphics::CreateStructuredBuffer(size_t element_size, size_t element_count, const void* data, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
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

		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------------------------------

	UABuffer Graphics::CreateUABuffer(size_t element_size, size_t element_count, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
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

		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UABuffer Graphics::CreateUABuffer(size_t element_size, size_t element_count, const void* data, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
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

		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	IndirectBuffer Graphics::CreateIndirectBuffer()
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

		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	AppendStructuredBuffer Graphics::CreateAppendStructuredBuffer(size_t element_size, size_t element_count) {
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

		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConsumeStructuredBuffer Graphics::CreateConsumeStructuredBuffer(size_t element_size, size_t element_count) {
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

		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	SamplerState Graphics::CreateSamplerState(D3D11_SAMPLER_DESC descriptor)
	{
		SamplerState state;
		HRESULT result = m_device->CreateSamplerState(&descriptor, &state.sampler);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Constructing sampler state failed!", true);
		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CreateInitialRenderTargetView(bool gamma_corrected)
	{
		// gain access to the texture subresource of the back buffer
		HRESULT result;
		ComPtr<ID3D11Resource> back_buffer;
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
		ComPtr<ID3D11Texture2D> depth_texture;
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
		m_device->CreateDeferredContext(0, &context);
		return context;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture1D Graphics::CreateTexture(const GraphicsTexture1DDescriptor* ecs_descriptor)
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

		if (ecs_descriptor->initial_data == nullptr) {
			result = m_device->CreateTexture1D(&descriptor, nullptr, &resource.tex);
		}
		else {
			D3D11_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = ecs_descriptor->initial_data;
			result = m_device->CreateTexture1D(&descriptor, &subresource_data, &resource.tex);
		}

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 1D failed!", true);
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture2D Graphics::CreateTexture(const GraphicsTexture2DDescriptor* ecs_descriptor)
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
		descriptor.Usage = ecs_descriptor->usage;

		HRESULT result;

		if (ecs_descriptor->initial_data == nullptr) {
			result = m_device->CreateTexture2D(&descriptor, nullptr, &resource.tex);
		}
		else {
			D3D11_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = ecs_descriptor->initial_data;
			subresource_data.SysMemPitch = ecs_descriptor->memory_pitch;
			subresource_data.SysMemSlicePitch = ecs_descriptor->memory_slice_pitch;
			result = m_device->CreateTexture2D(&descriptor, &subresource_data, &resource.tex);
		}

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 2D failed!", true);
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture3D Graphics::CreateTexture(const GraphicsTexture3DDescriptor* ecs_descriptor)
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

		if (ecs_descriptor->initial_data == nullptr) {
			result = m_device->CreateTexture3D(&descriptor, nullptr, &resource.tex);
		}
		else {
			D3D11_SUBRESOURCE_DATA initial_data = {};
			initial_data.pSysMem = ecs_descriptor->initial_data;
			result = m_device->CreateTexture3D(&descriptor, &initial_data, &resource.tex);
		}


		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 3D failed!", true);
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	ResourceView Graphics::CreateTextureShaderView(
		Texture1D texture, 
		DXGI_FORMAT format, 
		unsigned int most_detailed_mip,
		unsigned int mip_levels
	)
	{
		if (format == DXGI_FORMAT_FORCE_UINT) {
			D3D11_TEXTURE1D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = descriptor.Format;
		}

		ResourceView component;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = format;
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		descriptor.Texture1D.MipLevels = mip_levels;
		descriptor.Texture1D.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &component.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 1D Shader View failed.", true);
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture1D texture)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"CReating Texture 1D Shader Entire View failed.", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderView(
		Texture2D texture, 
		DXGI_FORMAT format, 
		unsigned int most_detailed_mip, 
		unsigned int mip_levels
	)
	{
		if (format == DXGI_FORMAT_FORCE_UINT) {
			D3D11_TEXTURE2D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = descriptor.Format;
		}

		ResourceView component;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = format;
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipLevels = mip_levels;
		descriptor.Texture2D.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &component.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 2D Shader View.", true);
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture2D texture)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 2D Shader Entire View failed.", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderView(
		Texture3D texture,
		DXGI_FORMAT format,
		unsigned int most_detailed_mip,
		unsigned int mip_levels
	)
	{
		if (format == DXGI_FORMAT_FORCE_UINT) {
			D3D11_TEXTURE3D_DESC descriptor;
			texture.tex->GetDesc(&descriptor);
			format = descriptor.Format;
		}

		ResourceView component;
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor = { };
		descriptor.Format = format;
		descriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		descriptor.Texture3D.MipLevels = mip_levels;
		descriptor.Texture3D.MostDetailedMip = most_detailed_mip;

		HRESULT result;
		result = m_device->CreateShaderResourceView(texture.tex, &descriptor, &component.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 3D Shader View failed.", true);
		return component;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture3D texture)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 3D Shader Entire View failed.", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateBufferView(StandardBuffer buffer)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(buffer.buffer, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer view failed.", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateBufferView(StructuredBuffer buffer)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(buffer.buffer, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating structured buffer view failed.", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RenderTargetView Graphics::CreateRenderTargetView(Texture2D texture)
	{
		HRESULT result;

		RenderTargetView view;
		result = m_device->CreateRenderTargetView(texture.tex, nullptr, &view.target);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed!", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DepthStencilView Graphics::CreateDepthStencilView(Texture2D texture)
	{
		HRESULT result;

		DepthStencilView view;
		result = m_device->CreateDepthStencilView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed!", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(UABuffer buffer, DXGI_FORMAT format, unsigned int first_element)
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

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(IndirectBuffer buffer) {
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

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(AppendStructuredBuffer buffer) {
		UAView view;

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor;
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = 0;
		descriptor.Buffer.NumElements = buffer.element_count;
		descriptor.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		descriptor.Format = DXGI_FORMAT_UNKNOWN;

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Append Buffer failed.", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(ConsumeStructuredBuffer buffer) {
		UAView view;

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		descriptor.Buffer.FirstElement = 0;
		descriptor.Buffer.NumElements = buffer.element_count;
		descriptor.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		descriptor.Format = DXGI_FORMAT_UNKNOWN;

		HRESULT result = m_device->CreateUnorderedAccessView(buffer.buffer, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Consume Buffer failed.", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture1D texture, unsigned int mip_slice) {
		UAView view;

		D3D11_TEXTURE1D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		descriptor.Texture1D.MipSlice = mip_slice;
		descriptor.Format = texture_desc.Format;

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Texture 1D failed.", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture2D texture, unsigned int mip_slice) {
		UAView view;

		D3D11_TEXTURE2D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipSlice = mip_slice;
		descriptor.Format = texture_desc.Format;

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Texture 2D failed.", true);

		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture3D texture, unsigned int mip_slice) {
		UAView view;

		D3D11_TEXTURE3D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		descriptor.Texture3D.MipSlice = mip_slice;
		descriptor.Format = texture_desc.Format;

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Texture 3D failed.", true);

		return view;
	}

	Material Graphics::CreateMaterial(VertexShader v_shader, PixelShader p_shader, DomainShader d_shader, HullShader h_shader, GeometryShader g_shader)
	{
		constexpr size_t REFLECTED_BUFFER_COUNT = 32;
		constexpr size_t REFLECTED_TEXTURE_COUNT = 32;

		ShaderReflectedBuffer _reflected_buffers[REFLECTED_BUFFER_COUNT];
		ShaderReflectedTexture _reflected_textures[REFLECTED_TEXTURE_COUNT];

		CapacityStream<ShaderReflectedBuffer> reflected_buffers(_reflected_buffers, 0, REFLECTED_BUFFER_COUNT);
		CapacityStream<ShaderReflectedTexture> reflected_textures(_reflected_textures, 0, REFLECTED_TEXTURE_COUNT);

		// Reflect the vertex shader
		bool success = ReflectShaderBuffers(v_shader.path, reflected_buffers);
		success &= ReflectShaderTextures(v_shader.path, reflected_textures);
		if (!success) {
			return Material();
		}


		return Material();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename Shader>
	void Graphics::FreeShader(Shader shader) {
		shader.shader->Release();
		if (shader.path.buffer != nullptr) {
			m_allocator->Deallocate(shader.path.buffer);
		}
	}

	ECS_TEMPLATE_FUNCTION(void, Graphics::FreeShader, VertexShader);
	ECS_TEMPLATE_FUNCTION(void, Graphics::FreeShader, PixelShader);
	ECS_TEMPLATE_FUNCTION(void, Graphics::FreeShader, HullShader);
	ECS_TEMPLATE_FUNCTION(void, Graphics::FreeShader, DomainShader);
	ECS_TEMPLATE_FUNCTION(void, Graphics::FreeShader, GeometryShader);
	ECS_TEMPLATE_FUNCTION(void, Graphics::FreeShader, ComputeShader);

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ClearBackBuffer(float red, float green, float blue) {
		const float color[] = { red, green, blue, 1.0f };
		m_context->ClearRenderTargetView(m_target_view.target, color);
		m_context->ClearDepthStencilView(m_depth_stencil_view.view, D3D11_CLEAR_DEPTH, 1.0f, 0u);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableAlphaBlending() {
		DisableAlphaBlending(m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableAlphaBlending(GraphicsContext* context) {
		context->OMSetBlendState(m_blend_disabled.Get(), nullptr, 0xFFFFFFFF);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableDepth()
	{
		DisableDepth(m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableCulling(bool wireframe)
	{
		DisableCulling(m_context.Get(), wireframe);
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

	void Graphics::Draw(UINT vertex_count, UINT start_slot)
	{
		ECSEngine::Draw(vertex_count, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawIndexed(unsigned int count, UINT start_index, INT base_vertex_location) {
		ECSEngine::DrawIndexed(count, m_context.Get(), start_index, base_vertex_location);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawInstanced(unsigned int vertex_count, unsigned int instance_count, unsigned int start_slot) {
		ECSEngine::DrawInstanced(vertex_count, instance_count, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawIndexedInstanced(unsigned int index_count, unsigned int instance_count, unsigned int start_slot) {
		ECSEngine::DrawIndexedInstanced(index_count, instance_count, m_context.Get(), start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawIndexedInstancedIndirect(IndirectBuffer buffer) {
		ECSEngine::DrawIndexedInstancedIndirect(buffer, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawInstancedIndirect(IndirectBuffer buffer) {
		ECSEngine::DrawInstancedIndirect(buffer, m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableAlphaBlending() {
		EnableAlphaBlending(m_context.Get());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableAlphaBlending(GraphicsContext* context) {
		context->OMSetBlendState(m_blend_enabled.Get(), nullptr, 0xFFFFFFFF);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableDepth()
	{
		EnableDepth(m_context.Get());
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

	ID3D11CommandList* Graphics::FinishCommandList(bool restore_state) {
		return ECSEngine::FinishCommandList(m_context.Get(), restore_state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsDevice* Graphics::GetDevice() {
		return m_device.Get();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsContext* Graphics::GetContext() {
		return m_context.Get();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsContext* Graphics::GetDeferredContext()
	{
		return m_deferred_context.Get();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapBuffer(ID3D11Buffer* buffer, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapBuffer(buffer, m_context.Get(), map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapTexture(Texture1D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, m_context.Get(), map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapTexture(Texture2D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, m_context.Get(), map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapTexture(Texture3D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, m_context.Get(), map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::GetWindowSize(unsigned int& width, unsigned int& height) const {
		width = m_window_size.x;
		height = m_window_size.y;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::GetWindowSize(uint2& size) const
	{
		size.x = m_window_size.x;
		size.y = m_window_size.y;
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
		BindRenderTargetViewFromInitialViews(m_context.Get());
		EnableDepth(m_context.Get());

		CreateInitialDepthStencilView();

		DisableAlphaBlending(m_context.Get());
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

	void Graphics::UpdateBuffer(ID3D11Buffer* buffer, const void* data, size_t data_size, D3D11_MAP map_type, unsigned int map_flags, unsigned int subresource_index)
	{
		ECSEngine::UpdateBuffer(buffer, data, data_size, m_context.Get(), map_type, map_flags, subresource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UnmapBuffer(ID3D11Buffer* buffer, unsigned int resource_index)
	{
		ECSEngine::UnmapBuffer(buffer, m_context.Get(), resource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UnmapTexture(Texture1D texture, unsigned int resource_index) {
		ECSEngine::UnmapTexture(texture, m_context.Get(), resource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UnmapTexture(Texture2D texture, unsigned int resource_index) {
		ECSEngine::UnmapTexture(texture, m_context.Get(), resource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UnmapTexture(Texture3D texture, unsigned int resource_index) {
		ECSEngine::UnmapTexture(texture, m_context.Get(), resource_index);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	InputLayout Graphics::ReflectVertexShaderInput(VertexShader shader, Stream<wchar_t> path)
	{
		constexpr size_t MAX_INPUT_FIELDS = 128;
		D3D11_INPUT_ELEMENT_DESC _element_descriptors[MAX_INPUT_FIELDS];
		CapacityStream<D3D11_INPUT_ELEMENT_DESC> element_descriptors(_element_descriptors, 0, MAX_INPUT_FIELDS);

		constexpr size_t NAME_POOL_SIZE = 8192;
		void* allocation = ECS_STACK_ALLOC(NAME_POOL_SIZE);
		CapacityStream<char> name_pool(allocation, 0, NAME_POOL_SIZE);
		Stream<wchar_t> source_code_path = path;
		if (path.buffer == nullptr) {
			source_code_path = shader.path;
		}
		bool success = m_shader_reflection.ReflectVertexShaderInput(source_code_path, element_descriptors, name_pool);

		if (!success) {
			//_freea(allocation);
			return {};
		}
		InputLayout layout = CreateInputLayout(element_descriptors, shader);
		//_freea(allocation);
		return layout;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderBuffers(const wchar_t* path, CapacityStream<ShaderReflectedBuffer>& buffers)
	{
		return ReflectShaderBuffers(ToStream(path), buffers);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderBuffers(Stream<wchar_t> path, CapacityStream<ShaderReflectedBuffer>& buffers) {
		constexpr size_t NAME_POOL_SIZE = 8192;
		char* name_allocation = (char*)ECS_STACK_ALLOC(NAME_POOL_SIZE);
		CapacityStream<char> name_pool(name_allocation, 0, NAME_POOL_SIZE);

		bool success = m_shader_reflection.ReflectShaderBuffers(path, buffers, name_pool);
		if (!success) {
			//_freea(name_allocation);
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
		//_freea(name_allocation);
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderTextures(const wchar_t* path, CapacityStream<ShaderReflectedTexture>& textures)
	{
		return ReflectShaderTextures(ToStream(path), textures);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderTextures(Stream<wchar_t> path, CapacityStream<ShaderReflectedTexture>& textures)
	{
		constexpr size_t NAME_POOL_SIZE = 8192;
		char* name_allocation = (char*)ECS_STACK_ALLOC(NAME_POOL_SIZE);
		CapacityStream<char> name_pool(name_allocation, 0, NAME_POOL_SIZE);

		bool success = m_shader_reflection.ReflectShaderTextures(path, textures, name_pool);
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

	bool Graphics::ReflectVertexBufferMapping(const wchar_t* path, CapacityStream<ECS_MESH_INDEX>& mapping)
	{
		return m_shader_reflection.ReflectVertexBufferMapping(path, mapping);
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	bool Graphics::ReflectVertexBufferMapping(Stream<wchar_t> path, CapacityStream<ECS_MESH_INDEX>& mapping) {
		return m_shader_reflection.ReflectVertexBufferMapping(path, mapping);
	}


#pragma region Free functions

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
		BindVertexBuffers(Stream<VertexBuffer>(vertex_buffers, mesh.mapping_count), context);

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

	template<typename Resource>
	void CopyGraphicsResource(Resource destination, Resource source, GraphicsContext* context) {
		context->CopyResource(destination.Resource(), source.Resource());
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
		box.top = box.bottom = box.back = box.front = 0;
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
		box.back = box.front = 0;
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

	void Draw(unsigned int vertex_count, GraphicsContext* context, UINT start_slot)
	{
		context->Draw(vertex_count, start_slot);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawIndexed(unsigned int count, GraphicsContext* context, UINT start_index, INT base_vertex_location) {
		context->DrawIndexed(count, start_index, base_vertex_location);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawInstanced(
		unsigned int vertex_count,
		unsigned int instance_count,
		GraphicsContext* context,
		unsigned int vertex_offset,
		unsigned int instance_offset) 
	{
		context->DrawInstanced(vertex_count, instance_count, vertex_offset, instance_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawIndexedInstanced(
		unsigned int index_count,
		unsigned int instance_count,
		GraphicsContext* context,
		unsigned int vertex_offset,
		unsigned int index_offset,
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

	ID3D11CommandList* FinishCommandList(GraphicsContext* context, bool restore_state) {
		ID3D11CommandList* list = nullptr;
		HRESULT result;
		result = context->FinishCommandList(restore_state, &list);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating command list failed!", true);
		return list;
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
		GraphicsContext* context
	) {
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(buffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped_subresource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Updating buffer resource failed.", true);

		memcpy(mapped_subresource.pData, data, data_size);
		context->Unmap(buffer, 0u);
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
		material.layout.layout->Release();

		// Release the mandatory shaders - pixel and vertex.
		graphics->FreeShader(material.vertex_shader);
		graphics->FreeShader(material.pixel_shader);

		// Release the mandatory shaders
		if (material.domain_shader.shader != nullptr) {
			graphics->FreeShader(material.domain_shader);
		}
		if (material.hull_shader.shader != nullptr) {
			graphics->FreeShader(material.hull_shader);
		}
		if (material.geometry_shader.shader != nullptr) {
			graphics->FreeShader(material.geometry_shader);
		}

		// Release the constant buffers - each one needs to be released
		// When they were assigned, their reference count was incremented
		// in order to avoid checking aliased buffers
		for (size_t index = 0; index < material.vc_buffer_count; index++) {
			material.vc_buffers[index].buffer->Release();
		}

		for (size_t index = 0; index < material.pc_buffer_count; index++) {
			material.pc_buffers[index].buffer->Release();
		}

		for (size_t index = 0; index < material.dc_buffer_count; index++) {
			material.dc_buffers[index].buffer->Release();
		}

		for (size_t index = 0; index < material.hc_buffer_count; index++) {
			material.hc_buffers[index].buffer->Release();
		}

		for (size_t index = 0; index < material.gc_buffer_count; index++) {
			material.gc_buffers[index].buffer->Release();
		}

		// Release the resource views - each one needs to be released
		// Same as constant buffers, their reference count was incremented upon
		// assignment alongside the resource that they view
		for (size_t index = 0; index < material.vertex_texture_count; index++) {
			ReleaseShaderView(material.vertex_textures[index]);
		}

		for (size_t index = 0; index < material.pixel_texture_count; index++) {
			ReleaseShaderView(material.pixel_textures[index]);
		}

		for (size_t index = 0; index < material.domain_texture_count; index++) {
			ReleaseShaderView(material.domain_textures[index]);
		}

		for (size_t index = 0; index < material.hull_texture_count; index++) {
			ReleaseShaderView(material.hull_textures[index]);
		}

		for (size_t index = 0; index < material.geometry_texture_count; index++) {
			ReleaseShaderView(material.geometry_textures[index]);
		}

		// Release the UAVs
		for (size_t index = 0; index < material.unordered_view_count; index++) {
			ReleaseUAView(material.unordered_views[index]);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Mesh MeshesToSubmeshes(Graphics* graphics, Stream<Mesh> meshes, Submesh* submeshes)
	{
		unsigned int* mask = (unsigned int*)ECS_STACK_ALLOC(meshes.size * sizeof(unsigned int));
		Stream<unsigned int> sequence(mask, meshes.size);
		function::MakeSequence(Stream<unsigned int>(mask, meshes.size));

		Mesh mesh = MeshesToSubmeshes(graphics, meshes, submeshes, sequence);
		//_freea(mask);
		return mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Mesh MeshesToSubmeshes(Graphics* graphics, Stream<Mesh> meshes, Submesh* submeshes, Stream<unsigned int> mesh_mask) {
		Mesh result;

		// Accumulate the total vertex buffer size and index buffer size
		size_t vertex_buffer_size = 0;
		size_t index_buffer_size = 0;

		for (size_t index = 0; index < mesh_mask.size; index++) {
			vertex_buffer_size += meshes[mesh_mask[index]].vertex_buffers[0].size;
			index_buffer_size += meshes[mesh_mask[index]].index_buffer.count;
		}

		for (size_t index = 0; index < meshes.size; index++) {
			vertex_buffer_size += meshes[index].vertex_buffers[0].size;
			index_buffer_size += meshes[index].index_buffer.count;
		}

		// Create the new vertex buffers and the index buffer
		VertexBuffer new_vertex_buffers[ECS_MESH_BUFFER_COUNT];
		for (size_t index = 0; index < meshes[0].mapping_count; index++) {
			new_vertex_buffers[index] = graphics->CreateVertexBuffer(meshes[0].vertex_buffers[index].stride, vertex_buffer_size, D3D11_USAGE_DEFAULT, 0);
		}

		IndexBuffer new_index_buffer = graphics->CreateIndexBuffer(meshes[0].index_buffer.int_size, index_buffer_size);

		size_t mapping_count = meshes[0].mapping_count;
		// all vertex buffers must have the same size - so a single offset suffices
		unsigned int vertex_buffer_offset = 0;
		unsigned int index_buffer_offset = 0;

		// Copy the vertex buffer and index buffer submeshes
		for (size_t index = 0; index < meshes.size; index++) {
			submeshes[index] = { vertex_buffer_offset, index_buffer_offset, meshes[mesh_mask[index]].index_buffer.count };

			// Vertex buffers
			for (size_t buffer_index = 0; buffer_index < mapping_count; buffer_index++) {
				CopyBufferSubresource(
					new_vertex_buffers[buffer_index],
					vertex_buffer_offset,
					meshes[index].vertex_buffers[buffer_index],
					0,
					meshes[index].vertex_buffers[buffer_index].size,
					graphics->GetContext()
				);
			}
			// all vertex buffers must have the same size
			vertex_buffer_offset += meshes[index].vertex_buffers[0].size;

			// Index buffer
			CopyBufferSubresource(new_index_buffer, index_buffer_offset, meshes[index].index_buffer, 0, meshes[index].index_buffer.count, graphics->GetContext());
			index_buffer_offset += meshes[index].index_buffer.count;
		}

		return result;
	}

	// ------------------------------------------------------------------------------------------------------------------------

#pragma endregion



#endif // ECSENGINE_DIRECTX11

#endif // ECSENGINE_PLATFORM_WINDOWS
}