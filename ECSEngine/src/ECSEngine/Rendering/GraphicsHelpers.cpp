#include "ecspch.h"
#include "../Utilities/Function.h"
#include "GraphicsHelpers.h"
#include "Graphics.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 GetTextureDimensions(Texture2D texture) {
		uint2 result;

		D3D11_TEXTURE2D_DESC descriptor;
		texture.tex->GetDesc(&descriptor);
		
		result.x = descriptor.Width;
		result.y = descriptor.Height;

		return result;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(Texture1D texture)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Texture1D> com_tex;
		com_tex.Attach(texture.tex);
		HRESULT result = com_tex.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Texture1D to resource failed!", true);

		return _resource.Detach();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(Texture2D texture)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
		com_tex.Attach(texture.tex);
		HRESULT result = com_tex.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Texture2D to resource failed!", true);

		return _resource.Detach();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(Texture3D texture)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Texture3D> com_tex;
		com_tex.Attach(texture.tex);
		HRESULT result = com_tex.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Texture3D to resource failed!", true);

		return _resource.Detach();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(RenderTargetView target)
	{
		ID3D11Resource* resource;
		target.target->GetResource(&resource);
		unsigned int count = resource->Release();
		return resource;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(DepthStencilView depth_view)
	{
		ID3D11Resource* resource;
		depth_view.view->GetResource(&resource);
		unsigned int count = resource->Release();
		return resource;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(ResourceView view)
	{
		ID3D11Resource* resource;
		view.view->GetResource(&resource);
		unsigned int count = resource->Release();
		return resource;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(VertexBuffer buffer)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Vertex Buffer to resource failed!", true);

		return _resource.Detach();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(ConstantBuffer vc_buffer)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(vc_buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Constant Buffer to resource failed!", true);

		return _resource.Detach();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(StructuredBuffer buffer) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Structured Buffer to resource failed!", true);

		return _resource.Detach();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(StandardBuffer buffer) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Standard Buffer to resource failed!", true);

		return _resource.Detach();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(IndirectBuffer buffer) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Indirect Buffer to resource failed!", true);

		return _resource.Detach();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(UABuffer buffer)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting UA Buffer to resource failed!", true);

		return _resource.Detach();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* GetResource(UAView view) {
		ID3D11Resource* resource;
		view.view->GetResource(&resource);
		unsigned int count = resource->Release();
		return resource;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ReleaseShaderView(ResourceView view) {
		ID3D11Resource* resource = GetResource(view);
		// Release the view
		unsigned int view_count = view.view->Release();

		// Release the resource
		unsigned int resource_count = resource->Release();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ReleaseUAView(UAView view) {
		ID3D11Resource* resource = GetResource(view);
		// Release the view
		unsigned int view_count = view.view->Release();

		// Release the resource
		unsigned int resource_count = resource->Release();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture1D ToStaging(Texture1D texture, GraphicsContext* context)
	{
		Texture1D new_texture;

		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE1D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		device = texture.GetDevice();

		texture_descriptor.Usage = D3D11_USAGE_STAGING;
		texture_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

		ID3D11Texture1D* _new_texture = nullptr;
		HRESULT result = device->CreateTexture1D(&texture_descriptor, nullptr, &_new_texture);

		new_texture = Texture1D(_new_texture);
		if (FAILED(result)) {
			return new_texture;
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		CopyGraphicsResource(new_texture, texture, context);
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ToStaging(Texture2D texture, GraphicsContext* context)
	{
		Texture2D new_texture;

		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		device = texture.GetDevice();

		texture_descriptor.BindFlags = 0;
		texture_descriptor.Usage = D3D11_USAGE_STAGING;
		texture_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		texture_descriptor.MiscFlags = 0;

		ID3D11Texture2D* _new_texture = nullptr;
		HRESULT result = device->CreateTexture2D(&texture_descriptor, nullptr, &_new_texture);

		new_texture = Texture2D(_new_texture);
		if (FAILED(result)) {
			return new_texture;
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		CopyGraphicsResource(new_texture, texture, context);
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture3D ToStaging(Texture3D texture, GraphicsContext* context)
	{
		Texture3D new_texture;

		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE3D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		device = texture.GetDevice();

		texture_descriptor.Usage = D3D11_USAGE_STAGING;
		texture_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

		ID3D11Texture3D* _new_texture = nullptr;
		HRESULT result = device->CreateTexture3D(&texture_descriptor, nullptr, &_new_texture);

		new_texture = Texture3D(_new_texture);
		if (FAILED(result)) {
			return new_texture;
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		CopyGraphicsResource(new_texture, texture, context);
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	Buffer ToStaging(Buffer buffer, GraphicsContext* context) {
		Buffer new_buffer;

		GraphicsDevice* device = nullptr;
		D3D11_BUFFER_DESC buffer_descriptor;

		buffer.buffer->GetDesc(&buffer_descriptor);
		device = buffer.GetDevice();

		buffer_descriptor.Usage = D3D11_USAGE_STAGING;
		buffer_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

		ID3D11Buffer* _new_buffer = nullptr;
		HRESULT result = device->CreateBuffer(&buffer_descriptor, nullptr, &_new_buffer);

		new_buffer = Buffer(_new_buffer);
		if (FAILED(result)) {
			return new_buffer;
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		CopyGraphicsResource(new_buffer, buffer, context);
		return new_buffer;
	}

#define EXPORT(type) ECS_TEMPLATE_FUNCTION(type, ToStaging, type, GraphicsContext*);

	EXPORT(StandardBuffer);
	EXPORT(StructuredBuffer);
	EXPORT(UABuffer);

#undef EXPORT

	// ----------------------------------------------------------------------------------------------------------------------

	constexpr size_t MAX_SUBRESOURCES = 32;

	Texture1D ToImmutableWithStaging(Texture1D texture, GraphicsContext* context) {
		Texture1D new_texture;

		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE1D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		device = texture.GetDevice();

		texture_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		texture_descriptor.CPUAccessFlags = 0;

		Texture1D staging_texture = ToStaging(texture, context);
		if (staging_texture.tex == nullptr) {
			return staging_texture;
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_SUBRESOURCE_DATA subresource_data[MAX_SUBRESOURCES];
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			D3D11_MAPPED_SUBRESOURCE resource = MapTextureEx(staging_texture, context, D3D11_MAP_READ, index, 0);
			subresource_data[index].pSysMem = resource.pData;
			subresource_data[index].SysMemPitch = resource.RowPitch;
			subresource_data[index].SysMemSlicePitch = resource.DepthPitch;
		}

		ID3D11Texture1D* _new_texture = nullptr;
		HRESULT result = device->CreateTexture1D(&texture_descriptor, subresource_data, &_new_texture);

		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			UnmapTexture(staging_texture, context, index);
		}
		unsigned int count = staging_texture.tex->Release();

		new_texture = _new_texture;
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ToImmutableWithStaging(Texture2D texture, GraphicsContext* context) {
		Texture2D new_texture;

		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		device = texture.GetDevice();

		texture_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		texture_descriptor.CPUAccessFlags = 0;

		Texture2D staging_texture = ToStaging(texture, context);
		if (staging_texture.tex == nullptr) {
			return staging_texture;
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_SUBRESOURCE_DATA subresource_data[MAX_SUBRESOURCES];
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			D3D11_MAPPED_SUBRESOURCE resource = MapTextureEx(staging_texture, context, D3D11_MAP_READ, index, 0);
			subresource_data[index].pSysMem = resource.pData;
			subresource_data[index].SysMemPitch = resource.RowPitch;
			subresource_data[index].SysMemSlicePitch = resource.DepthPitch;
		}

		ID3D11Texture2D* _new_texture = nullptr;
		HRESULT result = device->CreateTexture2D(&texture_descriptor, subresource_data, &_new_texture);

		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			UnmapTexture(staging_texture, context, index);
		}
		unsigned int count = staging_texture.tex->Release();

		new_texture = _new_texture;
		return new_texture;

	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture3D ToImmutableWithStaging(Texture3D texture, GraphicsContext* context) {
		Texture3D new_texture;

		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE3D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		device = texture.GetDevice();

		texture_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		texture_descriptor.CPUAccessFlags = 0;

		Texture3D staging_texture = ToStaging(texture, context);
		if (staging_texture.tex == nullptr) {
			return staging_texture;
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_SUBRESOURCE_DATA subresource_data[MAX_SUBRESOURCES];
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			D3D11_MAPPED_SUBRESOURCE resource = MapTextureEx(staging_texture, context, D3D11_MAP_READ, index, 0);
			subresource_data[index].pSysMem = resource.pData;
			subresource_data[index].SysMemPitch = resource.RowPitch;
			subresource_data[index].SysMemSlicePitch = resource.DepthPitch;
		}

		ID3D11Texture3D* _new_texture = nullptr;
		HRESULT result = device->CreateTexture3D(&texture_descriptor, subresource_data, &_new_texture);

		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			UnmapTexture(staging_texture, context, index);
		}
		unsigned int count = staging_texture.tex->Release();

		new_texture = _new_texture;
		return new_texture;

	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	Buffer ToImmutableWithStaging(Buffer buffer, GraphicsContext* context) {
		Buffer new_buffer;

		GraphicsDevice* device = nullptr;
		D3D11_BUFFER_DESC buffer_descriptor;
		buffer.buffer->GetDesc(&buffer_descriptor);
		device = buffer.GetDevice();

		buffer_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		buffer_descriptor.CPUAccessFlags = 0;

		Buffer staging_buffer = ToStaging(buffer, context);
		if (staging_buffer.buffer == nullptr) {
			return staging_buffer;
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_SUBRESOURCE_DATA subresource_data;
		D3D11_MAPPED_SUBRESOURCE resource = MapBufferEx(staging_buffer.buffer, context, D3D11_MAP_READ);
		subresource_data.pSysMem = resource.pData;
		subresource_data.SysMemPitch = resource.RowPitch;
		subresource_data.SysMemSlicePitch = resource.DepthPitch;

		ID3D11Buffer* _new_buffer = nullptr;
		HRESULT result = device->CreateBuffer(&buffer_descriptor, &subresource_data, &_new_buffer);

		UnmapBuffer(staging_buffer.buffer, context);
		unsigned int count = staging_buffer.buffer->Release();

		new_buffer = _new_buffer;
		return new_buffer;
	}

//#define EXPORT_BUFFER(type) ECS_TEMPLATE_FUNCTION(type, ECSEngine::ToImmutableWithStaging, type, GraphicsContext*);

	// CRINGE Visual studio intellisense bug that fills the file with errors even tho no error is present
	// Manual unroll
	ECS_TEMPLATE_FUNCTION(StandardBuffer, ToImmutableWithStaging, StandardBuffer, GraphicsContext*);
	ECS_TEMPLATE_FUNCTION(StructuredBuffer, ToImmutableWithStaging, StructuredBuffer, GraphicsContext*);
	ECS_TEMPLATE_FUNCTION(UABuffer, ToImmutableWithStaging, UABuffer, GraphicsContext*);
	ECS_TEMPLATE_FUNCTION(ConstantBuffer, ToImmutableWithStaging, ConstantBuffer, GraphicsContext*);

	// ----------------------------------------------------------------------------------------------------------------------

	Texture1D ToImmutableWithMap(Texture1D texture, GraphicsContext* context)
	{
		Texture1D new_texture;

		GraphicsDevice* device = texture.GetDevice();
		D3D11_TEXTURE1D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_SUBRESOURCE_DATA subresource_data[MAX_SUBRESOURCES];
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			D3D11_MAPPED_SUBRESOURCE resource = MapTextureEx(texture, context, D3D11_MAP_READ, index, 0);
			subresource_data[index].pSysMem = resource.pData;
			subresource_data[index].SysMemPitch = resource.RowPitch;
			subresource_data[index].SysMemSlicePitch = resource.DepthPitch;
		}

		ID3D11Texture1D* _new_texture = nullptr;
		HRESULT result = device->CreateTexture1D(&texture_descriptor, subresource_data, &_new_texture);

		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			UnmapTexture(texture, context, index);
		}

		new_texture = _new_texture;
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ToImmutableWithMap(Texture2D texture, GraphicsContext* context)
	{
		Texture2D new_texture;

		GraphicsDevice* device = texture.GetDevice();
		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_SUBRESOURCE_DATA subresource_data[MAX_SUBRESOURCES];
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			D3D11_MAPPED_SUBRESOURCE resource = MapTextureEx(texture, context, D3D11_MAP_READ, index, 0);
			subresource_data[index].pSysMem = resource.pData;
			subresource_data[index].SysMemPitch = resource.RowPitch;
			subresource_data[index].SysMemSlicePitch = resource.DepthPitch;
		}

		ID3D11Texture2D* _new_texture = nullptr;
		HRESULT result = device->CreateTexture2D(&texture_descriptor, subresource_data, &_new_texture);

		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			UnmapTexture(texture, context, index);
		}

		new_texture = _new_texture;
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture3D ToImmutableWithMap(Texture3D texture, GraphicsContext* context)
	{
		Texture3D new_texture;

		GraphicsDevice* device = texture.GetDevice();
		D3D11_TEXTURE3D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_SUBRESOURCE_DATA subresource_data[MAX_SUBRESOURCES];
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			D3D11_MAPPED_SUBRESOURCE resource = MapTextureEx(texture, context, D3D11_MAP_READ, index, 0);
			subresource_data[index].pSysMem = resource.pData;
			subresource_data[index].SysMemPitch = resource.RowPitch;
			subresource_data[index].SysMemSlicePitch = resource.DepthPitch;
		}

		ID3D11Texture3D* _new_texture = nullptr;
		HRESULT result = device->CreateTexture3D(&texture_descriptor, subresource_data, &_new_texture);

		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			UnmapTexture(texture, context, index);
		}

		new_texture = _new_texture;
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	Buffer ToImmutableWithMap(Buffer buffer, GraphicsContext* context)
	{
		Buffer new_buffer;

		GraphicsDevice* device = buffer.GetDevice();
		D3D11_BUFFER_DESC buffer_descriptor;
		buffer.buffer->GetDesc(&buffer_descriptor);

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_SUBRESOURCE_DATA subresource_data;
		D3D11_MAPPED_SUBRESOURCE resource = MapBufferEx(buffer.buffer, context, D3D11_MAP_READ);
		subresource_data.pSysMem = resource.pData;
		subresource_data.SysMemPitch = resource.RowPitch;
		subresource_data.SysMemSlicePitch = resource.DepthPitch;

		ID3D11Buffer* _new_buffer = nullptr;
		HRESULT result = device->CreateBuffer(&buffer_descriptor, &subresource_data, &_new_buffer);

		UnmapBuffer(buffer.buffer, context);

		new_buffer = _new_buffer;
		return new_buffer;
	}

	// CRINGE Visual studio intellisense bug that fills the file with errors even tho no error is present
	// Manual unroll
	ECS_TEMPLATE_FUNCTION(StandardBuffer, ToImmutableWithMap, StandardBuffer, GraphicsContext*);
	ECS_TEMPLATE_FUNCTION(StructuredBuffer, ToImmutableWithMap, StructuredBuffer, GraphicsContext*);
	ECS_TEMPLATE_FUNCTION(UABuffer, ToImmutableWithMap, UABuffer, GraphicsContext*);
	ECS_TEMPLATE_FUNCTION(ConstantBuffer, ToImmutableWithMap, ConstantBuffer, GraphicsContext*);

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTextureWithStaging(Texture2D texture, size_t new_width, size_t new_height, ResizeTextureFlag resize_flag, GraphicsContext* context)
	{
		Texture2D new_texture;

		GraphicsDevice* device = texture.GetDevice();
		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		Texture2D staging_texture = ToStaging(texture, context);
		D3D11_MAPPED_SUBRESOURCE first_mip = MapTextureEx(staging_texture, context, D3D11_MAP_READ);

		DirectX::Image dx_image;
		dx_image.pixels = (uint8_t*)first_mip.pData;
		dx_image.format = texture_descriptor.Format;
		dx_image.width = texture_descriptor.Width;
		dx_image.height = texture_descriptor.Height;
		dx_image.rowPitch = first_mip.RowPitch;
		dx_image.slicePitch = first_mip.DepthPitch;
		
		DirectX::TEX_FILTER_FLAGS filter_flag = DirectX::TEX_FILTER_LINEAR;
		switch (resize_flag) {
		case ECS_RESIZE_TEXTURE_FILTER_BOX:
			filter_flag = DirectX::TEX_FILTER_BOX;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_CUBIC:
			filter_flag = DirectX::TEX_FILTER_CUBIC;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_LINEAR:
			filter_flag = DirectX::TEX_FILTER_LINEAR;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_POINT:
			filter_flag = DirectX::TEX_FILTER_POINT;
			break;
		}

		DirectX::ScratchImage new_image;
		HRESULT result = DirectX::Resize(dx_image, new_width, new_height, filter_flag, new_image);

		UnmapTexture(staging_texture, context);
		unsigned int count = staging_texture.tex->Release();
		if (FAILED(result)) {
			return (ID3D11Texture2D*)nullptr;
		}

		const DirectX::Image* resized_image = new_image.GetImage(0, 0, 0);
		D3D11_SUBRESOURCE_DATA subresource_data;
		subresource_data.pSysMem = resized_image->pixels;
		subresource_data.SysMemPitch = resized_image->rowPitch;
		subresource_data.SysMemSlicePitch = resized_image->slicePitch;

		texture_descriptor.MipLevels = 1;
		texture_descriptor.Width = new_width;
		texture_descriptor.Height = new_height;
		ID3D11Texture2D* _new_texture;
		result = device->CreateTexture2D(&texture_descriptor, &subresource_data, &_new_texture);

		new_texture = _new_texture;
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTextureWithMap(Texture2D texture, size_t new_width, size_t new_height, ResizeTextureFlag resize_flag, GraphicsContext* context)
	{
		Texture2D new_texture;

		GraphicsDevice* device = texture.GetDevice();
		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		D3D11_MAPPED_SUBRESOURCE first_mip = MapTextureEx(texture, context, D3D11_MAP_READ);

		DirectX::Image dx_image;
		dx_image.pixels = (uint8_t*)first_mip.pData;
		dx_image.format = texture_descriptor.Format;
		dx_image.width = texture_descriptor.Width;
		dx_image.height = texture_descriptor.Height;
		dx_image.rowPitch = first_mip.RowPitch;
		dx_image.slicePitch = first_mip.DepthPitch;

		DirectX::TEX_FILTER_FLAGS filter_flag = DirectX::TEX_FILTER_LINEAR;
		switch (resize_flag) {
		case ECS_RESIZE_TEXTURE_FILTER_BOX:
			filter_flag = DirectX::TEX_FILTER_BOX;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_CUBIC:
			filter_flag = DirectX::TEX_FILTER_CUBIC;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_LINEAR:
			filter_flag = DirectX::TEX_FILTER_LINEAR;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_POINT:
			filter_flag = DirectX::TEX_FILTER_POINT;
			break;
		}

		DirectX::ScratchImage new_image;
		HRESULT result = DirectX::Resize(dx_image, new_width, new_height, filter_flag, new_image);

		UnmapTexture(texture, context);
		if (FAILED(result)) {
			return (ID3D11Texture2D*)nullptr;
		}

		const DirectX::Image* resized_image = new_image.GetImage(0, 0, 0);
		D3D11_SUBRESOURCE_DATA subresource_data;
		subresource_data.pSysMem = resized_image->pixels;
		subresource_data.SysMemPitch = resized_image->rowPitch;
		subresource_data.SysMemSlicePitch = resized_image->slicePitch;

		texture_descriptor.MipLevels = 1;
		texture_descriptor.Width = new_width;
		texture_descriptor.Height = new_height;
		ID3D11Texture2D* _new_texture;
		result = device->CreateTexture2D(&texture_descriptor, &subresource_data, &_new_texture);

		new_texture = _new_texture;
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

}