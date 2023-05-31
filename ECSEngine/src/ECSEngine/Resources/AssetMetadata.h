// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Rendering/RenderingStructures.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../Rendering/ShaderReflection.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Rendering/Compression/TextureCompressionTypes.h"
#include "../Utilities/Serialization/Binary/SerializationMacro.h"
#include "ResourceTypes.h"

namespace ECSEngine {

	enum ECS_ASSET_TYPE : unsigned char {
		ECS_ASSET_MESH,
		ECS_ASSET_TEXTURE,
		ECS_ASSET_GPU_SAMPLER,
		ECS_ASSET_SHADER,
		ECS_ASSET_MATERIAL,
		ECS_ASSET_MISC,
		ECS_ASSET_TYPE_COUNT
	};

	inline const ECS_ASSET_TYPE ECS_ASSET_TYPES_REFERENCEABLE[] = {
		ECS_ASSET_TEXTURE,
		ECS_ASSET_GPU_SAMPLER,
		ECS_ASSET_SHADER
	};

	inline const ECS_ASSET_TYPE ECS_ASSET_TYPES_NOT_REFERENCEABLE[] = {
		ECS_ASSET_MESH,
		ECS_ASSET_MATERIAL,
		ECS_ASSET_MISC
	};

	inline const ECS_ASSET_TYPE ECS_ASSET_TYPES_WITH_DEPENDENCIES[] = {
		ECS_ASSET_MATERIAL
	};

	struct AssetTypePair {
		ECS_ASSET_TYPE main_type;
		ECS_ASSET_TYPE dependency_type;
	};

	// Describes all asset types which have their metadata changed when the underlying file
	// of another dependency type is changed
	inline const AssetTypePair ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY[] = {
		{ ECS_ASSET_MATERIAL, ECS_ASSET_SHADER }
	};

	struct AssetTypedHandle {
		unsigned int handle;
		ECS_ASSET_TYPE type;
	};

	struct AssetTypedMetadata {
		void* metadata;
		ECS_ASSET_TYPE type;
	};

	struct CompareMaterialsResult {
		bool is_different;
		bool textures_different;
		bool samplers_different;
		bool buffers_different;
		bool vertex_shader_different;
		bool pixel_shader_different;
		bool name_different;
	};

	union CompareAssetsResult {
		ECS_INLINE CompareAssetsResult(bool _is_the_same) : is_the_same(_is_the_same) {}
		ECS_INLINE CompareAssetsResult(CompareMaterialsResult _material_result) : material_result(_material_result) {}

		// This is used for all assets besides materials
		bool is_the_same;
		// This is used only for materials
		CompareMaterialsResult material_result;
	};

	ECS_INLINE bool IsAssetTypeReferenceable(ECS_ASSET_TYPE type) {
		for (size_t index = 0; index < std::size(ECS_ASSET_TYPES_REFERENCEABLE); index++) {
			if (type == ECS_ASSET_TYPES_REFERENCEABLE[index]) {
				return true;
			}
		}
		return false;
	}

	ECS_INLINE bool IsAssetTypeWithDependencies(ECS_ASSET_TYPE type) {
		for (size_t index = 0; index < std::size(ECS_ASSET_TYPES_WITH_DEPENDENCIES); index++) {
			if (type == ECS_ASSET_TYPES_WITH_DEPENDENCIES[index]) {
				return true;
			}
		}
		return false;
	}

	ECS_INLINE bool IsAssetTypeMetadataWithDependencies(ECS_ASSET_TYPE type) {
		for (size_t index = 0; index < std::size(ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY); index++) {
			if (type == ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY[index].main_type) {
				return true;
			}
		}
		return false;
	}

	ECS_INLINE bool IsAssetTypeDependencyForMetadataChange(ECS_ASSET_TYPE type) {
		for (size_t index = 0; index < std::size(ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY); index++) {
			if (type == ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY[index].dependency_type) {
				return true;
			}
		}
		return false;
	}

	// Fills in all the types which are metadata dependencies for the given main_type (for the moment only materials)
	ECS_INLINE void FillAssetTypeMetadataWithDependencies(ECS_ASSET_TYPE main_type, CapacityStream<ECS_ASSET_TYPE>* types) {
		for (size_t index = 0; index < std::size(ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY); index++) {
			if (ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY[index].main_type == main_type) {
				types->AddAssert(ECS_ASSET_TYPES_METADATA_WITH_DEPENDENCY[index].dependency_type);
			}
		}
	}

	ECSENGINE_API bool AssetHasFile(ECS_ASSET_TYPE type);

	ECSENGINE_API ResourceType AssetTypeToResourceType(ECS_ASSET_TYPE type);

	struct AssetTypeEx {
		ECS_ASSET_TYPE type;

		// Useful only for shaders 
		union {
			ECS_SHADER_TYPE shader_type;
		};
	};

	// The shader_type can be used to narrow down the selection of shaders when the asset type is a shader
	struct AssetFieldTarget {
		Stream<char> name;
		AssetTypeEx type;
	};

	ECSENGINE_API extern AssetFieldTarget ECS_ASSET_METADATA_MACROS[];

	ECSENGINE_API size_t ECS_ASSET_METADATA_MACROS_SIZE();

	ECSENGINE_API extern AssetFieldTarget ECS_ASSET_TARGET_FIELD_NAMES[];

	ECSENGINE_API size_t ECS_ASSET_TARGET_FIELD_NAMES_SIZE();

	// Returns ECS_ASSET_TYPE_COUNT if it is not matching any macro definition
	ECSENGINE_API AssetTypeEx FindAssetMetadataMacro(Stream<char> string);

	// Returns ECS_ASSET_TYPE_COUNT if it is not matching any asset definition
	ECSENGINE_API AssetTypeEx FindAssetTargetField(Stream<char> string);

	// The string is read-only from the global memory (it is a constant)
	ECSENGINE_API const char* ConvertAssetTypeString(ECS_ASSET_TYPE type);

	ECSENGINE_API size_t AssetMetadataByteSize(ECS_ASSET_TYPE type);
	
	// For meshes and materials returns sizeof(CoallescedMesh) and sizeof(Material) respectively
	// For other asset types it returns 0
	ECSENGINE_API size_t TargetAssetMetadataByteSize(ECS_ASSET_TYPE type);

	enum ECS_REFLECT ECS_ASSET_MESH_OPTIMIZE_LEVEL : unsigned char {
		ECS_ASSET_MESH_OPTIMIZE_NONE,
		ECS_ASSET_MESH_OPTIMIZE_BASIC,
		ECS_ASSET_MESH_OPTIMIZE_ADVANCED
	};

	// This is the limit of assets which can be randomized on asset database load
#define ECS_ASSET_RANDOMIZED_ASSET_LIMIT 10'000

	// ------------------------------------------------------------------------------------------------------

	struct ECSENGINE_API ECS_REFLECT MeshMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		ECS_INLINE bool Compare(const MeshMetadata* other) const {
			return name == other->name && file == other->file && CompareOptions(other);
		}

		// Returns true if the parameters besides the name, target file and the asset pointer are the same
		ECS_INLINE bool CompareOptions(const MeshMetadata* other) const {
			return scale_factor == other->scale_factor && invert_z_axis == other->invert_z_axis && optimize_level == other->optimize_level;
		}

		ECS_INLINE void CopyOptions(const MeshMetadata* other) {
			scale_factor = other->scale_factor;
			invert_z_axis = other->invert_z_axis;
			optimize_level = other->optimize_level;
		}

		MeshMetadata Copy(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name, Stream<wchar_t> file);

		bool SameTarget(const MeshMetadata* other) const;

		ECS_INLINE void* Pointer() const {
			return mesh_pointer;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&mesh_pointer;
		}

		Stream<char> name;
		Stream<wchar_t> file;
		float scale_factor;
		bool invert_z_axis;
		ECS_ASSET_MESH_OPTIMIZE_LEVEL optimize_level;

		CoallescedMesh* mesh_pointer; ECS_SKIP_REFLECTION()
	};

	struct ECSENGINE_API ECS_REFLECT TextureMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;
		
		ECS_INLINE bool Compare(const TextureMetadata* other) const {
			return name == other->name && file == other->file && CompareOptions(other);
		}

		// Returns true if the parameters besides the name, target file and the asset pointer are the same
		ECS_INLINE bool CompareOptions(const TextureMetadata* other) const {
			return sRGB == other->sRGB && generate_mip_maps == other->generate_mip_maps && compression_type == other->compression_type;
		}

		ECS_INLINE void CopyOptions(const TextureMetadata* other) {
			sRGB = other->sRGB;
			generate_mip_maps = other->generate_mip_maps;
			compression_type = other->compression_type;
		}

		TextureMetadata Copy(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name, Stream<wchar_t> file);

		bool SameTarget(const TextureMetadata* other) const;

		ECS_INLINE void* Pointer() const {
			return texture.view;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&texture;
		}

		Stream<char> name;
		Stream<wchar_t> file;
		bool sRGB;
		bool generate_mip_maps;
		ECS_TEXTURE_COMPRESSION_EX compression_type;

		ResourceView texture; ECS_SKIP_REFLECTION(static_assert(sizeof(ResourceView) == 8))
	};

	struct ECSENGINE_API ECS_REFLECT GPUSamplerMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		ECS_INLINE bool Compare(const GPUSamplerMetadata* other) const {
			return name == other->name && CompareOptions(other);
		}

		// Returns true if the parameters besides the name, target file and the asset pointer are the same
		ECS_INLINE bool CompareOptions(const GPUSamplerMetadata* other) const {
			bool compare = address_mode == other->address_mode && filter_mode == other->filter_mode;
			if (compare) {
				if (filter_mode == ECS_SAMPLER_FILTER_ANISOTROPIC) {
					return anisotropic_level == other->anisotropic_level;
				}
				return true;
			}
			return false;
		}

		ECS_INLINE void CopyOptions(const GPUSamplerMetadata* other) {
			address_mode = other->address_mode;
			filter_mode = other->filter_mode;
			anisotropic_level = other->anisotropic_level;
		}

		GPUSamplerMetadata Copy(AllocatorPolymorphic allocator) const;

		void Default(Stream<char> name, Stream<wchar_t> file);

		ECS_INLINE void* Pointer() const {
			return sampler.sampler;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&sampler;
		}

		Stream<char> name;
		ECS_SAMPLER_ADDRESS_TYPE address_mode;
		ECS_SAMPLER_FILTER_TYPE filter_mode;
		unsigned char anisotropic_level;

		SamplerState sampler; ECS_SKIP_REFLECTION(static_assert(sizeof(SamplerState) == 8))
	};

	// Each macro definition and name is separately allocated
	struct ECSENGINE_API ECS_REFLECT ShaderMetadata {
		ShaderMetadata();
		ShaderMetadata(Stream<char> name, Stream<ShaderMacro> macros, AllocatorPolymorphic allocator);

		void AddMacro(Stream<char> name, Stream<char> definition, AllocatorPolymorphic allocator);

		bool Compare(const ShaderMetadata* other) const;

		// Returns true if the parameters besides the name, target file and the asset pointer are the same
		ECS_INLINE bool CompareOptions(const ShaderMetadata* other) const {
			bool compare = shader_type == other->shader_type && compile_flag == other->compile_flag;
			if (compare) {
				if (macros.size == other->macros.size) {
					for (size_t index = 0; index < macros.size; index++) {
						if (!macros[index].Compare(other->macros[index])) {
							return false;
						}
					}
					return true;
				}
			}
			return false;
		}

		// Daellocates the current options and allocates them accordingly
		void CopyOptions(const ShaderMetadata* other, AllocatorPolymorphic allocator);

		ShaderMetadata Copy(AllocatorPolymorphic allocator) const;

		bool SameTarget(const ShaderMetadata* other) const;

		void DeallocateMacros(AllocatorPolymorphic allocator) const;

		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name, Stream<wchar_t> file);

		// Returns -1 if the macro is not found
		unsigned int FindMacro(Stream<char> name) const;

		void RemoveMacro(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveMacro(Stream<char> name, AllocatorPolymorphic allocator);

		void UpdateMacro(unsigned int index, Stream<char> new_definition, AllocatorPolymorphic allocator);

		void UpdateMacro(Stream<char> name, Stream<char> new_definition, AllocatorPolymorphic allocator);

		ECS_INLINE void* Pointer() const {
			return shader_interface;
		}

		ECS_INLINE void** PtrToPointer() {
			return &shader_interface;
		}

		Stream<char> name;
		Stream<wchar_t> file;
		Stream<ShaderMacro> macros;
		ECS_SHADER_TYPE shader_type;
		ECS_SHADER_COMPILE_FLAGS compile_flag;

		// The Graphics object interface
		void* shader_interface; ECS_SKIP_REFLECTION()
		Stream<char> source_code; ECS_SKIP_REFLECTION()
		Stream<void> byte_code; ECS_SKIP_REFLECTION()
	};

	struct MaterialAssetResource {
		Stream<char> name;
		unsigned int metadata_handle;
		unsigned char slot;
	};

	// Constant buffer description
	// If it is dynamic, the data can be missing (the size must still be specified,
	// the pointer can be nullptr then). The tags is optional and is used to be passed
	// down to the material that this buffer needs some special treatment
	// The reflection type can be missing
	struct MaterialAssetBuffer {
		Reflection::ReflectionType* reflection_type = nullptr;
		Stream<char> name;
		Stream<void> data;
		Stream<char> tags = { nullptr, 0 };
		bool dynamic;
		unsigned char slot;
	};

	enum ECS_MATERIAL_SHADER : unsigned char {
		ECS_MATERIAL_SHADER_VERTEX,
		ECS_MATERIAL_SHADER_PIXEL,
		ECS_MATERIAL_SHADER_COUNT
	};

	ECS_INLINE ECS_SHADER_TYPE GetShaderFromMaterialShaderType(ECS_MATERIAL_SHADER material_shader) {
		return material_shader == ECS_MATERIAL_SHADER_VERTEX ? ECS_SHADER_VERTEX : ECS_SHADER_PIXEL;
	}

	// The name is separately allocated from the other buffers
	// The MaterialAssetResource buffers are maintained as a single coallesced buffer
	struct ECSENGINE_API MaterialAsset {
		MaterialAsset() = default;
		MaterialAsset(Stream<char> name, AllocatorPolymorphic allocator);

		CompareMaterialsResult Compare(const MaterialAsset* other) const;

		// Returns true if the parameters besides the name, target file and the asset pointer are the same
		bool CompareOptions(const MaterialAsset* other) const;

		// Daellocates the current options and allocates them accordingly
		void CopyOptions(const MaterialAsset* other, AllocatorPolymorphic allocator);

		// Does not allocate the names for the textures, samplers or buffers, neither it allocates
		// the reflection type with the given allocator
		void CopyAndResize(const MaterialAsset* asset, AllocatorPolymorphic allocator, bool allocate_name = false);

		// It will copy the buffer/texture/sampler information from the other asset for those resources
		// that match names. For textures/samplers it will only copy the slot, for the buffers it will copy
		// the slot, the dynamic status and the data. The reflection manager must be set
		// If the shader is left at default then it will deallocate all shader types
		void CopyMatchingNames(const MaterialAsset* asset, ECS_MATERIAL_SHADER shader = ECS_MATERIAL_SHADER_COUNT);

		// This asset can only be copied with an allocator - the difference between this function
		// and the CopyAndResize is that this function does copy the name for textures, samplers or buffers and
		// also allocates the reflection type with the allocator + the reflection manager
		MaterialAsset Copy(AllocatorPolymorphic allocator) const;

		// Deallocates everything that can be deallocated from the buffer reflection types
		// If the shader is left at default then it will deallocate all shader types
		// If the deallocate_pointer is left at true, then it will deallocate the ReflectionType* pointer itself
		void DeallocateBufferReflectionTypes(
			AllocatorPolymorphic allocator, 
			bool deallocate_pointer = true, 
			ECS_MATERIAL_SHADER shader = ECS_MATERIAL_SHADER_COUNT
		) const;

		// Deallocates the data.buffer pointer of each buffer
		// If the shader is left at default then it will deallocate all shader types
		void DeallocateBufferPointers(AllocatorPolymorphic allocator, ECS_MATERIAL_SHADER shader = ECS_MATERIAL_SHADER_COUNT) const;

		// Deallocates the names of the textures, samplers and buffers
		// If the shader is left at default then it will deallocate all shader types
		void DeallocateNames(AllocatorPolymorphic allocator, ECS_MATERIAL_SHADER shader = ECS_MATERIAL_SHADER_COUNT) const;

		// Deallocates all the possible allocations from the MaterialAssetBuffer structures. By default it will
		// also deallocate the reflection type
		// If the shader is left at default then it will deallocate all shader types
		void DeallocateBuffers(AllocatorPolymorphic allocator, bool include_reflection_type = true, ECS_MATERIAL_SHADER shader = ECS_MATERIAL_SHADER_COUNT) const;

		// Deallocates the name from the textures
		// If the shader is left at default then it will deallocate all shader types
		void DeallocateTextures(AllocatorPolymorphic allocator, ECS_MATERIAL_SHADER shader = ECS_MATERIAL_SHADER_COUNT) const;

		// Deallocates the name from the samplers
		// If the shader is left at default then it will deallocate all shader types
		void DeallocateSamplers(AllocatorPolymorphic allocator, ECS_MATERIAL_SHADER shader = ECS_MATERIAL_SHADER_COUNT) const;

		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name and the file
		void Default(Stream<char> name, Stream<wchar_t> file);

		// Returns the index of the buffer if it finds it else -1
		size_t FindBuffer(Stream<char> name, ECS_MATERIAL_SHADER shader) const;

		// Returns the index of the buffer if it finds it at the given slot with the given name else -1
		size_t FindBuffer(Stream<char> name, unsigned char slot, ECS_MATERIAL_SHADER shader) const;

		// Returns the index of the buffer if it finds it else -1
		size_t FindTexture(Stream<char> name, ECS_MATERIAL_SHADER shader) const;

		// Returns the index of the texture if it finds it at the given slot with the given name else -1
		size_t FindTexture(Stream<char> name, unsigned char slot, ECS_MATERIAL_SHADER shader) const;

		// Returns the index of the buffer if it finds it else -1
		size_t FindSampler(Stream<char> name, ECS_MATERIAL_SHADER shader) const;

		// Returns the index of the sampler if it finds it at the given slot with the given name else -1
		size_t FindSampler(Stream<char> name, unsigned char slot, ECS_MATERIAL_SHADER shader) const;

		// The textures can be iterated as if from a single stream
		size_t GetTextureTotalCount() const;

		// The samplers can be iterated as if from a single stream
		size_t GetSamplerTotalCount() const;

		// The buffers can be iterated as if from a single strema
		size_t GetBufferTotalCount() const;

		// Can be iterated in a single round
		inline Stream<MaterialAssetResource> GetCombinedTextures() const {
			return { textures[0].buffer, GetTextureTotalCount() };
		}

		// Can be iterated in a single round
		inline Stream<MaterialAssetResource> GetCombinedSamplers() const {
			return { samplers[0].buffer, GetSamplerTotalCount() };
		}

		// Can be iterated in a single round
		inline Stream<MaterialAssetBuffer> GetCombinedBuffers() const {
			return { buffers[0].buffer, GetBufferTotalCount() };
		}

		// There must be ECS_MATERIAL_SHADER_COUNT elements for each pointer type
		// If the do_not_copy flag is set to true, then it will not copy the data that is already stored in it
		void Resize(
			const unsigned int* texture_count, 
			const unsigned int* sampler_count, 
			const unsigned int* buffer_count, 
			AllocatorPolymorphic allocator, 
			bool do_not_copy = false
		);

		// Same as the above version, in this version the counts are contiguous
		void Resize(
			const unsigned int* counts,
			AllocatorPolymorphic allocator,
			bool do_not_copy = false
		);

		// Resizes the counts to the ones in the other material
		void Resize(
			const MaterialAsset* other,
			AllocatorPolymorphic allocator,
			bool do_not_copy = false
		);

		// Convinience function if you want to change a single buffer value
		void ResizeBufferNewValue(
			unsigned int value,
			ECS_MATERIAL_SHADER shader_type,
			AllocatorPolymorphic allocator,
			bool do_not_copy = false
		);

		// Convinience function if you want to change a single texture value
		void ResizeTexturesNewValue(
			unsigned int value,
			ECS_MATERIAL_SHADER shader_type,
			AllocatorPolymorphic allocator,
			bool do_not_copy = false
		);

		// Convinience function if you want to change a single sampler value
		void ResizeSamplersNewValue(
			unsigned int value,
			ECS_MATERIAL_SHADER shader_type,
			AllocatorPolymorphic allocator,
			bool do_not_copy = false
		);

		// Tries to retrieve the data for the given buffer from an existing source
		// The allocator is needed only when the types do not match
		void RetrieveBufferData(
			size_t current_index, 
			ECS_MATERIAL_SHADER shader, 
			const Reflection::ReflectionType* other_type, 
			const void* data,
			AllocatorPolymorphic allocator
		);

		void WriteCounts(bool write_texture, bool write_sampler, bool write_buffers, unsigned int* counts) const;

		// The counts is a contiguous array with all the counts for textures, samplers and buffers
		static void WriteTextureCount(unsigned int vertex, unsigned int pixel, unsigned int* counts);

		// The counts is a contiguous array with all the counts for textures, samplers and buffers
		static void WriteSamplerCount(unsigned int vertex, unsigned int pixel, unsigned int* counts);
		
		// The counts is a contiguous array with all the counts for textures, samplers and buffers
		static void WriteBufferCount(unsigned int vertex, unsigned int pixel, unsigned int* counts);

		ECS_INLINE void* Pointer() const {
			return material_pointer;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&material_pointer;
		}

		void GetDependencies(CapacityStream<AssetTypedHandle>* handles) const;

		void GetDependenciesForMetadata(CapacityStream<AssetTypedHandle>* handles) const;

		// After retrieving the dependencies using GetDependencies, you can change the handle values
		// inside the stream and call this function to change the handle values for these assets in their corresponding order
		void RemapDependencies(Stream<AssetTypedHandle> handles);

		// Uses the asset pointer of the metadata to verify that it belongs in the material pointer as well
		// - it does not use the handle values
		bool DependsUpon(const void* metadata, ECS_ASSET_TYPE type) const;

		Stream<char> name;
		// These are maintained as a coallesced buffer
		Stream<MaterialAssetResource> textures[ECS_MATERIAL_SHADER_COUNT];
		Stream<MaterialAssetResource> samplers[ECS_MATERIAL_SHADER_COUNT];
		Stream<MaterialAssetBuffer> buffers[ECS_MATERIAL_SHADER_COUNT];
		unsigned int vertex_shader_handle;
		unsigned int pixel_shader_handle;

		// A pointer to the graphics object
		Material* material_pointer;
		// This is used for serialization/deserialization and for CopyMatchingNames
		Reflection::ReflectionManager* reflection_manager;
	};

	struct ECSENGINE_API ECS_REFLECT MiscAsset {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		ECS_INLINE bool Compare(const MiscAsset* other) const {
			return name == other->name && file == other->file && data.Equals(other->data);
		}

		// Returns true if the parameters besides the name, target file and the asset pointer are the same
		ECS_INLINE bool CompareOptions(const MiscAsset* other) const {
			return true;
		}

		// This is just for interface uniformity
		ECS_INLINE void CopyOptions(const MiscAsset* other, AllocatorPolymorphic allocator) {}

		MiscAsset Copy(AllocatorPolymorphic allocator) const;

		// Returns true if they have the same target
		bool SameTarget(const MiscAsset* other) const;

		void Default(Stream<char> name, Stream<wchar_t> file);

		ECS_INLINE void* Pointer() const {
			return data.buffer;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&data;
		}

		Stream<char> name;
		Stream<wchar_t> file;

		Stream<void> data; ECS_SKIP_REFLECTION()
	};

	constexpr size_t AssetMetadataMaxByteSize() {
		// When using the initializer list the compiler will not evaluate this to a constant
		constexpr size_t max1 = std::max(sizeof(MeshMetadata), sizeof(TextureMetadata));
		constexpr size_t max2 = std::max(sizeof(GPUSamplerMetadata), sizeof(ShaderMetadata));
		constexpr size_t max3 = std::max(sizeof(MaterialAsset), sizeof(MiscAsset));
		constexpr size_t max4 = std::max(max1, max2);
		return std::max(max3, max4);
	}

	constexpr size_t AssetMetadataMaxSizetSize() {
		return AssetMetadataMaxByteSize() / sizeof(size_t) + (AssetMetadataMaxByteSize() % sizeof(size_t) != 0);
	}

	// If long format is specified then it will print the full file path for absolute paths instead of only the filename
	ECSENGINE_API void AssetToString(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<char>& string, bool long_format = false);

	ECSENGINE_API void CopyAssetBase(void* destination, const void* source, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator);

	ECSENGINE_API void CopyAssetOptions(void* destination, const void* source, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator);

	ECSENGINE_API void DeallocateAssetBase(const void* asset, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator);

	ECSENGINE_API void CreateDefaultAsset(void* asset, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

	inline VertexShader GetVertexShaderFromMetadata(const ShaderMetadata* metadata) {
		return (ID3D11VertexShader*)metadata->shader_interface;
	}

	inline PixelShader GetPixelShaderFromMetadata(const ShaderMetadata* metadata) {
		return (ID3D11PixelShader*)metadata->shader_interface;
	}

	inline ComputeShader GetComputeShaderFromMetadata(const ShaderMetadata* metadata) {
		return (ID3D11ComputeShader*)metadata->shader_interface;
	}

	ECSENGINE_API Stream<void> GetAssetFromMetadata(const void* metadata, ECS_ASSET_TYPE type);

	// Returns { nullptr, 0 } for materials and samplers
	ECSENGINE_API Stream<wchar_t> GetAssetFile(const void* asset, ECS_ASSET_TYPE type);

	ECSENGINE_API Stream<char> GetAssetName(const void* asset, ECS_ASSET_TYPE type);

	ECSENGINE_API void SetAssetToMetadata(void* metadata, ECS_ASSET_TYPE type, Stream<void> asset);

	// It will set a value to the pointer such that it will pass the validation test
	ECSENGINE_API void SetValidAssetToMetadata(void* metadata, ECS_ASSET_TYPE type);

	ECSENGINE_API void SetRandomizedAssetToMetadata(void* metadata, ECS_ASSET_TYPE type, unsigned int index);

	ECS_INLINE bool IsAssetPointerValid(const void* pointer) {
		return (size_t)pointer >= ECS_ASSET_RANDOMIZED_ASSET_LIMIT;
	}

	ECS_INLINE bool IsAssetPointerFromMetadataValid(Stream<void> asset_pointer) {
		return IsAssetPointerValid(asset_pointer.buffer);
	}

	ECS_INLINE bool IsAssetFromMetadataValid(const void* metadata, ECS_ASSET_TYPE type) {
		return IsAssetPointerFromMetadataValid(GetAssetFromMetadata(metadata, type));
	}

	ECS_INLINE unsigned int ExtractRandomizedAssetValue(const void* asset_pointer, ECS_ASSET_TYPE type) {
		if (type >= ECS_ASSET_TYPE_COUNT) {
			ECS_ASSERT(false, "Invalid asset type");
		}
		return (unsigned int)asset_pointer;
	}

	ECSENGINE_API bool CompareAssetPointers(const void* first, const void* second, ECS_ASSET_TYPE type);

	ECSENGINE_API bool CompareAssetPointers(const void* first, const void* second, size_t compare_size);

	// Returns true if the parameters besides the name, target file and the asset pointer are the same
	ECSENGINE_API bool CompareAssetOptions(const void* first, const void* second, ECS_ASSET_TYPE type);

	// Computes a detailed comparison of the asset options
	ECSENGINE_API CompareAssetsResult CompareAssetOptionsEx(const void* first, const void* second, ECS_ASSET_TYPE type);

	// Computes a detailed comparison (for assets that have dependencies and/or multiple ways they can be different
	// it will compute those multiple ways they can be different)
	ECSENGINE_API CompareAssetsResult CompareAssets(const void* first, const void* second, ECS_ASSET_TYPE type);

	// Returns true if the asset given references the given handle
	ECSENGINE_API bool DoesAssetReferenceOtherAsset(unsigned int handle, ECS_ASSET_TYPE handle_type, const void* asset, ECS_ASSET_TYPE type);

	ECSENGINE_API void GetAssetDependencies(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<AssetTypedHandle>* handles);

	ECSENGINE_API void GetAssetDependenciesForMetadata(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<AssetTypedHandle>* handles);

	ECSENGINE_API void RemapAssetDependencies(void* metadata, ECS_ASSET_TYPE type, Stream<AssetTypedHandle> handles);

	// The functor receives as arguments the handle and the asset type of the dependency
	// When early_exit is desired, return true to exit.
	// Returns true if it early exited, else false
	template<bool early_exit = false, typename Functor>
	bool ForEachAssetDependency(const void* metadata, ECS_ASSET_TYPE type, Functor&& functor) {
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
		GetAssetDependencies(metadata, type, &dependencies);
		for (unsigned int index = 0; index < dependencies.size; index++) {
			if constexpr (early_exit) {
				if (functor(dependencies[index].handle, dependencies[index].type)) {
					return true;
				}
			}
			else {
				functor(dependencies[index].handle, dependencies[index].type);
			}
		}
		return false;
	}

	struct ValidateAssetMetadataOptionsDesc {
		// If set, it will validate that all internal dependencies are set
		bool materials_validate_all = false;
	};

	// For all assets except materials it returns true. For materials it will verify that the shaders
	// are set. 
	ECSENGINE_API bool ValidateAssetMetadataOptions(const void* metadata, ECS_ASSET_TYPE type, ValidateAssetMetadataOptionsDesc options = {});

	// Makes sure all internal asset dependencies are set
	ECSENGINE_API bool ValidateAssetDependencies(const void* metadata, ECS_ASSET_TYPE type);

}