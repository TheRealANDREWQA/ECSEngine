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
#include "../Utilities/File.h"

#define ECS_ARCHETYPE_MAX_COMPONENTS 15
#define ECS_ARCHETYPE_MAX_SHARED_COMPONENTS 15


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

#define ECS_ENTITY_MAX_COUNT (1 << 26)

	struct ECSENGINE_API Entity {
		Entity() : value(0) {}
		Entity(unsigned int _index, unsigned int _generation_count) : index(_index), generation_count(_generation_count) {}
		Entity(unsigned int _value) : value(_value) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(Entity);

		ECS_INLINE bool operator == (const Entity& other) const {
			return value == other.value;
		}

		union {
			struct {
				unsigned int index : 26;
				unsigned int generation_count : 6;
			};
			unsigned int value;
		};
	};

#define ECS_MAIN_ARCHETYPE_MAX_COUNT (1 << 10)
#define ECS_BASE_ARCHETYPE_MAX_COUNT (1 << 10)
#define ECS_STREAM_ARCHETYPE_MAX_COUNT (1 << 24)

	struct ECSENGINE_API EntityInfo {
		EntityInfo() : main_archetype(0), base_archetype(0), stream_index(0) {}
		EntityInfo(unsigned short _archetype_index, unsigned short _subarchetype_index, unsigned int _index) :
			main_archetype(_archetype_index), base_archetype(_subarchetype_index), stream_index(_index) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(EntityInfo);

		// 32 bits for these 4 fields
		unsigned int main_archetype : 10;
		unsigned int base_archetype : 10;
		unsigned int generation_count : 6;
		unsigned int tags : 6;

		// 32 bits for these 2 fields
		unsigned int stream_index : 24;
		unsigned int layer : 8;
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

	static bool HasComponents(ComponentSignature query, ComponentSignature archetype_component) {
		for (size_t index = 0; index < query.count; index++) {
			unsigned char subindex = 0;
			for (; subindex < archetype_component.count; subindex++) {
				if (archetype_component.indices[subindex] == query.indices[index]) {
					// Exit the loop
					subindex = -2;
				}
			}
			if (subindex != 255) {
				return false;
			}
		}
		return true;
	}

	static bool ExcludesComponents(ComponentSignature query, ComponentSignature archetype_component) {
		for (size_t index = 0; index < query.count; index++) {
			unsigned char subindex = 0;
			for (; subindex < archetype_component.count; subindex++) {
				if (archetype_component.indices[subindex] == query.indices[index]) {
					// Exit the loop
					return false;
				}
			}
		}
		return true;
	}

	struct ECSENGINE_API SharedComponentSignature {
		SharedComponentSignature() : indices(nullptr), instances(nullptr), count(0) {}
		SharedComponentSignature(Component* _indices, SharedInstance* _instances, unsigned char _count) : indices(_indices), instances(_instances), count(_count) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(SharedComponentSignature);

		Component* indices;
		SharedInstance* instances;
		unsigned char count;
	};

	// Returns a memory manager that would suit an EntityPool with the given pool capacity
	ECSENGINE_API MemoryManager DefaultEntityPoolManager(GlobalMemoryManager* global_memory);

	struct ECSENGINE_API EntityPool {
	public:
		EntityPool(
			MemoryManager* memory_manager,
			unsigned int pool_power_of_two
		);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(EntityPool);

		void CreatePool();

		void CopyEntities(const EntityPool* entity_pool);

		// Allocates a single Entity
		Entity Allocate();

		// Allocates a single Entity and assigns the info to that entity
		Entity AllocateEx(unsigned short archetype, unsigned short base_archetype, unsigned int stream_index);

		// Allocates multiple entities
		// It will fill in the buffer with the entities generated
		void Allocate(Stream<Entity> entities);

		// Allocates multiple entities and it will assign to them the values from infos
		// It will fill in the buffer with entities generated
		void AllocateEx(Stream<Entity> entities,  const unsigned short* archetypes, const unsigned short* base_archetypes, const unsigned int* stream_indices);

		// It will set the infos according to the archetype indices and chunk positions
		void AllocateEx(Stream<Entity> entities, ushort2 archetype_indices, unsigned int copy_position);

		// The tag should be the bit position, not the actual value
		void ClearTag(Entity entity, unsigned char tag);

		void Deallocate(Entity entity);

		void Deallocate(Stream<Entity> entities);

		void DeallocatePool(unsigned int pool_index);

		// Checks to see if the given entity is valid in the current context
		bool IsValid(Entity entity) const;

		// The tag should be the bit position, not the actual value
		bool HasTag(Entity entity, unsigned char tag) const;

		Entity GetEntityFromPosition(unsigned int chunk_index, unsigned int stream_index) const;

		EntityInfo GetInfo(Entity entity) const;

		EntityInfo GetInfoNoChecks(Entity entity) const;

		EntityInfo* GetInfoPtr(Entity entity);

		EntityInfo* GetInfoPtrNoChecks(Entity entity);

		void SetEntityInfo(Entity entity, unsigned short archetype, unsigned short base_archetype, unsigned int stream_index);

		// The tag should be the bit position, not the actual value
		void SetTag(Entity entity, unsigned char tag);

		void SetLayer(Entity entity, unsigned int layer);

	//private:
		struct TaggedStableReferenceStream {
			// Use a queue free list because having a stack free list can pound multiple times
			// on the same free slot, creating problems for the generation counter
			StableReferenceStream<EntityInfo, true> stream;
			bool is_in_use;
		};

		MemoryManager* m_memory_manager;
		ResizableStream<TaggedStableReferenceStream> m_entity_infos;
		//unsigned int m_pool_capacity;
		unsigned int m_pool_power_of_two;
	};

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeEntityPool(const EntityPool* entity_pool, ECS_FILE_HANDLE file);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeEntityPool(const EntityPool* entity_pool, uintptr_t* stream);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeEntityPoolSize(const EntityPool* entity_pool);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool DeserializeEntityPool(EntityPool* entity_pool, ECS_FILE_HANDLE file);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API void DeserializeEntityPool(EntityPool* entity_pool, uintptr_t* stream);

	// -------------------------------------------------------------------------------------------------------------------------------------

	// Returns the number of entities in the data
	// If the version is incorrect, it will return -1
	ECSENGINE_API size_t DeserializeEntityPoolSize(uintptr_t stream);

	// -------------------------------------------------------------------------------------------------------------------------------------

}

