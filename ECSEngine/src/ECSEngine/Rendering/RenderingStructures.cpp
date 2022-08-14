#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Utilities/Function.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Utilities/File.h"
#include "../Utilities/ForEachFiles.h"

namespace ECSEngine {

	constexpr const wchar_t* PBR_MATERIAL_COLOR_TEXTURE_STRINGS[] = {
		L"diff",
		L"diffuse",
		L"color",
		L"Color",
		L"Diff",
		L"Diffuse",
		L"albedo",
		L"Albedo"
	};

	constexpr const wchar_t* PBR_MATERIAL_METALLIC_TEXTURE_STRINGS[] = {
		L"Metallic",
		L"metallic",
		L"met",
		L"Met"
	};

	constexpr const wchar_t* PBR_MATERIAL_ROUGHNESS_TEXTURE_STRINGS[] = {
		L"roughness",
		L"Roughness",
		L"rough",
		L"Rough"
	};

	constexpr const wchar_t* PBR_MATERIAL_OCCLUSION_TEXTURE_STRINGS[] = {
		L"AO",
		L"AmbientOcclusion",
		L"Occlusion",
		L"ao",
		L"OCC",
		L"occ"
	};

	constexpr const wchar_t* PBR_MATERIAL_NORMAL_TEXTURE_STRINGS[] = {
		L"Normal",
		L"normal",
		L"Nor",
		L"nor"
	};

	constexpr const wchar_t* PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS[] = {
		L"Emissive",
		L"emissive",
		L"emiss",
		L"Emiss"
	};

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	ID3D11Resource* GetResourceBuffer(Buffer buffer, const wchar_t* error_message) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, error_message, true);

		return _resource.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	ID3D11Resource* GetResourceTexture(Texture* texture, const wchar_t* error_message) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<Texture> com_tex;
		com_tex.Attach(texture);
		HRESULT result = com_tex.As(&_resource);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, error_message, true);

		return _resource.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename View>
	ID3D11Resource* GetResourceView(View view) {
		ID3D11Resource* resource;
		view.view->GetResource(&resource);
		unsigned int count = resource->Release();
		return resource;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	VertexBuffer::VertexBuffer() : stride(0), buffer(nullptr) {}

	VertexBuffer::VertexBuffer(UINT _stride, UINT _size, ID3D11Buffer* _buffer) 
		: stride(_stride), size(_size), buffer(_buffer) {}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* VertexBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, L"Converting VertexBuffer to resource failed.");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	VertexBuffer VertexBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		VertexBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating vertex buffer failed", true);

		return buffer;	
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	IndexBuffer::IndexBuffer() : count(0), buffer(nullptr) {}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* IndexBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, L"Converting IndexBuffer to resource failed.");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	IndexBuffer IndexBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		IndexBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating index buffer failed", true);

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	InputLayout::InputLayout() : layout(nullptr) {}

	InputLayout InputLayout::RawCreate(GraphicsDevice* device, Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, Stream<void> byte_code)
	{
		InputLayout layout;
		HRESULT result;

		result = device->CreateInputLayout(
			descriptor.buffer,
			descriptor.size,
			byte_code.buffer,
			byte_code.size,
			&layout.layout
		);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating input layout failed", true);

		return layout;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	VertexShader::VertexShader() : byte_code(nullptr, 0), shader(nullptr) {}

	VertexShader VertexShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		VertexShader shader;

		HRESULT result;
		result = device->CreateVertexShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Domain shader failed.", true);

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	PixelShader::PixelShader() : shader(nullptr) {}

	PixelShader PixelShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		PixelShader shader;

		HRESULT result;
		result = device->CreatePixelShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Domain shader failed.", true);

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	GeometryShader GeometryShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		GeometryShader shader;

		HRESULT result;
		result = device->CreateGeometryShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Geometry shader failed.", true);

		return shader;
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------

	DomainShader DomainShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		DomainShader shader;

		HRESULT result;
		result = device->CreateDomainShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Domain shader failed.", true);
		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	HullShader HullShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		HullShader shader;

		HRESULT result;
		result = device->CreateHullShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Hull shader failed.", true);

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ComputeShader ComputeShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		ComputeShader shader;

		HRESULT result;
		result = device->CreateComputeShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Geometry shader failed.", true);

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Topology::Topology() : value(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {}

	Topology::Topology(D3D11_PRIMITIVE_TOPOLOGY topology) : value(topology) {}

	// --------------------------------------------------------------------------------------------------------------------------------

	ResourceView::ResourceView() : view(nullptr) {}

	ResourceView::ResourceView(ID3D11ShaderResourceView* _view) : view(_view) {}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* ResourceView::GetResource() const
	{
		return GetResourceView(*this);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ResourceView ResourceView::RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_SHADER_RESOURCE_VIEW_DESC* descriptor)
	{
		ResourceView view;

		HRESULT result;
		result = device->CreateShaderResourceView(resource, descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture 1D Shader View failed.", true);

		return view;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ResourceView ResourceView::RawCopy(GraphicsDevice* device, ID3D11Resource* resource, ResourceView view)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC descriptor;
		view.view->GetDesc(&descriptor);

		return ResourceView::RawCreate(device, resource, &descriptor);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer::ConstantBuffer() : buffer(nullptr) {}

	ConstantBuffer::ConstantBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* ConstantBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, L"Converting ConstantBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer ConstantBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		ConstantBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating pixel constant buffer failed", true);

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	SamplerState::SamplerState() : sampler(nullptr) {}

	SamplerState::SamplerState(ID3D11SamplerState* _sampler) : sampler(_sampler) {}

	SamplerState SamplerState::RawCreate(GraphicsDevice* device, const D3D11_SAMPLER_DESC* descriptor)
	{
		SamplerState state;
		HRESULT result = device->CreateSamplerState(descriptor, &state.sampler);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Constructing sampler state failed!", true);

		return state;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* UAView::GetResource() const
	{
		return GetResourceView(*this);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	UAView UAView::RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* descriptor)
	{
		UAView view;

		HRESULT result = device->CreateUnorderedAccessView(resource, descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating UAView from Append Buffer failed.", true);

		return view;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	UAView UAView::RawCopy(GraphicsDevice* device, ID3D11Resource* resource, UAView view)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC descriptor;
		view.view->GetDesc(&descriptor);

		return UAView::RawCreate(device, resource, &descriptor);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* RenderTargetView::GetResource() const
	{
		return GetResourceView(*this);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	RenderTargetView RenderTargetView::RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_RENDER_TARGET_VIEW_DESC* descriptor)
	{
		RenderTargetView view;
		HRESULT result = device->CreateRenderTargetView(resource, descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed!", true);

		return view;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	RenderTargetView RenderTargetView::RawCopy(GraphicsDevice* device, ID3D11Resource* resource, RenderTargetView view)
	{
		D3D11_RENDER_TARGET_VIEW_DESC descriptor;
		view.view->GetDesc(&descriptor);

		return RenderTargetView::RawCreate(device, resource, &descriptor);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* DepthStencilView::GetResource() const
	{
		return GetResourceView(*this);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	DepthStencilView DepthStencilView::RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_DEPTH_STENCIL_VIEW_DESC* descriptor)
	{
		DepthStencilView view;

		HRESULT result = device->CreateDepthStencilView(resource, descriptor, &view.view);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating render target view failed!", true);

		return view;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	DepthStencilView DepthStencilView::RawCopy(GraphicsDevice* device, ID3D11Resource* resource, DepthStencilView view)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC descriptor;
		view.view->GetDesc(&descriptor);

		return DepthStencilView::RawCreate(device, resource, &descriptor);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* StandardBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, L"Converting StandardBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	StandardBuffer StandardBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		StandardBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* StructuredBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, L"Converting StructedBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	StructuredBuffer StructuredBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		StructuredBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* IndirectBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, L"Converting IndirectBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	IndirectBuffer IndirectBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		IndirectBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* UABuffer::GetResource() const
	{
		return GetResourceBuffer(*this, L"Converting UABuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	UABuffer UABuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		UABuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* AppendStructuredBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, L"Converting AppendStructuredBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	AppendStructuredBuffer AppendStructuredBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		AppendStructuredBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* ConsumeStructuredBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, L"Converting ConsumeStructuredBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ConsumeStructuredBuffer ConsumeStructuredBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		ConsumeStructuredBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating standard buffer failed.", true);

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture1D::Texture1D() : tex(nullptr) {}

	Texture1D::Texture1D(ID3D11Resource* _resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture1D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting resource to Texture1D failed!", true);
		tex = com_tex.Detach();
	}

	Texture1D::Texture1D(ID3D11Texture1D* _tex) : tex(_tex) {}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* Texture1D::GetResource() const
	{
		return GetResourceTexture(tex, L"Converting Texture1D into a resource failed.");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture1D Texture1D::RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		RawInterface* interface_;
		HRESULT result = device->CreateTexture1D(descriptor, initial_data, &interface_);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture1D failed.", true);

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture2D::Texture2D() : tex(nullptr) {}

	Texture2D::Texture2D(ID3D11Texture2D* _tex) : tex(_tex) {}

	Texture2D::Texture2D(ID3D11Resource* _resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting resource to Texture2D failed!", true);
		tex = com_tex.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* Texture2D::GetResource() const
	{
		return GetResourceTexture(tex, L"Converting Texture2D to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture2D Texture2D::RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		RawInterface* interface_;
		HRESULT result = device->CreateTexture2D(descriptor, initial_data, &interface_);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture2D failed.", true);

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture3D::Texture3D() : tex(nullptr) {}

	Texture3D::Texture3D(ID3D11Texture3D* _tex) : tex(_tex) {}

	Texture3D::Texture3D(ID3D11Resource* _resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture3D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting resource to Texture3D failed!", true);
		tex = com_tex.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* Texture3D::GetResource() const
	{
		return GetResourceTexture(tex, L"Converting Texture3D to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture3D Texture3D::RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		RawInterface* interface_;
		HRESULT result = device->CreateTexture3D(descriptor, initial_data, &interface_);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating Texture3D failed.", true);

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	TextureCube::TextureCube() : tex(nullptr) {}

	TextureCube::TextureCube(ID3D11Resource* _resource) {
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting resource to Texture3D failed!", true);
		tex = com_tex.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* TextureCube::GetResource() const
	{
		return GetResourceTexture(tex, L"Converting TextureCube to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	TextureCube TextureCube::RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		RawInterface* interface_;
		HRESULT result = device->CreateTexture2D(descriptor, initial_data, &interface_);
		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Creating TextureCube failed.", true);

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	TextureCube::TextureCube(ID3D11Texture2D* _tex) : tex(_tex) {}

	// --------------------------------------------------------------------------------------------------------------------------------

	Translation::Translation() : x(0.0f), y(0.0f), z(0.0f) {}

	Translation::Translation(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

	// --------------------------------------------------------------------------------------------------------------------------------

	Color::Color() : red(0), green(0), blue(0), alpha(255) {}

	Color::Color(unsigned char _red) : red(_red), green(0), blue(0), alpha(255) {}

	Color::Color(unsigned char _red, unsigned char _green) : red(_red), green(_green), blue(0), alpha(255) {}

	Color::Color(unsigned char _red, unsigned char _green, unsigned char _blue) : red(_red), green(_green), blue(_blue), alpha(255) {}

	Color::Color(unsigned char _red, unsigned char _green, unsigned char _blue, unsigned char _alpha)
	 : red(_red), green(_green), blue(_blue), alpha(_alpha) {}

	constexpr float COLOR_RANGE = 255;

	Color::Color(float _red, float _green, float _blue, float _alpha)
	{
		red = _red * COLOR_RANGE;
		green = _green * COLOR_RANGE;
		blue = _blue * COLOR_RANGE;
		alpha = _alpha * COLOR_RANGE;
	}

	Color::Color(ColorFloat color)
	{
		red = color.red * COLOR_RANGE;
		green = color.green * COLOR_RANGE;
		blue = color.blue * COLOR_RANGE;
		alpha = color.alpha * COLOR_RANGE;
	}

	Color::Color(const unsigned char* values) : red(values[0]), green(values[1]), blue(values[2]), alpha(values[3]) {}
 
	Color::Color(const float* values) : red(values[0] * COLOR_RANGE), green(values[1] * COLOR_RANGE), blue(values[2] * COLOR_RANGE), alpha(values[3] * COLOR_RANGE) {}

	bool Color::operator==(const Color& other) const
	{
		return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
	}

	bool Color::operator != (const Color& other) const {
		return red != other.red || green != other.green || blue != other.blue || alpha != other.alpha;
	}

	Color Color::operator+(const Color& other) const
	{
		return Color(red + other.red, green + other.green, blue + other.blue);
	}

	Color Color::operator*(const Color& other) const
	{
		constexpr float inverse_range = 1.0f / 65535;
		Color new_color;

		float current_red = static_cast<float>(red);
		new_color.red = current_red * other.red * inverse_range;

		float current_green = static_cast<float>(green);
		new_color.green = current_green * other.green * inverse_range;

		float current_blue = static_cast<float>(blue);
		new_color.blue = current_blue * other.blue * inverse_range;

		float current_alpha = static_cast<float>(alpha);
		new_color.alpha = current_alpha * other.alpha * inverse_range;

		return new_color;
	}

	Color Color::operator*(float percentage) const
	{
		Color new_color;
		new_color.red = static_cast<float>(red) * percentage;
		new_color.green = static_cast<float>(green) * percentage;
		new_color.blue = static_cast<float>(blue) * percentage;
		new_color.alpha = static_cast<float>(alpha) * percentage;
		return new_color;
	}

	void Color::Normalize(float* values) const
	{
		constexpr float inverse = 1.0f / 255.0f;
		values[0] = (float)red * inverse;
		values[1] = (float)green * inverse;
		values[2] = (float)blue * inverse;
		values[3] = (float)alpha * inverse;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ColorFloat::ColorFloat() : red(0.0f), green(0.0f), blue(0.0f), alpha(1.0f) {}

	ColorFloat::ColorFloat(float _red) : red(_red), green(0.0f), blue(0.0f), alpha(1.0f) {}

	ColorFloat::ColorFloat(float _red, float _green) : red(_red), green(_green), blue(0.0f), alpha(1.0f) {}

	ColorFloat::ColorFloat(float _red, float _green, float _blue)
		: red(_red), green(_green), blue(_blue), alpha(1.0f) {}

	ColorFloat::ColorFloat(float _red, float _green, float _blue, float _alpha)
		: red(_red), green(_green), blue(_blue), alpha(_alpha) {}

	ColorFloat::ColorFloat(Color color)
	{
		constexpr float inverse_range = 1.0f / Color::GetRange();
		red = static_cast<float>(color.red) * inverse_range;
		green = static_cast<float>(color.green) * inverse_range;
		blue = static_cast<float>(color.blue) * inverse_range;
		alpha = static_cast<float>(color.alpha) * inverse_range;
	}

	ColorFloat::ColorFloat(const float* values) : red(values[0]), green(values[1]), blue(values[2]), alpha(values[3]) {}

	ColorFloat ColorFloat::operator*(const ColorFloat& other) const
	{
		return ColorFloat(red * other.red, green * other.green, blue * other.blue, alpha * other.alpha);
	}

	ColorFloat ColorFloat::operator*(float percentage) const
	{
		return ColorFloat(red * percentage, green * percentage, blue * percentage, alpha * percentage);
	}

	ColorFloat ColorFloat::operator+(const ColorFloat& other) const
	{
		return ColorFloat(red + other.red, green + other.green, blue + other.blue, alpha + other.alpha);
	}

	ColorFloat ColorFloat::operator-(const ColorFloat& other) const
	{
		return ColorFloat(red - other.red, green - other.green, blue - other.blue, alpha - other.alpha);
	}

	void ColorFloat::Normalize(float* values) const
	{
		values[0] = red;
		values[1] = green;
		values[2] = blue;
		values[3] = alpha;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Camera::Camera() : translation(0.0f, 0.0f, 0.0f), rotation(0.0f, 0.0f, 0.0f) {}

	Camera::Camera(float3 _translation, float3 _rotation) : translation(_translation), rotation(_rotation) {}

	Camera::Camera(Matrix _projection, float3 _translation, float3 _rotation) : projection(_projection), translation(_translation), rotation(_rotation) {}

	void Camera::SetOrthographicProjection(float width, float height, float near_z, float far_z) {
		projection = MatrixOrthographic(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjection(float width, float height, float near_z, float far_z) {
		projection = MatrixPerspective(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjectionFOV(float angle_y, float aspect_ratio, float near_z, float far_z) {
		projection = MatrixPerspectiveFOV(angle_y, aspect_ratio, near_z, far_z);
	}

	Matrix Camera::GetViewProjectionMatrix() const {
		return MatrixTranslation(-translation) * MatrixRotationZ(-rotation.z) * MatrixRotationY(-rotation.y) * MatrixRotationX(-rotation.x) * projection;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void SetPBRMaterialTexture(PBRMaterial* material, uintptr_t& memory, Stream<wchar_t> texture, PBRMaterialTextureIndex texture_index) {
		void* base_address = (void*)function::AlignPointer(
			(uintptr_t)function::OffsetPointer(material, sizeof(const char*) + sizeof(float) + sizeof(float) + sizeof(Color) + sizeof(float3)),
			alignof(Stream<wchar_t>)
		);

		Stream<wchar_t>* texture_name = (Stream<wchar_t>*)function::OffsetPointer(
			base_address,
			sizeof(Stream<wchar_t>) * texture_index
		);
		texture_name->InitializeFromBuffer(memory, texture.size);
		texture_name->Copy(texture);
		texture_name->buffer[texture_name->size] = L'\0';
		memory += sizeof(wchar_t);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void AllocatePBRMaterial(
		PBRMaterial& material, 
		Stream<char> name, 
		Stream<PBRMaterialMapping> mappings,
		AllocatorPolymorphic allocator
	)
	{
		size_t total_allocation_size = name.size;

		for (size_t index = 0; index < mappings.size; index++) {
			total_allocation_size += (mappings[index].texture.size + 1) * sizeof(wchar_t);
		}

		void* allocation = Allocate(allocator, total_allocation_size, alignof(wchar_t));

		uintptr_t ptr = (uintptr_t)allocation;
		char* mutable_ptr = (char*)ptr;
		memcpy(mutable_ptr, name.buffer, (name.size + 1) * sizeof(char));
		mutable_ptr[name.size] = '\0';
		material.name = (const char*)ptr;
		ptr += sizeof(char) * (name.size + 1);

		ptr = function::AlignPointer(ptr, alignof(wchar_t));

		for (size_t index = 0; index < mappings.size; index++) {
			SetPBRMaterialTexture(&material, ptr, mappings[index].texture, mappings[index].index);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	// Everything is coallesced into a single allocation
	void FreePBRMaterial(const PBRMaterial& material, AllocatorPolymorphic allocator)
	{
		if (material.name != nullptr) {
			Deallocate(allocator, material.name);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	PBRMaterial CreatePBRMaterialFromName(
		Stream<char> material_name,
		Stream<char> texture_base_name,
		Stream<wchar_t> search_directory, 
		AllocatorPolymorphic allocator, 
		Stream<PBRMaterialTextureIndex>* texture_mask
	)
	{
		ECS_TEMP_STRING(wide_base_name, 512);
		ECS_ASSERT(texture_base_name.size < 512);
		function::ConvertASCIIToWide(wide_base_name, texture_base_name);

		return CreatePBRMaterialFromName(material_name, wide_base_name, search_directory, allocator, texture_mask);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	PBRMaterial CreatePBRMaterialFromName(
		Stream<char> material_name,
		Stream<wchar_t> texture_base_name,
		Stream<wchar_t> search_directory,
		AllocatorPolymorphic allocator,
		Stream<PBRMaterialTextureIndex>* texture_mask
	) {
		PBRMaterial material;
		memset(&material, 0, sizeof(PBRMaterial));

		material.tint = Color((unsigned char)255, 255, 255, 255);
		material.emissive_factor = { 0.0f, 0.0f, 0.0f };
		material.metallic_factor = 1.0f;
		material.roughness_factor = 1.0f;

		Stream<const wchar_t*> material_strings[ECS_PBR_MATERIAL_MAPPING_COUNT];
		material_strings[ECS_PBR_MATERIAL_COLOR] = { PBR_MATERIAL_COLOR_TEXTURE_STRINGS, std::size(PBR_MATERIAL_COLOR_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_EMISSIVE] = { PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS, std::size(PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_METALLIC] = { PBR_MATERIAL_METALLIC_TEXTURE_STRINGS, std::size(PBR_MATERIAL_METALLIC_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_NORMAL] = { PBR_MATERIAL_NORMAL_TEXTURE_STRINGS, std::size(PBR_MATERIAL_NORMAL_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_OCCLUSION] = { PBR_MATERIAL_OCCLUSION_TEXTURE_STRINGS, std::size(PBR_MATERIAL_OCCLUSION_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_ROUGHNESS] = { PBR_MATERIAL_ROUGHNESS_TEXTURE_STRINGS, std::size(PBR_MATERIAL_ROUGHNESS_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_EMISSIVE] = { PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS, std::size(PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS) };

		PBRMaterialTextureIndex _default_mappings[ECS_PBR_MATERIAL_MAPPING_COUNT];
		_default_mappings[0] = ECS_PBR_MATERIAL_COLOR;
		_default_mappings[1] = ECS_PBR_MATERIAL_EMISSIVE;
		_default_mappings[2] = ECS_PBR_MATERIAL_METALLIC;
		_default_mappings[3] = ECS_PBR_MATERIAL_NORMAL;
		_default_mappings[4] = ECS_PBR_MATERIAL_OCCLUSION;
		_default_mappings[5] = ECS_PBR_MATERIAL_ROUGHNESS;

		Stream<PBRMaterialTextureIndex> default_mappings(_default_mappings, ECS_PBR_MATERIAL_MAPPING_COUNT);
		if (texture_mask == nullptr) {
			texture_mask = &default_mappings;
		}

		ECS_TEMP_STRING(temp_memory, 2048);
		ECS_TEMP_STRING(__base_name, 256);
		__base_name.Copy(texture_base_name);
		__base_name[texture_base_name.size] = L'\0';
		texture_base_name.buffer = __base_name.buffer;

		PBRMaterialMapping _valid_textures[ECS_PBR_MATERIAL_MAPPING_COUNT];
		Stream<PBRMaterialMapping> valid_textures(_valid_textures, 0);

		struct Data {
			Stream<wchar_t> base_name;
			Stream<PBRMaterialTextureIndex>* texture_mask;
			Stream<PBRMaterialMapping>* valid_textures;
			CapacityStream<wchar_t>* temp_memory;
			Stream<Stream<const wchar_t*>> material_strings;
		};

		auto functor = [](Stream<wchar_t> path, void* _data) {
			Data* data = (Data*)_data;
			// find the base part first
			const wchar_t* base_start = wcsstr(path.buffer, data->base_name.buffer);
			if (base_start != nullptr) {
				base_start += data->base_name.size;
				while (*base_start == '_') {
					base_start++;
				}
				size_t mask_count = data->texture_mask->size;

				unsigned int found_index = -1;
				// linear search for every pbr material texture
				for (size_t index = 0; index < mask_count && found_index == -1; index++) {
					// linear search for every string in the material texture
					Stream<const wchar_t*> material_strings = data->material_strings[data->texture_mask->buffer[index]];
					for (size_t string_index = 0; string_index < material_strings.size && found_index == -1; string_index++) {
						if (memcmp(base_start, material_strings[string_index], sizeof(wchar_t) * wcslen(material_strings[string_index])) == 0) {
							found_index = index;
						}
					}
				}

				// The path is valid, a texture matches
				if (found_index != -1) {
					data->valid_textures->Add({ {data->temp_memory->buffer + data->temp_memory->size, path.size}, data->texture_mask->buffer[found_index] });
					data->texture_mask->RemoveSwapBack(found_index);
					data->temp_memory->AddStreamSafe(path);
				}
			}

			return data->texture_mask->size > 0;
		};

		Data data;
		data.base_name = texture_base_name;
		data.material_strings = { material_strings, std::size(material_strings) };
		data.temp_memory = &temp_memory;
		data.texture_mask = texture_mask;
		data.valid_textures = &valid_textures;
		ForEachFileInDirectoryRecursive(search_directory, &data, functor);

		size_t total_size = sizeof(wchar_t) * temp_memory.size + sizeof(char) * material_name.size;
		void* allocation = Allocate(allocator, total_size, alignof(wchar_t));

		uintptr_t buffer = (uintptr_t)allocation;
		char* mutable_char = (char*)buffer;
		memcpy(mutable_char, material_name.buffer, sizeof(char)* (material_name.size + 1));
		material.name = mutable_char;
		buffer += (material_name.size + 1) * sizeof(char);

		buffer = function::AlignPointer(buffer, alignof(wchar_t));
		for (size_t index = 0; index < valid_textures.size; index++) {
			SetPBRMaterialTexture(&material, buffer, valid_textures[index].texture, valid_textures[index].index);
		}

		return material;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Material::Material() : vertex_buffer_mapping_count(0), vc_buffer_count(0), pc_buffer_count(0), dc_buffer_count(0), hc_buffer_count(0),
		gc_buffer_count(0), vertex_texture_count(0), pixel_texture_count(0), domain_texture_count(0), hull_texture_count(0),
		geometry_texture_count(0), unordered_view_count(0), domain_shader(nullptr), hull_shader(nullptr), geometry_shader(nullptr) {}

	// --------------------------------------------------------------------------------------------------------------------------------

	VertexBuffer GetMeshVertexBuffer(const Mesh& mesh, ECS_MESH_INDEX buffer_type)
	{
		for (size_t index = 0; index < mesh.mapping_count; index++) {
			if (mesh.mapping[index] == buffer_type) {
				return mesh.vertex_buffers[index];
			}
		}
		return VertexBuffer();
	}

	// --------------------------------------------------------------------------------------------------------------------------------
	
	void SetMeshVertexBuffer(Mesh& mesh, ECS_MESH_INDEX buffer_type, VertexBuffer buffer)
	{
		for (size_t index = 0; index < mesh.mapping_count; index++) {
			if (mesh.mapping[index] == buffer_type) {
				mesh.vertex_buffers[index] = buffer;
				return;
			}
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------------------

}