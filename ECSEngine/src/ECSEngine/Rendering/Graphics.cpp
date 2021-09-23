#include "ecspch.h"
#include "Graphics.h"
#include "../Utilities/FunctionInterfaces.h"
#include "GraphicsHelpers.h"
#include "../Utilities/Function.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")

namespace ECSEngine {

	ECS_MICROSOFT_WRL;

#ifdef ECSENGINE_PLATFORM_WINDOWS

#ifdef ECSENGINE_DIRECTX11

	Graphics::Graphics(HWND hWnd, const GraphicsDescriptor* descriptor) 
		: m_target_view(nullptr), m_device(nullptr), m_context(nullptr), m_swap_chain(nullptr), m_allocator(descriptor->allocator)
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

		m_auto_context = m_context;
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
		rasterizer_state.CullMode = D3D11_CULL_NONE;
		rasterizer_state.FillMode = D3D11_FILL_SOLID;
		rasterizer_state.DepthClipEnable = TRUE;
		rasterizer_state.FrontCounterClockwise = FALSE;

		ID3D11RasterizerState* state;
		m_device->CreateRasterizerState(&rasterizer_state, &state);

		m_context->RSSetState(state);
	}

	void Graphics::BindVertexBuffer(VertexBuffer buffer, UINT slot) {
		BindVertexBuffer(buffer, m_auto_context.Get(), slot);
	}

	void Graphics::BindVertexBuffer(VertexBuffer buffer, GraphicsContext* context, UINT slot) {
		const UINT offset = 0u;
		context->IASetVertexBuffers(slot, 1u, &buffer.buffer, &buffer.stride, &offset);
	}

	void Graphics::BindVertexBufferImmediate(VertexBuffer buffer, UINT slot) {
		BindVertexBuffer(buffer, m_context.Get(), slot);
	}

	void Graphics::BindVertexBuffers(Stream<VertexBuffer> buffers, UINT start_slot) {
		BindVertexBuffers(buffers, m_auto_context.Get(), start_slot);
	}

	void Graphics::BindVertexBuffers(Stream<VertexBuffer> buffers, GraphicsContext* context, UINT start_slot) {
		ID3D11Buffer* v_buffers[256];
		UINT strides[256];
		UINT offsets[256] = { 0 };
		for (size_t index = 0; index < buffers.size; index++) {
			v_buffers[index] = buffers[index].buffer;
			strides[index] = buffers[index].stride;
		}
		context->IASetVertexBuffers(start_slot, buffers.size, v_buffers, strides, offsets);
	}

	void Graphics::BindVertexBuffersImmediate(Stream<VertexBuffer> buffers, UINT start_slot) {
		BindVertexBuffers(buffers, m_context.Get(), start_slot);
	}

	void Graphics::BindIndexBuffer(IndexBuffer index_buffer) {
		BindIndexBuffer(index_buffer, m_auto_context.Get());
	}

	void Graphics::BindIndexBuffer(IndexBuffer index_buffer, GraphicsContext* context) {
		const UINT index_offset = 0u;
		context->IASetIndexBuffer(index_buffer.buffer, DXGI_FORMAT_R32_UINT, index_offset);
	}

	void Graphics::BindIndexBufferImmediate(IndexBuffer index_buffer) {
		BindIndexBuffer(index_buffer, m_context.Get());
	}

	void Graphics::BindInputLayout(InputLayout input_layout) {
		BindInputLayout(input_layout, m_auto_context.Get());
	}

	void Graphics::BindInputLayout(InputLayout input_layout, GraphicsContext* context) {
		context->IASetInputLayout(input_layout.layout);
	}

	void Graphics::BindInputLayoutImmediate(InputLayout input_layout) {
		BindInputLayout(input_layout, m_context.Get());
	}

	void Graphics::BindVertexShader(VertexShader shader) {
		BindVertexShader(shader, m_auto_context.Get());
	}

	void Graphics::BindVertexShader(VertexShader shader, GraphicsContext* context) {
		context->VSSetShader(shader.shader, nullptr, 0u);
	}

	void Graphics::BindVertexShaderImmediate(VertexShader shader) {
		BindVertexShader(shader, m_context.Get());
	}

	void Graphics::BindPixelShader(PixelShader shader) {
		BindPixelShader(shader, m_auto_context.Get());
	}

	void Graphics::BindPixelShader(PixelShader shader, GraphicsContext* context) {
		context->PSSetShader(shader.shader, nullptr, 0u);
	}

	void Graphics::BindPixelShaderImmediate(PixelShader shader) {
		BindPixelShader(shader, m_context.Get());
	}

	void Graphics::BindPixelConstantBuffer(ConstantBuffer buffer, UINT slot) {
		BindPixelConstantBuffer(buffer, m_auto_context.Get(), slot);
	}

	void Graphics::BindPixelConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot) {
		context->PSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	void Graphics::BindPixelConstantBufferImmediate(ConstantBuffer buffer, UINT slot) {
		BindPixelConstantBuffer(buffer, m_context.Get(), slot);
	}

	void Graphics::BindPixelConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		BindPixelConstantBuffers(buffers, m_auto_context.Get(), start_slot);
	}

	void Graphics::BindPixelConstantBuffers(
		Stream<ConstantBuffer> buffers, 
		GraphicsContext* context,
		UINT start_slot
	) {
		ID3D11Buffer* p_buffers[256];
		for (size_t index = 0; index < buffers.size; index++) {
			p_buffers[index] = buffers[index].buffer;
		}
		context->PSSetConstantBuffers(start_slot, buffers.size, p_buffers);
	}

	void Graphics::BindPixelConstantBuffersImmediate(Stream<ConstantBuffer> buffers, UINT start_slot) {
		BindPixelConstantBuffers(buffers, m_context.Get(), start_slot);
	}

	void Graphics::BindVertexConstantBuffer(ConstantBuffer buffer, UINT slot) {
		BindVertexConstantBuffer(buffer, m_auto_context.Get(), slot);
	}

	void Graphics::BindVertexConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot) {
		context->VSSetConstantBuffers(slot, 1u, &buffer.buffer);
	}

	void Graphics::BindVertexConstantBufferImmediate(ConstantBuffer buffer, UINT slot) {
		BindVertexConstantBuffer(buffer, m_context.Get(), slot);
	}

	void Graphics::BindVertexConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot) {
		BindVertexConstantBuffers(buffers, m_auto_context.Get(), start_slot);
	}

	void Graphics::BindVertexConstantBuffers(
		Stream<ConstantBuffer> buffers, 
		GraphicsContext* context,
		UINT start_slot
	) {
		ID3D11Buffer* v_buffers[256];
		for (size_t index = 0; index < buffers.size; index++) {
			v_buffers[index] = buffers[index].buffer;
		}
		context->VSSetConstantBuffers(start_slot, buffers.size, v_buffers);
	}

	void Graphics::BindVertexConstantBuffersImmediate(Stream<ConstantBuffer> buffers, UINT start_slot) {
		BindVertexConstantBuffers(buffers, m_context.Get(), start_slot);
	}

	void Graphics::BindTopology(Topology topology)
	{
		BindTopology(topology, m_auto_context.Get());
	}

	void Graphics::BindTopology(Topology topology, GraphicsContext* context)
	{
		context->IASetPrimitiveTopology(topology.value);
	}

	void Graphics::BindTopologyImmediate(Topology topology)
	{
		BindTopology(topology, m_context.Get());
	}

	void Graphics::BindPSResourceView(ResourceView component, UINT slot) {
		BindPSResourceView(component, m_auto_context.Get(), slot);
	}

	void Graphics::BindPSResourceView(ResourceView component, GraphicsContext* context, UINT slot) {
		context->PSSetShaderResources(slot, 1, &component.view);
	}

	void Graphics::BindPSResourceViewImmediate(ResourceView component, UINT slot) {
		BindPSResourceView(component, m_context.Get(), slot);
	}

	void Graphics::BindPSResourceViews(Stream<ResourceView> views, UINT start_slot) {
		BindPSResourceViews(views, m_auto_context.Get(), start_slot);
	}

	void Graphics::BindPSResourceViews(Stream<ResourceView> views, GraphicsContext* context, UINT start_slot) {
		ID3D11ShaderResourceView* resource_views[256];
		for (size_t index = 0; index < views.size; index++) {
			resource_views[index] = views[index].view;
		}
		context->PSSetShaderResources(start_slot, views.size, resource_views);
	}

	void Graphics::BindPSResourceViewsImmediate(Stream<ResourceView> views, UINT start_slot) {
		BindPSResourceViews(views, m_context.Get(), start_slot);
	}

	void Graphics::BindSamplerState(SamplerState sampler, UINT slot)
	{
		BindSamplerState(sampler, m_auto_context.Get(), slot);
	}

	void Graphics::BindSamplerState(SamplerState sampler, GraphicsContext* context, UINT slot)
	{
		context->PSSetSamplers(slot, 1u, &sampler.sampler);
	}

	void Graphics::BindSamplerStateImmediate(SamplerState sampler, UINT slot)
	{
		BindSamplerState(sampler, m_context.Get(), slot);
	}

	void Graphics::BindSamplerStates(Stream<SamplerState> samplers, UINT start_slot)
	{
		BindSamplerStates(samplers, m_auto_context.Get(), start_slot);
	}

	void Graphics::BindSamplerStates(Stream<SamplerState> samplers, GraphicsContext* context, UINT start_slot)
	{
		ID3D11SamplerState* states[32];
		for (size_t index = 0; index < samplers.size; index++) {
			states[index] = samplers[index].sampler;
		}
		context->PSSetSamplers(start_slot, samplers.size, states);
	}

	void Graphics::BindSamplerStatesImmediate(Stream<SamplerState> samplers, UINT start_slot)
	{
		BindSamplerStates(samplers, m_context.Get(), start_slot);
	}

	void Graphics::BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view)
	{
		BindRenderTargetView(render_view, depth_stencil_view, m_auto_context.Get());
	}

	void Graphics::BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view, GraphicsContext* context)
	{
		context->OMSetRenderTargets(1u, &render_view.target, depth_stencil_view.view);
	}

	void Graphics::BindRenderTargetViewFromInitialViews()
	{
		m_auto_context->OMSetRenderTargets(1u, &m_target_view.target, m_depth_stencil_view.view);
	}

	void Graphics::BindRenderTargetViewFromInitialViews(GraphicsContext* context)
	{
		context->OMSetRenderTargets(1u, &m_target_view.target, m_depth_stencil_view.view);
	}

	void Graphics::BindViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth)
	{
		D3D11_VIEWPORT viewport = { 0 };
		viewport.TopLeftX = top_left_x;
		viewport.TopLeftY = top_left_y;
		viewport.Width = width;
		viewport.Height = height;
		viewport.MinDepth = min_depth;
		viewport.MaxDepth = max_depth;

		m_auto_context->RSSetViewports(1u, &viewport);
	}

	void Graphics::BindViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth, GraphicsContext* context)
	{
		D3D11_VIEWPORT viewport = { 0 };
		viewport.TopLeftX = top_left_x;
		viewport.TopLeftY = top_left_y;
		viewport.Width = width;
		viewport.Height = height;
		viewport.MinDepth = min_depth;
		viewport.MaxDepth = max_depth;

		/*char characters1[128];
		char characters2[128];
		char characters3[128];
		char characters4[128];
		char characters5[128];
		char characters6[128];
		char characters7[128];
		char characters8[128];
		Stream<char> float1 = Stream<char>(characters1, 0);
		Stream<char> float2 = Stream<char>(characters2, 0);
		Stream<char> float3 = Stream<char>(characters3, 0);
		Stream<char> float4 = Stream<char>(characters4, 0);
		Stream<char> float5 = Stream<char>(characters5, 0);
		Stream<char> float6 = Stream<char>(characters6, 0);
		Stream<char> float7 = Stream<char>(characters7, 0);
		Stream<char> float8 = Stream<char>(characters8, 0);
		function::ConvertFloatToChars<3>(viewport.Width, float1);
		function::ConvertFloatToChars<3>(viewport.Height, float2);
		function::ConvertFloatToChars<3>(width, float3);
		function::ConvertFloatToChars<3>(height, float4);
		function::ConvertFloatToChars<3>(top_left_x, float5);
		function::ConvertFloatToChars<3>(top_left_y, float6);
		function::ConvertFloatToChars<3>(viewport.TopLeftX, float7);
		function::ConvertFloatToChars<3>(viewport.TopLeftY, float8);

		if (viewport.TopLeftX < 0.0f || viewport.TopLeftX > 1370.0f || viewport.TopLeftY < 0.0f || viewport.TopLeftY > 800.0f) {

			wchar_t* c1 = function::ConvertASCIIToWide(characters5);
			wchar_t* c2 = function::ConvertASCIIToWide(characters6);
			OutputDebugString(c1);
			OutputDebugString(c2);

			__debugbreak();
		}*/

		context->RSSetViewports(1u, &viewport);
	}

	void Graphics::BindDefaultViewport() {
		BindViewport(0.0f, 0.0f, m_window_size.x, m_window_size.y, 0.0f, 1.0f);
	}

	void Graphics::BindDefaultViewport(GraphicsContext* context) {
		BindViewport(0.0f, 0.0f, m_window_size.x, m_window_size.y, 0.0f, 1.0f, context);
	}

	IndexBuffer Graphics::ConstructIndexBuffer(Stream<unsigned int> indices)
	{ 
		IndexBuffer component;
		component.count = indices.size;
		HRESULT result;
		D3D11_BUFFER_DESC index_buffer_descriptor = {};
		index_buffer_descriptor.ByteWidth = UINT(component.count * sizeof(unsigned int));
		index_buffer_descriptor.Usage = D3D11_USAGE_DEFAULT;
		index_buffer_descriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
		index_buffer_descriptor.CPUAccessFlags = 0;
		index_buffer_descriptor.MiscFlags = 0;
		index_buffer_descriptor.StructureByteStride = sizeof(unsigned int);

		D3D11_SUBRESOURCE_DATA initial_data_index = {};
		initial_data_index.pSysMem = indices.buffer;

		result = m_device->CreateBuffer(&index_buffer_descriptor, &initial_data_index, &component.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating index buffer failed", true);
		return component;
	}

	PixelShader Graphics::ConstructPSShader(const wchar_t* path)
	{
		PixelShader component;
		component.path = function::StringCopyTs(m_allocator, path);
		HRESULT result;
		Microsoft::WRL::ComPtr<ID3DBlob> blob;

		result = D3DReadFileToBlob(path, &blob);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Reading pixel shader failed", true);

		result = m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &component.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating pixel shader failed", true);

		return component;
	}
	
	PixelShader Graphics::ConstructPSShader(Stream<wchar_t> path)
	{
		PixelShader component;
		wchar_t* allocation = (wchar_t*)m_allocator->Allocate_ts((path.size + 1) * sizeof(wchar_t), alignof(wchar_t));
		memcpy(allocation, path.buffer, path.size * sizeof(wchar_t));
		allocation[path.size] = L'\0';
		component.path = { allocation, path.size };

		HRESULT result;
		Microsoft::WRL::ComPtr<ID3DBlob> blob;

		result = D3DReadFileToBlob(component.path.buffer, &blob);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Reading pixel shader failed", true);

		result = m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &component.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating pixel shader failed", true);

		return component;
	}

	VertexShader Graphics::ConstructVSShader(const wchar_t* path)
	{
		VertexShader component;
		component.path = function::StringCopyTs(m_allocator, path);
		HRESULT result;

		result = D3DReadFileToBlob(path, &component.byte_code);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Reading vertex shader location failed", true);

		result = m_device->CreateVertexShader(
			component.byte_code->GetBufferPointer(), 
			component.byte_code->GetBufferSize(), 
			nullptr,
			&component.shader
		);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating vertex shader failed", true);

		return component;
	}

	VertexShader Graphics::ConstructVSShader(Stream<wchar_t> path) {
		VertexShader component;
		wchar_t* allocation = (wchar_t*)m_allocator->Allocate_ts(sizeof(wchar_t) * (path.size + 1), alignof(wchar_t));
		memcpy(allocation, path.buffer, sizeof(wchar_t) * path.size);
		allocation[path.size] = L'\0';
		component.path = { allocation, path.size };

		HRESULT result;

		result = D3DReadFileToBlob(component.path.buffer, &component.byte_code);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Reading vertex shader location failed", true);

		result = m_device->CreateVertexShader(
			component.byte_code->GetBufferPointer(),
			component.byte_code->GetBufferSize(),
			nullptr,
			&component.shader
		);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating vertex shader failed", true);

		return component;
	}

	InputLayout Graphics::ConstructInputLayout(
		Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, 
		VertexShader vertex_shader
	)
	{
		InputLayout component;
		HRESULT result;
		auto byte_code = vertex_shader.byte_code;
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

	VertexBuffer Graphics::ConstructVertexBuffer(size_t element_size, size_t element_count, D3D11_USAGE usage, unsigned int cpuFlags, unsigned int miscFlags)
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

	VertexBuffer Graphics::ConstructVertexBuffer(size_t element_size, size_t element_count, const void* buffer, D3D11_USAGE usage, unsigned int cpuFlags, unsigned int miscFlags)
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

	ConstantBuffer Graphics::ConstructConstantBuffer(size_t byte_size, const void* buffer, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
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

	ConstantBuffer Graphics::ConstructConstantBuffer(size_t byte_size, D3D11_USAGE usage, unsigned int cpuAccessFlags, unsigned int miscFlags)
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

	SamplerState Graphics::ConstructSamplerState(D3D11_SAMPLER_DESC descriptor)
	{
		SamplerState state;
		HRESULT result = m_device->CreateSamplerState(&descriptor, &state.sampler);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Constructing sampler state failed!", true);
		return state;
	}

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

	GraphicsContext* Graphics::CreateDeferredContext(UINT context_flags)
	{
		GraphicsContext* context;
		m_device->CreateDeferredContext(0, &context);
		return context;
	}

	Texture1D Graphics::CreateTexture1D(const GraphicsTexture1DDescriptor* ecs_descriptor)
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

	Texture2D Graphics::CreateTexture2D(const GraphicsTexture2DDescriptor* ecs_descriptor)
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

	Texture3D Graphics::CreateTexture3D(const GraphicsTexture3DDescriptor* ecs_descriptor)
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

	ResourceView Graphics::CreateTexture1DShaderView(
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
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 1D Shader View!", true);
		return component;
	}

	ResourceView Graphics::CreateTexture2DShaderView(
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
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 2D Shader View!", true);
		return component;
	}

	ResourceView Graphics::CreateTexture3DShaderView(
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
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 3D Shader View!", true);
		return component;
	}

	RenderTargetView Graphics::CreateRenderTargetView(Texture2D texture)
	{
		HRESULT result;

		RenderTargetView view;
		result = m_device->CreateRenderTargetView(texture.tex, nullptr, &view.target);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed!", true);

		return view;
	}

	DepthStencilView Graphics::CreateDepthStencilView(Texture2D texture)
	{
		HRESULT result;

		DepthStencilView view;
		result = m_device->CreateDepthStencilView(texture.tex, nullptr, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed!", true);

		return view;
	}

	void Graphics::ClearBackBuffer(float red, float green, float blue) {
		ClearBackBuffer(red, green, blue, m_auto_context.Get());
	}

	void Graphics::ClearBackBuffer(float red, float green, float blue, GraphicsContext* context) {
		const float color[] = { red, green, blue, 1.0f };
		context->ClearRenderTargetView(m_target_view.target, color);
		context->ClearDepthStencilView(m_depth_stencil_view.view, D3D11_CLEAR_DEPTH, 1.0f, 0u);
	}

	void Graphics::ClearBackBufferImmediate(float red, float green, float blue) {
		ClearBackBuffer(red, blue, green, m_context.Get());
	}

	void Graphics::DisableAlphaBlending() {
		DisableAlphaBlending(m_auto_context.Get());
	}

	void Graphics::DisableAlphaBlending(GraphicsContext* context) {
		context->OMSetBlendState(m_blend_disabled.Get(), nullptr, 0xFFFFFFFF);
	}

	void Graphics::DisableAlphaBlendingImmediate() {
		DisableAlphaBlending(m_context.Get());
	}

	void Graphics::DisableDepth()
	{
		DisableDepth(m_auto_context.Get());
	}

	void Graphics::DisableCulling()
	{
		DisableCulling(m_auto_context.Get());
	}

	void Graphics::DisableDepth(GraphicsContext* context)
	{
		context->OMSetRenderTargets(1u, &m_target_view.target, nullptr);
	}

	void Graphics::DisableCulling(GraphicsContext* context)
	{
		D3D11_RASTERIZER_DESC rasterizer_desc = {};
		rasterizer_desc.AntialiasedLineEnable = TRUE;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.FrontCounterClockwise = FALSE;

		ID3D11RasterizerState* state;
		HRESULT result = m_device->CreateRasterizerState(&rasterizer_desc, &state);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Failed to disable culling.", true);
		context->RSSetState(state);
	}

	void Graphics::DisableDepthImmediate() {
		DisableDepth(m_context.Get());
	}

	void Graphics::DisableCullingImmediate()
	{
		DisableCulling(m_context.Get());
	}

	void Graphics::Draw(UINT vertex_count, UINT start_slot)
	{
		Draw(vertex_count, m_auto_context.Get(), start_slot);
	}

	void Graphics::Draw(UINT vertex_count, GraphicsContext* context, UINT start_slot)
	{
		context->Draw(vertex_count, start_slot);
	}

	void Graphics::DrawImmediate(UINT vertex_count, UINT start_slot) {
		Draw(vertex_count, m_context.Get(), start_slot);
	}

	void Graphics::DrawIndexed(unsigned int count, UINT start_vertex, INT base_vertex_location) {
		DrawIndexed(count, m_auto_context.Get(), start_vertex, base_vertex_location);
	}

	void Graphics::DrawIndexed(unsigned int count, GraphicsContext* context, UINT start_vertex, INT base_vertex_location) {
		context->DrawIndexed(count, start_vertex, base_vertex_location);
	}

	void Graphics::DrawIndexedImmediate(unsigned int count, UINT start_vertex, INT base_vertex_location) {
		DrawIndexed(count, m_context.Get(), start_vertex, base_vertex_location);
	}

	void Graphics::EnableAlphaBlending() {
		EnableAlphaBlending(m_auto_context.Get());
	}

	void Graphics::EnableAlphaBlending(GraphicsContext* context) {
		context->OMSetBlendState(m_blend_enabled.Get(), nullptr, 0xFFFFFFFF);
	}

	void Graphics::EnableAlphaBlendingImmediate() {
		EnableAlphaBlending(m_context.Get());
	}

	void Graphics::EnableDepth()
	{
		EnableDepth(m_auto_context.Get());
	}

	void Graphics::EnableDepth(GraphicsContext* context)
	{
		context->OMSetRenderTargets(1u, &m_target_view.target, m_depth_stencil_view.view);
	}

	void Graphics::EnableDepthImmediate() {
		EnableDepth(m_context.Get());
	}

	ID3D11CommandList* Graphics::FinishCommandList(bool restore_state) {
		return FinishCommandList(m_auto_context.Get(), restore_state);
	}

	ID3D11CommandList* Graphics::FinishCommandList(GraphicsContext* context, bool restore_state) {
		ID3D11CommandList* list = nullptr;
		HRESULT result;
		result = context->FinishCommandList(restore_state, &list);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating command list failed!", true);
		return list;
	}

	void Graphics::EndFrame(float value) {
		//ClearBackBuffer(value, 1.0f, value);
		
		//SwapBuffers();
	}

	GraphicsDevice* Graphics::GetDevice() {
		return m_device.Get();
	}

	GraphicsContext* Graphics::GetContext() {
		return m_context.Get();
	}

	GraphicsContext* Graphics::GetDeferredContext()
	{
		return m_deferred_context.Get();
	}
	
	GraphicsContext* Graphics::GetAutoContext() {
		return m_auto_context.Get();
	}

	void* Graphics::MapBuffer(ID3D11Buffer* buffer, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return MapBuffer(buffer, m_auto_context.Get(), map_type, subresource_index, map_flags);
	}

	void* Graphics::MapTexture1D(Texture1D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return MapTexture1D(texture, m_auto_context.Get(), map_type, subresource_index, map_flags);
	}

	void* Graphics::MapTexture2D(Texture2D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return MapTexture2D(texture, m_auto_context.Get(), map_type, subresource_index, map_flags);
	}

	void* Graphics::MapTexture3D(Texture3D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return MapTexture3D(texture, m_auto_context.Get(), map_type, subresource_index, map_flags);
	}

	void* Graphics::MapBuffer(
		ID3D11Buffer* buffer, 
		GraphicsContext* context,
		D3D11_MAP map_type, 
		unsigned int subresource_index, 
		unsigned int map_flags
	)
	{
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(buffer, subresource_index, map_type, map_flags, &mapped_subresource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Mapping constant buffer failed!", true);
		return mapped_subresource.pData;
	}

	void* Graphics::MapTexture1D(
		Texture1D texture, 
		GraphicsContext* context,
		D3D11_MAP map_type, 
		unsigned int subresource_index, 
		unsigned int map_flags
	)
	{
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(texture.tex, subresource_index, map_type, map_flags, &mapped_subresource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Mapping texture 1D failed!", true);
		return mapped_subresource.pData;
	}

	void* Graphics::MapTexture2D(
		Texture2D texture,
		GraphicsContext* context,
		D3D11_MAP map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	)
	{
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(texture.tex, subresource_index, map_type, map_flags, &mapped_subresource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Mapping texture 2D failed!", true);
		return mapped_subresource.pData;
	}

	void* Graphics::MapTexture3D(
		Texture3D texture,
		GraphicsContext* context,
		D3D11_MAP map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	)
	{
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(texture.tex, subresource_index, map_type, map_flags, &mapped_subresource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Mapping texture 1D failed!", true);
		return mapped_subresource.pData;
	}

	void* Graphics::MapBufferImmediate(ID3D11Buffer* buffer, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return MapBuffer(buffer, m_context.Get(), map_type, subresource_index, map_flags);
	}

	void* Graphics::MapTexture1DImmediate(Texture1D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return MapTexture1D(texture, m_context.Get(), map_type, subresource_index, map_flags);
	}

	void* Graphics::MapTexture2DImmediate(Texture2D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return MapTexture2D(texture, m_context.Get(), map_type, subresource_index, map_flags);
	}

	void* Graphics::MapTexture3DImmediate(Texture3D texture, D3D11_MAP map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return MapTexture3D(texture, m_context.Get(), map_type, subresource_index, map_flags);
	}

	void Graphics::GetWindowSize(unsigned int& width, unsigned int& height) const {
		width = m_window_size.x;
		height = m_window_size.y;
	}

	void Graphics::GetWindowSize(uint2& size) const
	{
		size.x = m_window_size.x;
		size.y = m_window_size.y;
	}

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

	void Graphics::SetAutoMode(GraphicsContext* context) {
		m_auto_context = context;
	}

	void Graphics::SetAutoModeImmediate() {
		m_auto_context = m_context;
	}

	void Graphics::SetAutoModeDeferred() {
		m_auto_context = m_deferred_context;
	}

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

	void Graphics::SetNewSize(HWND hWnd, unsigned int width, unsigned int height)
	{
		m_window_size.x = width;
		m_window_size.y = height;
		ResizeSwapChainSize(hWnd, width, height);
	}

	void Graphics::UpdateBuffer(ID3D11Buffer* buffer, const void* data, size_t data_size, unsigned int subresource_index)
	{
		UpdateBuffer(buffer, data, data_size, m_auto_context.Get(), subresource_index);
	}

	void Graphics::UpdateBuffer(
		ID3D11Buffer* buffer, 
		const void* data, 
		size_t data_size, 
		GraphicsContext* context,
		unsigned int subresource_index
	)
	{
		HRESULT result;

		D3D11_MAPPED_SUBRESOURCE mapped_subresource;
		result = context->Map(buffer, subresource_index, D3D11_MAP_WRITE_DISCARD, 0u, &mapped_subresource);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Updating constant buffer failed!", true);

		memcpy(mapped_subresource.pData, data, data_size);
		context->Unmap(buffer, subresource_index);
	}

	void Graphics::UpdateBufferImmediate(ID3D11Buffer* buffer, const void* data, size_t data_size, unsigned int subresource_index)
	{
		UpdateBuffer(buffer, data, data_size, m_context.Get(), subresource_index);
	}

	void Graphics::UnmapBuffer(ID3D11Buffer* buffer, unsigned int resource_index)
	{
		UnmapBuffer(buffer, m_auto_context.Get(), resource_index);
	}

	void Graphics::UnmapBuffer(ID3D11Buffer* buffer, GraphicsContext* context, unsigned int resource_index)
	{
		context->Unmap(buffer, resource_index);
	}

	void Graphics::UnmapBufferImmediate(ID3D11Buffer* buffer, unsigned int resource_index) {
		UnmapBuffer(buffer, m_context.Get(), resource_index);
	}

	void Graphics::UnmapTexture1D(Texture1D texture, unsigned int resource_index) {
		UnmapTexture1D(texture, m_auto_context.Get(), resource_index);
	}

	void Graphics::UnmapTexture1D(Texture1D buffer, GraphicsContext* context, unsigned int resource_index)
	{
		context->Unmap(buffer.tex, resource_index);
	}

	void Graphics::UnmapTexture1DImmediate(Texture1D texture, unsigned int resource_index) {
		UnmapTexture1D(texture, m_context.Get(), resource_index);
	}

	void Graphics::UnmapTexture2D(Texture2D texture, unsigned int resource_index) {
		UnmapTexture2D(texture, m_auto_context.Get(), resource_index);
	}

	void Graphics::UnmapTexture2D(Texture2D buffer, GraphicsContext* context, unsigned int resource_index)
	{
		context->Unmap(buffer.tex, resource_index);
	}

	void Graphics::UnmapTexture2DImmediate(Texture2D texture, unsigned int resource_index) {
		UnmapTexture2D(texture, m_context.Get(), resource_index);
	}

	void Graphics::UnmapTexture3D(Texture3D texture, unsigned int resource_index) {
		UnmapTexture3D(texture, m_auto_context.Get(), resource_index);
	}

	void Graphics::UnmapTexture3D(Texture3D buffer, GraphicsContext* context, unsigned int resource_index)
	{
		context->Unmap(buffer.tex, resource_index);
	}

	void Graphics::UnmapTexture3DImmediate(Texture3D texture, unsigned int resource_index) {
		UnmapTexture3D(texture, m_context.Get(), resource_index);
	}

	InputLayout Graphics::ReflectVertexShaderInput(VertexShader shader, const wchar_t* path)
	{
		return ReflectVertexShaderInput(shader, ToStream(path));
	}

	InputLayout Graphics::ReflectVertexShaderInput(VertexShader shader, Stream<wchar_t> path)
	{
		constexpr size_t MAX_INPUT_FIELDS = 128;
		D3D11_INPUT_ELEMENT_DESC _element_descriptors[MAX_INPUT_FIELDS];
		CapacityStream<D3D11_INPUT_ELEMENT_DESC> element_descriptors(_element_descriptors, 0, MAX_INPUT_FIELDS);

		constexpr size_t NAME_POOL_SIZE = 8192;
		void* allocation = _malloca(NAME_POOL_SIZE);
		CapacityStream<char> name_pool(allocation, 0, NAME_POOL_SIZE);
		bool success = m_shader_reflection.ReflectVertexShaderInput(path, element_descriptors, name_pool);

		if (!success) {
			_freea(allocation);
			return {};
		}
		InputLayout layout = ConstructInputLayout(element_descriptors, shader);
		_freea(allocation);
		return layout;
	}

#endif // ECSENGINE_DIRECTX11

#endif // ECSENGINE_PLATFORM_WINDOWS
}