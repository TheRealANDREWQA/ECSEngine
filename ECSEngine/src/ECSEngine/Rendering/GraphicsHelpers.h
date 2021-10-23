#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "RenderingStructures.h"

namespace ECSEngine {

	inline unsigned int GraphicsResourceRelease(Texture1D resource) {
		resource.tex->Release();
	}

	inline unsigned int GraphicsResourceRelease(Texture2D resource) {
		resource.tex->Release();
	}

	inline unsigned int GraphicsResourceRelease(Texture3D resource) {
		resource.tex->Release();
	}

	inline unsigned int GraphicsResourceRelease(ResourceView resource) {
		resource.view->Release();
	}

	inline unsigned int GraphicsResourceRelease(RenderTargetView resource) {
		resource.target->Release();
	}

	inline unsigned int GraphicsResourceRelease(DepthStencilView resource) {
		resource.view->Release();
	}

	inline unsigned int GraphicsResourceRelease(VertexBuffer resource) {
		resource.buffer->Release();
	}

	inline unsigned int GraphicsResourceRelease(ConstantBuffer resource) {
		resource.buffer->Release();
	}

	ECSENGINE_API ID3D11Resource* GetResource(Texture1D texture);

	ECSENGINE_API ID3D11Resource* GetResource(Texture2D texture);

	ECSENGINE_API ID3D11Resource* GetResource(Texture3D texture);
	
	ECSENGINE_API ID3D11Resource* GetResource(ResourceView ps_view);

	ECSENGINE_API ID3D11Resource* GetResource(RenderTargetView target_view);

	ECSENGINE_API ID3D11Resource* GetResource(DepthStencilView depth_view);

	ECSENGINE_API ID3D11Resource* GetResource(VertexBuffer vertex_buffer);

	ECSENGINE_API ID3D11Resource* GetResource(ConstantBuffer vc_buffer);

	ECSENGINE_API ID3D11Resource* GetResource(StructuredBuffer buffer);

	ECSENGINE_API ID3D11Resource* GetResource(StandardBuffer buffer);

	ECSENGINE_API ID3D11Resource* GetResource(IndirectBuffer buffer);

	ECSENGINE_API ID3D11Resource* GetResource(UABuffer buffer);

	ECSENGINE_API uint2 GetTextureDimensions(Texture2D texture);

	// It will make a staging texture that has copied the contents of the supplied texture
	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// If it fails, the returned interface will be nullptr
	ECSENGINE_API Texture1D ToStaging(Texture1D texture, GraphicsContext* context = nullptr);

	// It will make a staging texture that has copied the contents of the supplied texture
	// By default, it relies on the immediate context to copy the contents 
	// can be specified in order to make the copy happen on a separate command list
	// If it fails, the returned interface will be nullptr
	ECSENGINE_API Texture2D ToStaging(Texture2D texture, GraphicsContext* context = nullptr);

	// It will make a staging texture that has copied the contents of the supplied texture
	// By default, it relies on the immediate context to copy the contents 
	// can be specified in order to make the copy happen on a separate command list
	// If it fails, the returned interface will be nullptr
	ECSENGINE_API Texture3D ToStaging(Texture3D texture, GraphicsContext* context = nullptr);

	// It will make a buffer that has copied the contents of the supplied buffer
	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// If it fails, the returned interface will be nullptr
	// Available template arguments: StandardBuffer, StructuredBuffer, UABuffer
	template<typename Buffer>
	ECSENGINE_API Buffer ToStaging(Buffer buffer, GraphicsContext* context = nullptr);

	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// It will create a staging texture, than copy the contents back on the cpu
	// then create an immutable texture from it - a lot of round trip
	// If it fails, the returned interface will be nullptr
	ECSENGINE_API Texture1D ToImmutableWithStaging(Texture1D texture, GraphicsContext* context = nullptr);

	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// It will create a staging texture, than copy the contents back on the cpu
	// then create an immutable texture from it - a lot of round trip
	// If it fails, the returned interface will be nullptr
	ECSENGINE_API Texture2D ToImmutableWithStaging(Texture2D texture, GraphicsContext* context = nullptr);

	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// If it fails, the returned interface will be nullptr
	ECSENGINE_API Texture3D ToImmutableWithStaging(Texture3D texture, GraphicsContext* context = nullptr);

	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// If it fails, the returned interface will be nullptr
	// Available template arguments: StandardBuffer, StructuredBuffer, UABuffer,
	// ConstantBuffer
	template<typename Buffer>
	ECSENGINE_API Buffer ToImmutableWithStaging(Buffer buffer, GraphicsContext* context = nullptr);

	// It will make a staging texture that has copied the contents of the supplied texture
	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// It will map the current texture using D3D11_MAP_READ
	// If it fails, the returned interface will be nullptr
	ECSENGINE_API Texture1D ToImmutableWithMap(Texture1D texture, GraphicsContext* context = nullptr);

	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// It will map the current texture using D3D11_MAP_READ
	// If it fails, the returned interface will be nullptr
	ECSENGINE_API Texture2D ToImmutableWithMap(Texture2D texture, GraphicsContext* context = nullptr);

	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// It will map the current texture using D3D11_MAP_READ
	// If it fails, the returned interface will be nullptr
	ECSENGINE_API Texture3D ToImmutableWithMap(Texture3D texture, GraphicsContext* context = nullptr);

	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// It will map the current buffer using D3D11_MAP_READ
	// If it fails, the returned interface will be nullptr
	// Available template arguments: StandardBuffer, StructuredBuffer, UABuffer,
	// ConstantBuffer
	template<typename Buffer>
	ECSENGINE_API Buffer ToImmutableWithMap(Buffer buffer, GraphicsContext* context = nullptr);

	enum ResizeTextureFlag {
		ECS_RESIZE_TEXTURE_FILTER_POINT, // Nearest neighbour, weakest and fastest filter method
		ECS_RESIZE_TEXTURE_FILTER_LINEAR, // Bilinear/Trilinear Interpolation
		ECS_RESIZE_TEXTURE_FILTER_CUBIC, // Bicubic/Tricubic Interpolation
		ECS_RESIZE_TEXTURE_FILTER_BOX, // Box filtering
	};

	// Resizing is done on the CPU; a staging texture is created and then mapped
	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// Only the top mip is resized and no mips are generated
	// If it fails, it will return nullptr
	ECSENGINE_API Texture2D ResizeTextureWithStaging(
		Texture2D texture,
		size_t new_width,
		size_t new_height, 
		ResizeTextureFlag resize_flag = ECS_RESIZE_TEXTURE_FILTER_LINEAR,
		GraphicsContext* context = nullptr
	);

	// Resizing is done on the CPU; the texture is mapped directly with D3D11_MAP_READ
	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen on a separate command list
	// Only the top mip is resized and no mips are generated
	// If it fails, it will return nullptr
	ECSENGINE_API Texture2D ResizeTextureWithMap(
		Texture2D texture,
		size_t new_width,
		size_t new_height,
		ResizeTextureFlag resize_flag = ECS_RESIZE_TEXTURE_FILTER_LINEAR,
		GraphicsContext* context = nullptr
	);

}