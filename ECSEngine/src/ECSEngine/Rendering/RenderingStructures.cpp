#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Utilities/Function.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Utilities/File.h"

ECS_CONTAINERS;

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

	VertexBuffer::VertexBuffer() : stride(0), buffer(nullptr) {}

	VertexBuffer::VertexBuffer(UINT _stride, UINT _size, ID3D11Buffer* _buffer) 
		: stride(_stride), size(_size), buffer(_buffer) {}

	IndexBuffer::IndexBuffer() : count(0), buffer(nullptr) {}

	InputLayout::InputLayout() : layout(nullptr) {}

	VertexShader::VertexShader() : byte_code(nullptr), shader(nullptr) {}

	PixelShader::PixelShader() : shader(nullptr) {}

	Topology::Topology() : value(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {}

	Topology::Topology(D3D11_PRIMITIVE_TOPOLOGY topology) : value(topology) {}

	ResourceView::ResourceView() : view(nullptr) {}

	ResourceView::ResourceView(ID3D11ShaderResourceView* _view) : view(_view) {}

	ConstantBuffer::ConstantBuffer() : buffer(nullptr) {}

	ConstantBuffer::ConstantBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

	SamplerState::SamplerState() : sampler(nullptr) {}

	SamplerState::SamplerState(ID3D11SamplerState* _sampler) : sampler(_sampler) {}

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


	TextureCube::TextureCube() : tex(nullptr) {}

	TextureCube::TextureCube(ID3D11Resource* _resource) {
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting resource to Texture3D failed!", true);
		tex = com_tex.Detach();
	}

	TextureCube::TextureCube(ID3D11Texture2D* _tex) : tex(_tex) {}

	Translation::Translation() : x(0.0f), y(0.0f), z(0.0f) {}

	Translation::Translation(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

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

	Matrix Camera::GetProjectionViewMatrix() const {
		return MatrixTranslation(-translation) * MatrixRotationZ(-rotation.z) * MatrixRotationY(-rotation.y) * MatrixRotationX(-rotation.x) * projection;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void SetPBRMaterialTexture(PBRMaterial* material, uintptr_t& memory, Stream<wchar_t> texture, PBRMaterialTextureIndex texture_index) {
		void* base_address = (void*)function::align_pointer(
			(uintptr_t)function::OffsetPointer(material, sizeof(Stream<char>) + sizeof(float) + sizeof(float) + sizeof(Color) + sizeof(float3)),
			alignof(Stream<wchar_t>)
		);

		Stream<wchar_t>* texture_name = (Stream<wchar_t>*)function::OffsetPointer(
			base_address,
			sizeof(Stream<wchar_t>) * texture_index
		);
		texture_name->InitializeFromBuffer(memory, texture.size);
		texture_name->Copy(texture);
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void AllocatePBRMaterial(
		PBRMaterial& material, 
		containers::Stream<char> name, 
		containers::Stream<PBRMaterialMapping> mappings,
		AllocatorPolymorphic allocator
	)
	{
		size_t total_allocation_size = name.size;

		for (size_t index = 0; index < mappings.size; index++) {
			total_allocation_size += mappings[index].texture.size * sizeof(wchar_t);
		}

		void* allocation = Allocate(allocator, total_allocation_size, alignof(wchar_t));

		uintptr_t ptr = (uintptr_t)allocation;
		char* mutable_ptr = (char*)ptr;
		memcpy(mutable_ptr, name.buffer, (name.size + 1) * sizeof(char));
		mutable_ptr[name.size] = '\0';
		material.name = (const char*)ptr;
		ptr += sizeof(char) * (name.size + 1);

		ptr = function::align_pointer(ptr, alignof(wchar_t));

		for (size_t index = 0; index < mappings.size; index++) {
			SetPBRMaterialTexture(&material, ptr, mappings[index].texture, mappings[index].index);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------

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
		material.metallic_factor = 0.0f;
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

		auto functor = [](const std::filesystem::path& path, void* _data) {
			const wchar_t* c_path = path.c_str();
			Stream<wchar_t> stream_path = ToStream(c_path);

			Data* data = (Data*)_data;
			// find the base part first
			const wchar_t* base_start = wcsstr(c_path, data->base_name.buffer);
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
					data->valid_textures->Add({ {data->temp_memory->buffer + data->temp_memory->size, stream_path.size}, data->texture_mask->buffer[found_index] });
					data->texture_mask->RemoveSwapBack(found_index);
					data->temp_memory->AddStreamSafe(stream_path);
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

		buffer = function::align_pointer(buffer, alignof(wchar_t));
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