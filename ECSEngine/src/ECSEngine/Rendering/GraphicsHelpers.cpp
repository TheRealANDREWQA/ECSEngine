#include "ecspch.h"
#include "../Utilities/Function.h"
#include "GraphicsHelpers.h"
#include "Graphics.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "../Utilities/Path.h"
#include "../Allocators/AllocatorPolymorphic.h"

ECS_CONTAINERS;

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

	ID3D11Resource* GetResource(TextureCube texture)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
		com_tex.Attach(texture.tex);
		HRESULT result = com_tex.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting TextureCube to resource failed!", true);

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

	void ReleaseRenderView(RenderTargetView view)
	{
		ID3D11Resource* resource = GetResource(view);
		// Release the view
		unsigned int view_count = view.target->Release();

		// Release the resource
		unsigned int resource_count = resource->Release();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void CreateCubeVertexBuffer(Graphics* graphics, float positive_span, VertexBuffer& vertex_buffer, IndexBuffer& index_buffer)
	{
		float negative_span = -positive_span;
		float3 vertex_position[] = {
			{negative_span, negative_span, negative_span},
			{positive_span, negative_span, negative_span},
			{negative_span, positive_span, negative_span},
			{positive_span, positive_span, negative_span},
			{negative_span, negative_span, positive_span},
			{positive_span, negative_span, positive_span},
			{negative_span, positive_span, positive_span},
			{positive_span, positive_span, positive_span}
		};

		vertex_buffer = graphics->CreateVertexBuffer(sizeof(float3), std::size(vertex_position), vertex_position);

		unsigned int indices[] = {
			0, 2, 1,    2, 3, 1,
			1, 3, 5,    3, 7, 5,
			2, 6, 3,    3, 6, 7,
			4, 5, 7,    4, 7, 6,
			0, 4, 2,    2, 4, 6,
			0, 1, 4,    1, 5, 4
		};

		index_buffer = graphics->CreateIndexBuffer(Stream<unsigned int>(indices, std::size(indices)));
	}

	// ----------------------------------------------------------------------------------------------------------------------

	VertexBuffer CreateRectangleVertexBuffer(Graphics* graphics, float3 top_left, float3 bottom_right)
	{
		// a -- b
		// |    |
		// c -- d
		float3 positions[] = {
			top_left,                                           // a
			{ bottom_right.x, top_left.y, bottom_right.z },     // b
			{ top_left.x, bottom_right.y, top_left.z },         // c
			{bottom_right.x, top_left.y, bottom_right.z},       // b
			bottom_right,                                       // d
			{top_left.x, bottom_right.y, top_left.z}            // c
		};
		return graphics->CreateVertexBuffer(sizeof(float3), std::size(positions), positions);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture1D ToStaging(Texture1D texture)
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

		GraphicsContext* context;
		device->GetImmediateContext(&context);

		CopyGraphicsResource(new_texture, texture, context);
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ToStaging(Texture2D texture)
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

		GraphicsContext* context;
		device->GetImmediateContext(&context);

		CopyGraphicsResource(new_texture, texture, context);
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture3D ToStaging(Texture3D texture)
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

		GraphicsContext* context;
		device->GetImmediateContext(&context);

		CopyGraphicsResource(new_texture, texture, context);
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	Buffer ToStaging(Buffer buffer) {
		Buffer new_buffer;

		memcpy(&new_buffer, &buffer, sizeof(Buffer));
		GraphicsDevice* device = nullptr;
		D3D11_BUFFER_DESC buffer_descriptor;

		buffer.buffer->GetDesc(&buffer_descriptor);
		device = buffer.GetDevice();

		buffer_descriptor.Usage = D3D11_USAGE_STAGING;
		buffer_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		buffer_descriptor.BindFlags = 0;

		ID3D11Buffer* _new_buffer = nullptr;
		HRESULT result = device->CreateBuffer(&buffer_descriptor, nullptr, &_new_buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Could not create a staging buffer!", true);

		new_buffer.buffer = _new_buffer;
		GraphicsContext* context;
		device->GetImmediateContext(&context);

		CopyGraphicsResource(new_buffer, buffer, context);
		return new_buffer;
	}

#define EXPORT(type) ECS_TEMPLATE_FUNCTION(type, ToStaging, type);

	EXPORT(StandardBuffer);
	EXPORT(StructuredBuffer);
	EXPORT(UABuffer);
	EXPORT(IndexBuffer);
	EXPORT(VertexBuffer);

#undef EXPORT

	// ----------------------------------------------------------------------------------------------------------------------

	constexpr size_t MAX_SUBRESOURCES = 32;

	Texture1D ToImmutableWithStaging(Texture1D texture) {
		Texture1D new_texture;

		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE1D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		device = texture.GetDevice();

		texture_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		texture_descriptor.CPUAccessFlags = 0;

		GraphicsContext* context;
		device->GetImmediateContext(&context);

		Texture1D staging_texture = ToStaging(texture);
		if (staging_texture.tex == nullptr) {
			return staging_texture;
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

	Texture2D ToImmutableWithStaging(Texture2D texture) {
		Texture2D new_texture;

		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		device = texture.GetDevice();

		texture_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		texture_descriptor.CPUAccessFlags = 0;

		GraphicsContext* context;
		device->GetImmediateContext(&context);

		Texture2D staging_texture = ToStaging(texture);
		if (staging_texture.tex == nullptr) {
			return staging_texture;
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

	Texture3D ToImmutableWithStaging(Texture3D texture) {
		Texture3D new_texture;

		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE3D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		device = texture.GetDevice();

		texture_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		texture_descriptor.CPUAccessFlags = 0;

		GraphicsContext* context;
		device->GetImmediateContext(&context);

		Texture3D staging_texture = ToStaging(texture);
		if (staging_texture.tex == nullptr) {
			return staging_texture;
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
	Buffer ToImmutableWithStaging(Buffer buffer) {
		Buffer new_buffer;

		GraphicsDevice* device = nullptr;
		D3D11_BUFFER_DESC buffer_descriptor;
		buffer.buffer->GetDesc(&buffer_descriptor);
		device = buffer.GetDevice();

		buffer_descriptor.Usage = D3D11_USAGE_IMMUTABLE;
		buffer_descriptor.CPUAccessFlags = 0;

		GraphicsContext* context;
		device->GetImmediateContext(&context);

		Buffer staging_buffer = ToStaging(buffer);
		if (staging_buffer.buffer == nullptr) {
			return staging_buffer;
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
	ECS_TEMPLATE_FUNCTION(StandardBuffer, ToImmutableWithStaging, StandardBuffer);
	ECS_TEMPLATE_FUNCTION(StructuredBuffer, ToImmutableWithStaging, StructuredBuffer);
	ECS_TEMPLATE_FUNCTION(UABuffer, ToImmutableWithStaging, UABuffer);
	ECS_TEMPLATE_FUNCTION(ConstantBuffer, ToImmutableWithStaging, ConstantBuffer);

	// ----------------------------------------------------------------------------------------------------------------------

	Texture1D ToImmutableWithMap(Texture1D texture)
	{
		Texture1D new_texture;

		GraphicsDevice* device = texture.GetDevice();
		D3D11_TEXTURE1D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		GraphicsContext* context;
		device->GetImmediateContext(&context);

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

	Texture2D ToImmutableWithMap(Texture2D texture)
	{
		Texture2D new_texture;

		GraphicsDevice* device = texture.GetDevice();
		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		GraphicsContext* context;
		device->GetImmediateContext(&context);

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

	Texture3D ToImmutableWithMap(Texture3D texture)
	{
		Texture3D new_texture;

		GraphicsDevice* device = texture.GetDevice();
		D3D11_TEXTURE3D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		GraphicsContext* context;
		device->GetImmediateContext(&context);

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
	Buffer ToImmutableWithMap(Buffer buffer)
	{
		Buffer new_buffer;

		GraphicsDevice* device = buffer.GetDevice();
		D3D11_BUFFER_DESC buffer_descriptor;
		buffer.buffer->GetDesc(&buffer_descriptor);
		
		GraphicsContext* context;
		device->GetImmediateContext(&context);

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
	ECS_TEMPLATE_FUNCTION(StandardBuffer, ToImmutableWithMap, StandardBuffer);
	ECS_TEMPLATE_FUNCTION(StructuredBuffer, ToImmutableWithMap, StructuredBuffer);
	ECS_TEMPLATE_FUNCTION(UABuffer, ToImmutableWithMap, UABuffer);
	ECS_TEMPLATE_FUNCTION(ConstantBuffer, ToImmutableWithMap, ConstantBuffer);

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTextureWithStaging(Texture2D texture, size_t new_width, size_t new_height, size_t resize_flag)
	{
		GraphicsDevice* device = texture.GetDevice();
		GraphicsContext* context;
		device->GetImmediateContext(&context);

		Texture2D staging_texture = ToStaging(texture);
		D3D11_MAPPED_SUBRESOURCE first_mip = MapTextureEx(staging_texture, context, D3D11_MAP_READ);
		Texture2D new_texture = ResizeTexture(first_mip.pData, texture, new_width, new_height, nullptr, { nullptr }, resize_flag);
		UnmapTexture(staging_texture, context);
		staging_texture.tex->Release();
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTextureWithMap(Texture2D texture, size_t new_width, size_t new_height, size_t resize_flag)
	{
		GraphicsDevice* device = texture.GetDevice();
		GraphicsContext* context;
		device->GetImmediateContext(&context);

		D3D11_MAPPED_SUBRESOURCE first_mip = MapTextureEx(texture, context, D3D11_MAP_READ);
		return ResizeTexture(first_mip.pData, texture, new_width, new_height, nullptr, {nullptr}, resize_flag);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Texture2D ResizeTexture(void* data, Texture2D texture, size_t new_width, size_t new_height, GraphicsContext* context, AllocatorPolymorphic allocator, size_t resize_flag) {
		Texture2D new_texture = (ID3D11Texture2D*)nullptr;

		GraphicsDevice* device = texture.GetDevice();
		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);

		DirectX::Image dx_image;
		dx_image.pixels = (uint8_t*)data;
		dx_image.format = texture_descriptor.Format;
		dx_image.width = texture_descriptor.Width;
		dx_image.height = texture_descriptor.Height;
		HRESULT result = DirectX::ComputePitch(texture_descriptor.Format, texture_descriptor.Width, texture_descriptor.Height, dx_image.rowPitch, dx_image.slicePitch);
		if (FAILED(result)) {
			return Texture2D((ID3D11Texture2D*)nullptr);
		}

		DirectX::TEX_FILTER_FLAGS filter_flag = DirectX::TEX_FILTER_LINEAR;
		size_t filter_flags = function::ClearFlag(resize_flag, ECS_RESIZE_TEXTURE_MIP_MAPS);
		switch (filter_flags) {
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
		if (allocator.allocator != nullptr) {
			new_image.SetAllocator(allocator.allocator, GetAllocateFunction(allocator), GetDeallocateMutableFunction(allocator));
		}
		result = DirectX::Resize(dx_image, new_width, new_height, filter_flag, new_image);
		if (FAILED(result)) {
			return new_texture;
		}

		const DirectX::Image* resized_image = new_image.GetImage(0, 0, 0);
		D3D11_SUBRESOURCE_DATA subresource_data;
		subresource_data.pSysMem = resized_image->pixels;
		subresource_data.SysMemPitch = resized_image->rowPitch;
		subresource_data.SysMemSlicePitch = resized_image->slicePitch;

		ID3D11Texture2D* _new_texture;
		texture_descriptor.MipLevels = 1;
		texture_descriptor.Width = new_width;
		texture_descriptor.Height = new_height;

		result = device->CreateTexture2D(&texture_descriptor, &subresource_data, &_new_texture);
		if (FAILED(result)) {
			return new_texture;
		}
		if (function::HasFlag(resize_flag, ECS_RESIZE_TEXTURE_MIP_MAPS)) {
			ID3D11Texture2D* first_mip = _new_texture;
			texture_descriptor.MipLevels = 0;
			texture_descriptor.BindFlags |= D3D11_BIND_RENDER_TARGET;
			texture_descriptor.Usage = D3D11_USAGE_DEFAULT;
			texture_descriptor.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
			
			result = device->CreateTexture2D(&texture_descriptor, nullptr, &_new_texture);
			if (FAILED(result)) {
				return new_texture;
			}

			CopyTextureSubresource(_new_texture, { 0, 0 }, 0, first_mip, { 0, 0 }, { (unsigned int)resized_image->width, (unsigned int)resized_image->height }, 0, context);
			first_mip->Release();

			ID3D11ShaderResourceView* resource_view;
			result = device->CreateShaderResourceView(_new_texture, nullptr, &resource_view);
			if (FAILED(result)) {
				_new_texture->Release();
				return new_texture;
			}
			context->GenerateMips(resource_view);
			resource_view->Release();
		}

		new_texture = _new_texture;
		return new_texture;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Stream<void> ResizeTexture(void* texture_data, size_t current_width, size_t current_height, DXGI_FORMAT format, size_t new_width, size_t new_height, AllocatorPolymorphic allocator, size_t resize_flags)
	{
		Stream<void> data = { nullptr, 0 };

		DirectX::Image dx_image;
		dx_image.pixels = (uint8_t*)texture_data;
		dx_image.format = format;
		dx_image.width = current_width;
		dx_image.height = current_height;
		HRESULT result = DirectX::ComputePitch(dx_image.format, dx_image.width, dx_image.height, dx_image.rowPitch, dx_image.slicePitch);
		if (FAILED(result)) {
			return data;
		}

		DirectX::TEX_FILTER_FLAGS filter_flag = DirectX::TEX_FILTER_LINEAR;
		switch (resize_flags) {
		case ECS_RESIZE_TEXTURE_FILTER_BOX:
			filter_flag = DirectX::TEX_FILTER_BOX;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_CUBIC:
			filter_flag = DirectX::TEX_FILTER_CUBIC;
			break;
		case ECS_RESIZE_TEXTURE_FILTER_POINT:
			filter_flag = DirectX::TEX_FILTER_POINT;
			break;
		}

		DirectX::ScratchImage new_image;
		if (allocator.allocator != nullptr) {
			new_image.SetAllocator(allocator.allocator, GetAllocateFunction(allocator), GetDeallocateMutableFunction(allocator));
		}
		result = DirectX::Resize(dx_image, new_width, new_height, filter_flag, new_image);
		if (FAILED(result)) {
			return data;
		}

		data = { new_image.GetPixels(), new_image.GetPixelsSize() };
		new_image.DetachPixels();
		return data;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void InvertMeshZAxis(Graphics* graphics, Mesh& mesh)
	{
		// Invert positions, normals, tangents and winding order
		VertexBuffer position_buffer = GetMeshVertexBuffer(mesh, ECS_MESH_POSITION);
		VertexBuffer normal_buffer = GetMeshVertexBuffer(mesh, ECS_MESH_NORMAL);
		VertexBuffer tangent_buffer = GetMeshVertexBuffer(mesh, ECS_MESH_TANGENT);

		// Inverts the z axis of a buffer and returns a new one
		auto invert_z = [](Graphics* graphics, VertexBuffer buffer) {
			// Create a staging one with its data
			VertexBuffer staging = ToStaging(buffer);

			// Map the buffer
			float3* data = (float3*)graphics->MapBuffer(staging.buffer, D3D11_MAP_READ_WRITE);

			// Modify the data
			for (size_t index = 0; index < staging.size; index++) {
				data[index].z = -data[index].z;
			}

			// Create a copy of the staging data into a GPU resource
			D3D11_BUFFER_DESC buffer_descriptor;
			buffer.buffer->GetDesc(&buffer_descriptor);

			VertexBuffer new_buffer = graphics->CreateVertexBuffer(buffer.stride, buffer.size, data, buffer_descriptor.Usage, buffer_descriptor.CPUAccessFlags, buffer_descriptor.MiscFlags);

			// Unmap the buffer
			graphics->UnmapBuffer(staging.buffer);

			// Release the old one and the staging
			buffer.buffer->Release();
			staging.buffer->Release();

			return new_buffer;
		};

		if (position_buffer.buffer != nullptr) {
			SetMeshVertexBuffer(mesh, ECS_MESH_POSITION, invert_z(graphics, position_buffer));
		}
		if (normal_buffer.buffer != nullptr) {
			SetMeshVertexBuffer(mesh, ECS_MESH_NORMAL, invert_z(graphics, normal_buffer));
		}
		if (tangent_buffer.buffer != nullptr) {
			SetMeshVertexBuffer(mesh, ECS_MESH_TANGENT, invert_z(graphics, tangent_buffer));
		}

		// Invert the winding order
		IndexBuffer staging_index = ToStaging(mesh.index_buffer);

		void* _indices = graphics->MapBuffer(staging_index.buffer, D3D11_MAP_READ_WRITE);

		// Create an index buffer with the same specification
		D3D11_BUFFER_DESC index_descriptor;
		mesh.index_buffer.buffer->GetDesc(&index_descriptor);
		if (staging_index.int_size == 1) {
			// U char
			unsigned char* indices = (unsigned char*)_indices;
			// Start from 1 because only the last 2 vertices must be swapped for the winding order
			for (size_t index = 1; index < staging_index.count; index += 3) {
				unsigned char copy = indices[index];
				indices[index] = indices[index + 1];
				indices[index + 1] = copy;
			}
		}
		else if (staging_index.int_size == 2) {
			// U short
			unsigned short* indices = (unsigned short*)_indices;
			// Start from 1 because only the last 2 vertices must be swapped for the winding order
			for (size_t index = 1; index < staging_index.count; index += 3) {
				unsigned short copy = indices[index];
				indices[index] = indices[index + 1];
				indices[index + 1] = copy;
			}
		}
		else if (staging_index.int_size == 4) {
			// U int
			unsigned int* indices = (unsigned int*)_indices;
			// Start from 1 because only the last 2 vertices must be swapped for the winding order
			for (size_t index = 1; index < staging_index.count; index += 3) {
				unsigned int copy = indices[index];
				indices[index] = indices[index + 1];
				indices[index + 1] = copy;
			}
		}

		IndexBuffer new_indices = graphics->CreateIndexBuffer(staging_index.int_size, staging_index.count, index_descriptor.Usage, index_descriptor.CPUAccessFlags);

		// Release the old buffers
		staging_index.buffer->Release();
		mesh.index_buffer.buffer->Release();

		mesh.index_buffer = new_indices;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCube ConvertTexturesToCube(
		Texture2D x_positive,
		Texture2D x_negative, 
		Texture2D y_positive,
		Texture2D y_negative,  
		Texture2D z_positive,
		Texture2D z_negative,
		GraphicsContext* context
	)
	{
		GraphicsDevice* device = x_negative.GetDevice();
		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		D3D11_TEXTURE2D_DESC texture_descriptor;
		x_negative.tex->GetDesc(&texture_descriptor);

		texture_descriptor.ArraySize = 6;
		texture_descriptor.Usage = D3D11_USAGE_DEFAULT;
		texture_descriptor.CPUAccessFlags = 0;
		texture_descriptor.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;
		ID3D11Texture2D* texture = nullptr;
		HRESULT result = device->CreateTexture2D(&texture_descriptor, nullptr, &texture);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting textures to cube textures failed.", true);
		TextureCube cube_texture(texture);

		// For every mip, copy the mip into the corresponding array resource
		for (size_t index = 0; index < texture_descriptor.MipLevels; index++) {
			CopyGraphicsResource(cube_texture, x_negative, ECS_TEXTURE_CUBE_X_NEG, context, index);
			CopyGraphicsResource(cube_texture, x_positive, ECS_TEXTURE_CUBE_X_POS, context, index);
			CopyGraphicsResource(cube_texture, y_negative, ECS_TEXTURE_CUBE_Y_NEG, context, index);
			CopyGraphicsResource(cube_texture, y_positive, ECS_TEXTURE_CUBE_Y_POS, context, index);
			CopyGraphicsResource(cube_texture, z_negative, ECS_TEXTURE_CUBE_Z_NEG, context, index);
			CopyGraphicsResource(cube_texture, z_positive, ECS_TEXTURE_CUBE_Z_POS, context, index);
		}

		return cube_texture;
	}

	TextureCube ConvertTexturesToCube(const Texture2D* textures, GraphicsContext* context)
	{
		return ConvertTexturesToCube(textures[0], textures[1], textures[2], textures[3], textures[4], textures[5], context);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	TextureCube ConvertTextureToCube(ResourceView texture_view, Graphics* graphics, DXGI_FORMAT cube_format, uint2 face_size)
	{
		TextureCube cube;
		
		// Create the 6 faces as render targets
		GraphicsTexture2DDescriptor texture_descriptor;
		texture_descriptor.format = cube_format;
		texture_descriptor.bind_flag = static_cast<D3D11_BIND_FLAG>(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		texture_descriptor.size = face_size;
		texture_descriptor.mip_levels = 0;

		Texture2D cube_textures[6];
		RenderTargetView render_views[6];
		for (size_t index = 0; index < 6; index++) {
			cube_textures[index] = graphics->CreateTexture(&texture_descriptor);
			render_views[index] = graphics->CreateRenderTargetView(cube_textures[index]);
		}

		// Generate a unit cube vertex buffer - a cube is needed instead of a rectangle because it will be rotated
		// by the look at matrix
		VertexBuffer vertex_buffer;
		IndexBuffer index_buffer;
		CreateCubeVertexBuffer(graphics, 0.5f, vertex_buffer, index_buffer);

		// Bind a nullptr depth stencil view - remove depth
		RenderTargetView current_render_view = graphics->GetBoundRenderTarget();
		DepthStencilView current_depth_view = graphics->GetBoundDepthStencil();
		GraphicsViewport current_viewport = graphics->GetBoundViewport();

		graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_CREATE_TEXTURE_CUBE);
		graphics->BindVertexBuffer(vertex_buffer);
		graphics->BindIndexBuffer(index_buffer);
		graphics->BindTopology(Topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
		graphics->BindPixelResourceView(texture_view);

		Matrix projection_matrix = ProjectionMatrixTextureCube();
		
		ConstantBuffer vertex_constants = graphics->CreateConstantBuffer(sizeof(Matrix));
		graphics->BindVertexConstantBuffer(vertex_constants);
		GraphicsViewport cube_viewport = { 0.0f, 0.0f, face_size.x, face_size.y, 0.0f, 1.0f };
		graphics->BindViewport(cube_viewport);
		graphics->DisableDepth();
		graphics->DisableCulling();

		for (size_t index = 0; index < 6; index++) {
			Matrix current_matrix = MatrixTranspose(ViewMatrixTextureCube((TextureCubeFace)index) * projection_matrix);
			UpdateBufferResource(vertex_constants.buffer, &current_matrix, sizeof(Matrix), graphics->GetContext());
			graphics->BindRenderTargetView(render_views[index], nullptr);

			graphics->DrawIndexed(index_buffer.count);
		}
		graphics->EnableDepth();
		graphics->EnableCulling();

		graphics->BindRenderTargetView(current_render_view, current_depth_view);
		graphics->BindViewport(current_viewport);

		cube = ConvertTexturesToCube(cube_textures, graphics->GetContext());
		ResourceView cube_view = graphics->CreateTextureShaderViewResource(cube);
		graphics->GenerateMips(cube_view);
		cube_view.view->Release();

		vertex_buffer.buffer->Release();
		index_buffer.buffer->Release();
		vertex_constants.buffer->Release();
		for (size_t index = 0; index < 6; index++) {
			ReleaseRenderView(render_views[index]);
		}

		return cube;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 GetTextureDimensions(const wchar_t* filename)
	{
		uint2 dimensions = { 0,0 };

		Stream<wchar_t> path = ToStream(filename);
		Path2 extensions = function::PathExtensionBoth(path);

		if (extensions.absolute.size == 0 && extensions.relative.size == 0) {
			return dimensions;
		}
		Stream<wchar_t> valid_extension = function::GetValidPath(extensions);

		bool is_tga = false;
		bool is_hdr = false;

		is_tga = function::CompareStrings(valid_extension, ToStream(L".tga"));
		is_hdr = function::CompareStrings(valid_extension, ToStream(L".hdr"));

		DirectX::TexMetadata metadata;
		HRESULT result;

		if (is_tga) {
			result = DirectX::GetMetadataFromTGAFile(filename, metadata);
		}
		else if (is_hdr) {
			result = DirectX::GetMetadataFromHDRFile(filename, metadata);
		}
		else {
			result = DirectX::GetMetadataFromWICFile(filename, DirectX::WIC_FLAGS_NONE, metadata);
		}

		if (FAILED(result)) {
			return dimensions;
		}

		dimensions = { (unsigned int)metadata.width, (unsigned int)metadata.height };
		return dimensions;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	uint2 GetTextureDimensions(containers::Stream<void> data, TextureExtension extension)
	{
		using function = uint2(*)(containers::Stream<void> data);
		function functions[] = {
			GetTextureDimensionsJPG,
			GetTextureDimensionsPNG,
			GetTextureDimensionsTIFF,
			GetTextureDimensionsBMP,
			GetTextureDimensionsTGA,
			GetTextureDimensionsHDR
		};

		return functions[extension](data);
	}

	template<typename Handler>
	uint2 GetTextureDimensionsExtension(containers::Stream<void> data, Handler&& handler) {
		uint2 dimensions = { 0,0 };

		DirectX::TexMetadata metadata;
		HRESULT result;

		result = handler(metadata);

		if (FAILED(result)) {
			return dimensions;
		}

		dimensions = { (unsigned int)metadata.width, (unsigned int)metadata.height };
		return dimensions;
	}

	uint2 GetTextureDimensionsPNG(containers::Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
		});
	}

	uint2 GetTextureDimensionsJPG(containers::Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsBMP(containers::Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsTIFF(containers::Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromWICMemory(data.buffer, data.size, DirectX::WIC_FLAGS_NONE, metadata);
			});
	}

	uint2 GetTextureDimensionsTGA(containers::Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromTGAMemory(data.buffer, data.size, metadata);
			});
	}

	uint2 GetTextureDimensionsHDR(containers::Stream<void> data)
	{
		return GetTextureDimensionsExtension(data, [=](DirectX::TexMetadata& metadata) {
			return DirectX::GetMetadataFromHDRMemory(data.buffer, data.size, metadata);
			});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

}