#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"
#include "ShaderReflection.h"
#include "../Allocators/MemoryManager.h"

namespace ECSEngine {

	ECS_CONTAINERS;

#ifdef ECSENGINE_PLATFORM_WINDOWS

#ifdef ECSENGINE_DIRECTX11

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

	/* It has an immediate and a deferred context. The deferred context can be used to generate CommandLists */
	class ECSENGINE_API Graphics
	{
		friend class Bindable;
	public:
		Graphics(HWND hWnd, const GraphicsDescriptor* descriptor);
		Graphics& operator = (const Graphics& other) = default;

#pragma region Auto Context Bindings

		// ----------------------------------------------- Auto Bindings ------------------------------------------------------------------

		void BindIndexBuffer(IndexBuffer index_buffer);

		void BindInputLayout(InputLayout layout);

		void BindVertexShader(VertexShader shader);

		void BindPixelShader(PixelShader shader);

		void BindVertexConstantBuffer(ConstantBuffer buffer, UINT slot = 0u);

		void BindVertexConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindPixelConstantBuffer(ConstantBuffer buffer, UINT slot = 0u);

		void BindPixelConstantBuffers(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindVertexBuffer(VertexBuffer buffer, UINT slot = 0u);

		void BindVertexBuffers(Stream<VertexBuffer> buffers, UINT start_slot = 0u);

		void BindTopology(Topology topology);

		void BindPSResourceView(ResourceView component, UINT slot = 0u);

		void BindPSResourceViews(Stream<ResourceView> component, UINT start_slot = 0u);

		void BindSamplerState(SamplerState sampler, UINT slot = 0u);

		void BindSamplerStates(Stream<SamplerState> samplers, UINT start_slot = 0u);

#pragma endregion

#pragma region Argument (Deferred) Context Bindings

		// ----------------------------------------------- Argument (Deferred) Context Bindings --------------------------------

		void BindIndexBuffer(IndexBuffer index_buffer, GraphicsContext* context);

		void BindInputLayout(InputLayout layout, GraphicsContext* context);

		void BindVertexShader(VertexShader shader, GraphicsContext* context);

		void BindPixelShader(PixelShader shader, GraphicsContext* context);

		void BindVertexConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot = 0u);

		void BindVertexConstantBuffers(
			Stream<ConstantBuffer> buffers,
			GraphicsContext* context,
			UINT start_slot = 0u
		);

		void BindPixelConstantBuffer(ConstantBuffer buffer, GraphicsContext* context, UINT slot = 0u);

		void BindPixelConstantBuffers(
			Stream<ConstantBuffer> buffers,
			GraphicsContext* context,
			UINT start_slot = 0u
		);

		void BindVertexBuffer(VertexBuffer buffer, GraphicsContext* context, UINT slot = 0u);

		void BindVertexBuffers(Stream<VertexBuffer> buffers, GraphicsContext* context, UINT start_slot = 0u);

		void BindTopology(Topology topology, GraphicsContext* context);

		void BindPSResourceView(ResourceView component, GraphicsContext* context, UINT slot = 0u);

		void BindPSResourceViews(Stream<ResourceView> component, GraphicsContext* context, UINT start_slot = 0u);

		void BindSamplerState(SamplerState sampler, GraphicsContext* context, UINT slot = 0u);

		void BindSamplerStates(Stream<SamplerState> samplers, GraphicsContext* context, UINT start_slot = 0u);

		void BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view);

		void BindRenderTargetView(RenderTargetView render_view, DepthStencilView depth_stencil_view, GraphicsContext* context);

		void BindRenderTargetViewFromInitialViews();

		void BindRenderTargetViewFromInitialViews(GraphicsContext* context);

		void BindViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth);

		void BindViewport(float top_left_x, float top_left_y, float new_width, float new_height, float min_depth, float max_depth, GraphicsContext* context);

		void BindDefaultViewport();

		void BindDefaultViewport(GraphicsContext* context);

#pragma endregion

#pragma region Immediate Context Bindings

		// ----------------------------------------------- Immediate Bindings ------------------------------------------------------------------

		void BindIndexBufferImmediate(IndexBuffer index_buffer);

		void BindInputLayoutImmediate(InputLayout layout);

		void BindVertexShaderImmediate(VertexShader shader);

		void BindPixelShaderImmediate(PixelShader shader);

		void BindVertexConstantBufferImmediate(ConstantBuffer buffer, UINT slot = 0u);

		void BindVertexConstantBuffersImmediate(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindPixelConstantBufferImmediate(ConstantBuffer buffer, UINT slot = 0u);

		void BindPixelConstantBuffersImmediate(Stream<ConstantBuffer> buffers, UINT start_slot = 0u);

		void BindVertexBufferImmediate(VertexBuffer buffer, UINT slot = 0u);

		void BindVertexBuffersImmediate(Stream<VertexBuffer> buffers, UINT start_slot = 0u);

		void BindTopologyImmediate(Topology topology);

		void BindPSResourceViewImmediate(ResourceView component, UINT slot = 0u);

		void BindPSResourceViewsImmediate(Stream<ResourceView> component, UINT start_slot = 0u);

		void BindSamplerStateImmediate(SamplerState sampler, UINT slot = 0u);

		void BindSamplerStatesImmediate(Stream<SamplerState> samplers, UINT start_slot = 0u);

#pragma endregion

#pragma region Component Construction

		// ----------------------------------------------- Component Construction ----------------------------------------------------
		
		IndexBuffer ConstructIndexBuffer(Stream<unsigned int> indices);

		PixelShader ConstructPSShader(const wchar_t* path);

		PixelShader ConstructPSShader(Stream<wchar_t> path);

		VertexShader ConstructVSShader(const wchar_t* path);

		VertexShader ConstructVSShader(Stream<wchar_t> path);

		InputLayout ConstructInputLayout(Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, VertexShader vertex_shader);

		VertexBuffer ConstructVertexBuffer(
			size_t element_size, 
			size_t element_count, 
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC, 
			unsigned int cpuFlags = D3D11_CPU_ACCESS_WRITE, 
			unsigned int miscFlags = 0
		);

		VertexBuffer ConstructVertexBuffer(
			size_t element_size,
			size_t element_count,
			const void* buffer,
			D3D11_USAGE usage = D3D11_USAGE_IMMUTABLE,
			unsigned int cpuFlags = 0,
			unsigned int miscFlags = 0
		);
		
		ConstantBuffer ConstructConstantBuffer(
			size_t byte_size,
			const void* buffer,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0
		);

		ConstantBuffer ConstructConstantBuffer(
			size_t byte_size,
			D3D11_USAGE usage = D3D11_USAGE_DYNAMIC,
			unsigned int cpuAccessFlags = D3D11_CPU_ACCESS_WRITE,
			unsigned int miscFlags = 0
		);

		SamplerState ConstructSamplerState(D3D11_SAMPLER_DESC descriptor);

		Texture1D CreateTexture1D(const GraphicsTexture1DDescriptor* descriptor);

		Texture2D CreateTexture2D(const GraphicsTexture2DDescriptor* descriptor);

		Texture3D CreateTexture3D(const GraphicsTexture3DDescriptor* descriptor);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTexture1DShaderView(
			Texture1D texture,
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1
		);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTexture2DShaderView(
			Texture2D texture,
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1
		);

		// DXGI_FORMAT_FORCE_UINT means get the format from the texture descriptor
		ResourceView CreateTexture3DShaderView(
			Texture3D texture,
			DXGI_FORMAT format = DXGI_FORMAT_FORCE_UINT,
			unsigned int most_detailed_mip = 0u,
			unsigned int mip_levels = -1
		);

		RenderTargetView CreateRenderTargetView(Texture2D texture);

		DepthStencilView CreateDepthStencilView(Texture2D texture);

#pragma endregion

#pragma region Change Context Mode

		// -------------------------------------------------- Change Context Mode ---------------------------------------------

		void SetAutoMode(GraphicsContext* context);

		void SetAutoModeImmediate();

		void SetAutoModeDeferred();

#pragma endregion

#pragma region Auto Context Pipeline State Changes

		// ------------------------------------------- Auto Context Pipeline State Changes ------------------------------------

		// auto context
		void ClearBackBuffer(float red, float green, float blue);

		// auto context
		void DisableAlphaBlending();

		// auto context
		void DisableDepth();

		// auto context
		void DisableCulling();

		// auto context
		void Draw(UINT vertex_count, UINT start_slot = 0u);

		// auto context
		void DrawIndexed(unsigned int index_count, UINT start_vertex = 0u, INT base_vertex_location = 0);

		// auto context
		void EnableAlphaBlending();

		// auto context
		void EnableDepth();

		// It must be unmapped manually; auto context
		void* MapBuffer(ID3D11Buffer* buffer, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture1D(Texture1D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture2D(Texture2D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture3D(Texture3D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		// auto context
		ID3D11CommandList* FinishCommandList(bool restore_state = false);

		// auto context
		void UpdateBuffer(ID3D11Buffer* buffer, const void* data, size_t data_size, unsigned int subresource_index = 0);

		// auto context
		void UnmapBuffer(ID3D11Buffer* buffer, unsigned int subresource_index = 0);

		// auto context
		void UnmapTexture1D(Texture1D texture, unsigned int subresource_index = 0);

		// auto context
		void UnmapTexture2D(Texture2D texture, unsigned int subresource_index = 0);

		// auto context
		void UnmapTexture3D(Texture3D texture, unsigned int subresource_index = 0);

#pragma endregion

#pragma region Argument (Deferred) Context Pipeline State Changes

		// -------------------------------------- Argument(Deferred) Context Pipeline State Changes ---------------------------

		void ClearBackBuffer(float red, float green, float blue, GraphicsContext* context);

		void DisableAlphaBlending(GraphicsContext* context);

		void DisableDepth(GraphicsContext* context);

		void DisableCulling(GraphicsContext* context);

		void Draw(UINT vertex_count, GraphicsContext* context, UINT start_slot = 0u);

		void DrawIndexed(unsigned int index_count, GraphicsContext* context, UINT start_vertex = 0u, INT base_vertex_location = 0);

		void EnableAlphaBlending(GraphicsContext* context);

		void EnableDepth(GraphicsContext* context);

		ID3D11CommandList* FinishCommandList(GraphicsContext* context, bool restore_state = false);

		// It must be unmapped manually
		void* MapBuffer(
			ID3D11Buffer* buffer,
			GraphicsContext* context,
			D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, 
			unsigned int subresource_index = 0,
			unsigned int map_flags = 0
		);

		void* MapTexture1D(
			Texture1D texture, 
			GraphicsContext* context,
			D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, 
			unsigned int subresource_index = 0,
			unsigned int map_flags = 0
		);

		void* MapTexture2D(
			Texture2D texture,
			GraphicsContext* context,
			D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD,
			unsigned int subresource_index = 0,
			unsigned int map_flags = 0
		);

		void* MapTexture3D(
			Texture3D texture,
			GraphicsContext* context,
			D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD,
			unsigned int subresource_index = 0,
			unsigned int map_flags = 0
		);

		void UpdateBuffer(
			ID3D11Buffer* buffer,
			const void* data,
			size_t data_size,
			GraphicsContext* context,
			unsigned int subresource_index = 0
		);

		void UnmapBuffer(ID3D11Buffer* buffer, GraphicsContext* context, unsigned int subresource_index = 0);

		void UnmapTexture1D(Texture1D texture, GraphicsContext* context, unsigned int subresource_index = 0);

		void UnmapTexture2D(Texture2D texture, GraphicsContext* context, unsigned int subresource_index = 0);

		void UnmapTexture3D(Texture3D texture, GraphicsContext* context, unsigned int subresource_index = 0);

#pragma endregion

#pragma region Immediate Context Pipeline State Changes

		// ----------------------------------------------- Immediate Context State Changes -------------------------------------

		void ClearBackBufferImmediate(float red, float green, float blue);

		void DisableAlphaBlendingImmediate();

		void DisableDepthImmediate();

		void DisableCullingImmediate();

		void DrawImmediate(UINT vertex_count, UINT start_slot = 0u);

		void DrawIndexedImmediate(unsigned int index_count, UINT start_vertex = 0u, INT base_vertex_location = 0);

		void EnableAlphaBlendingImmediate();

		void EnableDepthImmediate();

		// It must be unmapped manually
		void* MapBufferImmediate(ID3D11Buffer* buffer, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture1DImmediate(Texture1D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture2DImmediate(Texture2D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void* MapTexture3DImmediate(Texture3D texture, D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD, unsigned int subresource_index = 0, unsigned int map_flags = 0);

		void UpdateBufferImmediate(ID3D11Buffer* buffer, const void* data, size_t data_size, unsigned int subresource_index = 0);

		void UnmapBufferImmediate(ID3D11Buffer* buffer, unsigned int subresource_index = 0);

		void UnmapTexture1DImmediate(Texture1D texture, unsigned int subresource_index = 0);

		void UnmapTexture2DImmediate(Texture2D texture, unsigned int subresource_index = 0);

		void UnmapTexture3DImmediate(Texture3D texture, unsigned int subresource_index = 0);

#pragma endregion

#pragma region

		// ------------------------------------------------- Shader Reflection --------------------------------------------------

		// Shader path might point to a .cso so a path parameter should be used to supply the actual 
		// source code
		InputLayout ReflectVertexShaderInput(VertexShader shader, const wchar_t* path);

		InputLayout ReflectVertexShaderInput(VertexShader shader, Stream<wchar_t> path);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		void ReflectShaderBuffers(const wchar_t* path, CapacityStream<ShaderReflectedBuffer>& buffers);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		void ReflectShaderBuffers(Stream<wchar_t> path, CapacityStream<ShaderReflectedBuffer>& buffers);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		void ReflectShaderTextures(const wchar_t* path, CapacityStream<ShaderReflectedTexture>& textures);

		// The memory needed for the buffer names will be allocated from the assigned allocator
		void ReflectShaderBuffers(Stream<wchar_t> path, CapacityStream<ShaderReflectedTexture>& textures);

#pragma endregion

#pragma region Getters and other operations
		
		// -------------------------------------------- Getters and other operations --------------------------------------------

		void CreateInitialRenderTargetView(bool gamma_corrected);

		void CreateInitialDepthStencilView();

		GraphicsContext* CreateDeferredContext(UINT context_flags = 0u);

		void EndFrame(float value);

		GraphicsDevice* GetDevice();

		GraphicsContext* GetContext();

		GraphicsContext* GetDeferredContext();

		GraphicsContext* GetAutoContext();

		void GetWindowSize(unsigned int& width, unsigned int& height) const;

		void GetWindowSize(uint2& size) const;

		void ResizeSwapChainSize(HWND hWnd, float width, float height);

		void ResizeViewport(float top_left_x, float top_left_y, float new_width, float new_height);

		void SwapBuffers(unsigned int sync_interval);

		void SetNewSize(HWND hWnd, unsigned int width, unsigned int height);

#pragma endregion

	//private:
		uint2 m_window_size;
		Microsoft::WRL::ComPtr<GraphicsDevice> m_device;
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swap_chain;
		Microsoft::WRL::ComPtr<GraphicsContext> m_context;
		Microsoft::WRL::ComPtr<GraphicsContext> m_deferred_context;
		Microsoft::WRL::ComPtr<GraphicsContext> m_auto_context;
		RenderTargetView m_target_view;
		DepthStencilView m_depth_stencil_view;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_blend_disabled;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_blend_enabled;
		ShaderReflection m_shader_reflection;
		MemoryManager* m_allocator;
	};

#endif // ECSENGINE_DIRECTX11


#endif // ECSENGINE_PLATFORM_WINDOWS

}

