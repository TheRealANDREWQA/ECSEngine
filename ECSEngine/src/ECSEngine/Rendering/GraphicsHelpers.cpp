#include "ecspch.h"
#include "../Utilities/Function.h"
#include "GraphicsHelpers.h"

namespace ECSEngine {

	void GetTextureDimensions(ID3D11Resource* _resource, unsigned int& width, unsigned int& height) {
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		resource.Attach(_resource);
		HRESULT result;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		result = resource.As(&texture);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting sprite texture resource failed!", true);

		D3D11_TEXTURE2D_DESC descriptor;
		texture->GetDesc(&descriptor);
		width = descriptor.Width;
		height = descriptor.Height;
	}

	void GetTextureDimensions(ID3D11Resource* _resource, unsigned short& width, unsigned short& height) {
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		resource.Attach(_resource);
		HRESULT result;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		result = resource.As(&texture);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting sprite texture resource failed!", true);

		D3D11_TEXTURE2D_DESC descriptor;
		texture->GetDesc(&descriptor);
		width = descriptor.Width;
		height = descriptor.Height;
	}

	ID3D11Resource* GetResource(Texture1D texture)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Texture1D> com_tex;
		com_tex.Attach(texture.tex);
		HRESULT result = com_tex.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Texture1D to resource failed!", true);

		return _resource.Detach();
	}

	ID3D11Resource* GetResource(Texture2D texture)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
		com_tex.Attach(texture.tex);
		HRESULT result = com_tex.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Texture2D to resource failed!", true);

		return _resource.Detach();
	}

	ID3D11Resource* GetResource(Texture3D texture)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Texture3D> com_tex;
		com_tex.Attach(texture.tex);
		HRESULT result = com_tex.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Texture3D to resource failed!", true);

		return _resource.Detach();
	}

	ID3D11Resource* GetResource(RenderTargetView target)
	{
		ID3D11Resource* resource;
		target.target->GetResource(&resource);
		unsigned int count = resource->Release();
		return resource;
	}

	ID3D11Resource* GetResource(DepthStencilView depth_view)
	{
		ID3D11Resource* resource;
		depth_view.view->GetResource(&resource);
		unsigned int count = resource->Release();
		return resource;
	}

	ID3D11Resource* GetResource(ResourceView ps_view)
	{
		ID3D11Resource* resource;
		ps_view.view->GetResource(&resource);
		unsigned int count = resource->Release();
		return resource;
	}

	ID3D11Resource* GetResource(VertexBuffer buffer)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Vertex Buffer to resource failed!", true);

		return _resource.Detach();
	}

	ID3D11Resource* GetResource(ConstantBuffer vc_buffer)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(vc_buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Constant Buffer to resource failed!", true);

		return _resource.Detach();
	}

	ID3D11Resource* GetResource(StructuredBuffer buffer) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Structured Buffer to resource failed!", true);

		return _resource.Detach();
	}

	ID3D11Resource* GetResource(StandardBuffer buffer) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Standard Buffer to resource failed!", true);

		return _resource.Detach();
	}

	ID3D11Resource* GetResource(IndirectBuffer buffer) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting Indirect Buffer to resource failed!", true);

		return _resource.Detach();
	}

	ID3D11Resource* GetResource(UABuffer buffer)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting UA Buffer to resource failed!", true);

		return _resource.Detach();
	}

	Texture1D ToStagingTexture(Texture1D texture, bool* success, GraphicsContext* context)
	{
		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE1D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		texture.tex->GetDevice(&device);

		texture_descriptor.Usage = D3D11_USAGE_STAGING;
		texture_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

		ID3D11Texture1D* new_texture = nullptr;
		HRESULT result = device->CreateTexture1D(&texture_descriptor, nullptr, &new_texture);
		if (FAILED(result)) {
			if (success != nullptr) {
				*success = false;
			}
			return Texture1D((ID3D11Texture1D*)nullptr);
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		context->CopyResource(new_texture, texture.tex);
		return Texture1D(new_texture);
	}

	Texture2D ToStagingTexture(Texture2D texture, bool* success, GraphicsContext* context)
	{
		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE2D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		texture.tex->GetDevice(&device);

		texture_descriptor.BindFlags = 0;
		texture_descriptor.Usage = D3D11_USAGE_STAGING;
		texture_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		texture_descriptor.MiscFlags = 0;

		ID3D11Texture2D* new_texture = nullptr;
		HRESULT result = device->CreateTexture2D(&texture_descriptor, nullptr, &new_texture);
		if (FAILED(result)) {
			if (success != nullptr) {
				*success = false;
			}
			return Texture2D((ID3D11Texture2D*)nullptr);
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		context->CopyResource(new_texture, texture.tex);
		return Texture2D(new_texture);
	}

	Texture3D ToStagingTexture(Texture3D texture, bool* success, GraphicsContext* context)
	{
		GraphicsDevice* device = nullptr;
		D3D11_TEXTURE3D_DESC texture_descriptor;
		texture.tex->GetDesc(&texture_descriptor);
		texture.tex->GetDevice(&device);

		texture_descriptor.Usage = D3D11_USAGE_STAGING;
		texture_descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

		ID3D11Texture3D* new_texture = nullptr;
		HRESULT result = device->CreateTexture3D(&texture_descriptor, nullptr, &new_texture);
		if (FAILED(result)) {
			if (success != nullptr) {
				*success = false;
			}
			return Texture3D((ID3D11Texture3D*)nullptr);
		}

		if (context == nullptr) {
			device->GetImmediateContext(&context);
		}

		context->CopyResource(new_texture, texture.tex);
		return Texture3D(new_texture);
	}

	template<typename GraphicsResource>
	void CopyGraphicsResource(GraphicsResource destination, GraphicsResource source) {
		GraphicsContext* context = nullptr;
		GraphicsDevice* device = source.GetDevice();
		device->GetImmediateContext(&context);
		context->CopyResource(destination.Resource(), source.Resource());
	}

#define EXPORT(type) template ECSENGINE_API void CopyGraphicsResource(type, type);

	ECS_GRAPHICS_RESOURCES(EXPORT);

#undef EXPORT

}