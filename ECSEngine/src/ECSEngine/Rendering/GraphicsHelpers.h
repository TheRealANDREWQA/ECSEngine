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

	ECSENGINE_API void GetTextureDimensions(ID3D11Resource* _resource, unsigned int& width, unsigned int& height);

	ECSENGINE_API void GetTextureDimensions(ID3D11Resource* _resource, unsigned short& width, unsigned short& height);

	// It will make a staging texture that has copied the contents of the supplied texture
	// By default, it relies on the immediate context to copy the contents but a context
	// can be specified in order to make the copy happen
	ECSENGINE_API Texture1D ToStagingTexture(Texture1D texture, bool* success = nullptr, GraphicsContext* context = nullptr);

	// It will make a staging texture that has copied the contents of the supplied texture
	// By default, it relies on the immediate context to copy the contents 
	ECSENGINE_API Texture2D ToStagingTexture(Texture2D texture, bool* success = nullptr, GraphicsContext* context = nullptr);

	// It will make a staging texture that has copied the contents of the supplied texture
	// By default, it relies on the immediate context to copy the contents 
	ECSENGINE_API Texture3D ToStagingTexture(Texture3D texture, bool* success = nullptr, GraphicsContext* context = nullptr);

	template<typename GraphicsResource>
	ECSENGINE_API void CopyGraphicsResource(GraphicsResource destination, GraphicsResource source);

}