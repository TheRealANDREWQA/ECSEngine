#pragma once
#include "../Core.h"
#include "../Allocators/MemoryManager.h"
#include "../Allocators/LinearAllocator.h"
#include "../Containers/Stream.h"
#include "ecspch.h"

#ifndef ECS_ARCHETYPE_MAX_COMPONENTS
#define ECS_ARCHETYPE_MAX_COMPONENTS 15
#endif

#ifndef ECS_ARCHETYPE_MAX_SHARED_COMPONENTS
#define ECS_ARCHETYPE_MAX_SHARED_COMPONENTS 15
#endif


namespace ECSEngine {

	ECS_CONTAINERS;

	// Sequences should not overlapp
	struct ECSENGINE_API Sequence {

		Sequence() : first(0), last(0), buffer_start(0), size(0) {}
		Sequence(unsigned int _first, unsigned int _last, unsigned int _buffer_start, unsigned int _size)
			: first(_first), last(_last), buffer_start(_buffer_start), size(_size) {}

		Sequence& operator = (const Sequence& other) = default;

		unsigned int first;
		unsigned int last;
		unsigned int buffer_start;
		unsigned int size;
	};

	struct ECSENGINE_API ComponentInfo {
		ComponentInfo() : size(0), alignment(0), index(0) {}
		ComponentInfo(unsigned short _size, unsigned char _alignment) : size(_size), alignment(_alignment), index(0) {}
		ComponentInfo(unsigned short _size, unsigned char _alignment, unsigned char _index) : size(_size), alignment(_alignment), index(_index) {}

		ComponentInfo& operator = (const ComponentInfo& other) = default;

		unsigned short size;
		unsigned char alignment;
		unsigned char index;
	};

	struct ECSENGINE_API SharedComponentInfo {
		SharedComponentInfo() : size(0), alignment(0), index(0), subindex(0) {}
		SharedComponentInfo(unsigned short _size, unsigned char _alignment) : size(_size), alignment(_alignment), index(0), subindex(0) {}
		SharedComponentInfo(unsigned short _size, unsigned char _alignment, unsigned char _index) 
			: size(_size), alignment(_alignment), index(_index), subindex(0) {}
		SharedComponentInfo(unsigned short _size, unsigned char _alignment, unsigned char _index, unsigned char _subindex) 
			: size(_size), alignment(_alignment), index(_index), subindex(_subindex) {}

		SharedComponentInfo& operator = (const SharedComponentInfo& other) = default;

		unsigned short size;
		unsigned short subindex;
		unsigned char alignment;
		unsigned char index;
	};

	struct ECSENGINE_API Substream {
		Substream() : size(0), offset(0) {}
		Substream(unsigned int _size, unsigned int _offset) : size(_size), offset(_offset) {}

		inline bool operator >= (const Substream& other) {
			return offset >= other.offset;
		}
		
		inline bool operator <= (const Substream& other) {
			return offset <= other.offset;
		}
		
		inline bool operator > (const Substream& other) {
			return offset > other.offset;
		}
		
		inline bool operator < (const Substream& other) {
			return offset < other.offset;
		}

		unsigned int size;
		unsigned int offset;
	};

	struct ECSENGINE_API VectorComponentSignature {
		VectorComponentSignature() : value(0) {}
		VectorComponentSignature(Vec32uc _value) : value(_value) {}
		// components[0] is the count
		VectorComponentSignature(const unsigned char* components) { ConvertComponents(components); }
		VectorComponentSignature(const Stream<ComponentInfo>& infos) { ConvertComponents(infos); }

		// components1[0] and components2[0] are the counts
		VectorComponentSignature(const unsigned char* components1, const unsigned char* components2) { ConvertComponents(components1, components2); }

		VectorComponentSignature& operator = (const VectorComponentSignature& other) = default;

		inline bool ECS_VECTORCALL operator == (VectorComponentSignature other) const {
			return horizontal_and(value == other.value);
		}

		inline operator Vec32uc() {
			return value;
		}

		void ConvertComponents(const unsigned char* components);

		void ConvertComponents(const unsigned char* components1, const unsigned char* components2);

		void ConvertComponents(const Stream<ComponentInfo>& infos);

		inline bool ECS_VECTORCALL HasComponents(VectorComponentSignature components) const {
			return horizontal_and((value & components.value) == components.value);
		}

		inline bool ECS_VECTORCALL ExcludesComponents(VectorComponentSignature components) const {
			return horizontal_and((value & components.value) == Vec32uc(0));
		}

		inline bool ECS_VECTORCALL ExcludesComponents(VectorComponentSignature components, VectorComponentSignature zero_signature) const {
			return horizontal_and((value & components.value) == zero_signature.value);
		}

		bool HasComponents(const unsigned char* components) const;

		bool ExcludesComponents(const unsigned char* components) const;

		bool ExcludesComponents(const unsigned char* components, VectorComponentSignature zero_signature) const;

		Vec32uc value;
	};

	struct ECSENGINE_API ArchetypeQuery {
		ArchetypeQuery();
		ArchetypeQuery(const unsigned char* components, const unsigned char* shared_components);

		VectorComponentSignature unique;
		VectorComponentSignature shared;
	};

	struct ECSENGINE_API ArchetypeQueryExclude {
		ArchetypeQueryExclude();
		ArchetypeQueryExclude(
			const unsigned char* components,
			const unsigned char* shared_components, 
			const unsigned char* exclude_components,
			const unsigned char* exclude_shared
		);

		VectorComponentSignature unique;
		VectorComponentSignature shared;
		VectorComponentSignature unique_excluded;
		VectorComponentSignature shared_excluded;
	};

	// 1 byte chunk and sequence indices, 2 byte archetype index
	struct ECSENGINE_API EntityInfo {
		EntityInfo() : chunk(0), sequence(0), archetype(0), archetype_subindex(0) {}
		EntityInfo(unsigned int _chunk, unsigned int _sequence, unsigned int _archetype, unsigned int _subindex) : chunk(_chunk), sequence(_sequence), archetype(_archetype), archetype_subindex(_subindex) {}

		EntityInfo& operator = (const EntityInfo& other) = default;

		unsigned char chunk;
		unsigned char archetype_subindex;
		unsigned short sequence;
		unsigned short archetype;
	};

	struct ECSENGINE_API ArchetypeInfo {
		unsigned char components[ECS_ARCHETYPE_MAX_COMPONENTS + 1];
		unsigned char shared_components[ECS_ARCHETYPE_MAX_SHARED_COMPONENTS + 1];
	};

	// can also be used for static archetype base
	struct ECSENGINE_API ArchetypeBaseDescriptor {
		unsigned int chunk_size;
		unsigned short max_sequence_count;
		unsigned char max_chunk_count;
		unsigned char initial_chunk_count;
	};

	struct ECSENGINE_API RebalanceArchetypeDescriptor {
		unsigned char chunk_index = 0xFF;
		unsigned short count = 0xFFFF;
		unsigned int min_sequence_size = 0xFFFFFFFF;
		unsigned int max_sequence_size = 0xFFFFFFFF;
	};

	class ECSENGINE_API EntityPool {
	public:
		EntityPool(
			unsigned int power_of_two,
			unsigned int arena_count, 
			unsigned int pool_block_count,
			MemoryManager* memory_manager
		);

		EntityPool& operator = (const EntityPool& other) = default;
		EntityPool& operator = (EntityPool&& other) = default;

		void CreatePool();

		void FreeSequence(unsigned int first);

		EntityInfo GetEntityInfo(unsigned int entity) const;
		EntityInfo* GetEntityInfoBuffer(unsigned int buffer_index) const;
		unsigned int GetSequence(unsigned int size);
		unsigned int GetSequenceFast(unsigned int size);

		void SetEntityInfo(unsigned int entity, EntityInfo new_info);
		void SetEntityInfoToSequence(unsigned int first_entity, unsigned int size, EntityInfo new_info);

		static size_t MemoryOf(unsigned int pool_count);

	//private:
		MemoryManager* m_memory_manager;
		unsigned int m_arena_count;
		unsigned int m_pool_block_count;
		unsigned int m_pool_size;
		unsigned char m_power_of_two;
		Stream<BlockRange> m_pools;
		EntityInfo** m_entity_infos;
	};

#define ECS_RESOURCE_IDENTIFIER(name) ResourceIdentifier identifier = ResourceIdentifier(name, strlen(name_size));
#define ECS_RESOURCE_IDENTIFIER_WITH_HASH(name, hash_function) size_t name_size = strlen(name); ResourceIdentifier identifier = ResourceIdentifier(name, name_size); unsigned int hash = hash_function::Hash(name, name_size);

	// filename can be used as a general purpose pointer if other identifier than the filename is used
	// Compare function uses AVX2 32 byte SIMD char compare
	struct ECSENGINE_API ResourceIdentifier {
		ResourceIdentifier();
		ResourceIdentifier(LPCWSTR filename);
		// if the identifier is something other than a LPCWSTR path
		ResourceIdentifier(const void* id, unsigned int size);
		ResourceIdentifier(Stream<void> identifier);
		//ResourceIdentifier(Stream<wchar_t> identifier);

		ResourceIdentifier(const ResourceIdentifier& other) = default;
		ResourceIdentifier& operator = (const ResourceIdentifier& other) = default;

		bool operator == (const ResourceIdentifier& other) const;

		bool Compare(const ResourceIdentifier& other) const;

		const void* ptr;
		unsigned int size;
	};

	struct ECSENGINE_API HashFunctionAdditiveString {
		static unsigned int Hash(Stream<const char> string);
		static unsigned int Hash(Stream<const wchar_t> string);
		static unsigned int Hash(const char* string);
		static unsigned int Hash(const wchar_t* string);
		static unsigned int Hash(const void* identifier, unsigned int identifier_size);
		static unsigned int Hash(ResourceIdentifier identifier);
	};

	struct ECSENGINE_API HashFunctionMultiplyString {
		static unsigned int Hash(Stream<const char> string);
		static unsigned int Hash(Stream<const wchar_t> string);
		static unsigned int Hash(const char* string);
		static unsigned int Hash(const wchar_t* string);
		static unsigned int Hash(const void* identifier, unsigned int identifier_size);
		static unsigned int Hash(ResourceIdentifier identifier);
	};

}

