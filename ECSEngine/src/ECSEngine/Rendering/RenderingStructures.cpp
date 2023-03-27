#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Utilities/Function.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Utilities/File.h"
#include "../Utilities/ForEachFiles.h"
#include "../Utilities/Crash.h"

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
	ID3D11Resource* GetResourceBuffer(Buffer buffer, const char* error_message) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), nullptr, error_message);

		return _resource.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	ID3D11Resource* GetResourceTexture(Texture* texture, const char* error_message) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<Texture> com_tex;
		com_tex.Attach(texture);
		HRESULT result = com_tex.As(&_resource);

		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), nullptr, error_message);

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

	ID3D11Resource* VertexBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, "Converting VertexBuffer to resource failed.");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	VertexBuffer VertexBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		VertexBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating vertex buffer failed");

		return buffer;	
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* IndexBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, "Converting IndexBuffer to resource failed.");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	IndexBuffer IndexBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		IndexBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating index buffer failed");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

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
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), layout, "Creating input layout failed");

		return layout;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	VertexShader VertexShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		VertexShader shader;

		HRESULT result;
		result = device->CreateVertexShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Vertex shader failed.");

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	PixelShader PixelShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		PixelShader shader;

		HRESULT result;
		result = device->CreatePixelShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Pixel shader failed.");

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	GeometryShader GeometryShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		GeometryShader shader;

		HRESULT result;
		result = device->CreateGeometryShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Geometry shader failed.");

		return shader;
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------

	DomainShader DomainShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		DomainShader shader;

		HRESULT result;
		result = device->CreateDomainShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Domain shader failed.");
		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	HullShader HullShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		HullShader shader;

		HRESULT result;
		result = device->CreateHullShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Hull shader failed.");

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ComputeShader ComputeShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		ComputeShader shader;

		HRESULT result;
		result = device->CreateComputeShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), shader, "Creating Compute shader failed.");

		return shader;
	}

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
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating Texture Shader View failed.");

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

	ID3D11Resource* ConstantBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, "Converting ConstantBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ConstantBuffer ConstantBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		ConstantBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating ConstantBuffer failed!");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	SamplerState SamplerState::RawCreate(GraphicsDevice* device, const D3D11_SAMPLER_DESC* descriptor)
	{
		SamplerState state;
		HRESULT result = device->CreateSamplerState(descriptor, &state.sampler);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), state, "Constructing SamplerState failed!");

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
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating UAView failed.");

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
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating RenderTargetView failed!");

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
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), view, "Creating DepthStencilView failed!");

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
		return GetResourceBuffer(*this, "Converting StandardBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	StandardBuffer StandardBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		StandardBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating StandardBuffer failed.");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* StructuredBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, "Converting StructedBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	StructuredBuffer StructuredBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		StructuredBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating StructuredBuffer failed.");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* IndirectBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, "Converting IndirectBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	IndirectBuffer IndirectBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		IndirectBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating IndirectBuffer failed.");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* UABuffer::GetResource() const
	{
		return GetResourceBuffer(*this, "Converting UABuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	UABuffer UABuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		UABuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating UABuffer failed.");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* AppendStructuredBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, "Converting AppendStructuredBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	AppendStructuredBuffer AppendStructuredBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		AppendStructuredBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating AppendStructuredBuffer failed.");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* ConsumeStructuredBuffer::GetResource() const
	{
		return GetResourceBuffer(*this, "Converting ConsumeStructuredBuffer to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ConsumeStructuredBuffer ConsumeStructuredBuffer::RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		ConsumeStructuredBuffer buffer;

		HRESULT result = device->CreateBuffer(descriptor, initial_data, &buffer.buffer);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), buffer, "Creating ConsumeStructuredBuffer failed.");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture1D::Texture1D(ID3D11Resource* _resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture1D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CRASH_RETURN(SUCCEEDED(result), "Converting resource to Texture1D failed!");
		tex = com_tex.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* Texture1D::GetResource() const
	{
		return GetResourceTexture(tex, "Converting Texture1D into a resource failed.");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture1D Texture1D::RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		RawInterface* interface_;
		HRESULT result = device->CreateTexture1D(descriptor, initial_data, &interface_);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), interface_, "Creating Texture1D failed.");

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture2D::Texture2D(ID3D11Resource* _resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CRASH_RETURN(SUCCEEDED(result), "Converting resource to Texture2D failed!");
		tex = com_tex.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* Texture2D::GetResource() const
	{
		return GetResourceTexture(tex, "Converting Texture2D to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture2D Texture2D::RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		RawInterface* interface_;
		HRESULT result = device->CreateTexture2D(descriptor, initial_data, &interface_);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), interface_, "Creating Texture2D failed.");

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture3D::Texture3D(ID3D11Resource* _resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture3D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CRASH_RETURN(SUCCEEDED(result), "Converting resource to Texture3D failed!");
		tex = com_tex.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* Texture3D::GetResource() const
	{
		return GetResourceTexture(tex, "Converting Texture3D to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture3D Texture3D::RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		RawInterface* interface_;
		HRESULT result = device->CreateTexture3D(descriptor, initial_data, &interface_);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), interface_, "Creating Texture3D failed.");

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	TextureCube::TextureCube(ID3D11Resource* _resource) {
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CRASH_RETURN(SUCCEEDED(result), "Converting resource to TextureCube failed!");
		tex = com_tex.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ID3D11Resource* TextureCube::GetResource() const
	{
		return GetResourceTexture(tex, "Converting TextureCube to resource failed!");
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	TextureCube TextureCube::RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data)
	{
		RawInterface* interface_;
		HRESULT result = device->CreateTexture2D(descriptor, initial_data, &interface_);
		ECS_CRASH_RETURN_VALUE(SUCCEEDED(result), interface_, "Creating TextureCube failed.");

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Camera::Camera() : translation(0.0f, 0.0f, 0.0f), rotation(0.0f, 0.0f, 0.0f) {}

	Camera::Camera(float3 _translation, float3 _rotation) : translation(_translation), rotation(_rotation) {}

	Camera::Camera(Matrix _projection, float3 _translation, float3 _rotation) : projection(_projection), translation(_translation), rotation(_rotation) {}

	Camera::Camera(const CameraParameters& parameters) {
		translation = parameters.translation;
		rotation = parameters.rotation;

		if (parameters.is_orthographic) {
			SetOrthographicProjection(parameters.width, parameters.height, parameters.near_z, parameters.far_z);
		}
		else {
			SetPerspectiveProjection(parameters.width, parameters.height, parameters.near_z, parameters.far_z);
		}
	}

	Camera::Camera(const CameraParametersFOV& parameters) {
		translation = parameters.translation;
		rotation = parameters.rotation;

		SetPerspectiveProjectionFOV(parameters.fov, parameters.aspect_ratio, parameters.near_z, parameters.far_z);
	}

	void Camera::SetOrthographicProjection(float width, float height, float near_z, float far_z) {
		projection = MatrixOrthographic(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjection(float width, float height, float near_z, float far_z) {
		projection = MatrixPerspective(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjectionFOV(float fov, float aspect_ratio, float near_z, float far_z) {
		projection = MatrixPerspectiveFOV(fov, aspect_ratio, near_z, far_z);
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
		if (material.name.buffer != nullptr) {
			Deallocate(allocator, material.name.buffer);
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

	void UserMaterialTexture::GenerateSettingsSuffix(CapacityStream<void>& suffix) const {
		suffix.Add(&srgb);
		suffix.Add(&generate_mips);
		suffix.Add(&compression);
		suffix.Add(name);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void GenerateShaderCompileOptionsSuffix(ShaderCompileOptions compile_options, CapacityStream<void>& suffix, Stream<void> optional_addition) {
		suffix.Add(&compile_options.compile_flags);
		suffix.Add(&compile_options.target);
		for (size_t index = 0; index < compile_options.macros.size; index++) {
			size_t name_size = strlen(compile_options.macros[index].name);
			size_t definition_size = strlen(compile_options.macros[index].definition);
			suffix.Add({ compile_options.macros[index].name, name_size });
			suffix.Add({ compile_options.macros[index].definition, definition_size });
		}

		suffix.Add(optional_addition);
	}

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

	bool IsGraphicsFormatUINT(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_R8_UINT || format == ECS_GRAPHICS_FORMAT_R8_UINT || format == ECS_GRAPHICS_FORMAT_RG8_UINT
			|| format == ECS_GRAPHICS_FORMAT_RGBA8_UINT || format == ECS_GRAPHICS_FORMAT_R16_UINT || format == ECS_GRAPHICS_FORMAT_RG16_UINT
			|| format == ECS_GRAPHICS_FORMAT_RGBA16_UINT || format == ECS_GRAPHICS_FORMAT_R32_UINT || format == ECS_GRAPHICS_FORMAT_RG32_UINT
			|| format == ECS_GRAPHICS_FORMAT_RGB32_UINT || format == ECS_GRAPHICS_FORMAT_RGBA32_UINT;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatSINT(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_R8_SINT || format == ECS_GRAPHICS_FORMAT_R8_SINT || format == ECS_GRAPHICS_FORMAT_RG8_SINT
			|| format == ECS_GRAPHICS_FORMAT_RGBA8_SINT || format == ECS_GRAPHICS_FORMAT_R16_SINT || format == ECS_GRAPHICS_FORMAT_RG16_SINT
			|| format == ECS_GRAPHICS_FORMAT_RGBA16_SINT || format == ECS_GRAPHICS_FORMAT_R32_SINT || format == ECS_GRAPHICS_FORMAT_RG32_SINT
			|| format == ECS_GRAPHICS_FORMAT_RGB32_SINT || format == ECS_GRAPHICS_FORMAT_RGBA32_SINT;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatUNORM(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_R8_UNORM || format == ECS_GRAPHICS_FORMAT_RG8_UNORM || format == ECS_GRAPHICS_FORMAT_RGBA8_UNORM
			|| format == ECS_GRAPHICS_FORMAT_R16_UNORM || format == ECS_GRAPHICS_FORMAT_RG16_UNORM || format == ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatSNORM(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_R8_SNORM || format == ECS_GRAPHICS_FORMAT_RG8_SNORM || format == ECS_GRAPHICS_FORMAT_RGBA8_SNORM
			|| format == ECS_GRAPHICS_FORMAT_R16_SNORM || format == ECS_GRAPHICS_FORMAT_RG16_SNORM || format == ECS_GRAPHICS_FORMAT_RGBA16_SNORM;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatFloat(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_R16_FLOAT || format == ECS_GRAPHICS_FORMAT_RG16_FLOAT || format == ECS_GRAPHICS_FORMAT_RGBA16_FLOAT
			|| format == ECS_GRAPHICS_FORMAT_R32_FLOAT || format == ECS_GRAPHICS_FORMAT_RG32_FLOAT || format == ECS_GRAPHICS_FORMAT_RGB32_FLOAT
			|| format == ECS_GRAPHICS_FORMAT_RGBA32_FLOAT || format == ECS_GRAPHICS_FORMAT_R11G11B10_FLOAT;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatDepth(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_D16_UNORM || format == ECS_GRAPHICS_FORMAT_D24_UNORM_S8_UINT || format == ECS_GRAPHICS_FORMAT_D32_FLOAT;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatTypeless(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_R8_TYPELESS || format == ECS_GRAPHICS_FORMAT_RG8_TYPELESS || format == ECS_GRAPHICS_FORMAT_RGBA8_TYPELESS
			|| format == ECS_GRAPHICS_FORMAT_R16_TYPELESS || format == ECS_GRAPHICS_FORMAT_RG16_TYPELESS || format == ECS_GRAPHICS_FORMAT_RGBA16_TYPELESS
			|| format == ECS_GRAPHICS_FORMAT_R32_TYPELESS || format == ECS_GRAPHICS_FORMAT_RG32_TYPELESS || format == ECS_GRAPHICS_FORMAT_RGB32_TYPELESS
			|| format == ECS_GRAPHICS_FORMAT_RGBA32_TYPELESS;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatBC(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_BC1 || format == ECS_GRAPHICS_FORMAT_BC1_SRGB || format == ECS_GRAPHICS_FORMAT_BC3
			|| format == ECS_GRAPHICS_FORMAT_BC3_SRGB || format == ECS_GRAPHICS_FORMAT_BC4 || format == ECS_GRAPHICS_FORMAT_BC5
			|| format == ECS_GRAPHICS_FORMAT_BC6 || format == ECS_GRAPHICS_FORMAT_BC7 || format == ECS_GRAPHICS_FORMAT_BC7_SRGB;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatSRGB(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB || format == ECS_GRAPHICS_FORMAT_BC1_SRGB || format == ECS_GRAPHICS_FORMAT_BC3_SRGB
			|| format == ECS_GRAPHICS_FORMAT_BC7_SRGB;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	size_t GetMeshIndexElementByteSize(ECS_MESH_INDEX type) {
		static alignas(ECS_CACHE_LINE_SIZE) size_t values[ECS_MESH_BUFFER_COUNT] = {
			sizeof(float3),
			sizeof(float3),
			sizeof(float2),
			sizeof(Color),
			sizeof(float4),
			sizeof(uint4)
		};

		return values[type];
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	D3D11_FILTER GetGraphicsNativeFilter(ECS_SAMPLER_FILTER_TYPE filter) {
		switch (filter) {
		case ECS_SAMPLER_FILTER_POINT:
			return D3D11_FILTER_MIN_MAG_MIP_POINT;
		case ECS_SAMPLER_FILTER_LINEAR:
			return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		case ECS_SAMPLER_FILTER_ANISOTROPIC:
			return D3D11_FILTER_ANISOTROPIC;
		}

		ECS_ASSERT(false);
		return D3D11_FILTER_MIN_MAG_MIP_POINT;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_SAMPLER_FILTER_TYPE GetGraphicsFilterFromNative(D3D11_FILTER filter) {
		switch (filter) {
		case D3D11_FILTER_MIN_MAG_MIP_POINT:
			return ECS_SAMPLER_FILTER_POINT;
		case D3D11_FILTER_MIN_MAG_MIP_LINEAR:
			return ECS_SAMPLER_FILTER_LINEAR;
		case D3D11_FILTER_ANISOTROPIC:
			return ECS_SAMPLER_FILTER_ANISOTROPIC;
		}

		ECS_ASSERT(false);
		return ECS_SAMPLER_FILTER_POINT;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	D3D11_TEXTURE_ADDRESS_MODE GetGraphicsNativeAddressMode(ECS_SAMPLER_ADDRESS_TYPE address_type) {
		switch (address_type) {
		case ECS_SAMPLER_ADDRESS_WRAP:
			return D3D11_TEXTURE_ADDRESS_WRAP;
		case ECS_SAMPLER_ADDRESS_BORDER:
			return D3D11_TEXTURE_ADDRESS_BORDER;
		case ECS_SAMPLER_ADDRESS_CLAMP:
			return D3D11_TEXTURE_ADDRESS_CLAMP;
		case ECS_SAMPLER_ADDRESS_MIRROR:
			return D3D11_TEXTURE_ADDRESS_MIRROR;
		}

		ECS_ASSERT(false);
		return D3D11_TEXTURE_ADDRESS_WRAP;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_SAMPLER_ADDRESS_TYPE GetGraphicsAddressModeFromNative(D3D11_TEXTURE_ADDRESS_MODE address_mode) {
		switch (address_mode) {
		case D3D11_TEXTURE_ADDRESS_WRAP:
			return ECS_SAMPLER_ADDRESS_WRAP;
		case D3D11_TEXTURE_ADDRESS_BORDER:
			return ECS_SAMPLER_ADDRESS_BORDER;
		case D3D11_TEXTURE_ADDRESS_CLAMP:
			return ECS_SAMPLER_ADDRESS_CLAMP;
		case D3D11_TEXTURE_ADDRESS_MIRROR:
			return ECS_SAMPLER_ADDRESS_MIRROR;
		}

		ECS_ASSERT(false);
		return ECS_SAMPLER_ADDRESS_WRAP;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool Material::ContainsTexture(ResourceView texture) const
	{		
		for (unsigned char index = 0; index < v_texture_count; index++) {
			if (v_textures[index].Interface() == texture.Interface()) {
				return true;
			}
		}

		for (unsigned char index = 0; index < p_texture_count; index++) {
			if (p_textures[index].Interface() == texture.Interface()) {
				return true;
			}
		}

		return false;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool Material::ContainsSampler(SamplerState sampler_state) const
	{
		for (unsigned char index = 0; index < v_sampler_count; index++) {
			if (v_samplers[index].Interface() == sampler_state.Interface()) {
				return true;
			}
		}

		for (unsigned char index = 0; index < p_sampler_count; index++) {
			if (p_samplers[index].Interface() == sampler_state.Interface()) {
				return true;
			}
		}

		return false;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void Material::CopyVertexShader(const Material* other) {
		layout = other->layout;
		vertex_shader = other->vertex_shader;
		vertex_buffer_mapping_count = other->vertex_buffer_mapping_count;
		memcpy(vertex_buffer_mappings, other->vertex_buffer_mappings, sizeof(other->vertex_buffer_mappings[0]) * other->vertex_buffer_mapping_count);
	}

	// --------------------------------------------------------------------------------------------------------------------------------
	
	void Material::CopyPixelShader(const Material* other) {
		pixel_shader = other->pixel_shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------
	
	void Material::CopyTextures(const Material* other) {
		v_texture_count = other->v_texture_count;
		memcpy(v_textures, other->v_textures, sizeof(v_textures[0]) * v_texture_count);
		memcpy(v_texture_slot, other->v_texture_slot, sizeof(other->v_texture_slot[0]) * v_texture_count);

		p_texture_count = other->p_texture_count;
		memcpy(p_textures, other->p_textures, sizeof(p_textures[0]) * p_texture_count);
		memcpy(p_texture_slot, other->p_texture_slot, sizeof(other->p_texture_slot[0]) * p_texture_count);
	}

	// --------------------------------------------------------------------------------------------------------------------------------
	
	void Material::CopyConstantBuffers(const Material* other) {
		v_buffer_count = other->v_buffer_count;
		memcpy(v_buffers, other->v_buffers, sizeof(v_buffers[0]) * v_buffer_count);
		memcpy(v_buffer_slot, other->v_buffer_slot, sizeof(v_buffer_slot[0]) * v_buffer_count);

		p_buffer_count = other->p_buffer_count;
		memcpy(p_buffers, other->p_buffers, sizeof(p_buffers[0]) * p_buffer_count);
		memcpy(p_buffer_slot, other->p_buffer_slot, sizeof(p_buffer_slot[0]) * p_buffer_count);
	}

	// --------------------------------------------------------------------------------------------------------------------------------
	
	void Material::CopySamplers(const Material * other) {
		v_sampler_count = other->v_sampler_count;
		memcpy(v_samplers, other->v_samplers, sizeof(v_samplers[0]) * v_sampler_count);
		memcpy(v_sampler_slot, other->v_sampler_slot, sizeof(v_sampler_slot[0]) * v_sampler_count);

		p_sampler_count = other->p_sampler_count;
		memcpy(p_samplers, other->p_samplers, sizeof(p_samplers[0]) * p_sampler_count);
		memcpy(p_sampler_slot, other->p_sampler_slot, sizeof(p_sampler_slot[0]) * p_sampler_count);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void Material::CopyUnorderedViews(const Material* other)
	{
		unordered_view_count = other->unordered_view_count;
		memcpy(unordered_views, other->unordered_views, sizeof(unordered_views[0]) * unordered_view_count);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void Material::Copy(const Material* other)
	{
		CopyVertexShader(other);
		CopyPixelShader(other);
		CopyTextures(other);
		CopySamplers(other);
		CopyConstantBuffers(other);
		CopyUnorderedViews(other);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------------------------------------------------

}