#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Utilities/PointerUtilities.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Utilities/File.h"
#include "../Utilities/ForEachFiles.h"
#include "../Utilities/Crash.h"
#include "../Utilities/Path.h"

namespace ECSEngine {

	static const wchar_t* PBR_MATERIAL_COLOR_TEXTURE_STRINGS[] = {
		L"diff",
		L"diffuse",
		L"color",
		L"Color",
		L"Diff",
		L"Diffuse",
		L"albedo",
		L"Albedo",
		L"A"
	};

	static const wchar_t* PBR_MATERIAL_METALLIC_TEXTURE_STRINGS[] = {
		L"Metallic",
		L"metallic",
		L"met",
		L"Met",
		L"M"
	};

	static const wchar_t* PBR_MATERIAL_ROUGHNESS_TEXTURE_STRINGS[] = {
		L"roughness",
		L"Roughness",
		L"rough",
		L"Rough",
		L"R"
	};

	static const wchar_t* PBR_MATERIAL_OCCLUSION_TEXTURE_STRINGS[] = {
		L"AO",
		L"AmbientOcclusion",
		L"Occlusion",
		L"ao",
		L"OCC",
		L"occ"
	};

	static const wchar_t* PBR_MATERIAL_NORMAL_TEXTURE_STRINGS[] = {
		L"Normal",
		L"normal",
		L"Nor",
		L"nor",
		L"N"
	};

	static const wchar_t* PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS[] = {
		L"Emissive",
		L"emissive",
		L"emiss",
		L"Emiss",
		L"E"
	};

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Buffer>
	ID3D11Resource* GetResourceBuffer(Buffer buffer, const char* error_message) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> com_buffer;
		com_buffer.Attach(buffer.buffer);
		HRESULT result = com_buffer.As(&_resource);

		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), nullptr, error_message);

		return _resource.Detach();
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Texture>
	ID3D11Resource* GetResourceTexture(Texture* texture, const char* error_message) {
		Microsoft::WRL::ComPtr<ID3D11Resource> _resource;
		Microsoft::WRL::ComPtr<Texture> com_tex;
		com_tex.Attach(texture);
		HRESULT result = com_tex.As(&_resource);

		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), nullptr, error_message);

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating vertex buffer failed");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating index buffer failed");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), layout, "Creating input layout failed");

		return layout;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	VertexShader VertexShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		VertexShader shader;

		HRESULT result;
		result = device->CreateVertexShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), shader, "Creating Vertex shader failed.");

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	PixelShader PixelShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		PixelShader shader;

		HRESULT result;
		result = device->CreatePixelShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), shader, "Creating Pixel shader failed.");

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	GeometryShader GeometryShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		GeometryShader shader;

		HRESULT result;
		result = device->CreateGeometryShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), shader, "Creating Geometry shader failed.");

		return shader;
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------

	DomainShader DomainShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		DomainShader shader;

		HRESULT result;
		result = device->CreateDomainShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), shader, "Creating Domain shader failed.");
		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	HullShader HullShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		HullShader shader;

		HRESULT result;
		result = device->CreateHullShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), shader, "Creating Hull shader failed.");

		return shader;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ComputeShader ComputeShader::RawCreate(GraphicsDevice* device, Stream<void> byte_code)
	{
		ComputeShader shader;

		HRESULT result;
		result = device->CreateComputeShader(byte_code.buffer, byte_code.size, nullptr, &shader.shader);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), shader, "Creating Compute shader failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating Texture Shader View failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating ConstantBuffer failed!");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	SamplerState SamplerState::RawCreate(GraphicsDevice* device, const D3D11_SAMPLER_DESC* descriptor)
	{
		SamplerState state;
		HRESULT result = device->CreateSamplerState(descriptor, &state.sampler);
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), state, "Constructing SamplerState failed!");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating UAView failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating RenderTargetView failed!");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), view, "Creating DepthStencilView failed!");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating StandardBuffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating StructuredBuffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating IndirectBuffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating UABuffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating AppendStructuredBuffer failed.");

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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), buffer, "Creating ConsumeStructuredBuffer failed.");

		return buffer;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture1D::Texture1D(ID3D11Resource* _resource)
	{
		if (_resource) {
			Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
			ptr.Attach(_resource);
			Microsoft::WRL::ComPtr<ID3D11Texture1D> com_tex;
			HRESULT result = ptr.As(&com_tex);

			ECS_CRASH_CONDITION(SUCCEEDED(result), "Converting resource to Texture1D failed!");
			tex = com_tex.Detach();
		}
		else {
			tex = nullptr;
		}
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), interface_, "Creating Texture1D failed.");

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture2D::Texture2D(ID3D11Resource* _resource)
	{
		if (_resource) {
			Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
			ptr.Attach(_resource);
			Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
			HRESULT result = ptr.As(&com_tex);

			ECS_CRASH_CONDITION(SUCCEEDED(result), "Converting resource to Texture2D failed!");
			tex = com_tex.Detach();
		}
		else {
			tex = nullptr;
		}
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), interface_, "Creating Texture2D failed.");

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Texture3D::Texture3D(ID3D11Resource* _resource)
	{
		if (_resource) {
			Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
			ptr.Attach(_resource);
			Microsoft::WRL::ComPtr<ID3D11Texture3D> com_tex;
			HRESULT result = ptr.As(&com_tex);

			ECS_CRASH_CONDITION(SUCCEEDED(result), "Converting resource to Texture3D failed!");
			tex = com_tex.Detach();
		}
		else {
			tex = nullptr;
		}
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), interface_, "Creating Texture3D failed.");

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	TextureCube::TextureCube(ID3D11Resource* _resource) {
		if (_resource) {
			Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
			ptr.Attach(_resource);
			Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
			HRESULT result = ptr.As(&com_tex);

			ECS_CRASH_CONDITION(SUCCEEDED(result), "Converting resource to TextureCube failed!");
			tex = com_tex.Detach();
		}
		else {
			tex = nullptr;
		}
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
		ECS_CRASH_CONDITION_RETURN(SUCCEEDED(result), interface_, "Creating TextureCube failed.");

		return interface_;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void SetPBRMaterialTexture(PBRMaterial* material, uintptr_t& memory, Stream<wchar_t> texture, PBRMaterialTextureIndex texture_index) {
		Stream<wchar_t>* material_texture = material->TextureStart() + texture_index;

		material_texture->InitializeFromBuffer(memory, texture.size);
		material_texture->CopyOther(texture);
		material_texture->buffer[material_texture->size] = L'\0';
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
		size_t total_allocation_size = sizeof(char) * (name.size + 1);

		for (size_t index = 0; index < mappings.size; index++) {
			total_allocation_size += (mappings[index].texture.size + 1) * sizeof(wchar_t);
		}

		void* allocation = AllocateEx(allocator, total_allocation_size);

		uintptr_t ptr = (uintptr_t)allocation;
		char* mutable_ptr = (char*)ptr;
		memcpy(mutable_ptr, name.buffer, (name.size + 1) * sizeof(char));
		mutable_ptr[name.size] = '\0';
		material.name = (const char*)ptr;
		ptr += sizeof(char) * (name.size + 1);

		ptr = AlignPointer(ptr, alignof(wchar_t));

		for (size_t index = 0; index < mappings.size; index++) {
			SetPBRMaterialTexture(&material, ptr, mappings[index].texture, mappings[index].index);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	// Everything is coalesced into a single allocation
	void FreePBRMaterial(const PBRMaterial& material, AllocatorPolymorphic allocator)
	{
		if (material.name.buffer != nullptr) {
			DeallocateEx(allocator, material.name.buffer);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	PBRMaterial CreatePBRMaterialFromName(
		Stream<char> material_name,
		Stream<char> texture_base_name,
		Stream<wchar_t> search_directory, 
		AllocatorPolymorphic allocator, 
		CreatePBRMaterialFromNameOptions options
	)
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, wide_base_name, 512);
		ECS_ASSERT(texture_base_name.size < 512);
		ConvertASCIIToWide(wide_base_name, texture_base_name);

		return CreatePBRMaterialFromName(material_name, wide_base_name, search_directory, allocator, options);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	PBRMaterial CreatePBRMaterialFromName(
		Stream<char> material_name,
		Stream<wchar_t> texture_base_name,
		Stream<wchar_t> search_directory,
		AllocatorPolymorphic allocator,
		CreatePBRMaterialFromNameOptions options
	) {
		PBRMaterial material;
		memset(&material, 0, sizeof(PBRMaterial));

		material.tint = Color((unsigned char)255, 255, 255, 255);
		material.emissive_factor = { 0.0f, 0.0f, 0.0f };
		material.metallic_factor = 1.0f;
		material.roughness_factor = 1.0f;

		Stream<const wchar_t*> material_strings[ECS_PBR_MATERIAL_MAPPING_COUNT];
		material_strings[ECS_PBR_MATERIAL_COLOR] = { PBR_MATERIAL_COLOR_TEXTURE_STRINGS, ECS_COUNTOF(PBR_MATERIAL_COLOR_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_EMISSIVE] = { PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS, ECS_COUNTOF(PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_METALLIC] = { PBR_MATERIAL_METALLIC_TEXTURE_STRINGS, ECS_COUNTOF(PBR_MATERIAL_METALLIC_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_NORMAL] = { PBR_MATERIAL_NORMAL_TEXTURE_STRINGS, ECS_COUNTOF(PBR_MATERIAL_NORMAL_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_OCCLUSION] = { PBR_MATERIAL_OCCLUSION_TEXTURE_STRINGS, ECS_COUNTOF(PBR_MATERIAL_OCCLUSION_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_ROUGHNESS] = { PBR_MATERIAL_ROUGHNESS_TEXTURE_STRINGS, ECS_COUNTOF(PBR_MATERIAL_ROUGHNESS_TEXTURE_STRINGS) };
		material_strings[ECS_PBR_MATERIAL_EMISSIVE] = { PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS, ECS_COUNTOF(PBR_MATERIAL_EMISSIVE_TEXTURE_STRINGS) };

		PBRMaterialTextureIndex _default_mappings[ECS_PBR_MATERIAL_MAPPING_COUNT];
		_default_mappings[0] = ECS_PBR_MATERIAL_COLOR;
		_default_mappings[1] = ECS_PBR_MATERIAL_EMISSIVE;
		_default_mappings[2] = ECS_PBR_MATERIAL_METALLIC;
		_default_mappings[3] = ECS_PBR_MATERIAL_NORMAL;
		_default_mappings[4] = ECS_PBR_MATERIAL_OCCLUSION;
		_default_mappings[5] = ECS_PBR_MATERIAL_ROUGHNESS;

		Stream<PBRMaterialTextureIndex> default_mappings(_default_mappings, ECS_PBR_MATERIAL_MAPPING_COUNT);
		Stream<PBRMaterialTextureIndex>* texture_mask = &default_mappings;
		if (options.texture_mask != nullptr) {
			texture_mask = options.texture_mask;
		}

		ECS_STACK_CAPACITY_STREAM(wchar_t, temp_memory, 2048);
		ECS_STACK_CAPACITY_STREAM(wchar_t, __base_name, 256);
		__base_name.CopyOther(texture_base_name);
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
			Stream<wchar_t> search_directory;
			bool relativize_paths;
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

				Stream<wchar_t> search_string = { base_start, wcslen(base_start) };
				
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
					Stream<wchar_t> texture_path = { data->temp_memory->buffer + data->temp_memory->size, path.size };
					data->temp_memory->AddStreamSafe(path);
					if (data->relativize_paths) {
						texture_path = PathRelativeToAbsolute(texture_path, data->search_directory, true);
					}
					data->valid_textures->Add({ texture_path, data->texture_mask->buffer[found_index] });
					data->texture_mask->RemoveSwapBack(found_index);
				}
			}

			return data->texture_mask->size > 0;
		};

		Data data;
		data.base_name = texture_base_name;
		data.material_strings = { material_strings, ECS_COUNTOF(material_strings) };
		data.temp_memory = &temp_memory;
		data.texture_mask = texture_mask;
		data.valid_textures = &valid_textures;
		if (options.search_directory_is_mount_point) {
			data.relativize_paths = true;
			data.search_directory = search_directory;
		}
		else {
			data.relativize_paths = false;
		}
		ForEachFileInDirectoryRecursive(search_directory, &data, functor);

		size_t total_size = sizeof(wchar_t) * temp_memory.size + sizeof(char) * material_name.size;
		void* allocation = Allocate(allocator, total_size, alignof(wchar_t));

		uintptr_t buffer = (uintptr_t)allocation;
		char* mutable_char = (char*)buffer;
		memcpy(mutable_char, material_name.buffer, sizeof(char)* (material_name.size + 1));
		material.name = mutable_char;
		buffer += (material_name.size + 1) * sizeof(char);

		buffer = AlignPointer(buffer, alignof(wchar_t));
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
			suffix.Add(compile_options.macros[index].name);
			suffix.Add(compile_options.macros[index].definition);
		}

		suffix.Add(optional_addition);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void SetDepthStencilDescOP(
		D3D11_DEPTH_STENCIL_DESC* descriptor, 
		bool front_face, 
		D3D11_COMPARISON_FUNC comparison_func, 
		D3D11_STENCIL_OP fail_op, 
		D3D11_STENCIL_OP depth_fail_op, 
		D3D11_STENCIL_OP pass_op
	)
	{
		if (front_face) {
			descriptor->FrontFace.StencilFunc = comparison_func;
			descriptor->FrontFace.StencilFailOp = fail_op;
			descriptor->FrontFace.StencilDepthFailOp = depth_fail_op;
			descriptor->FrontFace.StencilPassOp = pass_op;
		}
		else {
			descriptor->BackFace.StencilFunc = comparison_func;
			descriptor->BackFace.StencilFailOp = fail_op;
			descriptor->BackFace.StencilDepthFailOp = depth_fail_op;
			descriptor->BackFace.StencilPassOp = pass_op;
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	template<size_t count, typename BasicType>
	double4 ExtractBasicType(const void*& pixel) {
		BasicType* value = (BasicType*)pixel;
		pixel = OffsetPointer(pixel, sizeof(BasicType) * count);
		if constexpr (count == 1) {
			return double4(value[0], 0.0, 0.0, 0.0);
		}
		else if constexpr (count == 2) {
			return double4(value[0], value[1], 0.0, 0.0);
		}
		else if constexpr (count == 3) {
			return double4(value[0], value[1], value[2], 0.0);
		}
		else if constexpr (count == 4) {
			return double4(value[0], value[1], value[2], value[3]);
		}
	}

	template<size_t count>
	double4 ExtractUNORM8(const void*& pixel) {
		double4 result = ExtractBasicType<count, unsigned char>(pixel);
		return result * double4::Splat(1.0 / (double)UCHAR_MAX);
	}

	template<size_t count>
	double4 ExtractSNORM8(const void*& pixel) {
		double4 result = ExtractBasicType<count, char>(pixel);
		return result * double4::Splat(2.0 / (double)UCHAR_MAX);
	}

	template<size_t count>
	double4 ExtractUNORM16(const void*& pixel) {
		double4 result = ExtractBasicType<count, unsigned short>(pixel);
		return result * double4::Splat(1.0 / (double)USHORT_MAX);
	}

	template<size_t count>
	double4 ExtractSNORM16(const void*& pixel) {
		double4 result = ExtractBasicType<count, short>(pixel);
		return result * double4::Splat(2.0 / (double)USHORT_MAX);
	}

	double4 ExtractRGBA8_UNORM_SRGB(const void*& pixel) {
		double4 result = ExtractUNORM8<4>(pixel);
		return SRGBToLinear(result);
	}

	double4 ExtractDBL_MAX(const void*& pixel) {
		// We don't need to move the pointer since we are setting the values
		// to the same DBL_MAX
		return double4::Splat(DBL_MAX);
	}

	void ExtractPixelFromGraphicsFormat(ECS_GRAPHICS_FORMAT format, size_t count, const void* pixels, double4* values)
	{
		// Get value is a functor that takes const void*& and returns a double4
		auto loop = [&](auto get_value) {
			for (size_t index = 0; index < count; index++) {
				values[index] = get_value(pixels);
			}
		};

#define BASIC_CASE_124(bits, ending, type) \
	case ECS_GRAPHICS_FORMAT_R##bits##_##ending: \
		{ \
			loop(ExtractBasicType<1, type>); \
		} \
		break; \
		case ECS_GRAPHICS_FORMAT_RG##bits##_##ending: \
		{ \
			loop(ExtractBasicType<2, type>); \
		} \
		break; \
		case ECS_GRAPHICS_FORMAT_RGBA##bits##_##ending: \
		{ \
			loop(ExtractBasicType<4, type>); \
		} \
		break;

#define NORM_CASE_124(bits, ending) \
	case ECS_GRAPHICS_FORMAT_R##bits##_##ending: \
	{ \
		loop(Extract##ending##bits<1>); \
	} \
	break; \
	case ECS_GRAPHICS_FORMAT_RG##bits##_##ending: \
	{ \
		loop(Extract##ending##bits<2>); \
	} \
	break; \
	case ECS_GRAPHICS_FORMAT_RGBA##bits##_##ending: \
	{ \
		loop(Extract##ending##bits<4>);\
	} \
	break;

#define BASIC_CASE_1234(bits, ending, type) \
	case ECS_GRAPHICS_FORMAT_R##bits##_##ending: \
	{ \
		loop(ExtractBasicType<1, type>); \
	} \
		break; \
	case ECS_GRAPHICS_FORMAT_RG##bits##_##ending: \
	{ \
		loop(ExtractBasicType<2, type>); \
	} \
		break; \
	case ECS_GRAPHICS_FORMAT_RGB##bits##_##ending: \
	{ \
		loop(ExtractBasicType<3, type>); \
	} \
		break; \
	case ECS_GRAPHICS_FORMAT_RGBA##bits##_##ending: \
	{ \
		loop(ExtractBasicType<4, type>); \
	} \
		break;
												   

		switch (format) {
			BASIC_CASE_124(8, SINT, char);
			BASIC_CASE_124(8, UINT, unsigned char);
			BASIC_CASE_124(16, SINT, short);
			BASIC_CASE_124(16, UINT, short);
			BASIC_CASE_1234(32, SINT, int);
			BASIC_CASE_1234(32, UINT, unsigned int);
			NORM_CASE_124(8, UNORM);
			NORM_CASE_124(8, SNORM);
			NORM_CASE_124(16, UNORM);
			NORM_CASE_124(16, SNORM);

		case ECS_GRAPHICS_FORMAT_R32_FLOAT:
		{
			loop(ExtractBasicType<1, float>);
		}
		break;
		case ECS_GRAPHICS_FORMAT_RG32_FLOAT:
		{
			loop(ExtractBasicType<2, float>);
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGB32_FLOAT:
		{
			loop(ExtractBasicType<3, float>);
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGBA32_FLOAT:
		{
			loop(ExtractBasicType<4, float>);
		}
		break;
		case ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB:
		{
			loop(ExtractRGBA8_UNORM_SRGB);
		}
		break;
		default:
		{
			loop(ExtractDBL_MAX);
		}
		break;
		}

#undef BASIC_CASE_124
#undef NORM_CASE_124
#undef BASIC_CASE_1234
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatUINT(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_R8_UINT || format == ECS_GRAPHICS_FORMAT_RG8_UINT
			|| format == ECS_GRAPHICS_FORMAT_RGBA8_UINT || format == ECS_GRAPHICS_FORMAT_R16_UINT || format == ECS_GRAPHICS_FORMAT_RG16_UINT
			|| format == ECS_GRAPHICS_FORMAT_RGBA16_UINT || format == ECS_GRAPHICS_FORMAT_R32_UINT || format == ECS_GRAPHICS_FORMAT_RG32_UINT
			|| format == ECS_GRAPHICS_FORMAT_RGB32_UINT || format == ECS_GRAPHICS_FORMAT_RGBA32_UINT;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatSINT(ECS_GRAPHICS_FORMAT format)
	{
		return format == ECS_GRAPHICS_FORMAT_R8_SINT || format == ECS_GRAPHICS_FORMAT_RG8_SINT
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

	ECS_GRAPHICS_FORMAT GetGraphicsFormatNoSRGB(ECS_GRAPHICS_FORMAT format)
	{
		switch (format) {
		case ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB:
			return ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		case ECS_GRAPHICS_FORMAT_BC1_SRGB:
			return ECS_GRAPHICS_FORMAT_BC1;
		case ECS_GRAPHICS_FORMAT_BC3_SRGB:
			return ECS_GRAPHICS_FORMAT_BC3;
		case ECS_GRAPHICS_FORMAT_BC7_SRGB:
			return ECS_GRAPHICS_FORMAT_BC7;
		}

		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetGraphicsFormatWithSRGB(ECS_GRAPHICS_FORMAT format)
	{
		switch (format) {
		case ECS_GRAPHICS_FORMAT_RGBA8_UNORM:
			return ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB;
		case ECS_GRAPHICS_FORMAT_BC1:
			return ECS_GRAPHICS_FORMAT_BC1_SRGB;
		case ECS_GRAPHICS_FORMAT_BC3:
			return ECS_GRAPHICS_FORMAT_BC3_SRGB;
		case ECS_GRAPHICS_FORMAT_BC7:
			return ECS_GRAPHICS_FORMAT_BC7_SRGB;
		}

		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToUNORM(ECS_GRAPHICS_FORMAT format)
	{
		// The 32 bit typeless channels don't have unorm values
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R8_UNORM;
		case ECS_GRAPHICS_FORMAT_RG8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG8_UNORM;
		case ECS_GRAPHICS_FORMAT_RGBA8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA8_UNORM;
		case ECS_GRAPHICS_FORMAT_R16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R16_UNORM;
		case ECS_GRAPHICS_FORMAT_RG16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG16_UNORM;
		case ECS_GRAPHICS_FORMAT_RGBA16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA16_UNORM;
		case ECS_GRAPHICS_FORMAT_R24G8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R24G8_UNORM;
		}
		
		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToSNORM(ECS_GRAPHICS_FORMAT format)
	{
		// The 32 bit typeless channels don't have snorm values
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R8_SNORM;
		case ECS_GRAPHICS_FORMAT_RG8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG8_SNORM;
		case ECS_GRAPHICS_FORMAT_RGBA8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA8_SNORM;
		case ECS_GRAPHICS_FORMAT_R16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R16_SNORM;
		case ECS_GRAPHICS_FORMAT_RG16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG16_SNORM;
		case ECS_GRAPHICS_FORMAT_RGBA16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA16_SNORM;
		}

		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToUINT(ECS_GRAPHICS_FORMAT format)
	{
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R8_UINT;
		case ECS_GRAPHICS_FORMAT_RG8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG8_UINT;
		case ECS_GRAPHICS_FORMAT_RGBA8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA8_UINT;
		case ECS_GRAPHICS_FORMAT_R16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R16_UINT;
		case ECS_GRAPHICS_FORMAT_RG16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG16_UINT;
		case ECS_GRAPHICS_FORMAT_RGBA16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA16_UINT;
		case ECS_GRAPHICS_FORMAT_R32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R32_UINT;
		case ECS_GRAPHICS_FORMAT_RG32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG32_UINT;
		case ECS_GRAPHICS_FORMAT_RGB32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGB32_UINT;
		case ECS_GRAPHICS_FORMAT_RGBA32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA32_UINT;
		}

		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToSINT(ECS_GRAPHICS_FORMAT format)
	{
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R8_SINT;
		case ECS_GRAPHICS_FORMAT_RG8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG8_SINT;
		case ECS_GRAPHICS_FORMAT_RGBA8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA8_SINT;
		case ECS_GRAPHICS_FORMAT_R16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R16_SINT;
		case ECS_GRAPHICS_FORMAT_RG16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG16_SINT;
		case ECS_GRAPHICS_FORMAT_RGBA16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA16_SINT;
		case ECS_GRAPHICS_FORMAT_R32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R32_SINT;
		case ECS_GRAPHICS_FORMAT_RG32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG32_SINT;
		case ECS_GRAPHICS_FORMAT_RGB32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGB32_SINT;
		case ECS_GRAPHICS_FORMAT_RGBA32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA32_SINT;
		}

		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToFloat(ECS_GRAPHICS_FORMAT format)
	{
		// 8 bit channels don't have float values

		switch (format) {
		case ECS_GRAPHICS_FORMAT_R16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R16_FLOAT;
		case ECS_GRAPHICS_FORMAT_RG16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG16_FLOAT;
		case ECS_GRAPHICS_FORMAT_RGBA16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA16_FLOAT;
		case ECS_GRAPHICS_FORMAT_R32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_R32_FLOAT;
		case ECS_GRAPHICS_FORMAT_RG32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RG32_FLOAT;
		case ECS_GRAPHICS_FORMAT_RGB32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGB32_FLOAT;
		case ECS_GRAPHICS_FORMAT_RGBA32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_RGBA32_FLOAT;
		}

		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetGraphicsFormatTypelessToDepth(ECS_GRAPHICS_FORMAT format)
	{
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R16_TYPELESS:
			return ECS_GRAPHICS_FORMAT_D16_UNORM;
		case ECS_GRAPHICS_FORMAT_R24G8_TYPELESS:
			return ECS_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
		case ECS_GRAPHICS_FORMAT_R32_TYPELESS:
			return ECS_GRAPHICS_FORMAT_D32_FLOAT;
		}

		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetGraphicsFormatToTypeless(ECS_GRAPHICS_FORMAT format)
	{
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R8_SINT:
		case ECS_GRAPHICS_FORMAT_R8_UINT:
		case ECS_GRAPHICS_FORMAT_R8_UNORM:
		case ECS_GRAPHICS_FORMAT_R8_SNORM:
			return ECS_GRAPHICS_FORMAT_R8_TYPELESS;
		case ECS_GRAPHICS_FORMAT_RG8_SINT:
		case ECS_GRAPHICS_FORMAT_RG8_UINT:
		case ECS_GRAPHICS_FORMAT_RG8_SNORM:
		case ECS_GRAPHICS_FORMAT_RG8_UNORM:
			return ECS_GRAPHICS_FORMAT_RG8_TYPELESS;
		case ECS_GRAPHICS_FORMAT_RGBA8_SINT:
		case ECS_GRAPHICS_FORMAT_RGBA8_UINT:
		case ECS_GRAPHICS_FORMAT_RGBA8_SNORM:
		case ECS_GRAPHICS_FORMAT_RGBA8_UNORM:
		case ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB:
			return ECS_GRAPHICS_FORMAT_RGBA8_TYPELESS;
		case ECS_GRAPHICS_FORMAT_R16_SINT:
		case ECS_GRAPHICS_FORMAT_R16_UINT:
		case ECS_GRAPHICS_FORMAT_R16_SNORM:
		case ECS_GRAPHICS_FORMAT_R16_UNORM:
		case ECS_GRAPHICS_FORMAT_R16_FLOAT:
			return ECS_GRAPHICS_FORMAT_R16_TYPELESS;
		case ECS_GRAPHICS_FORMAT_RG16_SINT:
		case ECS_GRAPHICS_FORMAT_RG16_UINT:
		case ECS_GRAPHICS_FORMAT_RG16_SNORM:
		case ECS_GRAPHICS_FORMAT_RG16_UNORM:
		case ECS_GRAPHICS_FORMAT_RG16_FLOAT:
			return ECS_GRAPHICS_FORMAT_RG16_TYPELESS;
		case ECS_GRAPHICS_FORMAT_RGBA16_SINT:
		case ECS_GRAPHICS_FORMAT_RGBA16_UINT:
		case ECS_GRAPHICS_FORMAT_RGBA16_SNORM:
		case ECS_GRAPHICS_FORMAT_RGBA16_UNORM:
		case ECS_GRAPHICS_FORMAT_RGBA16_FLOAT:
			return ECS_GRAPHICS_FORMAT_RGBA16_TYPELESS;
		case ECS_GRAPHICS_FORMAT_R24G8_UNORM:
			return ECS_GRAPHICS_FORMAT_R24G8_TYPELESS;
		case ECS_GRAPHICS_FORMAT_R32_SINT:
		case ECS_GRAPHICS_FORMAT_R32_UINT:
		case ECS_GRAPHICS_FORMAT_R32_FLOAT:
			return ECS_GRAPHICS_FORMAT_R32_TYPELESS;
		case ECS_GRAPHICS_FORMAT_RG32_SINT:
		case ECS_GRAPHICS_FORMAT_RG32_UINT:
		case ECS_GRAPHICS_FORMAT_RG32_FLOAT:
			return ECS_GRAPHICS_FORMAT_RG32_TYPELESS;
		case ECS_GRAPHICS_FORMAT_RGB32_SINT:
		case ECS_GRAPHICS_FORMAT_RGB32_UINT:
		case ECS_GRAPHICS_FORMAT_RGB32_FLOAT:
			return ECS_GRAPHICS_FORMAT_RGB32_TYPELESS;
		case ECS_GRAPHICS_FORMAT_RGBA32_SINT:
		case ECS_GRAPHICS_FORMAT_RGBA32_UINT:
		case ECS_GRAPHICS_FORMAT_RGBA32_FLOAT:
			return ECS_GRAPHICS_FORMAT_RGBA32_TYPELESS;
		}

		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatFloatCompatible(ECS_GRAPHICS_FORMAT format, bool include_unormalized_float)
	{
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R8_UNORM:
		case ECS_GRAPHICS_FORMAT_R8_SNORM:
		case ECS_GRAPHICS_FORMAT_R16_UNORM:
		case ECS_GRAPHICS_FORMAT_R16_SNORM:
		case ECS_GRAPHICS_FORMAT_R24G8_UNORM:
		case ECS_GRAPHICS_FORMAT_RG8_UNORM:
		case ECS_GRAPHICS_FORMAT_RG8_SNORM:
		case ECS_GRAPHICS_FORMAT_RG16_UNORM:
		case ECS_GRAPHICS_FORMAT_RG16_SNORM:
		case ECS_GRAPHICS_FORMAT_RGBA8_UNORM:
		case ECS_GRAPHICS_FORMAT_RGBA8_SNORM:
		case ECS_GRAPHICS_FORMAT_RGBA16_UNORM:
		case ECS_GRAPHICS_FORMAT_RGBA16_SNORM:
			return true;
		}

		if (IsGraphicsFormatBC(format)) {
			return true;
		}

		if (include_unormalized_float) {
			return IsGraphicsFormatFloat(format);
		}

		return false;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatSingleChannel(ECS_GRAPHICS_FORMAT format) {
		switch (format) {
		case ECS_GRAPHICS_FORMAT_R8_SINT:
		case ECS_GRAPHICS_FORMAT_R8_SNORM:
		case ECS_GRAPHICS_FORMAT_R8_UNORM:
		case ECS_GRAPHICS_FORMAT_R8_UINT:
		case ECS_GRAPHICS_FORMAT_R8_TYPELESS:
		case ECS_GRAPHICS_FORMAT_R16_SINT:
		case ECS_GRAPHICS_FORMAT_R16_SNORM:
		case ECS_GRAPHICS_FORMAT_R16_UINT:
		case ECS_GRAPHICS_FORMAT_R16_UNORM:
		case ECS_GRAPHICS_FORMAT_R16_TYPELESS:
		case ECS_GRAPHICS_FORMAT_R16_FLOAT:
		case ECS_GRAPHICS_FORMAT_R32_FLOAT:
		case ECS_GRAPHICS_FORMAT_R32_SINT:
		case ECS_GRAPHICS_FORMAT_R32_UINT:
		case ECS_GRAPHICS_FORMAT_R32_TYPELESS:
			return true;
		}

		return false;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatDoubleChannel(ECS_GRAPHICS_FORMAT format) {
		switch (format) {
		case ECS_GRAPHICS_FORMAT_RG8_SINT:
		case ECS_GRAPHICS_FORMAT_RG8_SNORM:
		case ECS_GRAPHICS_FORMAT_RG8_UNORM:
		case ECS_GRAPHICS_FORMAT_RG8_UINT:
		case ECS_GRAPHICS_FORMAT_RG8_TYPELESS:
		case ECS_GRAPHICS_FORMAT_RG16_SINT:
		case ECS_GRAPHICS_FORMAT_RG16_SNORM:
		case ECS_GRAPHICS_FORMAT_RG16_UINT:
		case ECS_GRAPHICS_FORMAT_RG16_UNORM:
		case ECS_GRAPHICS_FORMAT_RG16_TYPELESS:
		case ECS_GRAPHICS_FORMAT_RG16_FLOAT:
		case ECS_GRAPHICS_FORMAT_RG32_FLOAT:
		case ECS_GRAPHICS_FORMAT_RG32_SINT:
		case ECS_GRAPHICS_FORMAT_RG32_UINT:
		case ECS_GRAPHICS_FORMAT_RG32_TYPELESS:
		case ECS_GRAPHICS_FORMAT_R24G8_UNORM:
			return true;
		}

		return false;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatTripleChannel(ECS_GRAPHICS_FORMAT format) {
		switch (format) {
		case ECS_GRAPHICS_FORMAT_RGB32_FLOAT:
		case ECS_GRAPHICS_FORMAT_RGB32_SINT:
		case ECS_GRAPHICS_FORMAT_RGB32_UINT:
		case ECS_GRAPHICS_FORMAT_RGB32_TYPELESS:
		case ECS_GRAPHICS_FORMAT_R11G11B10_FLOAT:
			return true;
		}

		return false;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool IsGraphicsFormatQuadrupleChannel(ECS_GRAPHICS_FORMAT format) {
		switch (format) {
		case ECS_GRAPHICS_FORMAT_RGBA8_SINT:
		case ECS_GRAPHICS_FORMAT_RGBA8_SNORM:
		case ECS_GRAPHICS_FORMAT_RGBA8_UNORM:
		case ECS_GRAPHICS_FORMAT_RGBA8_UINT:
		case ECS_GRAPHICS_FORMAT_RGBA8_TYPELESS:
		case ECS_GRAPHICS_FORMAT_RGBA16_SINT:
		case ECS_GRAPHICS_FORMAT_RGBA16_SNORM:
		case ECS_GRAPHICS_FORMAT_RGBA16_UINT:
		case ECS_GRAPHICS_FORMAT_RGBA16_UNORM:
		case ECS_GRAPHICS_FORMAT_RGBA16_TYPELESS:
		case ECS_GRAPHICS_FORMAT_RGBA16_FLOAT:
		case ECS_GRAPHICS_FORMAT_RGBA32_FLOAT:
		case ECS_GRAPHICS_FORMAT_RGBA32_SINT:
		case ECS_GRAPHICS_FORMAT_RGBA32_UINT:
		case ECS_GRAPHICS_FORMAT_RGBA32_TYPELESS:
		case ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB:
			return true;
		}

		return false;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	unsigned int GetGraphicsFormatChannelCount(ECS_GRAPHICS_FORMAT format)
	{
		if (IsGraphicsFormatSingleChannel(format)) {
			return 1;
		}
		else if (IsGraphicsFormatDoubleChannel(format)) {
			return 2;
		}
		else if (IsGraphicsFormatTripleChannel(format)) {
			return 3;
		}
		else if (IsGraphicsFormatQuadrupleChannel(format)) {
			return 4;
		}

		if (IsGraphicsFormatDepth(format)) {
			return 1;
		}
		
		switch (format) {
		case ECS_GRAPHICS_FORMAT_BC1:
		case ECS_GRAPHICS_FORMAT_BC1_SRGB:
		case ECS_GRAPHICS_FORMAT_BC7:
		case ECS_GRAPHICS_FORMAT_BC7_SRGB:
			// Assume BC7 is triple channel
			return 3;
		case ECS_GRAPHICS_FORMAT_BC3:
		case ECS_GRAPHICS_FORMAT_BC3_SRGB:
			return 4;
		case ECS_GRAPHICS_FORMAT_BC4:
			return 1;
		case ECS_GRAPHICS_FORMAT_BC5:
			return 2;
		case ECS_GRAPHICS_FORMAT_BC6:
			return 3;
		}

		// Invalid format
		ECS_ASSERT(false);
		return -1;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT GetGraphicsFormatChannelCount(ECS_GRAPHICS_FORMAT format, unsigned int new_count)
	{
		ECS_ASSERT(new_count <= 4, "Trying to convert a graphics format to a format with more than 4 channels");

		struct StaticFormatArray {
			StaticFormatArray() {
				auto splat_format = [&](ECS_GRAPHICS_FORMAT format) {
					for (size_t index = 0; index < 4; index++) {
						formats[format][index] = format;
					}
				};

				auto set_format = [&](
					ECS_GRAPHICS_FORMAT format,
					ECS_GRAPHICS_FORMAT first,
					ECS_GRAPHICS_FORMAT second,
					ECS_GRAPHICS_FORMAT third,
					ECS_GRAPHICS_FORMAT fourth
				) {
					formats[format][0] = first;
					formats[format][1] = second;
					formats[format][2] = third;
					formats[format][3] = fourth;
				};

				splat_format(ECS_GRAPHICS_FORMAT_UNKNOWN);
				splat_format(ECS_GRAPHICS_FORMAT_BC1);
				splat_format(ECS_GRAPHICS_FORMAT_BC3);
				splat_format(ECS_GRAPHICS_FORMAT_BC4);
				splat_format(ECS_GRAPHICS_FORMAT_BC5);
				splat_format(ECS_GRAPHICS_FORMAT_BC6);
				splat_format(ECS_GRAPHICS_FORMAT_BC7);
				splat_format(ECS_GRAPHICS_FORMAT_BC1_SRGB);
				splat_format(ECS_GRAPHICS_FORMAT_BC3_SRGB);
				splat_format(ECS_GRAPHICS_FORMAT_BC7_SRGB);
				splat_format(ECS_GRAPHICS_FORMAT_D16_UNORM);
				splat_format(ECS_GRAPHICS_FORMAT_D24_UNORM_S8_UINT);
				splat_format(ECS_GRAPHICS_FORMAT_D32_FLOAT);
				splat_format(ECS_GRAPHICS_FORMAT_R11G11B10_FLOAT);
				splat_format(ECS_GRAPHICS_FORMAT_RGBA8_UNORM_SRGB);

#define SET_FORMAT_8(suffix) set_format(ECS_GRAPHICS_FORMAT_R8_##suffix, ECS_GRAPHICS_FORMAT_R8_##suffix, ECS_GRAPHICS_FORMAT_RG8_##suffix, ECS_GRAPHICS_FORMAT_UNKNOWN, ECS_GRAPHICS_FORMAT_RGBA8_##suffix); \
	set_format(ECS_GRAPHICS_FORMAT_RG8_##suffix, ECS_GRAPHICS_FORMAT_R8_##suffix, ECS_GRAPHICS_FORMAT_RG8_##suffix, ECS_GRAPHICS_FORMAT_UNKNOWN, ECS_GRAPHICS_FORMAT_RGBA8_##suffix); \
	set_format(ECS_GRAPHICS_FORMAT_RGBA8_##suffix, ECS_GRAPHICS_FORMAT_R8_##suffix, ECS_GRAPHICS_FORMAT_RG8_##suffix, ECS_GRAPHICS_FORMAT_UNKNOWN, ECS_GRAPHICS_FORMAT_RGBA8_##suffix);

#define SET_FORMAT_16(suffix) set_format(ECS_GRAPHICS_FORMAT_R16_##suffix, ECS_GRAPHICS_FORMAT_R16_##suffix, ECS_GRAPHICS_FORMAT_RG16_##suffix, ECS_GRAPHICS_FORMAT_UNKNOWN, ECS_GRAPHICS_FORMAT_RGBA16_##suffix); \
	set_format(ECS_GRAPHICS_FORMAT_RG16_##suffix, ECS_GRAPHICS_FORMAT_R16_##suffix, ECS_GRAPHICS_FORMAT_RG16_##suffix, ECS_GRAPHICS_FORMAT_UNKNOWN, ECS_GRAPHICS_FORMAT_RGBA16_##suffix); \
	set_format(ECS_GRAPHICS_FORMAT_RGBA16_##suffix, ECS_GRAPHICS_FORMAT_R16_##suffix, ECS_GRAPHICS_FORMAT_RG16_##suffix, ECS_GRAPHICS_FORMAT_UNKNOWN, ECS_GRAPHICS_FORMAT_RGBA16_##suffix);

#define SET_FORMAT_32(suffix) set_format(ECS_GRAPHICS_FORMAT_R32_##suffix, ECS_GRAPHICS_FORMAT_R32_##suffix, ECS_GRAPHICS_FORMAT_RG32_##suffix, ECS_GRAPHICS_FORMAT_RGB32_##suffix, ECS_GRAPHICS_FORMAT_RGBA32_##suffix); \
	set_format(ECS_GRAPHICS_FORMAT_RG32_##suffix, ECS_GRAPHICS_FORMAT_R32_##suffix, ECS_GRAPHICS_FORMAT_RG32_##suffix, ECS_GRAPHICS_FORMAT_RGB32_##suffix, ECS_GRAPHICS_FORMAT_RGBA32_##suffix); \
	set_format(ECS_GRAPHICS_FORMAT_RGB32_##suffix, ECS_GRAPHICS_FORMAT_R32_##suffix, ECS_GRAPHICS_FORMAT_RG32_##suffix, ECS_GRAPHICS_FORMAT_RGB32_##suffix, ECS_GRAPHICS_FORMAT_RGBA32_##suffix); \
	set_format(ECS_GRAPHICS_FORMAT_RGBA32_##suffix, ECS_GRAPHICS_FORMAT_R32_##suffix, ECS_GRAPHICS_FORMAT_RG32_##suffix, ECS_GRAPHICS_FORMAT_RGB32_##suffix, ECS_GRAPHICS_FORMAT_RGBA32_##suffix);

				SET_FORMAT_8(SINT);
				SET_FORMAT_8(UINT);
				SET_FORMAT_8(SNORM);
				SET_FORMAT_8(UNORM);
				SET_FORMAT_8(TYPELESS);

				SET_FORMAT_16(SINT);
				SET_FORMAT_16(UINT);
				SET_FORMAT_16(SNORM);
				SET_FORMAT_16(UNORM);
				SET_FORMAT_16(TYPELESS);
				SET_FORMAT_16(FLOAT);

				SET_FORMAT_32(FLOAT);
				SET_FORMAT_32(UINT);
				SET_FORMAT_32(SINT);
				SET_FORMAT_32(TYPELESS);

#undef SET_FORMAT_8
#undef SET_FORMAT_16
#undef SET_FORMAT_32
			}

			// We currently don't have a COUNT for graphics format
			ECS_GRAPHICS_FORMAT formats[UCHAR_MAX][4];
		};

		static StaticFormatArray format_array = StaticFormatArray();
		return format_array.formats[format][new_count - 1];
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT ConvertDepthToRenderFormat(ECS_GRAPHICS_FORMAT format)
	{
		switch (format) {
		case ECS_GRAPHICS_FORMAT_D16_UNORM:
			return ECS_GRAPHICS_FORMAT_R16_UNORM;
		case ECS_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
			return ECS_GRAPHICS_FORMAT_R24G8_UNORM;
		case ECS_GRAPHICS_FORMAT_D32_FLOAT:
			return ECS_GRAPHICS_FORMAT_R32_FLOAT;
		}

		return format;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_GRAPHICS_FORMAT ConvertDepthToTypelessFormat(ECS_GRAPHICS_FORMAT format)
	{
		switch (format) {
		case ECS_GRAPHICS_FORMAT_D16_UNORM:
			return ECS_GRAPHICS_FORMAT_R16_TYPELESS;
		case ECS_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
			return ECS_GRAPHICS_FORMAT_R24G8_TYPELESS;
		case ECS_GRAPHICS_FORMAT_D32_FLOAT:
			return ECS_GRAPHICS_FORMAT_R32_TYPELESS;
		}

		return format;
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

	D3D11_BLEND GetGraphicsNativeBlendFactor(ECS_BLEND_FACTOR blend_factor)
	{
		switch (blend_factor)
		{
		case ECS_BLEND_ZERO:
			return D3D11_BLEND_ZERO;
		case ECS_BLEND_ONE:
			return D3D11_BLEND_ONE;
		case ECS_BLEND_SRC_COLOR:
			return D3D11_BLEND_SRC_COLOR;
		case ECS_BLEND_INV_SRC_COLOR:
			return D3D11_BLEND_INV_SRC_COLOR;
		case ECS_BLEND_SRC_ALPHA:
			return D3D11_BLEND_SRC_ALPHA;
		case ECS_BLEND_INV_SRC_ALPHA:
			return D3D11_BLEND_INV_SRC_ALPHA;
		case ECS_BLEND_DEST_COLOR:
			return D3D11_BLEND_DEST_COLOR;
		case ECS_BLEND_INV_DEST_COLOR:
			return D3D11_BLEND_INV_DEST_COLOR;
		case ECS_BLEND_DEST_ALPHA:
			return D3D11_BLEND_DEST_ALPHA;
		case ECS_BLEND_INV_DEST_ALPHA:
			return D3D11_BLEND_INV_DEST_ALPHA;
		}

		ECS_ASSERT(false);
		return D3D11_BLEND_ZERO;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_BLEND_FACTOR GetGraphicsBlendFactorFromNative(D3D11_BLEND blend_factor)
	{
		switch (blend_factor)
		{
		case D3D11_BLEND_ZERO:
			return ECS_BLEND_ZERO;
		case D3D11_BLEND_ONE:
			return ECS_BLEND_ONE;
		case D3D11_BLEND_SRC_COLOR:
			return ECS_BLEND_SRC_COLOR;
		case D3D11_BLEND_INV_SRC_COLOR:
			return ECS_BLEND_INV_SRC_COLOR;
		case D3D11_BLEND_SRC_ALPHA:
			return ECS_BLEND_SRC_ALPHA;
		case D3D11_BLEND_INV_SRC_ALPHA:
			return ECS_BLEND_INV_SRC_ALPHA;
		case D3D11_BLEND_DEST_COLOR:
			return ECS_BLEND_DEST_COLOR;
		case D3D11_BLEND_INV_DEST_COLOR:
			return ECS_BLEND_INV_DEST_COLOR;
		case D3D11_BLEND_DEST_ALPHA:
			return ECS_BLEND_DEST_ALPHA;
		case D3D11_BLEND_INV_DEST_ALPHA:
			return ECS_BLEND_INV_DEST_ALPHA;
		}

		ECS_ASSERT(false);
		return ECS_BLEND_ZERO;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	D3D11_BLEND_OP GetGraphicsNativeBlendOp(ECS_BLEND_OP blend_op) {
		switch (blend_op) {
		case ECS_BLEND_OP_ADD:
			return D3D11_BLEND_OP_ADD;
		case ECS_BLEND_OP_SUBTRACT:
			return D3D11_BLEND_OP_SUBTRACT;
		case ECS_BLEND_OP_INVERTED_SUBTRACT:
			return D3D11_BLEND_OP_REV_SUBTRACT;
		case ECS_BLEND_OP_MIN:
			return D3D11_BLEND_OP_MIN;
		case ECS_BLEND_OP_MAX:
			return D3D11_BLEND_OP_MAX;
		}

		ECS_ASSERT(false);
		return D3D11_BLEND_OP_ADD;
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------

	ECS_BLEND_OP GetGraphicsBlendOpFromNative(D3D11_BLEND_OP blend_op) {
		switch (blend_op) {
		case D3D11_BLEND_OP_ADD:
			return ECS_BLEND_OP_ADD;
		case D3D11_BLEND_OP_SUBTRACT:
			return ECS_BLEND_OP_SUBTRACT;
		case D3D11_BLEND_OP_REV_SUBTRACT:
			return ECS_BLEND_OP_INVERTED_SUBTRACT;
		case D3D11_BLEND_OP_MIN:
			return ECS_BLEND_OP_MIN;
		case D3D11_BLEND_OP_MAX:
			return ECS_BLEND_OP_MAX;
		}

		ECS_ASSERT(false);
		return ECS_BLEND_OP_ADD;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	D3D11_SAMPLER_DESC GetGraphicsNativeSamplerDescriptor(const SamplerDescriptor& descriptor) {
		D3D11_SAMPLER_DESC native_descriptor;
		native_descriptor.MaxAnisotropy = descriptor.max_anisotropic_level;
		native_descriptor.Filter = GetGraphicsNativeFilter(descriptor.filter_type);
		native_descriptor.AddressU = GetGraphicsNativeAddressMode(descriptor.address_type_u);
		native_descriptor.AddressV = GetGraphicsNativeAddressMode(descriptor.address_type_v);
		native_descriptor.AddressW = GetGraphicsNativeAddressMode(descriptor.address_type_w);

		native_descriptor.MaxLOD = descriptor.max_lod;
		native_descriptor.MinLOD = descriptor.min_lod;
		native_descriptor.MipLODBias = descriptor.mip_bias;

		memcpy(native_descriptor.BorderColor, &descriptor.border_color, sizeof(descriptor.border_color));
		return native_descriptor;
	}

	SamplerDescriptor GetGraphicsSamplerDescriptorFromNative(const D3D11_SAMPLER_DESC& descriptor) {
		SamplerDescriptor ecs_descriptor;

		ecs_descriptor.max_anisotropic_level = descriptor.MaxAnisotropy;
		ecs_descriptor.filter_type = GetGraphicsFilterFromNative(descriptor.Filter);
		ecs_descriptor.address_type_u = GetGraphicsAddressModeFromNative(descriptor.AddressU);
		ecs_descriptor.address_type_v = GetGraphicsAddressModeFromNative(descriptor.AddressV);
		ecs_descriptor.address_type_w = GetGraphicsAddressModeFromNative(descriptor.AddressW);
		ecs_descriptor.max_lod = (unsigned int)descriptor.MaxLOD;
		ecs_descriptor.min_lod = (unsigned int)descriptor.MinLOD;
		ecs_descriptor.mip_bias = descriptor.MipLODBias;
		memcpy(&ecs_descriptor.border_color, &descriptor.BorderColor, sizeof(descriptor.BorderColor));

		return ecs_descriptor;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	D3D11_BLEND_DESC GetGraphicsNativeBlendDescriptor(const BlendDescriptor& descriptor) {
		D3D11_BLEND_DESC native_descriptor;

		native_descriptor.AlphaToCoverageEnable = FALSE;
		native_descriptor.IndependentBlendEnable = FALSE;
		
		D3D11_RENDER_TARGET_BLEND_DESC& render_desc = native_descriptor.RenderTarget[0];
		render_desc.BlendEnable = descriptor.enabled;
		render_desc.BlendOp = GetGraphicsNativeBlendOp(descriptor.color_op);
		render_desc.BlendOpAlpha = GetGraphicsNativeBlendOp(descriptor.alpha_op);
		render_desc.DestBlend = GetGraphicsNativeBlendFactor(descriptor.color_destination_factor);
		render_desc.DestBlendAlpha = GetGraphicsNativeBlendFactor(descriptor.alpha_destination_factor);
		render_desc.RenderTargetWriteMask = GetGraphicsNativeBlendColorChannel(descriptor.write_mask);
		render_desc.SrcBlend = GetGraphicsNativeBlendFactor(descriptor.color_source_factor);
		render_desc.SrcBlendAlpha = GetGraphicsNativeBlendFactor(descriptor.alpha_source_factor);

		return native_descriptor;
	}

	BlendDescriptor GetGraphicsBlendDescriptorFromNative(const D3D11_BLEND_DESC& descriptor) {
		BlendDescriptor ecs_descriptor;

		const D3D11_RENDER_TARGET_BLEND_DESC& render_desc = descriptor.RenderTarget[0];
		ecs_descriptor.enabled = render_desc.BlendEnable;
		ecs_descriptor.alpha_destination_factor = GetGraphicsBlendFactorFromNative(render_desc.DestBlendAlpha);
		ecs_descriptor.alpha_op = GetGraphicsBlendOpFromNative(render_desc.BlendOpAlpha);
		ecs_descriptor.alpha_source_factor = GetGraphicsBlendFactorFromNative(render_desc.SrcBlendAlpha);
		ecs_descriptor.color_destination_factor = GetGraphicsBlendFactorFromNative(render_desc.DestBlend);
		ecs_descriptor.color_op = GetGraphicsBlendOpFromNative(render_desc.BlendOp);
		ecs_descriptor.color_source_factor = GetGraphicsBlendFactorFromNative(render_desc.SrcBlend);
		ecs_descriptor.write_mask = GetGraphicsBlendColorChannelFromNative(render_desc.RenderTargetWriteMask);

		return ecs_descriptor;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	D3D11_DEPTH_STENCIL_DESC GetGraphicsNativeDepthStencilDescriptor(const DepthStencilDescriptor& descriptor) {
		auto convert_to_native_face = [](const DepthStencilDescriptor::FaceStencilOp& face) {
			D3D11_DEPTH_STENCILOP_DESC native_descriptor;
			native_descriptor.StencilFailOp = GetGraphicsNativeStencilOp(face.stencil_fail);
			native_descriptor.StencilDepthFailOp = GetGraphicsNativeStencilOp(face.depth_fail);
			native_descriptor.StencilPassOp = GetGraphicsNativeStencilOp(face.pass);
			native_descriptor.StencilFunc = GetGraphicsNativeComparisonOp(face.stencil_comparison);
			return native_descriptor;
		};

		D3D11_DEPTH_STENCIL_DESC native_descriptor;
		native_descriptor.DepthEnable = descriptor.depth_enabled;
		native_descriptor.DepthWriteMask = descriptor.write_depth ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		native_descriptor.DepthFunc = GetGraphicsNativeComparisonOp(descriptor.depth_op);
		native_descriptor.StencilEnable = descriptor.stencil_enabled;
		native_descriptor.StencilReadMask = descriptor.stencil_read_mask;
		native_descriptor.StencilWriteMask = descriptor.stencil_write_mask;
		native_descriptor.FrontFace = convert_to_native_face(descriptor.stencil_front_face);
		native_descriptor.BackFace = convert_to_native_face(descriptor.stencil_back_face);
		return native_descriptor;
	}

	DepthStencilDescriptor GetGraphicsDepthStencilDescriptorFromNative(const D3D11_DEPTH_STENCIL_DESC& descriptor) {
		auto convert_face_from_native = [](const D3D11_DEPTH_STENCILOP_DESC& face) {
			DepthStencilDescriptor::FaceStencilOp native_descriptor;
			native_descriptor.stencil_fail = GetGraphicsStencilOpFromNative(face.StencilFailOp);
			native_descriptor.depth_fail = GetGraphicsStencilOpFromNative(face.StencilDepthFailOp);
			native_descriptor.pass = GetGraphicsStencilOpFromNative(face.StencilPassOp);
			native_descriptor.stencil_comparison = GetGraphicsComparisonOpFromNative(face.StencilFunc);
			return native_descriptor;
		};

		DepthStencilDescriptor ecs_descriptor;
		ecs_descriptor.depth_enabled = descriptor.DepthEnable;
		ecs_descriptor.write_depth = descriptor.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
		ecs_descriptor.depth_op = GetGraphicsComparisonOpFromNative(descriptor.DepthFunc);
		ecs_descriptor.stencil_enabled = descriptor.StencilEnable;
		ecs_descriptor.stencil_read_mask = descriptor.StencilReadMask;
		ecs_descriptor.stencil_write_mask = descriptor.StencilWriteMask;
		ecs_descriptor.stencil_front_face = convert_face_from_native(descriptor.FrontFace);
		ecs_descriptor.stencil_back_face = convert_face_from_native(descriptor.BackFace);
		return ecs_descriptor;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	D3D11_RASTERIZER_DESC GetGraphicsNativeRasterizerDescriptor(const RasterizerDescriptor& descriptor) {
		D3D11_RASTERIZER_DESC native_descriptor;
		native_descriptor.FillMode = descriptor.solid_fill ? D3D11_FILL_SOLID : D3D11_FILL_WIREFRAME;
		native_descriptor.CullMode = GetGraphicsNativeCullMode(descriptor.cull_mode);
		native_descriptor.FrontCounterClockwise = descriptor.front_face_is_counter_clockwise;
		native_descriptor.DepthBias = 0;
		native_descriptor.DepthBiasClamp = 0.0f;
		native_descriptor.SlopeScaledDepthBias = 0.0f;
		native_descriptor.ScissorEnable = descriptor.enable_scissor;
		native_descriptor.DepthClipEnable = TRUE;
		native_descriptor.MultisampleEnable = FALSE;
		native_descriptor.AntialiasedLineEnable = FALSE;
		return native_descriptor;
	}

	RasterizerDescriptor GetGraphicsRasterizerDescriptorFromNative(const D3D11_RASTERIZER_DESC& descriptor) {
		RasterizerDescriptor ecs_descriptor;
		ecs_descriptor.cull_mode = GetGraphicsCullModeFromNative(descriptor.CullMode);
		ecs_descriptor.enable_scissor = descriptor.ScissorEnable;
		ecs_descriptor.front_face_is_counter_clockwise = descriptor.FrontCounterClockwise;
		ecs_descriptor.solid_fill = descriptor.FillMode == D3D11_FILL_SOLID;
		return ecs_descriptor;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	unsigned char Material::AddTag(
		Stream<char> tag, 
		unsigned short byte_offset,
		unsigned char buffer_index,
		unsigned char* tag_count, 
		uchar2* tags, 
		unsigned short* byte_offsets,
		unsigned char* buffer_indices,
		unsigned char max_count
	)
	{
		unsigned char current_index = *tag_count;
		ECS_ASSERT(current_index < max_count);
		ECS_ASSERT((size_t)tag_storage_size + tag.size < ECS_MATERIAL_TAG_STORAGE_CAPACITY);
		tags[current_index].x = tag_storage_size;
		tags[current_index].y = (unsigned char)tag.size;
		byte_offsets[current_index] = byte_offset;
		buffer_indices[current_index] = buffer_index;
		(*tag_count)++;
		tag.CopyTo(tag_storage + tag_storage_size);
		tag_storage_size += tag.size;

		return current_index;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	unsigned char Material::AddConstantBuffer(
		ConstantBuffer buffer, 
		unsigned char slot, 
		unsigned char* current_count, 
		ConstantBuffer* buffers, 
		unsigned char* slots,
		unsigned char max_count
	)
	{
		unsigned char current_index = *current_count;
		ECS_ASSERT(current_index < max_count);

		buffers[current_index] = buffer;
		slots[current_index] = slot;
		(*current_count)++;
		return current_index;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	unsigned char Material::AddSampler(
		SamplerState sampler, 
		unsigned char slot, 
		unsigned char* current_count, 
		SamplerState* samplers, 
		unsigned char* slots,
		unsigned char max_count
	)
	{
		unsigned char current_index = *current_count;
		ECS_ASSERT(current_index < max_count);

		samplers[current_index] = sampler;
		slots[current_index] = slot;
		(*current_count)++;
		return current_index;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	unsigned char Material::AddResourceView(
		ResourceView resource_view, 
		unsigned char slot, 
		unsigned char* current_count, 
		ResourceView* resource_views, 
		unsigned char* slots,
		unsigned char max_count
	)
	{
		unsigned char current_index = *current_count;
		ECS_ASSERT(current_index < max_count);

		resource_views[current_index] = resource_view;
		slots[current_index] = slot;
		(*current_count)++;
		return current_index;
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

	uint2 Material::FindTag(Stream<char> tag, bool is_vertex_shader) const
	{
		auto find = [&](unsigned char count, const uchar2* tags) {
			for (unsigned char index = 0; index < count; index++) {
				Stream<char> current_tag = GetTag(index, tags);
				if (current_tag == tag) {
					return index;
				}
			}
			return (unsigned char)UCHAR_MAX;
		};

		if (is_vertex_shader) {
			unsigned char found_index = find(v_tag_count, v_tags);
			if (found_index != UCHAR_MAX) {
				return { v_tag_buffer_index[found_index], v_tag_byte_offset[found_index] };
			}
		}
		else {
			unsigned char found_index = find(p_tag_count, p_tags);
			if (found_index != UCHAR_MAX) {
				return { p_tag_buffer_index[found_index], p_tag_byte_offset[found_index] };
			}
		}

		return uint2(-1, -1);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void Material::GetGPUResources(AdditionStream<void*> resources) const {
		resources.Add(layout.Interface());
		resources.Add(vertex_shader.Interface());
		resources.Add(pixel_shader.Interface());
		for (size_t index = 0; index < (size_t)v_buffer_count; index++) {
			resources.Add(v_buffers[index].Interface());
		}
		for (size_t index = 0; index < (size_t)p_buffer_count; index++) {
			resources.Add(p_buffers[index].Interface());
		}
		for (size_t index = 0; index < (size_t)v_texture_count; index++) {
			resources.Add(v_textures[index].Interface());
		}
		for (size_t index = 0; index < (size_t)p_texture_count; index++) {
			resources.Add(p_textures[index].Interface());
		}
		for (size_t index = 0; index < (size_t)unordered_view_count; index++) {
			resources.Add(unordered_views[index].Interface());
		}
		for (size_t index = 0; index < (size_t)v_sampler_count; index++) {
			resources.Add(v_samplers[index].Interface());
		}
		for (size_t index = 0; index < (size_t)p_sampler_count; index++) {
			resources.Add(p_samplers[index].Interface());
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	bool PBRMaterial::HasTextures() const
	{
		const Stream<wchar_t>* texture_start = &color_texture;
		for (size_t index = 0; index < ECS_PBR_MATERIAL_MAPPING_COUNT; index++) {
			if (texture_start[index].size > 0) {
				return true;
			}
		}
		return false;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

}