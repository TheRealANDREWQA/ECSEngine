#include "ecspch.h"
#include "Graphics.h"
#include "TextureOperations.h"
#include "GraphicsHelpers.h"
#include "../Utilities/File.h"
#include "../Utilities/Path.h"
#include "../Utilities/StreamUtilities.h"
#include "ShaderInclude.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Utilities/Crash.h"
#include "../Utilities/Console.h"
#include "../Utilities/ParsingUtilities.h"
#include "../Allocators/ResizableLinearAllocator.h"

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
		"ps_5_0",
		"ds_5_0",
		"hs_5_0",
		"gs_5_0",
		"cs_5_0",
	};

	static_assert(ECS_COUNTOF(SHADER_COMPILE_TARGET) == (ECS_SHADER_TYPE_COUNT * ECS_SHADER_TARGET_COUNT));

	const wchar_t* SHADER_HELPERS_VERTEX[] = {
		ECS_VERTEX_SHADER_SOURCE(EquirectangleToCube),
		ECS_VERTEX_SHADER_SOURCE(Skybox),
		ECS_VERTEX_SHADER_SOURCE(ConvoluteDiffuseEnvironment),
		ECS_VERTEX_SHADER_SOURCE(ConvoluteSpecularEnvironment),
		ECS_VERTEX_SHADER_SOURCE(BRDFIntegration),
		ECS_VERTEX_SHADER_SOURCE(GLTFThumbnail),
		ECS_VERTEX_SHADER_SOURCE(BasicTransform),
		nullptr,
		ECS_VERTEX_SHADER_SOURCE(Passthrough),
		ECS_VERTEX_SHADER_SOURCE(MousePick),
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		ECS_VERTEX_SHADER_SOURCE(SolidColorInstanced)
	};

	const wchar_t* SHADER_HELPERS_PIXEL[] = {
		ECS_PIXEL_SHADER_SOURCE(EquirectangleToCube),
		ECS_PIXEL_SHADER_SOURCE(Skybox),
		ECS_PIXEL_SHADER_SOURCE(ConvoluteDiffuseEnvironment),
		ECS_PIXEL_SHADER_SOURCE(ConvoluteSpecularEnvironment),
		ECS_PIXEL_SHADER_SOURCE(BRDFIntegration),
		ECS_PIXEL_SHADER_SOURCE(GLTFThumbnail),
		ECS_PIXEL_SHADER_SOURCE(HighlightStencil),
		ECS_PIXEL_SHADER_SOURCE(HighlightBlend),
		nullptr,
		ECS_PIXEL_SHADER_SOURCE(MousePick),
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		ECS_PIXEL_SHADER_SOURCE(SolidColorInstanced)
	};

	const wchar_t* SHADER_HELPERS_COMPUTE[] = {
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		ECS_COMPUTE_SHADER_SOURCE(VisualizeTexture), // Each shader will have a different macro annotation
		ECS_COMPUTE_SHADER_SOURCE(VisualizeTexture), // Each shader will have a different macro annotation
		ECS_COMPUTE_SHADER_SOURCE(VisualizeTexture), // Each shader will have a different macro annotation
		ECS_COMPUTE_SHADER_SOURCE(VisualizeTexture), // Each shader will have a different macro annotation
		nullptr,
	};

	static_assert(ECS_COUNTOF(SHADER_HELPERS_VERTEX) == ECS_GRAPHICS_SHADER_HELPER_COUNT);
	static_assert(ECS_COUNTOF(SHADER_HELPERS_PIXEL) == ECS_GRAPHICS_SHADER_HELPER_COUNT);
	static_assert(ECS_COUNTOF(SHADER_HELPERS_COMPUTE) == ECS_GRAPHICS_SHADER_HELPER_COUNT);

	const char* ECS_GRAPHICS_RESOURCE_TYPE_STRING[] = {
		"VertexShader",
		"PixelShader",
		"DomainShader",
		"HullShader",
		"GeometryShader",
		"ComputeShader",
		"IndexBuffer",
		"InputLayout",
		"ConstantBuffer",
		"VertexBuffer",
		"ResourceView",
		"SamplerState",
		"UnorderedView",
		"GraphicsContext",
		"RenderTargetView",
		"DepthStencilView",
		"StandardBuffer",
		"StructuredBuffer",
		"UnorderedBuffer",
		"IndirectBuffer",
		"AppendBuffer",
		"ConsumeBuffer",
		"Texture1D",
		"Texture2D",
		"Texture3D",
		"TextureCube",
		"BlendState",
		"DepthStencilState",
		"RasterizerState",
		"CommandList",
		"DXResourceInterface"
	};

	static_assert(ECS_COUNTOF(ECS_GRAPHICS_RESOURCE_TYPE_STRING) == ECS_GRAPHICS_RESOURCE_TYPE_COUNT);

#define GRAPHICS_INTERNAL_RESOURCE_STARTING_COUNT 1024
	
	static ECS_GRAPHICS_RESOURCE_TYPE StringToResourceType(Stream<char> string) {
		ECS_GRAPHICS_RESOURCE_TYPE resource_type = ECS_GRAPHICS_RESOURCE_TYPE_COUNT;
		// Now try to match the interface type with the resource type
		if (string == "ID3D11BlendState") {
			resource_type = ECS_GRAPHICS_RESOURCE_BLEND_STATE;
		}
		else if (string == "ID3D11Buffer") {
			resource_type = ECS_GRAPHICS_RESOURCE_GENERIC_BUFFER;
		}
		else if (string == "ID3D11CommandList") {
			resource_type = ECS_GRAPHICS_RESOURCE_COMMAND_LIST;
		}
		else if (string == "ID3D11ComputeShader") {
			resource_type = ECS_GRAPHICS_RESOURCE_COMPUTE_SHADER;
		}
		else if (string == "ID3D11Counter") {
			resource_type = ECS_GRAPHICS_RESOURCE_COUNTER;
		}
		else if (string == "ID3D11DepthStencilState") {
			resource_type = ECS_GRAPHICS_RESOURCE_DEPTH_STENCIL_STATE;
		}
		else if (string == "ID3D11DepthStencilView") {
			resource_type = ECS_GRAPHICS_RESOURCE_DEPTH_STENCIL_VIEW;
		}
		else if (string == "ID3D11DepthStencilState") {
			resource_type = ECS_GRAPHICS_RESOURCE_DEPTH_STENCIL_STATE;
		}
		else if (string == "ID3D11Device") {
			resource_type = ECS_GRAPHICS_RESOURCE_DEVICE;
		}
		else if (string == "ID3D11Context") {
			resource_type = ECS_GRAPHICS_RESOURCE_GRAPHICS_CONTEXT;
		}
		else if (string == "ID3D11DomainShader") {
			resource_type = ECS_GRAPHICS_RESOURCE_DOMAIN_SHADER;
		}
		else if (string == "ID3D11GeometryShader") {
			resource_type = ECS_GRAPHICS_RESOURCE_GEOMETRY_SHADER;
		}
		else if (string == "ID3D11HullShader") {
			resource_type = ECS_GRAPHICS_RESOURCE_HULL_SHADER;
		}
		else if (string == "ID3D11InputLayout") {
			resource_type = ECS_GRAPHICS_RESOURCE_INPUT_LAYOUT;
		}
		else if (string == "ID3D11Query") {
			resource_type = ECS_GRAPHICS_RESOURCE_QUERY;
		}
		else if (string == "ID3D11RasterizerState") {
			resource_type = ECS_GRAPHICS_RESOURCE_RASTERIZER_STATE;
		}
		else if (string == "ID3D11RenderTargetView") {
			resource_type = ECS_GRAPHICS_RESOURCE_RENDER_TARGET_VIEW;
		}
		else if (string == "ID3D11SamplerState") {
			resource_type = ECS_GRAPHICS_RESOURCE_SAMPLER_STATE;
		}
		else if (string == "ID3D11ShaderResourceView") {
			resource_type = ECS_GRAPHICS_RESOURCE_RESOURCE_VIEW;
		}
		else if (string == "ID3D11Texture1D") {
			resource_type = ECS_GRAPHICS_RESOURCE_TEXTURE_1D;
		}
		else if (string == "ID3D11Texture2D") {
			resource_type = ECS_GRAPHICS_RESOURCE_TEXTURE_2D;
		}
		else if (string == "ID3D11Texture3D") {
			resource_type = ECS_GRAPHICS_RESOURCE_TEXTURE_3D;
		}
		else if (string == "ID3D11UnorderedAccessView") {
			resource_type = ECS_GRAPHICS_RESOURCE_UA_VIEW;
		}
		else if (string == "ID3D11VertexShader") {
			resource_type = ECS_GRAPHICS_RESOURCE_VERTEX_SHADER;
		}
		else if (string == "IDXGISwapChain") {
			resource_type = ECS_GRAPHICS_RESOURCE_SWAP_CHAIN;
		}
		
		return resource_type;
	}

	// Returns true if it succeeded, else false
	static bool ExtractLiveObjectsFromDebugInterface(const Graphics* graphics, ResizableStream<GraphicsLiveObject>* live_objects) {
		ID3D11Debug* debug_device = nullptr;
		if (graphics->m_debug_device == nullptr) {
			HRESULT result = graphics->m_device->QueryInterface(__uuidof(ID3D11Debug), (void**)(&debug_device));
			if (FAILED(result)) {
				return false;
			}
		}
		else {
			debug_device = graphics->m_debug_device;
		}

		auto release_debug_interface = StackScope([graphics, debug_device]() {
			if (graphics->m_debug_device != debug_device) {
				debug_device->Release();
			}
		});

		ID3D11InfoQueue* info_queue = nullptr;
		if (graphics->m_info_queue == nullptr) {
			HRESULT result = graphics->m_device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)(&info_queue));
			if (FAILED(result)) {
				return false;
			}
		}
		else {
			info_queue = graphics->m_info_queue;
		}

		auto release_info_queue = StackScope([graphics, info_queue]() {
			if (graphics->m_info_queue != info_queue) {
				info_queue->Release();
			}
		});

		// Clear the messages in order to have enough space for all objects since each object will take
		// One message and we have around 4K message limit
		info_queue->ClearStoredMessages();
		HRESULT result = debug_device->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
		if (FAILED(result)) {
			return false;
		}
		
		size_t discarded_message_count = info_queue->GetNumMessagesDiscardedByMessageCountLimit();
		if (discarded_message_count > 0) {
			// Clear the queue, increase the limit and retry again
			info_queue->ClearStoredMessages();
			info_queue->SetMessageCountLimit(ECS_KB * 64);
			debug_device->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);

			discarded_message_count = info_queue->GetNumMessagesDiscardedByMessageCountLimit();
			if (discarded_message_count > 0) {
				// Fail if we still got denied messages
				return false;
			}
		}
		size_t message_count = info_queue->GetNumStoredMessagesAllowedByRetrievalFilter();

		ECS_STACK_CAPACITY_STREAM(char, current_message, ECS_KB * 4);
		D3D11_MESSAGE* queue_message = (D3D11_MESSAGE*)current_message.buffer;
		size_t message_capacity = current_message.capacity - sizeof(D3D11_MESSAGE);

		for (size_t index = 0; index < message_count; index++) {
			message_capacity = current_message.capacity - sizeof(D3D11_MESSAGE);
			HRESULT hr = info_queue->GetMessage(index, queue_message, &message_capacity);
			if (SUCCEEDED(hr)) {
				// Parse the string
				// The description byte length contains the terminating '\0'
				Stream<char> string = { queue_message->pDescription, queue_message->DescriptionByteLength - 1 };
				if (string[0] == '\t') {
					string.Advance();
				}
				Stream<char> interface_type;
				void* address;
				unsigned int reference_count;
				unsigned int internal_reference_count;
				bool success;
				if (index == 0) {
					// The first object is always the device
					success = ParseValuesFromFormat(string, "Live {#} at {#}, Refcount: {#}", &interface_type, &address, &reference_count);
				}
				else {
					success = ParseValuesFromFormat(string, "Live {#} at {#}, Refcount: {#}, IntRef: {#}", &interface_type, &address, &reference_count, &internal_reference_count);
					if (!success) {
						// Retry with the single refcount - the IDXGISwapChain is like this that appears afterwards with a single refcount
						success = ParseValuesFromFormat(string, "Live {#} at {#}, Refcount: {#}", &interface_type, &address, &reference_count);
					}
				}

				if (success) {
					ECS_GRAPHICS_RESOURCE_TYPE resource_type = StringToResourceType(interface_type);
					live_objects->Add({ address, resource_type, reference_count });
				}
				else {
					__debugbreak();
				}
			}
		}

		// Clear the queue such that we don't let these prints be received by the PrintRuntimeMessage
		info_queue->ClearStoredMessages();

		return true;
	}

	static void InitializeGraphicsCachedResources(Graphics* graphics) {
		// A quad is represented like
		// A - B
		// |   |
		// C - D
		// ACB, BCD

		float3 quad_positions[6] = {
			{ -1.0f, -1.0f, 0.0f },
			{ -1.0f, 1.0f, 0.0f },
			{ 1.0f, -1.0f, 0.0f },
			{ 1.0f, -1.0f, 0.0f },
			{ -1.0f, 1.0f, 0.0f },
			{ 1.0f, 1.0f, 0.0f }
		};
		graphics->m_cached_resources.vertex_buffer[ECS_GRAPHICS_CACHED_VERTEX_BUFFER_QUAD] = graphics->CreateVertexBuffer(
			sizeof(float3),
			ECS_COUNTOF(quad_positions),
			quad_positions,
			true
		);
	}

	static void InitializeGraphicsHelpers(Graphics* graphics) {
		graphics->m_shader_helpers.Initialize(graphics->Allocator(), ECS_GRAPHICS_SHADER_HELPER_COUNT, ECS_GRAPHICS_SHADER_HELPER_COUNT);
		graphics->m_depth_stencil_helpers.Initialize(graphics->Allocator(), ECS_GRAPHICS_DEPTH_STENCIL_HELPER_COUNT, ECS_GRAPHICS_DEPTH_STENCIL_HELPER_COUNT);

		// The shader helpers
		auto load_source_code = [&](const wchar_t* path) {
			return ReadWholeFileText(path, graphics->m_allocator);
		};

		Stream<wchar_t> include_directory = ECS_SHADER_DIRECTORY;
		ShaderIncludeFiles include(graphics->m_allocator, { &include_directory, 1 });
		for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
			if (SHADER_HELPERS_VERTEX[index] != nullptr) {
				Stream<char> vertex_source = load_source_code(SHADER_HELPERS_VERTEX[index]);
				ECS_ASSERT(vertex_source.buffer != nullptr);

				Stream<void> vertex_byte_code;
				graphics->m_shader_helpers[index].vertex = graphics->CreateVertexShaderFromSource(vertex_source, &include, {}, &vertex_byte_code, true);
				graphics->m_shader_helpers[index].input_layout = graphics->ReflectVertexShaderInput(vertex_source, vertex_byte_code, true, true);
				ECS_ASSERT(graphics->m_shader_helpers[index].input_layout.layout != nullptr);
				graphics->m_allocator->Deallocate(vertex_source.buffer);
			}
			else {
				graphics->m_shader_helpers[index].vertex = nullptr;
				graphics->m_shader_helpers[index].input_layout = nullptr;
			}

			if (SHADER_HELPERS_PIXEL[index] != nullptr) {
				Stream<char> pixel_source = load_source_code(SHADER_HELPERS_PIXEL[index]);
				ECS_ASSERT(pixel_source.buffer != nullptr);

				graphics->m_shader_helpers[index].pixel = graphics->CreatePixelShaderFromSource(pixel_source, &include, {}, true);
				graphics->m_allocator->Deallocate(pixel_source.buffer);
			}
			else {
				graphics->m_shader_helpers[index].pixel = nullptr;
			}

			if (SHADER_HELPERS_COMPUTE[index] != nullptr) {
				ShaderMacro compute_macro;

				ShaderCompileOptions compile_options = {};
				if (index == ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_DEPTH) {
					compile_options.macros = { &compute_macro, 1 };
					compute_macro.name = "DEPTH";
				}
				else if (index == ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_FLOAT) {
					compile_options.macros = { &compute_macro, 1 };
					compute_macro.name = "FLOAT";
				}
				else if (index == ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_SINT) {
					compile_options.macros = { &compute_macro, 1 };
					compute_macro.name = "SINT";
				}
				else if (index == ECS_GRAPHICS_SHADER_HELPER_VISUALIZE_UINT) {
					compile_options.macros = { &compute_macro, 1 };
					compute_macro.name = "UINT";
				}

				Stream<char> compute_source = load_source_code(SHADER_HELPERS_COMPUTE[index]);
				ECS_ASSERT(compute_source.buffer != nullptr);

				graphics->m_shader_helpers[index].compute = graphics->CreateComputeShaderFromSource(compute_source, &include, compile_options, true);
				ECS_ASSERT(graphics->ReflectComputeShaderDispatchSize(compute_source, &graphics->m_shader_helpers[index].compute_shader_dispatch_size));
				graphics->m_allocator->Deallocate(compute_source.buffer);
			}
			else {
				graphics->m_shader_helpers[index].compute = nullptr;
			}
		}

		SamplerDescriptor sampler_descriptor;
		sampler_descriptor.address_type_u = ECS_SAMPLER_ADDRESS_CLAMP;
		sampler_descriptor.address_type_v = ECS_SAMPLER_ADDRESS_CLAMP;
		sampler_descriptor.address_type_w = ECS_SAMPLER_ADDRESS_CLAMP;
		for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
			graphics->m_shader_helpers[index].pixel_sampler = graphics->CreateSamplerState(sampler_descriptor, true);
		}

		// The depth stencil helpers
		D3D11_DEPTH_STENCIL_DESC depth_stencil_state = {};
		depth_stencil_state.DepthEnable = FALSE;
		depth_stencil_state.StencilEnable = TRUE;
		depth_stencil_state.StencilWriteMask = 0xFF;
		SetDepthStencilDescOP(&depth_stencil_state, true, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE);
		SetDepthStencilDescOP(&depth_stencil_state, false, D3D11_COMPARISON_NEVER, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP);
		
		graphics->m_depth_stencil_helpers[ECS_GRAPHICS_DEPTH_STENCIL_HELPER_HIGHLIGHT] = graphics->CreateDepthStencilState(depth_stencil_state, true);
	}

	static void EnumGPU(CapacityStream<IDXGIAdapter1*>& adapters, DXGI_GPU_PREFERENCE preference) {
		IDXGIAdapter1* adapter;
		IDXGIFactory6* factory = NULL;

		// Create a DXGIFactory object
		if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory6), (void**)&factory))) {
			return;
		}

		for (unsigned int index = 0; factory->EnumAdapterByGpuPreference(index, preference, __uuidof(*adapter), (void**)&adapter) != DXGI_ERROR_NOT_FOUND; index++) {
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
				continue;
			}

			adapters.AddAssert(adapter);
		}

		if (factory != nullptr) {
			factory->Release();
		}
	}
	
	static void EnumerateDiscreteGPU(CapacityStream<IDXGIAdapter1*>& adapters) {
		//EnumGPU(adapters, DXGI_GPU_PREFERENCE_MINIMUM_POWER);
		EnumGPU(adapters, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE);
	}

	static void EnumerateIntegratedGPU(CapacityStream<IDXGIAdapter1*>& adapters) {
		EnumGPU(adapters, DXGI_GPU_PREFERENCE_MINIMUM_POWER);
	}

	Graphics::Graphics(const GraphicsDescriptor* descriptor)
		: m_target_view(nullptr), m_depth_stencil_view(nullptr), m_device(nullptr), m_context(nullptr), m_swap_chain(nullptr), m_allocator(descriptor->allocator),
		m_bound_render_target_count(1)
	{
		// The internal resources
		m_internal_resources.Initialize(descriptor->allocator, GRAPHICS_INTERNAL_RESOURCE_STARTING_COUNT);
		m_internal_resources_lock.Unlock();

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

		ECS_CRASH_CONDITION(SUCCEEDED(result), "Initializing device and swap chain failed!");

		result = m_device->CreateDeferredContext(0, &m_deferred_context);

		ECS_CRASH_CONDITION(SUCCEEDED(result), "Creating deferred context failed!");

		if (HasFlag(flags, D3D11_CREATE_DEVICE_DEBUG)) {
			HRESULT result = m_device->QueryInterface(__uuidof(ID3D11Debug), (void**)(&m_debug_device));
			if (FAILED(result)) {
				m_debug_device = nullptr;
			}
			result = m_device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)(&m_info_queue));
			if (FAILED(result)) {
				m_info_queue = nullptr;
			}
			else {
				// Change the maximum count of messages to 4K - a higher limit just in case
				m_info_queue->SetMessageCountLimit(ECS_KB * 4);
			}
		}
		else {
			m_debug_device = nullptr;
			m_info_queue = nullptr;
		}

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

		if (m_target_view.Interface() != nullptr) {
			AddInternalResource(m_target_view);
			AddInternalResource(m_depth_stencil_view);
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
		InitializeGraphicsCachedResources(this);
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
		BindRenderTargetViewFromInitialViews(m_context);
		m_bound_render_targets[0] = m_target_view;
		m_bound_render_target_count = 1;
		m_current_depth_stencil = m_depth_stencil_view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindRenderTargetViewFromInitialViews(GraphicsContext* context)
	{
		ECSEngine::BindRenderTargetView(m_target_view, m_depth_stencil_view, context);
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
		BindDefaultViewport(m_context);
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

#pragma region Helper Shader

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::BindMousePickShaderPixelCBuffer(ConstantBuffer buffer)
	{
		BindPixelConstantBuffer(buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer Graphics::CreateMousePickShaderPixelCBuffer(bool temporary)
	{
		return CreateConstantBuffer(sizeof(unsigned int), temporary);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UpdateMousePickShaderPixelCBuffer(ConstantBuffer buffer, unsigned int blended_value)
	{
		UpdateBuffer(buffer.buffer, &blended_value, sizeof(blended_value));
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexBuffer Graphics::SolidColorHelperShaderCreateVertexBufferInstances(unsigned int count, bool temporary) {
		return CreateVertexBuffer(sizeof(GraphicsSolidColorInstance), count, temporary);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsSolidColorInstance* Graphics::SolidColorHelperShaderMap(VertexBuffer vertex_buffer) {
		return (GraphicsSolidColorInstance*)MapBuffer(vertex_buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::SolidColorHelperShaderSetInstances(VertexBuffer vertex_buffer, Stream<GraphicsSolidColorInstance> instances)
	{
		GraphicsSolidColorInstance* data = (GraphicsSolidColorInstance*)SolidColorHelperShaderMap(vertex_buffer);
		instances.CopyTo(data);
		SolidColorHelperShaderUnmap(vertex_buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename IntegerType, typename IntegerType2>
	void SolidColorHelperShaderSetInstancesImpl(
		Graphics* graphics,
		VertexBuffer vertex_buffer,
		Stream<GraphicsSolidColorInstance> instances,
		Stream<IntegerType> instance_submesh,
		CapacityStream<IntegerType2>& submeshes
	) {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temporary_allocator, ECS_KB * 128, ECS_MB * 32);

		GraphicsSolidColorInstance* data = (GraphicsSolidColorInstance*)graphics->SolidColorHelperShaderMap(vertex_buffer);
		// Use the utility function ForEachGroup
		ForEachGroup(instance_submesh, &temporary_allocator,
			[&](size_t ordered_index, size_t original_index, size_t group_index) {
				data[ordered_index] = instances[original_index];
			},
			[&](size_t group_index, ulong2 group) {
				submeshes.AddAssert(group);
			});
		graphics->SolidColorHelperShaderUnmap(vertex_buffer);
	}

	void Graphics::SolidColorHelperShaderSetInstances(
		VertexBuffer vertex_buffer, 
		Stream<GraphicsSolidColorInstance> instances, 
		Stream<unsigned int> instance_submesh, 
		CapacityStream<uint2>& submeshes
	)
	{
		SolidColorHelperShaderSetInstancesImpl(this, vertex_buffer, instances, instance_submesh, submeshes);
	}

	void Graphics::SolidColorHelperShaderSetInstances(
		VertexBuffer vertex_buffer,
		Stream<GraphicsSolidColorInstance> instances,
		Stream<size_t> instance_submesh,
		CapacityStream<ulong2>& submeshes
	)
	{
		SolidColorHelperShaderSetInstancesImpl(this, vertex_buffer, instances, instance_submesh, submeshes);

	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::SolidColorHelperShaderUnmap(VertexBuffer vertex_buffer) {
		UnmapBuffer(vertex_buffer.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::SolidColorHelperShaderBind(VertexBuffer instance_vertex_buffer) {
		BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_SOLID_COLOR);
		SolidColorHelperShaderBindBuffer(instance_vertex_buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::SolidColorHelperShaderBindBuffer(VertexBuffer instance_vertex_buffer) {
		BindVertexBuffer(instance_vertex_buffer, 1);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::SolidColorHelperShaderMeshProperties(CapacityStream<ECS_MESH_INDEX>& properties) {
		// At the moment, only the position is needed
		properties.AddAssert(ECS_MESH_POSITION);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::SolidColorHelperShaderDraw(const CoalescedMesh& mesh, VertexBuffer instance_vertex_buffer, Stream<uint2> submeshes, bool bind_mesh_buffers) {
		ECS_STACK_CAPACITY_STREAM(ECS_MESH_INDEX, vertex_properties, ECS_MESH_BUFFER_COUNT);
		SolidColorHelperShaderMeshProperties(vertex_properties);
		if (bind_mesh_buffers) {
			BindMesh(mesh.mesh, vertex_properties);
		}
		SolidColorHelperShaderBind(instance_vertex_buffer);
		unsigned int drawn_instance_count = 0;
		for (size_t index = 0; index < submeshes.size; index++) {
			const Submesh& submesh = mesh.submeshes[submeshes[index].x];
			DrawIndexedInstanced(submesh.index_count, submeshes[index].y, submesh.index_buffer_offset, submesh.vertex_buffer_offset, drawn_instance_count);
			drawn_instance_count += submeshes[index].y;
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

#pragma endregion

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating index buffer failed");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating index buffer failed");
		AddInternalResource(buffer, temporary, debug_info);
		return buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ID3DBlob* ShaderByteCode(GraphicsDevice* device, Stream<char> source_code, ShaderCompileOptions options, ID3DInclude* include_policy, ECS_SHADER_TYPE type) {
		D3D_SHADER_MACRO macros[64];
		// For strings that are nullptr or of size 0, redirect them to this
		char empty_string = '\0';
		ECS_CRASH_CONDITION_RETURN(options.macros.size <= ECS_COUNTOF(macros) - 1, nullptr, "Too many shaders");
		ECS_STACK_CAPACITY_STREAM(char, null_terminated_strings, ECS_KB * 8);

		for (size_t index = 0; index < options.macros.size; index++) {
			// Write the strings into a temp buffer and null terminate them
			macros[index].Name = null_terminated_strings.buffer + null_terminated_strings.size;
			null_terminated_strings.AddStreamAssert(options.macros[index].name);
			null_terminated_strings.Add('\0');
			macros[index].Definition = options.macros[index].definition.size == 0 ? &empty_string : options.macros[index].definition.buffer;
		}
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

		return blob;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	PixelShader Graphics::CreatePixelShader(Stream<void> byte_code, bool temporary, DebugInfo debug_info)
	{
		PixelShader shader = { nullptr };

		HRESULT result;
		result = m_device->CreatePixelShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		if (FAILED(result)) {
			return shader;
		}
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
		VertexShader shader = { nullptr };

		HRESULT result;
		result = m_device->CreateVertexShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		if (FAILED(result)) {
			return shader;
		}
		
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

		VertexShader shader = CreateVertexShader({ byte_code->GetBufferPointer(), byte_code->GetBufferSize() }, temporary, debug_info);

		if (shader.shader == nullptr) {
			return shader;
		}

		if (vertex_byte_code != nullptr) {
			void* allocation = m_allocator->Allocate_ts(byte_code->GetBufferSize());
			vertex_byte_code->buffer = allocation;
			memcpy(allocation, byte_code->GetBufferPointer(), byte_code->GetBufferSize());
			vertex_byte_code->size = byte_code->GetBufferSize();
		}

		byte_code->Release();
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader Graphics::CreateDomainShader(Stream<void> byte_code, bool temporary, DebugInfo debug_info)
	{
		DomainShader shader = { nullptr };

		HRESULT result; 
		result = m_device->CreateDomainShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		
		if (FAILED(result)) {
			return shader;
		}

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
		HullShader shader = { nullptr };

		HRESULT result;
		result = m_device->CreateHullShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		
		if (FAILED(result)) {
			return shader;
		}

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
		GeometryShader shader = { nullptr };

		HRESULT result;
		result = m_device->CreateGeometryShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		
		if (FAILED(result)) {
			return shader;
		}

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
		ComputeShader shader = { nullptr };

		HRESULT result;
		result = m_device->CreateComputeShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);

		if (FAILED(result)) {
			return shader;
		}

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
			allocator = m_allocator;
			allocator.allocation_type = ECS_ALLOCATION_MULTI;
		}

		ID3DBlob* blob = ShaderByteCode(GetDevice(), source_code, options, include_policy, type);
		if (blob == nullptr) {
			return { nullptr, 0 };
		}

		void* allocation = AllocateTs(allocator.allocator, allocator.allocator_type, blob->GetBufferSize());
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), layout, "Creating input layout failed");
		
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

		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating vertex buffer failed");
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating vertex buffer failed");
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
		constant_buffer_descriptor.ByteWidth = AlignPointer(byte_size, 16);
		constant_buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		constant_buffer_descriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constant_buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		constant_buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		constant_buffer_descriptor.StructureByteStride = 0u;

		D3D11_SUBRESOURCE_DATA initial_data_constant = {};
		initial_data_constant.pSysMem = buffer_data;

		result = m_device->CreateBuffer(&constant_buffer_descriptor, &initial_data_constant, &buffer.buffer);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating pixel constant buffer failed.");
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
		constant_buffer_descriptor.ByteWidth = AlignPointer(byte_size, 16);
		constant_buffer_descriptor.Usage = GetGraphicsNativeUsage(usage);
		constant_buffer_descriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constant_buffer_descriptor.CPUAccessFlags = GetGraphicsNativeCPUAccess(cpu_access);
		constant_buffer_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(misc_flags);
		constant_buffer_descriptor.StructureByteStride = 0u;

		result = m_device->CreateBuffer(&constant_buffer_descriptor, nullptr, &buffer.buffer);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating pixel constant buffer failed.");
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating standard buffer failed.");
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating standard buffer failed.");
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating structred buffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating Structred Buffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating UA Buffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating UA Buffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating Indirect Buffer failed.");
		
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating Append Buffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating Consume Buffer failed.");

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

		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), state, "Creating sampler state failed!");
		AddInternalResource(state, temporary, debug_info);
		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<bool assert>
	void RemoveResourceFromTrackingImplementation(Graphics* graphics, void* resource) {
		// Spin lock while the resizing is finished
		graphics->m_internal_resources_lock.WaitLocked();
		graphics->m_internal_resources_reader_count.fetch_add(1, ECS_RELAXED);

		// If the resource_count and the write count are different, then wait until they get equal
		graphics->m_internal_resources.SpinWaitWrites();
		unsigned int resource_count = graphics->m_internal_resources.size.load(ECS_RELAXED);

		bool encountered_protected = false;
		for (unsigned int index = 0; index < resource_count && resource != nullptr; index++) {
			if (graphics->m_internal_resources[index].interface_pointer == resource) {
				bool expected = false;
				if (graphics->m_internal_resources[index].is_deleted.compare_exchange_weak(expected, true)) {
					// Check to see if the resource is protected. If it is protected, then set
					// The is_deleted_flag to false again, and continue onwards
					if (graphics->m_internal_resources[index].is_protected) {
						graphics->m_internal_resources[index].is_deleted.store(false, ECS_RELAXED);
						encountered_protected = true;
					}
					else {
						// Exit by changing the interface to nullptr
						resource = nullptr;
					}
				}
			}
		}

		graphics->m_internal_resources_reader_count.fetch_sub(1, ECS_RELAXED);
		if constexpr (assert) {
			const char* error_string = nullptr;
			if (encountered_protected) {
				error_string = "Trying to remove a Graphics protected resource!";
			}
			else {
				error_string = "Trying to remove resource from Graphics tracking that was not added";
			}
			ECS_CRASH_CONDITION_RETURN_VOID(resource == nullptr, error_string);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::FreeRenderDestination(RenderDestination destination)
	{
		FreeView(destination.render_view);
		FreeView(destination.depth_view);
		FreeResource(destination.output_view);
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

	static void SetProtectionStatusForResources(Graphics* graphics, Stream<void*> resources, bool status) {
		// Acquire the lock - we need exclusive rights for this operation
		graphics->m_internal_resources_lock.Lock();
		// Wait for any readers to exit
		SpinWait<'>'>(graphics->m_internal_resources_reader_count, (unsigned int)0);
		
		// For each resource that matches the ones given, change its protection status,
		// For all of the copies.
		unsigned int resource_count = graphics->m_internal_resources.SpinWaitWrites();
		for (unsigned int index = 0; index < resource_count; index++) {
			void* current_pointer = graphics->m_internal_resources[index].interface_pointer;
			if (SearchBytes(resources, current_pointer) != -1) {
				graphics->m_internal_resources[index].is_protected = status;
			}
		}

		graphics->m_internal_resources_lock.Unlock();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ProtectResources(Stream<void*> resources)
	{
		SetProtectionStatusForResources(this, resources, true);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::UnprotectResources(Stream<void*> resources)
	{
		SetProtectionStatusForResources(this, resources, false);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CreateWindowRenderTargetView(bool gamma_corrected)
	{
		// gain access to the texture subresource of the back buffer
		Texture2DDescriptor descriptor;
		descriptor.format = gamma_corrected ? ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB : ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		descriptor.bind_flag |= ECS_GRAPHICS_BIND_RENDER_TARGET;
		descriptor.size = m_window_size;

		Texture2D texture = CreateTexture(&descriptor);
		m_target_view = CreateRenderTargetView(texture);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RenderDestination Graphics::CreateRenderDestination(uint2 texture_size, GraphicsRenderDestinationOptions options) {
		RenderDestination render_destination;

		// Create the 2 textures
		Texture2DDescriptor render_texture_descriptor;
		render_texture_descriptor.size = texture_size;
		//render_texture_descriptor.format = options.gamma_corrected ? ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB : ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		// Create the texture with a typeless format such that it can be used as render target view, shader resource view or unordered resource view
		// This allows for all possible modes to be used - the type is given when constructing the unordered view
		render_texture_descriptor.format = ECS_GRAPHICS_FORMAT_RGBA8_TYPELESS;
		render_texture_descriptor.mip_levels = 1;
		render_texture_descriptor.bind_flag = ECS_GRAPHICS_BIND_RENDER_TARGET | ECS_GRAPHICS_BIND_SHADER_RESOURCE;
		if (options.unordered_render_view) {
			render_texture_descriptor.bind_flag |= ECS_GRAPHICS_BIND_UNORDERED_ACCESS;
		}
		render_texture_descriptor.misc_flag = options.render_misc;

		Texture2D render_texture = CreateTexture(&render_texture_descriptor);

		Texture2DDescriptor depth_texture_descriptor;
		depth_texture_descriptor.size = texture_size;
		depth_texture_descriptor.format = options.no_stencil ? ECS_GRAPHICS_FORMAT_D32_FLOAT : ECS_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
		depth_texture_descriptor.mip_levels = 1;
		depth_texture_descriptor.bind_flag = ECS_GRAPHICS_BIND_DEPTH_STENCIL;
		depth_texture_descriptor.misc_flag = options.depth_misc;

		Texture2D depth_texture = CreateTexture(&depth_texture_descriptor);

		ECS_GRAPHICS_FORMAT graphics_format = options.gamma_corrected ? ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB : ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		// Create the views now
		render_destination.render_view = CreateRenderTargetView(render_texture, 0, graphics_format);
		//render_destination.render_view = CreateRenderTargetView(render_texture);
		render_destination.depth_view = CreateDepthStencilView(depth_texture);
		render_destination.output_view = CreateTextureShaderView(render_texture, graphics_format);
		//render_destination.output_view = CreateTextureShaderView(render_texture);
		if (options.unordered_render_view) {
			render_destination.render_ua_view = CreateUAView(render_texture, 0, ECS_GRAPHICS_FORMAT_RGBA8_UNORM);
		}
		else {
			render_destination.render_ua_view = nullptr;
		}

		return render_destination;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CreateRenderTargetViewFromSwapChain(bool gamma_corrected)
	{
		// gain access to the texture subresource of the back buffer
		HRESULT result;
		Microsoft::WRL::ComPtr<ID3D11Resource> back_buffer;
		result = m_swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), &back_buffer);

		ECS_CRASH_CONDITION_RETURN_VOID(SUCCEEDED(result), "Acquiring buffer resource failed");

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

		ECS_CRASH_CONDITION_RETURN_VOID(SUCCEEDED(result), "Creating render target view failed");
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

		ECS_CRASH_CONDITION_RETURN_VOID(SUCCEEDED(result), "Creating depth texture failed");

		// creating depth stencil texture view
		result = m_device->CreateDepthStencilView(depth_texture, nullptr, &m_depth_stencil_view.view);

		ECS_CRASH_CONDITION_RETURN_VOID(SUCCEEDED(result), "Creating depth stencil view failed");
		depth_texture->Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsContext* Graphics::CreateDeferredContext(unsigned int context_flags, DebugInfo debug_info)
	{
		GraphicsContext* context = nullptr;
		HRESULT result = m_device->CreateDeferredContext(0, &context);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), context, "Failed to create deferred context.");

		AddInternalResource(context, debug_info);
		return context;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture1D Graphics::CreateTexture(const Texture1DDescriptor* ecs_descriptor, bool temporary, DebugInfo debug_info)
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

		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), resource, "Creating Texture 1D failed!");
		AddInternalResource(resource, temporary, debug_info);
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture2D Graphics::CreateTexture(const Texture2DDescriptor* ecs_descriptor, bool temporary, DebugInfo debug_info)
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

		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), resource, "Creating Texture 2D failed!");
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

		dx_descriptor.MiscFlags = GetGraphicsNativeMiscFlags(ECS_GRAPHICS_MISC_GENERATE_MIPS);
		dx_descriptor.Usage = GetGraphicsNativeUsage(ECS_GRAPHICS_USAGE_DEFAULT);
		dx_descriptor.Width = size.x;
		dx_descriptor.Height = size.y;
		dx_descriptor.MipLevels = 0;

		ID3D11Texture2D* texture_interface;
		ID3D11ShaderResourceView* texture_view = nullptr;

		HRESULT result = GetDevice()->CreateTexture2D(&dx_descriptor, nullptr, &texture_interface);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), nullptr, "Creating texture mip chain from first mip failed");

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
		if (FAILED(result)) {
			// Release the texture
			texture_interface->Release();
			ECS_CRASH_CONDITION_RETURN(false, nullptr, "Creating texture mip chain view from first mip failed");
		}

		// Generate mips
		GetContext()->GenerateMips(texture_view);

		if (!temporary) {
			AddInternalResource(Texture2D(texture_interface), debug_info);
			AddInternalResource(ResourceView(texture_view), debug_info);
		}

		return texture_view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Texture3D Graphics::CreateTexture(const Texture3DDescriptor* ecs_descriptor, bool temporary, DebugInfo debug_info)
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

		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), resource, "Creating Texture 3D failed!");
		AddInternalResource(resource, temporary, debug_info);
		return resource;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	TextureCube Graphics::CreateTexture(const TextureCubeDescriptor* ecs_descriptor, bool temporary, DebugInfo debug_info) {
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

		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), resource, "Creating Texture Cube failed!");
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating Texture 1D Shader View failed.");
		
		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture1D texture, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating Texture 1D Shader Entire View failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating Texture 2D Shader View.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture2D texture, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating Texture 2D Shader Entire View failed.");
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating Texture 3D Shader View failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(Texture3D texture, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating Texture 3D Shader Entire View failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating Texture Cube Shader View.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateTextureShaderViewResource(TextureCube texture, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(texture.tex, nullptr, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating Texture Cube Shader Entire View failed.");
		
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating standard buffer view failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ResourceView Graphics::CreateBufferView(StructuredBuffer buffer, bool temporary, DebugInfo debug_info)
	{
		ResourceView view;

		HRESULT result = m_device->CreateShaderResourceView(buffer.buffer, nullptr, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating structured buffer view failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RenderTargetView Graphics::CreateRenderTargetView(Texture2D texture, unsigned int mip_level, ECS_GRAPHICS_FORMAT format, bool temporary, DebugInfo debug_info)
	{
		HRESULT result;

		D3D11_TEXTURE2D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		RenderTargetView view;
		D3D11_RENDER_TARGET_VIEW_DESC descriptor;
		descriptor.Format = format == ECS_GRAPHICS_FORMAT_UNKNOWN ? texture_desc.Format : GetGraphicsNativeFormat(format);
		descriptor.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipSlice = mip_level;
		result = m_device->CreateRenderTargetView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating render target view failed!");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RenderTargetView Graphics::CreateRenderTargetView(TextureCube cube, TextureCubeFace face, unsigned int mip_level, ECS_GRAPHICS_FORMAT format, bool temporary, DebugInfo debug_info)
	{
		HRESULT result;

		D3D11_TEXTURE2D_DESC descriptor;
		cube.tex->GetDesc(&descriptor);

		RenderTargetView view;
		D3D11_RENDER_TARGET_VIEW_DESC view_descriptor;
		view_descriptor.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		view_descriptor.Format = format == ECS_GRAPHICS_FORMAT_UNKNOWN ? descriptor.Format : GetGraphicsNativeFormat(format);
		view_descriptor.Texture2DArray.ArraySize = 1;
		view_descriptor.Texture2DArray.FirstArraySlice = face;
		view_descriptor.Texture2DArray.MipSlice = mip_level;
		result = m_device->CreateRenderTargetView(cube.tex, &view_descriptor, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating render target view failed!");

		if (!temporary) {
			AddInternalResource(view, debug_info);
		}
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DepthStencilView Graphics::CreateDepthStencilView(Texture2D texture, bool temporary, DebugInfo debug_info)
	{
		HRESULT result;

		Texture2DDescriptor descriptor = GetTextureDescriptor(texture);
		D3D11_DEPTH_STENCIL_VIEW_DESC view_desc;
		D3D11_DEPTH_STENCIL_VIEW_DESC* view_desc_ptr = nullptr;
		if (IsGraphicsFormatTypeless(descriptor.format)) {
			view_desc_ptr = &view_desc;
			view_desc.Texture2D.MipSlice = 0;
			view_desc.Format = GetGraphicsNativeFormat(GetGraphicsFormatTypelessToDepth(descriptor.format));
			view_desc.Flags = 0;
			view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		}

		DepthStencilView view;
		result = m_device->CreateDepthStencilView(texture.tex, view_desc_ptr, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating render target view failed!");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating UAView from UABuffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating UAView from Indirect Buffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating UAView from Append Buffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating UAView from Consume Buffer failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture1D texture, unsigned int mip_slice, ECS_GRAPHICS_FORMAT format, bool temporary, DebugInfo debug_info) {
		UAView view;

		D3D11_TEXTURE1D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		descriptor.Texture1D.MipSlice = mip_slice;
		descriptor.Format = format == ECS_GRAPHICS_FORMAT_UNKNOWN ? texture_desc.Format : GetGraphicsNativeFormat(format);

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating UAView from Texture 1D failed.");
		
		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture2D texture, unsigned int mip_slice, ECS_GRAPHICS_FORMAT format, bool temporary, DebugInfo debug_info) {
		UAView view;

		D3D11_TEXTURE2D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		descriptor.Texture2D.MipSlice = mip_slice;
		// No SRGB formats
		descriptor.Format = format == ECS_GRAPHICS_FORMAT_UNKNOWN ? texture_desc.Format : GetGraphicsNativeFormat(format);

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating UAView from Texture 2D failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	UAView Graphics::CreateUAView(Texture3D texture, unsigned int mip_slice, ECS_GRAPHICS_FORMAT format, bool temporary, DebugInfo debug_info) {
		UAView view;

		D3D11_TEXTURE3D_DESC texture_desc;
		texture.tex->GetDesc(&texture_desc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor = {};
		descriptor.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		descriptor.Texture3D.MipSlice = mip_slice;
		descriptor.Format = format == ECS_GRAPHICS_FORMAT_UNKNOWN ? texture_desc.Format : GetGraphicsNativeFormat(format);

		HRESULT result = m_device->CreateUnorderedAccessView(texture.tex, &descriptor, &view.view);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating UAView from Texture 3D failed.");

		AddInternalResource(view, temporary, debug_info);
		return view;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	RasterizerState Graphics::CreateRasterizerState(const D3D11_RASTERIZER_DESC& descriptor, bool temporary, DebugInfo debug_info)
	{
		RasterizerState state;

		HRESULT result = m_device->CreateRasterizerState(&descriptor, &state.state);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), state, "Creating Rasterizer state failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), state, "Creating Depth Stencil State failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), state, "Creating Blend State failed.");

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

	GPUQuery Graphics::CreateQuery(bool temporary) {
		GPUQuery query;

		//GetDevice()->CreateQuery()

		return query;
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

	void Graphics::ClearRenderTarget(RenderTargetView target, ColorFloat color)
	{
		ECSEngine::ClearRenderTarget(target, GetContext(), color);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ClearDepth(DepthStencilView depth_stencil, float depth) 
	{
		ECSEngine::ClearDepth(depth_stencil, GetContext(), depth);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ClearStencil(DepthStencilView depth_stencil, unsigned char stencil) 
	{
		ECSEngine::ClearStencil(depth_stencil, GetContext(), stencil);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ClearDepthStencil(DepthStencilView depth_stencil, float depth, unsigned char stencil)
	{
		ECSEngine::ClearDepthStencil(depth_stencil, GetContext(), depth, stencil);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ClearBackBuffer(float red, float green, float blue) {
		ClearRenderTarget(m_target_view, ColorFloat(red, green, blue, 1.0f));
		ClearDepth(m_depth_stencil_view);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableAlphaBlending() {
		DisableAlphaBlending(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableAlphaBlending(GraphicsContext* context) {
		GraphicsPipelineBlendState state = ECSEngine::GetBlendState(context);

		D3D11_BLEND_DESC blend_desc = {};
		if (state.blend_state.Interface() != nullptr) {
			state.blend_state.state->GetDesc(&blend_desc);
		}
		blend_desc.RenderTarget[0].BlendEnable = FALSE;

		BlendState new_state = CreateBlendState(blend_desc, true);
		ECSEngine::BindBlendState(new_state, context);
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
		GraphicsPipelineDepthStencilState depth_stencil_state = ECSEngine::GetDepthStencilState(context);

		if (depth_stencil_state.depth_stencil_state.Interface() != nullptr) {
			depth_stencil_state.depth_stencil_state.state->GetDesc(&depth_desc);
		}
		depth_desc.DepthEnable = FALSE;

		DepthStencilState state = CreateDepthStencilState(depth_desc, true);
		ECSEngine::BindDepthStencilState(state, context);
		// This will be kept alive by the binding
		unsigned int count = state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableCulling(GraphicsContext* context, bool wireframe)
	{
		D3D11_RASTERIZER_DESC rasterizer_desc = {};

		GraphicsPipelineRasterizerState raster_state = ECSEngine::GetRasterizerState(context);
		if (raster_state.rasterizer_state.Interface() != nullptr) {
			raster_state.rasterizer_state.state->GetDesc(&rasterizer_desc);
		}
		
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.FillMode = wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;

		RasterizerState new_state = CreateRasterizerState(rasterizer_desc, true);
		ECSEngine::BindRasterizerState(new_state, context);
		// The binding will keep the state alive
		unsigned int count = new_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableWireframe()
	{
		DisableWireframe(GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DisableWireframe(GraphicsContext* context)
	{
		D3D11_RASTERIZER_DESC rasterizer_desc = {};

		GraphicsPipelineRasterizerState raster_state = ECSEngine::GetRasterizerState(context);
		if (raster_state.rasterizer_state.Interface() != nullptr) {
			raster_state.rasterizer_state.state->GetDesc(&rasterizer_desc);
		}

		if (rasterizer_desc.FillMode == D3D11_FILL_WIREFRAME) {
			rasterizer_desc.FillMode = D3D11_FILL_SOLID;

			RasterizerState new_state = CreateRasterizerState(rasterizer_desc, true);
			ECSEngine::BindRasterizerState(new_state, context);
			// The binding will keep the state alive
			unsigned int count = new_state.Release();
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::Dispatch(uint3 dispatch_size)
	{
		ECSEngine::Dispatch(dispatch_size, GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::Dispatch(Texture2D texture, uint3 compute_shader_threads)
	{
		ECSEngine::Dispatch(texture, compute_shader_threads, GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DispatchIndirect(IndirectBuffer buffer)
	{
		ECSEngine::DispatchIndirect(buffer, GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::Draw(unsigned int vertex_count, unsigned int vertex_offset)
	{
		ECSEngine::Draw(vertex_count, GetContext(), vertex_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawIndexed(unsigned int count, unsigned int start_index, unsigned int base_vertex_location) {
		ECSEngine::DrawIndexed(count, GetContext(), start_index, base_vertex_location);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawInstanced(
		unsigned int vertex_count, 
		unsigned int instance_count, 
		unsigned int vertex_buffer_offset,
		unsigned int instance_offset
	) {
		ECSEngine::DrawInstanced(vertex_count, instance_count, GetContext(), vertex_buffer_offset, instance_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawIndexedInstanced(
		unsigned int index_count, 
		unsigned int instance_count, 
		unsigned int index_buffer_offset,
		unsigned int vertex_buffer_offset, 
		unsigned int instance_offset
	) {
		ECSEngine::DrawIndexedInstanced(index_count, instance_count, GetContext(), index_buffer_offset, vertex_buffer_offset, instance_offset);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawIndexedInstancedIndirect(IndirectBuffer buffer) {
		ECSEngine::DrawIndexedInstancedIndirect(buffer, GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawInstancedIndirect(IndirectBuffer buffer) {
		ECSEngine::DrawInstancedIndirect(buffer, GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawMesh(const Mesh& mesh, const Material& material)
	{
		ECSEngine::DrawMesh(mesh, material, GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawMesh(const CoalescedMesh& mesh, unsigned int submesh_index, const Material& material)
	{
		ECSEngine::DrawMesh(mesh, submesh_index, material, GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawSubmeshCommand(Submesh submesh, unsigned int count)
	{
		ECSEngine::DrawSubmeshCommand(submesh, GetContext(), count);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::DrawCoalescedMeshCommand(const CoalescedMesh& mesh, unsigned int count)
	{
		ECSEngine::DrawCoalescedMeshCommand(mesh, GetContext(), count);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableAlphaBlending() {
		EnableAlphaBlending(GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableAlphaBlending(GraphicsContext* context) {
		GraphicsPipelineBlendState state = ECSEngine::GetBlendState(context);

		D3D11_BLEND_DESC blend_desc = {};
		if (state.blend_state.state != nullptr) {
			state.blend_state.state->GetDesc(&blend_desc);
		}
		
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		BlendState new_state = CreateBlendState(blend_desc, true);
		ECSEngine::BindBlendState(new_state, context);
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
		GraphicsPipelineDepthStencilState depth_stencil_state = ECSEngine::GetDepthStencilState(context);
		if (depth_stencil_state.depth_stencil_state.Interface() != nullptr) {
			depth_stencil_state.depth_stencil_state.state->GetDesc(&depth_desc);
		}

		depth_desc.DepthEnable = TRUE;
		depth_desc.DepthFunc = D3D11_COMPARISON_LESS;
		depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

		DepthStencilState state = CreateDepthStencilState(depth_desc, true);
		ECSEngine::BindDepthStencilState(state, context);
		// The binding will keep the resource alive
		unsigned int count = state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableCulling(bool wireframe)
	{
		EnableCulling(GetContext(), wireframe);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableCulling(GraphicsContext* context, bool wireframe)
	{
		D3D11_RASTERIZER_DESC rasterizer_desc = {};

		GraphicsPipelineRasterizerState raster_state = ECSEngine::GetRasterizerState(GetContext());
		if (raster_state.rasterizer_state.Interface() != nullptr) {
			raster_state.rasterizer_state.state->GetDesc(&rasterizer_desc);
		}
		
		rasterizer_desc.CullMode = D3D11_CULL_BACK;
		rasterizer_desc.FillMode = wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;

		RasterizerState new_state = CreateRasterizerState(rasterizer_desc, true);
		ECSEngine::BindRasterizerState(new_state, context);
		// The binding will keep the resource alive
		unsigned int count = new_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableWireframe()
	{
		EnableWireframe(GetContext());
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::EnableWireframe(GraphicsContext* context)
	{
		D3D11_RASTERIZER_DESC rasterizer_desc = {};

		GraphicsPipelineRasterizerState raster_state = ECSEngine::GetRasterizerState(context);
		if (raster_state.rasterizer_state.Interface() != nullptr) {
			raster_state.rasterizer_state.state->GetDesc(&rasterizer_desc);
		}

		if (rasterizer_desc.FillMode != D3D11_FILL_WIREFRAME) {
			rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;

			RasterizerState new_state = CreateRasterizerState(rasterizer_desc, true);
			ECSEngine::BindRasterizerState(new_state, context);
			// The binding will keep the resource alive
			unsigned int count = new_state.Release();
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	CommandList Graphics::FinishCommandList(bool restore_state, bool temporary, DebugInfo debug_info) {
		CommandList list = nullptr;
		HRESULT result;
		result = m_context->FinishCommandList(restore_state, &list.list);

		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), list, "Creating command list failed!");
		AddInternalResource(list, temporary, debug_info);
		return list;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::MapBuffer(ID3D11Buffer* buffer, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapBuffer(buffer, GetContext(), map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	MappedTexture Graphics::MapTexture(Texture1D texture, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, GetContext(), map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	MappedTexture Graphics::MapTexture(Texture2D texture, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, GetContext(), map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	MappedTexture Graphics::MapTexture(Texture3D texture, ECS_GRAPHICS_MAP_TYPE map_type, unsigned int subresource_index, unsigned int map_flags)
	{
		return ECSEngine::MapTexture(texture, GetContext(), map_type, subresource_index, map_flags);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::ChangeInitialRenderTarget(RenderTargetView render_target, DepthStencilView depth_stencil, bool bind) {
		m_depth_stencil_view = depth_stencil;
		m_target_view = render_target;

		if (bind) {
			BindRenderTargetView(render_target, depth_stencil);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::CommitInternalResourcesToBeFreed()
	{
		int32_t last_index = m_internal_resources.size.load(ECS_RELAXED);
		if (last_index < m_internal_resources.capacity) {
			m_internal_resources.SpinWaitWrites();
		}

		last_index = min(last_index, (int32_t)m_internal_resources.capacity);
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

	void Graphics::FreeTrackedResources()
	{
		// First commit the releases
		CommitInternalResourcesToBeFreed();

		unsigned int resource_count = m_internal_resources.size.load(ECS_RELAXED);

		for (unsigned int index = 0; index < resource_count; index++) {
			void* pointer = m_internal_resources[index].interface_pointer;
			ECS_GRAPHICS_RESOURCE_TYPE type = (ECS_GRAPHICS_RESOURCE_TYPE)m_internal_resources[index].type;

			const char* file = m_internal_resources[index].debug_info.file;
			const char* function = m_internal_resources[index].debug_info.function;
			unsigned int line = m_internal_resources[index].debug_info.line;

			FreeGraphicsResourceInterface(pointer, type);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::AddInternalResource(void* interface_ptr, ECS_GRAPHICS_RESOURCE_TYPE resource_type, DebugInfo debug_info)
	{
		GraphicsInternalResource internal_res = {
			interface_ptr,
			debug_info,
			resource_type,
			false,
			false
		};

		unsigned int initial_capacity = m_internal_resources.capacity;
		unsigned int write_index = m_internal_resources.RequestInt(1);
		if (write_index >= initial_capacity) {
			bool do_resizing = m_internal_resources_lock.TryLock();
			// The first one to acquire the lock - do the resizing or the flush of removals
			if (do_resizing) {
				// Spin wait while there are still readers
				SpinWait<'>'>(m_internal_resources_reader_count, (unsigned int)0);

				// Commit the removals
				CommitInternalResourcesToBeFreed();

				// Do the resizing if no resources were deleted from the main array
				if (m_internal_resources.size.load(ECS_RELAXED) >= m_internal_resources.capacity) {
					unsigned int new_capacity = initial_capacity * ECS_GRAPHICS_INTERNAL_RESOURCE_GROW_FACTOR;
					void* allocation = m_allocator->Allocate(sizeof(GraphicsInternalResource) * new_capacity);
					memcpy(allocation, m_internal_resources.buffer, sizeof(GraphicsInternalResource) * initial_capacity);
					m_allocator->Deallocate(m_internal_resources.buffer);
					m_internal_resources.InitializeFromBuffer(allocation, m_internal_resources.capacity, new_capacity);
				}
				write_index = m_internal_resources.RequestInt(1);
				m_internal_resources[write_index] = internal_res;
				m_internal_resources.FinishRequest(1);

				m_internal_resources_lock.Unlock();
			}
			// Other thread got to do the resizing or freeing of the resources - stall until it finishes
			else {
				m_internal_resources_lock.WaitLocked();
				// There might be threads that execute other operations that might acquire the lock
				// So it is not a guarantee that a resize has happened. Retrying again ensures that
				// No matter what operation was performed, then we will get to resize eventually.
				AddInternalResource(interface_ptr, resource_type, debug_info);
				return;
			}
		}
		// Write into the internal resources
		else {
			m_internal_resources[write_index] = internal_res;
			m_internal_resources.FinishRequest(1);
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

	GraphicsPipelineShaders Graphics::GetPipelineShaders() const
	{
		return ECSEngine::GetPipelineShaders(m_context);
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

	VertexShader Graphics::GetCurrentVertexShader() const
	{
		return ECSEngine::GetCurrentVertexShader(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	PixelShader Graphics::GetCurrentPixelShader() const
	{
		return ECSEngine::GetCurrentPixelShader(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader Graphics::GetCurrentDomainShader() const
	{
		return ECSEngine::GetCurrentDomainShader(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GeometryShader Graphics::GetCurrentGeometryShader() const
	{
		return ECSEngine::GetCurrentGeometryShader(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HullShader Graphics::GetCurrentHullShader() const
	{
		return ECSEngine::GetCurrentHullShader(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ComputeShader Graphics::GetCurrentComputeShader() const
	{
		return ECSEngine::GetCurrentComputeShader(m_context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* Graphics::GetCurrentShader(ECS_SHADER_TYPE shader_type) const
	{
		return ECSEngine::GetCurrentShader(m_context, shader_type);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsResourceSnapshot Graphics::GetResourceSnapshot(AllocatorPolymorphic allocator) {
		GraphicsResourceSnapshot snapshot;

		unsigned int resource_count = m_internal_resources.SpinWaitWrites();
		size_t total_size_to_allocate = snapshot.interface_pointers.MemoryOf(resource_count) + snapshot.types.MemoryOf(resource_count) 
			+ snapshot.debug_infos.MemoryOf(resource_count);

		void* allocation = Allocate(allocator, total_size_to_allocate);
		uintptr_t allocation_ptr = (uintptr_t)allocation;
		snapshot.interface_pointers.InitializeFromBuffer(allocation_ptr, resource_count);
		snapshot.types.InitializeFromBuffer(allocation_ptr, resource_count);
		snapshot.debug_infos.InitializeFromBuffer(allocation_ptr, resource_count);

		// There might be pending resources to be removed
		snapshot.interface_pointers.size = 0;
		snapshot.types.size = 0;
		snapshot.debug_infos.size = 0;
		for (unsigned int index = 0; index < resource_count; index++) {
			if (!m_internal_resources[index].is_deleted.load(ECS_RELAXED)) {
				snapshot.interface_pointers.Add(m_internal_resources[index].interface_pointer);
				snapshot.types.Add(m_internal_resources[index].type);
				snapshot.debug_infos.Add(m_internal_resources[index].debug_info);
			}
		}

		return snapshot;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	size_t Graphics::GetMemoryUsage(ECS_BYTE_UNIT_TYPE byte_unit)
	{
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device = nullptr;
		HRESULT success = m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device);
		if (FAILED(success)) {
			return 0;
		}

		Microsoft::WRL::ComPtr<IDXGIAdapter> base_adapter = nullptr;
		success = dxgi_device->GetAdapter(&base_adapter);
		if (FAILED(success)) {
			return 0;
		}

		Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter = nullptr;
		success = base_adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&adapter);
		if (FAILED(success)) {
			return 0;
		}

		DXGI_QUERY_VIDEO_MEMORY_INFO query_info;
		success = adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &query_info);
		if (FAILED(success)) {
			return 0;
		}

		return ConvertToByteUnit(query_info.CurrentUsage, byte_unit);
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

	void Graphics::ResizeSwapChainSize(HWND hWnd, unsigned int width, unsigned int height)
	{
		if (width != 0 && height != 0) {
			D3D11_RENDER_TARGET_VIEW_DESC descriptor;
			m_target_view.view->GetDesc(&descriptor);
			m_context->ClearState();
			m_target_view.view->Release();
			m_depth_stencil_view.view->Release();

			HRESULT result = m_swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

			ECS_CRASH_CONDITION_RETURN_VOID(SUCCEEDED(result), "Resizing swap chain failed!");

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
			CreateWindowDepthStencilView();

			ResizeViewport(0.0f, 0.0f, (float)width, (float)height);
			BindRenderTargetViewFromInitialViews(m_context);
			EnableDepth(m_context);

			DisableAlphaBlending(m_context);
		}
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

	template<typename Resource>
	Resource Graphics::TransferGPUResource(Resource resource, bool temporary, DebugInfo debug_info) {
		resource = ECSEngine::TransferGPUResource(resource, GetDevice());
		AddInternalResource(resource, temporary, debug_info);
		return resource;
	}

#define EXPANSION(Resource) ECS_TEMPLATE_FUNCTION(Resource, Graphics::TransferGPUResource, Resource, bool, DebugInfo);

	ECS_GRAPHICS_RESOURCES(EXPANSION);

#undef EXPANSION

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename View>
	View Graphics::TransferGPUView(View view, bool temporary, DebugInfo debug_info) {
		view = ECSEngine::TransferGPUView(view, GetDevice());
		//AddInternalResource(view, temporary, debug_info);
		//AddInternalResource(view.GetResource(), temporary, debug_info);
		return view;
	}

#define EXPANSION(View) ECS_TEMPLATE_FUNCTION(View, Graphics::TransferGPUView, View, bool, DebugInfo);

	ECS_GRAPHICS_VIEWS(EXPANSION);

#undef EXPANSION

	// ------------------------------------------------------------------------------------------------------------------------

	Mesh Graphics::TransferMesh(const Mesh* mesh, bool temporary, DebugInfo debug_info)
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
			AddInternalResource(new_mesh.index_buffer, debug_info);
			for (size_t index = 0; index < mesh->mapping_count; index++) {
				AddInternalResource(new_mesh.vertex_buffers[index], debug_info);
			}
		}

		return new_mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	CoalescedMesh Graphics::TransferCoalescedMesh(const CoalescedMesh* mesh, bool temporary, DebugInfo debug_info)
	{
		CoalescedMesh new_mesh;

		new_mesh.submeshes = mesh->submeshes;
		new_mesh.mesh = TransferMesh(&mesh->mesh, temporary, debug_info);

		return new_mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	PBRMesh Graphics::TransferPBRMesh(const PBRMesh* mesh, bool temporary, DebugInfo debug_info)
	{
		PBRMesh new_mesh;

		new_mesh.materials = mesh->materials;
		new_mesh.mesh = TransferCoalescedMesh(&mesh->mesh, temporary, debug_info);

		return new_mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	Material Graphics::TransferMaterial(const Material* material, bool temporary, DebugInfo debug_info)
	{
		Material new_material = *material;

		auto copy_buffers = [&](size_t offset, unsigned char copy_count) {
			ConstantBuffer* old_buffers = (ConstantBuffer*)OffsetPointer(material, offset);
			ConstantBuffer* new_buffers = (ConstantBuffer*)OffsetPointer(&new_material, offset);
			
			for (unsigned char index = 0; index < copy_count; index++) {
				new_buffers[index] = TransferGPUResource(old_buffers[index], GetDevice());
				AddInternalResource(new_buffers[index], temporary, debug_info);
			}
		};
		
		auto copy_textures = [&](size_t offset, unsigned char copy_count) {
			ResourceView* old_views = (ResourceView*)OffsetPointer(material, offset);
			ResourceView* new_views = (ResourceView*)OffsetPointer(&new_material, offset);

			for (unsigned char index = 0; index < copy_count; index++) {
				new_views[index] = TransferGPUView(old_views[index], GetDevice());
				AddInternalResource(new_views[index], temporary, debug_info);
				AddInternalResource(new_views[index].GetResource(), temporary, debug_info);
			}
		};

		copy_buffers(offsetof(Material, p_buffers), material->p_buffer_count);
		copy_buffers(offsetof(Material, v_buffers), material->v_buffer_count);

		copy_textures(offsetof(Material, v_textures), material->v_texture_count);
		copy_textures(offsetof(Material, p_textures), material->p_texture_count);;

		// The unordered views need to be handled separately
		for (unsigned char index = 0; index < material->unordered_view_count; index++) {
			new_material.unordered_views[index] = TransferGPUView(material->unordered_views[index], GetDevice());
			AddInternalResource(new_material.unordered_views[index], temporary, debug_info);
			AddInternalResource(new_material.unordered_views[index].GetResource(), temporary, debug_info);
		}

		return new_material;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::IsInternalStateValid()
	{
		// Commit the resource freeing first
		CommitInternalResourcesToBeFreed();

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
		ResizableStream<GraphicsLiveObject> live_objects(&stack_allocator, 512);
		bool success = ExtractLiveObjectsFromDebugInterface(this, &live_objects);
		if (!success) {
			return false;
		}
		
		// Try to validate the internal resources now
		unsigned int resource_count = m_internal_resources.Size();
		for (unsigned int index = 0; index < resource_count; index++) {
			// Don't have to check for is_deleted since we commited those freed resources

			unsigned int subindex = 0;
			for (; subindex < live_objects.size; subindex++) {
				if (live_objects[subindex].interface_pointer == m_internal_resources[index].interface_pointer) {
					break;
				}
			}
			if (subindex == live_objects.size) {
				// It wasn't found in the live objects list - fail
				return false;
			}
			else {
				// Decrement the reference count such that we know that decrementing it is still valid
				if (live_objects[subindex].external_reference_count == 0) {
					return false;
				}
				live_objects[subindex].external_reference_count--;
			}
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::PrintRuntimeMessagesToConsole()
	{
		if (m_info_queue) {
			unsigned int message_count = m_info_queue->GetNumStoredMessagesAllowedByRetrievalFilter();

			if (message_count > 0) {
				ECS_STACK_CAPACITY_STREAM(char, current_message, ECS_KB * 4);
				D3D11_MESSAGE* queue_message = (D3D11_MESSAGE*)current_message.buffer;
				size_t message_capacity = current_message.capacity - sizeof(D3D11_MESSAGE);

				for (unsigned int index = 0; index < message_count; index++) {
					message_capacity = message_capacity = current_message.capacity - sizeof(D3D11_MESSAGE);
					HRESULT result = m_info_queue->GetMessageW(index, queue_message, &message_capacity);
					if (SUCCEEDED(result)) {
						GetConsole()->Graphics({ queue_message->pDescription, queue_message->DescriptionByteLength - 1 });
					}
				}
				// Always clear the stored messages such that we never get into issues with not enough space
				m_info_queue->ClearStoredMessages();
			}
		}
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

	void Graphics::RestorePipelineRenderState(const GraphicsPipelineRenderState* state)
	{
		ECSEngine::RestorePipelineRenderState(GetContext(), state);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RestorePipelineShaders(const GraphicsPipelineShaders* shaders)
	{
		ECSEngine::RestorePipelineShaders(GetContext(), shaders);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RestoreBoundViews(GraphicsBoundViews views)
	{
		BindRenderTargetView(views.target, views.depth_stencil);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void Graphics::RestorePipelineState(const GraphicsPipelineState* state)
	{
		RestorePipelineRenderState(&state->render_state);
		RestoreBoundViews(state->views);
		BindViewport(state->viewport);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::RestoreResourceSnapshot(const GraphicsResourceSnapshot& snapshot, CapacityStream<char>* mismatch_string) {
		unsigned int current_resource_count = m_internal_resources.SpinWaitWrites();
		bool size_success = true;
		if (current_resource_count < snapshot.interface_pointers.size) {
			if (mismatch_string == nullptr) {
				return false;
			}
			else {
				FormatString(*mismatch_string, "Expected {#} resources in the graphics object, instead there are {#}.",
					snapshot.interface_pointers.size, current_resource_count);
				size_success = false;
			}
		}

		// Keep a boolean record of all the resources in the snapshot that have been identified
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(bool, was_found, snapshot.interface_pointers.size);
		memset(was_found.buffer, 0, was_found.MemoryOf(snapshot.interface_pointers.size));

		for (unsigned int index = 0; index < current_resource_count; index++) {
			// Locate the value in the snapshot.
			if (!m_internal_resources[index].is_deleted.load(ECS_RELAXED)) {
				// DirectX can return the same pointer when resources are created with the same parameters.
				// So keep looking for places that were also not yet checked
				void* interface_pointer = m_internal_resources[index].interface_pointer;
				size_t snapshot_index = SearchBytes(snapshot.interface_pointers, interface_pointer);
				while (snapshot_index != -1 && (snapshot_index < snapshot.interface_pointers.size - 1) && was_found[snapshot_index]) {
					size_t current_offset = SearchBytes(
						snapshot.interface_pointers.buffer + snapshot_index + 1,
						snapshot.interface_pointers.size - snapshot_index - 1,
						(size_t)interface_pointer,
						sizeof(interface_pointer)
					);
					if (current_offset != -1) {
						snapshot_index += current_offset + 1;
					}
				}

				// It doesn't exist, it means it was added in the meantime
				if (snapshot_index == -1) {
					// Free the type and mark the current slot as deleted
					m_internal_resources[index].is_deleted.store(true, ECS_RELAXED);

					// For debugging purposes
					DebugInfo debug_info = m_internal_resources[index].debug_info;

					if (mismatch_string != nullptr) {
						Stream<char> resource_type = GraphicsResourceTypeString(m_internal_resources[index].type);
						ECS_STACK_CAPACITY_STREAM(char, location_string, 512);
						DebugLocationString(m_internal_resources[index].debug_info, &location_string);
						FormatString(*mismatch_string, "Graphics resource with type {#} was added in between snapshots. {#}\n", resource_type, location_string);
					}

					FreeGraphicsResourceInterface(interface_pointer, m_internal_resources[index].type);
				}
				else {
					was_found[snapshot_index] = true;
				}
			}
		}

		if (mismatch_string == nullptr) {
			// Now check for all the old resources to see if they have been accidentally removed
			// If there is a false value then it means that an old resource was removed
			size_t first_missing = SearchBytes(was_found.buffer, snapshot.interface_pointers.size, (size_t)false, sizeof(bool));
			return first_missing == -1 && size_success;
		}
		else {
			bool is_missing = false;
			size_t missing = SearchBytes(was_found.buffer, snapshot.interface_pointers.size, (size_t)false, sizeof(bool));
			while (missing < snapshot.interface_pointers.size) {
				is_missing = true;
				Stream<char> resource_type = GraphicsResourceTypeString(snapshot.types[missing]);
				ECS_STACK_CAPACITY_STREAM(char, location_string, 512);
				DebugLocationString(snapshot.debug_infos[missing], &location_string);
				FormatString(*mismatch_string, "Graphics resource with type {#} was removed in between snapshots. {#}\n", resource_type, location_string);

				missing += SearchBytes(was_found.buffer + missing, snapshot.interface_pointers.size - missing, (size_t)false, sizeof(bool));
			}

			return !is_missing && size_success;
		}
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
		bool success = m_shader_reflection->ReflectVertexShaderInputSource(source_code, element_descriptors, &allocator);
		if (!success) {
			return {};
		}

		InputLayout layout = CreateInputLayout(element_descriptors, vertex_byte_code, deallocate_byte_code, temporary, debug_info);
		return layout;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderBuffers(Stream<char> source_code, CapacityStream<ShaderReflectedBuffer>& buffers, AllocatorPolymorphic allocator) const {
		return m_shader_reflection->ReflectShaderBuffersSource(source_code, buffers, allocator);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderTextures(Stream<char> source_code, CapacityStream<ShaderReflectedTexture>& textures, AllocatorPolymorphic allocator) const
	{
		return m_shader_reflection->ReflectShaderTexturesSource(source_code, textures, allocator);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderMacros(
		Stream<char> source_code, 
		CapacityStream<Stream<char>>* defined_macros, 
		CapacityStream<Stream<char>>* conditional_macros, 
		AllocatorPolymorphic allocator
	) const {
		return m_shader_reflection->ReflectShaderMacros(source_code, defined_macros, conditional_macros, allocator);
	}

	// ------------------------------------------------------------------------------------------------------------------------
	
	bool Graphics::ReflectVertexBufferMapping(Stream<char> source_code, CapacityStream<ECS_MESH_INDEX>& mapping) const {
		return m_shader_reflection->ReflectVertexBufferMappingSource(source_code, mapping);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShaderSamplers(Stream<char> source_code, CapacityStream<ShaderReflectedSampler>& samplers, AllocatorPolymorphic allocator) const {
		return m_shader_reflection->ReflectShaderSamplersSource(source_code, samplers, allocator);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectShader(Stream<char> source_code, const ReflectedShader* reflected_shader) const
	{
		return m_shader_reflection->ReflectShader(source_code, reflected_shader);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::ReflectComputeShaderDispatchSize(Stream<char> source_code, uint3* dispatch_size) const
	{
		return m_shader_reflection->ReflectComputeShaderDispatchSize(source_code, dispatch_size);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	// This function is used both by the VerifyLeaks and VerifyLeaksAndValidateInteralState
	template<bool validate_internal_state>
	static bool VerifyLeaksAndInternalStateImpl(Graphics* graphics, bool& internal_state_valid, CapacityStream<GraphicsLiveObject>* leaks) {
		// Commit the internal resources to be freed
		graphics->CommitInternalResourcesToBeFreed();

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
		ResizableStream<GraphicsLiveObject> live_objects(&stack_allocator, 512);
		bool success = ExtractLiveObjectsFromDebugInterface(graphics, &live_objects);
		if (!success) {
			return false;
		}

		// Returns true if it was found, else false
		auto decrement_count = [&](void* pointer, unsigned int decrement_count = 1) {
			for (unsigned int index = 0; index < live_objects.size; index++) {
				if (pointer == live_objects[index].interface_pointer) {
					// Decrement the reference count for the live_object
					live_objects[index].external_reference_count -= decrement_count;
					return true;
				}
			}
			return false;
		};

		internal_state_valid = true;
		unsigned int resource_count = graphics->m_internal_resources.Size();
		// Go through our all internal resources and decrement the count for the corresponding live object
		for (unsigned int index = 0; index < resource_count; index++) {
			// Don't need to check for the is_deleted flag since the freed resources were removed
			bool was_found = decrement_count(graphics->m_internal_resources[index].interface_pointer);
			if constexpr (validate_internal_state) {
				if (!was_found) {
					internal_state_valid = false;
				}
			}
		}

		// Now search for the known resources
		unsigned int device_decrement_count = 1;
		device_decrement_count += graphics->m_debug_device != nullptr;
		device_decrement_count += graphics->m_info_queue != nullptr;
		// The device has only 1 reference count - which includes an increment from all the other objects
		decrement_count(graphics->m_device, device_decrement_count + live_objects.size);
		decrement_count(graphics->m_context);
		decrement_count(graphics->m_swap_chain);
		decrement_count(graphics->m_deferred_context);

		for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
			if (graphics->m_shader_helpers[index].vertex.Interface() != nullptr) {
				decrement_count(graphics->m_shader_helpers[index].vertex.Interface());
			}
			if (graphics->m_shader_helpers[index].pixel.Interface() != nullptr) {
				decrement_count(graphics->m_shader_helpers[index].pixel.Interface());
			}
			if (graphics->m_shader_helpers[index].input_layout.Interface() != nullptr) {
				decrement_count(graphics->m_shader_helpers[index].input_layout.Interface());
			}
			if (graphics->m_shader_helpers[index].pixel_sampler.Interface() != nullptr) {
				decrement_count(graphics->m_shader_helpers[index].pixel_sampler.Interface());
			}
			if (graphics->m_shader_helpers[index].compute.Interface() != nullptr) {
				decrement_count(graphics->m_shader_helpers[index].compute.Interface());
			}
		}

		for (size_t index = 0; index < ECS_GRAPHICS_DEPTH_STENCIL_HELPER_COUNT; index++) {
			decrement_count(graphics->m_depth_stencil_helpers[index].Interface());
		}

		for (size_t index = 0; index < ECS_GRAPHICS_CACHED_VERTEX_BUFFER_COUNT; index++) {
			decrement_count(graphics->m_cached_resources.vertex_buffer[index].Interface());
		}

		// Now go through all live objects and if one of them has a reference count non zero, then we have a leak
		for (unsigned int index = 0; index < live_objects.size; index++) {
			if (live_objects[index].external_reference_count > 0) {
				leaks->AddAssert(live_objects[index]);
			}
		}

		return true;
	}

	bool Graphics::VerifyLeaks(CapacityStream<GraphicsLiveObject>* leaks)
	{
		bool valid_state_dummy;
		bool success = VerifyLeaksAndInternalStateImpl<false>(this, valid_state_dummy, leaks);
		if (!success) {
			return false;
		}
		return leaks->size == 0;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	bool Graphics::VerifyLeaksAndValidateInternalState(bool& internal_state_valid, CapacityStream<GraphicsLiveObject>* leaks)
	{
		return VerifyLeaksAndInternalStateImpl<true>(this, internal_state_valid, leaks);
	}

	// ------------------------------------------------------------------------------------------------------------------------

#pragma region Free functions

	// ------------------------------------------------------------------------------------------------------------------------

	MemoryManager DefaultGraphicsAllocator(GlobalMemoryManager* manager)
	{
		return MemoryManager(DefaultGraphicsAllocatorSize(), ECS_KB * 4, DefaultGraphicsAllocatorSize(), manager);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	size_t DefaultGraphicsAllocatorSize()
	{
		return 150'000;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void FreeGraphicsResourceInterface(void* interface_pointer, ECS_GRAPHICS_RESOURCE_TYPE type)
	{
#define CASE(type, pointer_type) case type: { pointer_type* resource = (pointer_type*)interface_pointer; ref_count = resource->Release(); } break;
#define CASE2(type0, type1, pointer_type) case type0: case type1: { pointer_type* resource = (pointer_type*)interface_pointer; ref_count = resource->Release(); } break;
#define CASE3(type0, type1, type2, pointer_type) case type0: case type1: case type2: { pointer_type* resource = (pointer_type*)interface_pointer; ref_count = resource->Release(); } break;

		unsigned int ref_count = 0;

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
			CASE(ECS_GRAPHICS_RESOURCE_DX_RESOURCE_INTERFACE, ID3D11Resource);
		default:
			ECS_ASSERT(false);

#undef CASE
#undef CASE2
#undef CASE3
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DestroyGraphics(Graphics* graphics)
	{
		// Walk through the list of resources that the graphic stores and free them
		// Use a switch case to handle the pointer identification
		
		// Unbind the resource bound to the pipeline
		graphics->m_context->ClearState();	

		graphics->FreeTrackedResources();

		for (size_t index = 0; index < ECS_GRAPHICS_SHADER_HELPER_COUNT; index++) {
			if (graphics->m_shader_helpers[index].vertex.Interface() != nullptr) {
				graphics->m_shader_helpers[index].vertex.Release();
			}
			if (graphics->m_shader_helpers[index].pixel.Interface() != nullptr) {
				graphics->m_shader_helpers[index].pixel.Release();
			}
			if (graphics->m_shader_helpers[index].input_layout.Interface() != nullptr) {
				graphics->m_shader_helpers[index].input_layout.Release();
			}
			if (graphics->m_shader_helpers[index].pixel_sampler.Interface() != nullptr) {
				graphics->m_shader_helpers[index].pixel_sampler.Release();
			}
			if (graphics->m_shader_helpers[index].compute.Interface() != nullptr) {
				graphics->m_shader_helpers[index].compute.Release();
			}
		}

		for (size_t index = 0; index < ECS_GRAPHICS_DEPTH_STENCIL_HELPER_COUNT; index++) {
			graphics->m_depth_stencil_helpers[index].Release();
		}

		for (size_t index = 0; index < ECS_GRAPHICS_CACHED_VERTEX_BUFFER_COUNT; index++) {
			graphics->m_cached_resources.vertex_buffer[index].Release();
		}

		// Deallocate the helper resources array
		graphics->m_allocator->Deallocate(graphics->m_shader_helpers.buffer);
		graphics->m_allocator->Deallocate(graphics->m_depth_stencil_helpers.buffer);

		graphics->m_context->Flush();

		if (graphics->m_debug_device == nullptr) {
			ID3D11Debug* debug_device = nullptr;
			HRESULT result = graphics->GetDevice()->QueryInterface(__uuidof(ID3D11Debug), (void**)(&debug_device));
			if (SUCCEEDED(result)) {
				result = debug_device->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
				debug_device->Release();
			}
		}
		else {
			graphics->m_debug_device->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
			graphics->m_debug_device->Release();
		}

		if (graphics->m_info_queue != nullptr) {
			graphics->m_info_queue->Release();
		}

		unsigned int count = graphics->m_device->Release();
		count = graphics->m_deferred_context->Release();
		count = graphics->m_context->Release();
		// Deallocate the buffer for the resource tracking
		graphics->m_allocator->Deallocate(graphics->m_internal_resources.buffer);

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
			context->PSSetSamplers(start_slot, samplers.size, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_VERTEX:
			context->VSSetSamplers(start_slot, samplers.size, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_COMPUTE:
			context->CSSetSamplers(start_slot, samplers.size, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_HULL:
			context->HSSetSamplers(start_slot, samplers.size, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_DOMAIN:
			context->DSSetSamplers(start_slot, samplers.size, (ID3D11SamplerState**)samplers.buffer);
			break;
		case ECS_SHADER_GEOMETRY:
			context->GSSetSamplers(start_slot, samplers.size, (ID3D11SamplerState**)samplers.buffer);
			break;
		}
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

		for (size_t index = 0; index < mapping.size; index++) {
			vertex_buffers[index] = mesh.GetBuffer(mapping[index]);
			ECS_ASSERT(vertex_buffers[index].Interface() != nullptr, "The given mesh does not have the necessary vertex buffers.");
		}

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

	void ClearDepth(DepthStencilView view, GraphicsContext* context, float depth)
	{
		context->ClearDepthStencilView(view.view, D3D11_CLEAR_DEPTH, depth, 0);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void ClearStencil(DepthStencilView view, GraphicsContext* context, unsigned char stencil)
	{
		context->ClearDepthStencilView(view.view, D3D11_CLEAR_STENCIL, 0.0f, stencil);
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
		descriptor.Width = ClampMin<unsigned int>(descriptor.Width >> mip_level, 1);
		descriptor.Height = ClampMin<unsigned int>(descriptor.Height >> mip_level, 1);
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

	void Dispatch(Texture2D texture, uint3 compute_thread_sizes, GraphicsContext* context)
	{
		D3D11_TEXTURE2D_DESC descriptor;
		texture.tex->GetDesc(&descriptor);

		unsigned int x_count = SlotsFor(descriptor.Width, compute_thread_sizes.x);
		unsigned int y_count = SlotsFor(descriptor.Height, compute_thread_sizes.y);

		Dispatch({ x_count, y_count, 1 }, context);
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

	void DrawMesh(const Mesh& mesh, const Material& material, GraphicsContext* context)
	{
		BindMesh(mesh, context);
		BindMaterial(material, context);
		DrawIndexed(mesh.index_buffer.count, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawMesh(const CoalescedMesh& coalesced_mesh, unsigned int submesh_index, const Material& material, GraphicsContext* context)
	{
		BindMesh(coalesced_mesh.mesh, context);
		BindMaterial(material, context);
		Submesh submesh = coalesced_mesh.submeshes[submesh_index];
		DrawSubmeshCommand(submesh, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawSubmeshCommand(Submesh submesh, GraphicsContext* context, unsigned int count)
	{
		if (count == 1) {
			DrawIndexed(submesh.index_count, context, submesh.index_buffer_offset, submesh.vertex_buffer_offset);
		}
		else {
			DrawIndexedInstanced(submesh.index_count, count, context, submesh.index_buffer_offset, submesh.vertex_buffer_offset);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void DrawCoalescedMeshCommand(const CoalescedMesh& coalesced_mesh, GraphicsContext* context, unsigned int count)
	{
		if (count == 1) {
			DrawIndexed(coalesced_mesh.mesh.index_buffer.count, context);
		}
		else {
			DrawIndexedInstanced(coalesced_mesh.mesh.index_buffer.count, count, context);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineRenderState GetPipelineRenderState(GraphicsContext* context)
	{
		GraphicsPipelineRenderState state;

		state.rasterizer_state = GetRasterizerState(context);
		state.blend_state = GetBlendState(context);
		state.depth_stencil_state = GetDepthStencilState(context);
		state.shaders = GetPipelineShaders(context);

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

		return { viewport.Width, viewport.Height, viewport.TopLeftX, viewport.TopLeftY, viewport.MinDepth, viewport.MaxDepth };
	}

	// ------------------------------------------------------------------------------------------------------------------------

	InputLayout GetCurrentInputLayout(GraphicsContext* context)
	{
		ID3D11InputLayout* layout;
		context->IAGetInputLayout(&layout);
		if (layout != nullptr) {
			layout->Release();
		}
		return layout;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	VertexShader GetCurrentVertexShader(GraphicsContext* context)
	{
		ID3D11VertexShader* shader;
		context->VSGetShader(&shader, nullptr, nullptr);
		if (shader != nullptr) {
			shader->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	PixelShader GetCurrentPixelShader(GraphicsContext* context)
	{
		ID3D11PixelShader* shader;
		context->PSGetShader(&shader, nullptr, nullptr);
		if (shader != nullptr) {
			shader->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	DomainShader GetCurrentDomainShader(GraphicsContext* context)
	{
		ID3D11DomainShader* shader;
		context->DSGetShader(&shader, nullptr, nullptr);
		if (shader != nullptr) {
			shader->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GeometryShader GetCurrentGeometryShader(GraphicsContext* context)
	{
		ID3D11GeometryShader* shader;
		context->GSGetShader(&shader, nullptr, nullptr);
		if (shader != nullptr) {
			shader->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HullShader GetCurrentHullShader(GraphicsContext* context)
	{
		ID3D11HullShader* shader;
		context->HSGetShader(&shader, nullptr, nullptr);
		if (shader != nullptr) {
			shader->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ComputeShader GetCurrentComputeShader(GraphicsContext* context)
	{
		ID3D11ComputeShader* shader;
		context->CSGetShader(&shader, nullptr, nullptr);
		if (shader != nullptr) {
			shader->Release();
		}
		return shader;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void* GetCurrentShader(GraphicsContext* context, ECS_SHADER_TYPE shader_type)
	{
		switch (shader_type) {
		case ECS_SHADER_VERTEX:
			return GetCurrentVertexShader(context).Interface();
		case ECS_SHADER_PIXEL:
			return GetCurrentPixelShader(context).Interface();
		case ECS_SHADER_DOMAIN:
			return GetCurrentDomainShader(context).Interface();
		case ECS_SHADER_GEOMETRY:
			return GetCurrentGeometryShader(context).Interface();
		case ECS_SHADER_HULL:
			return GetCurrentHullShader(context).Interface();
		case ECS_SHADER_COMPUTE:
			return GetCurrentComputeShader(context).Interface();
		default:
			ECS_ASSERT(false, "Invalid shader type");
		}

		return nullptr;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineRasterizerState GetRasterizerState(GraphicsContext* context)
	{
		GraphicsPipelineRasterizerState state;

		context->RSGetState(&state.rasterizer_state.state);
		unsigned int count = -1;
		if (state.rasterizer_state.state != nullptr) {
			count = state.rasterizer_state.Release();
		}

		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineShaders GetPipelineShaders(GraphicsContext* context)
	{
		GraphicsPipelineShaders shaders;
		
		shaders.layout = GetCurrentInputLayout(context);
		shaders.vertex_shader = GetCurrentVertexShader(context);
		shaders.pixel_shader = GetCurrentPixelShader(context);
		shaders.domain_shader = GetCurrentDomainShader(context);
		shaders.hull_shader = GetCurrentHullShader(context);
		shaders.geometry_shader = GetCurrentGeometryShader(context);
		shaders.compute_shader = GetCurrentComputeShader(context);

		return shaders;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineDepthStencilState GetDepthStencilState(GraphicsContext* context)
	{
		GraphicsPipelineDepthStencilState state;

		context->OMGetDepthStencilState(&state.depth_stencil_state.state, &state.stencil_ref);
		unsigned int count = -1;
		if (state.depth_stencil_state.Interface() != nullptr) {
			count = state.depth_stencil_state.Release();
		}
		
		return state;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineBlendState GetBlendState(GraphicsContext* context)
	{
		GraphicsPipelineBlendState state;

		context->OMGetBlendState(&state.blend_state.state, state.blend_factors, &state.sample_mask);
		unsigned int count = -1;
		if (state.blend_state.Interface() != nullptr) {
			count = state.blend_state.Release();
		}

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), mapped_subresource, error_string);
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
	MappedTexture MapTexture(
		Texture texture,
		GraphicsContext* context,
		ECS_GRAPHICS_MAP_TYPE map_type,
		unsigned int subresource_index,
		unsigned int map_flags
	)
	{
		D3D11_MAPPED_SUBRESOURCE mapped_subresource = MapResourceInternal(texture, context, map_type, subresource_index, map_flags, "Mapping a texture failed.");
		return { mapped_subresource.pData, mapped_subresource.RowPitch, mapped_subresource.DepthPitch };
	}

	// Cringe bug from intellisense that makes all the file full of errors when in reality everything is fine; instantiations must
	// be unrolled manually
	ECS_TEMPLATE_FUNCTION(MappedTexture, MapTexture, Texture1D, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(MappedTexture, MapTexture, Texture2D, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);
	ECS_TEMPLATE_FUNCTION(MappedTexture, MapTexture, Texture3D, GraphicsContext*, ECS_GRAPHICS_MAP_TYPE, unsigned int, unsigned int);

	// ------------------------------------------------------------------------------------------------------------------------

	void RestorePipelineRenderState(GraphicsContext* context, const GraphicsPipelineRenderState* render_state)
	{
		RestoreBlendState(context, render_state->blend_state);
		RestoreDepthStencilState(context, render_state->depth_stencil_state);
		RestoreRasterizerState(context, render_state->rasterizer_state);
		RestorePipelineShaders(context, &render_state->shaders);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreBoundViews(GraphicsContext* context, GraphicsBoundViews views)
	{
		BindRenderTargetView(views.target, views.depth_stencil, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestorePipelineState(GraphicsContext* context, const GraphicsPipelineState* state)
	{
		RestorePipelineRenderState(context, &state->render_state);
		RestoreBoundViews(context, state->views);
		BindViewport(state->viewport, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreBlendState(GraphicsContext* context, GraphicsPipelineBlendState blend_state)
	{
		context->OMSetBlendState(blend_state.blend_state.state, blend_state.blend_factors, blend_state.sample_mask);
		//blend_state.blend_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreRasterizerState(GraphicsContext* context, GraphicsPipelineRasterizerState rasterizer_state)
	{
		BindRasterizerState(rasterizer_state.rasterizer_state, context);
		//rasterizer_state.rasterizer_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestorePipelineShaders(GraphicsContext* context, const GraphicsPipelineShaders* shaders)
	{
		BindInputLayout(shaders->layout, context);
		BindVertexShader(shaders->vertex_shader, context);
		BindPixelShader(shaders->pixel_shader, context);
		BindDomainShader(shaders->domain_shader, context);
		BindHullShader(shaders->hull_shader, context);
		BindGeometryShader(shaders->geometry_shader, context);
		BindComputeShader(shaders->compute_shader, context);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void RestoreDepthStencilState(GraphicsContext* context, GraphicsPipelineDepthStencilState depth_stencil_state)
	{
		BindDepthStencilState(depth_stencil_state.depth_stencil_state, context, depth_stencil_state.stencil_ref);
		//depth_stencil_state.depth_stencil_state.Release();
	}

	// ------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* TransferGPUResourceImpl(ID3D11Resource* dx_resource, GraphicsDevice* device) {
		// Acquire the DXGIResource interface from the DX resource
		ID3D11Resource* new_dx_resource = nullptr;

		IDXGIResource* dxgi_resource;
		HRESULT result = dx_resource->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgi_resource);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), new_dx_resource, "Getting the DXGI resource from shared resource failed.");

		// Query interface updates the reference count, release it to maintain invariance
		unsigned int count = dxgi_resource->Release();

		HANDLE handle;
		result = dxgi_resource->GetSharedHandle(&handle);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), new_dx_resource, "Acquiring a handle for a shared resource failed.");

		ECS_CRASH_CONDITION_RETURN(handle, new_dx_resource, "Trying to transfer a GPU resource that was not created with MISC flag");

		result = device->OpenSharedResource(handle, __uuidof(ID3D11Resource), (void**)&new_dx_resource);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), new_dx_resource, "Obtaining shared resource from handle failed.");

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
			ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Converting a shared resource to buffer failed.");

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
			static_assert(false);
			ECS_ASSERT(false);
		}

		return new_resource;
	}

#define EXPORT_TRANSFER_GPU(resource) template ECSENGINE_API resource TransferGPUResource(resource, GraphicsDevice*);

	ECS_GRAPHICS_RESOURCES(EXPORT_TRANSFER_GPU);

#undef EXPORT_TRANSFER_GPU

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename View>
	View TransferGPUView(View view, GraphicsDevice* device) {
		ID3D11Resource* resource = view.GetResource();

		ID3D11Resource* copied_resource = TransferGPUResourceImpl(resource, device);
		return View::RawCopy(device, copied_resource, view);
	}

#define EXPORT_TRANSFER_GPU(resource) template ECSENGINE_API resource TransferGPUView(resource, GraphicsDevice*);

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
		ECS_CRASH_CONDITION_RETURN_VOID(SUCCEEDED(result), "Updating texture failed.");

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
		ECS_CRASH_CONDITION_RETURN_VOID(SUCCEEDED(result), "Updating texture failed.");

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
		ECS_CRASH_CONDITION_RETURN_VOID(SUCCEEDED(result), "Updating buffer failed.");

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
		ECS_CRASH_CONDITION_RETURN_VOID(SUCCEEDED(result), "Updating buffer resource failed.");

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
			graphics->FreeView(material->v_textures[index]);
		}

		for (size_t index = 0; index < material->p_texture_count; index++) {
			graphics->FreeView(material->p_textures[index]);
		}

		// Release the UAVs
		for (size_t index = 0; index < material->unordered_view_count; index++) {
			graphics->FreeView(material->unordered_views[index]);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	void FreeCoalescedMesh(Graphics* graphics, CoalescedMesh* mesh, bool coalesced_allocation, AllocatorPolymorphic allocator)
	{
		graphics->FreeMesh(mesh->mesh);
		if (coalesced_allocation) {
			Deallocate(allocator, mesh->submeshes[0].name.buffer);
		}
		else {
			for (size_t index = 0; index < mesh->submeshes.size; index++) {
				mesh->submeshes[index].Deallocate(allocator);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------

	template<typename MeshType>
	CoalescedMesh MergeMeshesImplementation(Graphics* graphics, Stream<MeshType*> meshes, AllocatorPolymorphic allocator, const MergeMeshesOptions* options) {
		static_assert(std::is_same_v<MeshType, Mesh> || std::is_same_v<MeshType, CoalescedMesh>, "MergeMeshes handle additional mesh type!");

		MergeMeshesOptions default_options;
		if (options == nullptr) {
			options = &default_options;
		}

		CoalescedMesh result;
		result.mesh.bounds = InfiniteAABBScalar();

		// Walk through the meshes and determine the maximum amount of buffers. The meshes that are missing some buffers will have them
		// zeroed in the final mesh

		typedef Mesh& (*GetMesh)(MeshType* mesh);
		typedef size_t (*GetSubmeshCount)(const MeshType* mesh);
		typedef Submesh (*GetSubmesh)(const MeshType* mesh, size_t index);

		GetMesh get_mesh = nullptr;
		GetSubmeshCount get_submesh_count = nullptr;
		GetSubmesh get_submesh = nullptr;
		if constexpr (std::is_same_v<MeshType, Mesh>) {
			get_mesh = [](MeshType* mesh) -> Mesh& {
				return *mesh;
			};
			get_submesh_count = [](const MeshType* mesh) {
				return (size_t)1;
			};
			get_submesh = [](const MeshType* mesh, size_t index) {
				Submesh submesh;
				submesh.bounds = InfiniteAABBScalar();
				submesh.index_buffer_offset = 0;
				submesh.vertex_buffer_offset = 0;
				submesh.index_count = mesh->index_buffer.count;
				submesh.vertex_count = mesh->vertex_buffers[0].size;
				submesh.name = mesh->name;
				return submesh;
			};
		}
		else if constexpr (std::is_same_v<MeshType, CoalescedMesh>) {
			get_mesh = [](MeshType* mesh) -> Mesh& {
				return mesh->mesh;
			};
			get_submesh_count = [](const MeshType* mesh) {
				return mesh->submeshes.size;
			};
			get_submesh = [](const MeshType* mesh, size_t index) {
				return mesh->submeshes[index];
			};
		}

		ECS_ASSERT(get_mesh(meshes[0]).index_buffer.int_size == 4, "Merging meshes with integer size different from 4 is not accepted!");

		Stream<Submesh> submeshes;

		// Accumulate the total vertex buffer size and index buffer size
		size_t vertex_buffer_size = 0;
		size_t index_buffer_size = 0;
		size_t mapping_count = get_mesh(meshes[0]).mapping_count;
		size_t index_buffer_int_size = get_mesh(meshes[0]).index_buffer.int_size;

		ECS_MESH_INDEX mappings[ECS_MESH_BUFFER_COUNT];
		memcpy(mappings, get_mesh(meshes[0]).mapping, sizeof(ECS_MESH_INDEX) * mapping_count);
		size_t submesh_count = 0;

		for (size_t index = 0; index < meshes.size; index++) {
			const Mesh& current_mesh = get_mesh(meshes[index]);
			vertex_buffer_size += current_mesh.vertex_buffers[0].size;
			index_buffer_size += current_mesh.index_buffer.count;
			if (mapping_count < current_mesh.mapping_count) {
				mapping_count = current_mesh.mapping_count;
				// Update the mapping order
				memcpy(mappings, current_mesh.mapping, sizeof(ECS_MESH_INDEX) * mapping_count);
			}

			ECS_ASSERT(index_buffer_int_size == current_mesh.index_buffer.int_size, "Merging meshes with different index buffer int sizes is not accepted!");

			submesh_count += get_submesh_count(meshes[index]);
		}

		// Allocate the submeshes
		submeshes.Initialize(allocator, submesh_count);

		// Create a temporary zero vertex buffer for those cases when some mappings are missing
		// Allocate using calloc since it will yield better results
		void* zero_memory = calloc(vertex_buffer_size, sizeof(float4));
		VertexBuffer zero_vertex_buffer = graphics->CreateVertexBuffer(sizeof(float4), vertex_buffer_size, true);
		free(zero_memory);

		// Create the new vertex buffers and the index buffer
		VertexBuffer new_vertex_buffers[ECS_MESH_BUFFER_COUNT];
		for (size_t index = 0; index < mapping_count; index++) {
			new_vertex_buffers[index] = graphics->CreateVertexBuffer(
				GetMeshIndexElementByteSize(mappings[index]),
				vertex_buffer_size,
				false,
				ECS_GRAPHICS_USAGE_DEFAULT,
				ECS_GRAPHICS_CPU_ACCESS_NONE,
				options->misc_flags
			);
		}

		IndexBuffer new_index_buffer = graphics->CreateIndexBuffer(get_mesh(meshes[0]).index_buffer.int_size, index_buffer_size, false, ECS_GRAPHICS_USAGE_DEFAULT, ECS_GRAPHICS_CPU_ACCESS_NONE, options->misc_flags);

		// All vertex buffers must have the same size - so a single offset suffices
		unsigned int vertex_buffer_offset = 0;
		unsigned int index_buffer_offset = 0;

		size_t overall_submesh_index = 0;

		// Copy the vertex buffer and index buffer submeshes
		for (size_t index = 0; index < meshes.size; index++) {
			Mesh& current_mesh = get_mesh(meshes[index]);

			bool buffers_comitted[ECS_MESH_BUFFER_COUNT] = { false };

			size_t mesh_vertex_buffer_size = current_mesh.vertex_buffers[0].size;
			// Vertex buffers
			for (size_t buffer_index = 0; buffer_index < current_mesh.mapping_count; buffer_index++) {
				size_t new_vertex_index = 0;
				// Determine the vertex buffer position from the global vertex buffer
				for (size_t subindex = 0; subindex < mapping_count; subindex++) {
					if (mappings[subindex] == current_mesh.mapping[buffer_index]) {
						new_vertex_index = subindex;
						buffers_comitted[subindex] = true;
						// Exit the loop
						subindex = mapping_count;
					}
				}

				CopyBufferSubresource(
					new_vertex_buffers[new_vertex_index],
					vertex_buffer_offset,
					current_mesh.vertex_buffers[buffer_index],
					0,
					mesh_vertex_buffer_size,
					graphics->GetContext()
				);

				if (options->free_existing_meshes) {
					// Release all the old buffers
					graphics->FreeResource(current_mesh.vertex_buffers[buffer_index]);
					current_mesh.vertex_buffers[buffer_index].buffer = nullptr;
				}
			}

			// For all the buffers which are missing, zero the data with a global zero vector
			for (size_t buffer_index = 0; buffer_index < mapping_count; buffer_index++) {
				if (!buffers_comitted[buffer_index]) {
					// Temporarly set the zero_vertex_buffer.stride to the stride of the new vertex buffer such that the copy is done correctly,
					// else it will overcopy for types smaller than float4
					zero_vertex_buffer.stride = new_vertex_buffers[buffer_index].stride;
					CopyBufferSubresource(new_vertex_buffers[buffer_index], vertex_buffer_offset, zero_vertex_buffer, 0, mesh_vertex_buffer_size, graphics->GetContext());
				}
			}

			
			unsigned int index_buffer_count = current_mesh.index_buffer.count;
			unsigned int current_mesh_vertex_offset = vertex_buffer_offset;
			unsigned int current_mesh_index_offset = index_buffer_offset;

			// Copy the index buffer data
			CopyBufferSubresource(new_index_buffer, index_buffer_offset, current_mesh.index_buffer, 0, index_buffer_count, graphics->GetContext());
			
			index_buffer_offset += index_buffer_count;

			// all vertex buffers must have the same size
			vertex_buffer_offset += current_mesh.vertex_buffers[0].size;

			if (options->free_existing_meshes) {
				graphics->FreeResource(current_mesh.index_buffer);
				current_mesh.index_buffer.buffer = nullptr;
				current_mesh.mapping_count = 0;
			}

			size_t submesh_count = get_submesh_count(meshes[index]);
			for (size_t submesh_index = 0; submesh_index < submesh_count; submesh_index++) {
				Submesh current_submesh = get_submesh(meshes[index], submesh_index);

				submeshes[overall_submesh_index] = current_submesh;
				// Modify the offsets, since these have changed
				submeshes[overall_submesh_index].index_buffer_offset = current_mesh_index_offset;
				submeshes[overall_submesh_index].vertex_buffer_offset = current_mesh_vertex_offset;
				if (submeshes[overall_submesh_index].name.size > 0) {
					submeshes[overall_submesh_index].name = submeshes[overall_submesh_index].name.Copy(allocator);
				}
				overall_submesh_index++;

				current_mesh_index_offset += current_submesh.index_count;
				current_mesh_vertex_offset += current_submesh.vertex_count;
			}

			ECS_ASSERT(current_mesh_index_offset == index_buffer_offset, "Merging an invalid mesh: its submesh index count is different from the its index buffer count.");
			ECS_ASSERT(current_mesh_vertex_offset == vertex_buffer_offset, "Merging an invalid mesh: its submesh vertex count is different from the its vertex buffer count.");
		}

		result.mesh.index_buffer = new_index_buffer;
		for (size_t index = 0; index < mapping_count; index++) {
			result.mesh.vertex_buffers[index] = new_vertex_buffers[index];
			result.mesh.mapping[index] = mappings[index];
		}
		result.mesh.mapping_count = mapping_count;
		result.submeshes = submeshes;
		result.mesh.name = {};

		// Release the zero vertex buffer
		zero_vertex_buffer.Release();

		return result;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	CoalescedMesh MergeMeshes(Graphics* graphics, Stream<Mesh*> meshes, AllocatorPolymorphic allocator, const MergeMeshesOptions* options) {
		return MergeMeshesImplementation(graphics, meshes, allocator, options);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	CoalescedMesh MergeCoalescedMeshes(Graphics* graphics, Stream<CoalescedMesh*> meshes, AllocatorPolymorphic allocator, const MergeMeshesOptions* options) {
		return MergeMeshesImplementation(graphics, meshes, allocator, options);
	}

	// ------------------------------------------------------------------------------------------------------------------------

	CoalescedMesh MergeCoalescedMeshesAsCoalescedSubmeshes(
		Graphics* graphics,
		Stream<CoalescedMesh*> meshes,
		AllocatorPolymorphic allocator,
		const MergeMeshesOptions* options
	) {
		// Use the normal call, even tho it is a bit more inefficient in the sense that a larger submesh
		// Buffer is used, but it is not all that important
		CoalescedMesh coalesced_mesh = MergeCoalescedMeshes(graphics, meshes, allocator, options);
		
		// For each mesh from the original set, count the length of the submeshes and replace the entries
		unsigned int total_submesh_vertex_length = 0;
		unsigned int total_submesh_index_length = 0;
		for (size_t index = 0; index < meshes.size; index++) {
			unsigned int current_vertex_length = meshes[index]->mesh.vertex_buffers[0].size;
			unsigned int current_index_length = meshes[index]->mesh.index_buffer.count;
			coalesced_mesh.submeshes[index].vertex_buffer_offset = total_submesh_vertex_length;
			coalesced_mesh.submeshes[index].index_buffer_offset = total_submesh_index_length;
			coalesced_mesh.submeshes[index].vertex_count = current_vertex_length;
			coalesced_mesh.submeshes[index].index_count = current_index_length;

			total_submesh_index_length += current_index_length;
			total_submesh_vertex_length += current_vertex_length;
		}

		coalesced_mesh.submeshes.size = meshes.size;
		return coalesced_mesh;
	}

	// ------------------------------------------------------------------------------------------------------------------------

#pragma endregion



#endif // ECSENGINE_DIRECTX11

#endif // ECSENGINE_PLATFORM_WINDOWS
}