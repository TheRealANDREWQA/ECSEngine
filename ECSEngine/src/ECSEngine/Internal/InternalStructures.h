#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Allocators/MemoryManager.h"
#include "../Allocators/LinearAllocator.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "ecspch.h"
#include "../Math/VCLExtensions.h"
#include "../Containers/StableReferenceStream.h"

#define ECS_ARCHETYPE_MAX_COMPONENTS 15
#define ECS_ARCHETYPE_MAX_SHARED_COMPONENTS 15

#define ECS_MAX_MAIN_ARCHETYPES (1 << 12)
#define ECS_ARCHETYPE_MAX_CHUNKS /* How many chunks an archetype can have at maximum */ (1 << 12)
#define ECS_ARCHETYPE_MAX_BASE_ARCHETYPES /* How many base archetypes a main archetype can have */ (1 << 8)


namespace ECSEngine {

	constexpr bool ConstexprMemcmp(const void* source, const void* destination, size_t size) {
		const unsigned char* byte_source = (const unsigned char*)source;
		const unsigned char* byte_destination = (const unsigned char*)destination;
		for (size_t index = 0; index < size; index++) {
			if (byte_source[index] != byte_destination[index]) {
				return false;
			}
		}
		return true;
	}

	constexpr unsigned short PhysicsSystemComponentIndex(const char* string) {
		if (ConstexprMemcmp(string, "Yes", sizeof("Yes"))) {
			return 0;
		}
		else {
			return 1;
		}
	}

	ECS_CONTAINERS;

	struct ECSENGINE_API Entity {
		Entity() : value(0) {}
		Entity(unsigned int _value) : value(_value) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(Entity);

		unsigned int value;
	};

	struct ECSENGINE_API EntityInfo {
		EntityInfo() : chunk(0), main_archetype(0), base_archetype(0), stream_index(0) {}
		EntityInfo(unsigned short _chunk_index, unsigned short _archetype_index, unsigned short _subarchetype_index, unsigned int _index) :
			chunk(_chunk_index), main_archetype(_archetype_index), base_archetype(_subarchetype_index), stream_index(_index) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(EntityInfo);

		// Pack the info so no space is wasted
		unsigned int chunk : 12;
		unsigned int main_archetype : 12;
		unsigned int base_archetype : 8;
		unsigned int stream_index;
	};

	struct Component {
		unsigned short value;
	};

	inline bool operator == (const Component& lhs, const Component& rhs) {
		return lhs.value == rhs.value;
	}

	inline bool operator != (const Component& lhs, const Component& rhs) {
		return !(lhs == rhs);
	}

	struct SharedInstance {
		unsigned short value;
	};

	inline bool operator == (const SharedInstance& lhs, const SharedInstance& rhs) {
		return lhs.value == rhs.value;
	}

	inline bool operator != (const SharedInstance& lhs, const SharedInstance& rhs) {
		return !(lhs == rhs);
	}

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
		ComponentInfo() : size(0) {}
		ComponentInfo(unsigned short _size) : size(_size) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ComponentInfo);

		unsigned short size;
	};

	struct ECSENGINE_API SharedComponentInfo {
		ComponentInfo info;
		CapacityStream<void*> instances;
		HashTable<SharedInstance, ResourceIdentifier, HashFunctionPowerOfTwo, HashFunctionMultiplyString> named_instances;
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

	struct ECSENGINE_API ComponentSignature {
		ComponentSignature() : indices(nullptr), count(0) {}
		ComponentSignature(Component* _indices, unsigned char _count) : indices(_indices), count(_count) {}
		
		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ComponentSignature);

		Component* indices;
		unsigned char count;
	};

	struct ECSENGINE_API SharedComponentSignature {
		SharedComponentSignature() : indices(nullptr), instances(nullptr), count(0) {}
		SharedComponentSignature(Component* _indices, SharedInstance* _instances, unsigned char _count) : indices(_indices), instances(_instances), count(_count) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(SharedComponentSignature);

		Component* indices;
		SharedInstance* instances;
		unsigned char count;
	};

	struct ECSENGINE_API VectorComponentSignature {
		VectorComponentSignature() : value(0) {}
		VectorComponentSignature(Vec16us _value) : value(_value) {}

		VectorComponentSignature(ComponentSignature signature) { ConvertComponents(signature); }

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(VectorComponentSignature);

		ECS_INLINE bool ECS_VECTORCALL operator == (VectorComponentSignature other) const {
			return horizontal_and(value == other.value);
		}

		inline operator Vec32uc() {
			return value;
		}

		void ConvertComponents(ComponentSignature signature);

		void ConvertInstances(const SharedInstance* instances, unsigned char count);

		bool ECS_VECTORCALL HasComponents(VectorComponentSignature components) const;

		bool ECS_VECTORCALL ExcludesComponents(VectorComponentSignature components) const;

		void InitializeSharedComponent(SharedComponentSignature shared_signature);

		void InitializeSharedInstances(SharedComponentSignature shared_signature);

		Vec16us value;
	};

	bool ECS_VECTORCALL SharedComponentSignatureHasInstances(
		VectorComponentSignature archetype_components,
		VectorComponentSignature archetype_instances,
		VectorComponentSignature query_components,
		VectorComponentSignature query_instances
	);

	struct ECSENGINE_API ArchetypeQuery {
		ArchetypeQuery();
		ArchetypeQuery(VectorComponentSignature unique, VectorComponentSignature shared);

		bool ECS_VECTORCALL Verifies(VectorComponentSignature unique, VectorComponentSignature shared) const;

		bool ECS_VECTORCALL VerifiesUnique(VectorComponentSignature unique) const;

		bool ECS_VECTORCALL VerifiesShared(VectorComponentSignature shared) const;

		VectorComponentSignature unique;
		VectorComponentSignature shared;
	};

	struct ECSENGINE_API ArchetypeQueryExclude {
		ArchetypeQueryExclude();
		ArchetypeQueryExclude(
			VectorComponentSignature unique,
			VectorComponentSignature shared,
			VectorComponentSignature exclude_unique,
			VectorComponentSignature exclude_shared
		);

		bool ECS_VECTORCALL Verifies(VectorComponentSignature unique, VectorComponentSignature shared) const;

		bool ECS_VECTORCALL VerifiesUnique(VectorComponentSignature unique) const;

		bool ECS_VECTORCALL VerifiesShared(VectorComponentSignature shared) const;

		VectorComponentSignature unique;
		VectorComponentSignature shared;
		VectorComponentSignature unique_excluded;
		VectorComponentSignature shared_excluded;
	};

	struct EntitySequence {
		unsigned short chunk_index;
		unsigned int stream_offset;
		unsigned int entity_count;
	};

	struct ECSENGINE_API EntityPool {
	public:
		EntityPool(
			MemoryManager* memory_manager,
			unsigned int pool_capacity,
			unsigned int pool_power_of_two
		);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(EntityPool);

		void CreatePool();

		// Allocates a single Entity
		Entity Allocate();

		// Allocates a single Entity and assigns the info to that entity
		Entity AllocateEx(EntityInfo info);

		// Allocates multiple entities
		// It will fill in the buffer with the entities generated
		void Allocate(Stream<Entity> entities);

		// Allocates multiple entities and it will assign to them the values from infos
		// It will fill in the buffer with entities generated
		void AllocateEx(Stream<Entity> entities, EntityInfo* infos);

		// It will set the infos according to the archetype indices and chunk positions
		void AllocateEx(Stream<Entity> entities, ushort2 archetype_indices, Stream<EntitySequence> chunk_positions);

		void Deallocate(Entity entity);

		void Deallocate(Stream<Entity> entities);

		EntityInfo GetInfo(Entity entity) const;

		EntityInfo* GetInfoPtr(Entity entity);

		void SetEntityInfo(Entity entity, EntityInfo new_info);

	//private:
		struct TaggedStableReferenceStream {
			StableReferenceStream<EntityInfo> stream;
			bool is_in_use;
		};

		MemoryManager* m_memory_manager;
		ResizableStream<TaggedStableReferenceStream> m_entity_infos;
		unsigned int m_pool_capacity;
		unsigned int m_pool_power_of_two;
	};

}

