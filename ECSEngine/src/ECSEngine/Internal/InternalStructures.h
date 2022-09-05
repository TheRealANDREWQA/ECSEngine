#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Allocators/MemoryManager.h"
#include "../Allocators/MemoryArena.h"
#include "../Allocators/LinearAllocator.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "ecspch.h"
#include "../Containers/StableReferenceStream.h"
#include "../Containers/ResizableStableReferenceStream.h"
#include "../Utilities/File.h"
#include "../Containers/DataPointer.h"

#define ECS_ARCHETYPE_MAX_COMPONENTS 15
#define ECS_ARCHETYPE_MAX_SHARED_COMPONENTS 15


namespace ECSEngine {

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

	// If the extended string is specified, it will write the value and in parentheses the index and generation count
	// Else just the value
	ECSENGINE_API void EntityToString(Entity entity, CapacityStream<char>& string, bool extended_string = false);

	// Returns the entity from that string
	ECSENGINE_API Entity StringToEntity(Stream<char> string);

#define ECS_MAIN_ARCHETYPE_MAX_COUNT (1 << 10)
#define ECS_BASE_ARCHETYPE_MAX_COUNT (1 << 10)
#define ECS_STREAM_ARCHETYPE_MAX_COUNT (1 << 24)
#define ECS_ENTITY_HIERARCHY_MAX_COUNT (4)

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

		// 32 bits for these 3 fields
		unsigned int stream_index : 24;
		unsigned int layer : 4;
		unsigned int hierarchy : 4;
	};

	struct Component {
		unsigned int Hash() const {
			return value;
		}

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

	struct ECSENGINE_API ComponentInfo {
		ComponentInfo() : size(0) {}
		ComponentInfo(unsigned short _size) : size(_size) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ComponentInfo);

		unsigned short size;
		MemoryArena* allocator;
	};

	struct ECSENGINE_API SharedComponentInfo {
		ComponentInfo info;
		ResizableStableReferenceStream<void*> instances;
		HashTableDefault<SharedInstance> named_instances;
	};

	struct ECSENGINE_API ComponentSignature {
		ComponentSignature() : indices(nullptr), count(0) {}
		ComponentSignature(Component* _indices, unsigned char _count) : indices(_indices), count(_count) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ComponentSignature);

		ComponentSignature Copy(uintptr_t& ptr);

		Component* indices;
		unsigned char count;
	};

	ECSENGINE_API bool HasComponents(ComponentSignature query, ComponentSignature archetype_component);

	ECSENGINE_API bool ExcludesComponents(ComponentSignature query, ComponentSignature archetype_component);

	struct ECSENGINE_API SharedComponentSignature {
		SharedComponentSignature() : indices(nullptr), instances(nullptr), count(0) {}
		SharedComponentSignature(Component* _indices, SharedInstance* _instances, unsigned char _count) : indices(_indices), instances(_instances), count(_count) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(SharedComponentSignature);

		Component* indices;
		SharedInstance* instances;
		unsigned char count;
	};

	struct DeferredAction {
		DataPointer data_and_type;
		DebugInfo debug_info;
	};

	typedef CapacityStream<DeferredAction> EntityManagerCommandStream;

	// Returns a memory manager that would suit an EntityPool with the given pool capacity
	//ECSENGINE_API MemoryManager DefaultEntityPoolManager(GlobalMemoryManager* global_memory);

	struct ECSENGINE_API EntityPool {
	public:
		EntityPool(
			MemoryManager* memory_manager,
			unsigned int pool_power_of_two
		);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(EntityPool);

		// Allocates a single Entity
		Entity Allocate();

		// Allocates a single Entity and assigns the info to that entity
		Entity AllocateEx(unsigned int archetype, unsigned int base_archetype, unsigned int stream_index);

		// Allocates multiple entities
		// It will fill in the buffer with the entities generated
		void Allocate(Stream<Entity> entities);

		// Allocates multiple entities and it will assign to them the values from infos
		// It will fill in the buffer with entities generated
		void AllocateEx(Stream<Entity> entities, unsigned int archetype, unsigned int base_archetype, const unsigned int* stream_indices);

		// It will set the infos according to the archetype indices and chunk positions
		void AllocateEx(Stream<Entity> entities, uint2 archetype_indices, unsigned int copy_position);

		void CreatePool();

		void CopyEntities(const EntityPool* entity_pool);

		// The tag should be the bit position, not the actual value
		void ClearTag(Entity entity, unsigned char tag);

		void Deallocate(Entity entity);

		void Deallocate(Stream<Entity> entities);

		void DeallocatePool(unsigned int pool_index);
		
		// Receives as parameter the entity and its entity info
		template<bool early_exit = false, typename Functor>
		void ForEach(Functor&& functor) {
			bool should_continue = true;
			for (unsigned int index = 0; index < m_entity_infos.size && should_continue; index++) {
				if (m_entity_infos[index].is_in_use) {
					m_entity_infos[index].stream.ForEachIndex<early_exit>([&](unsigned int stream_index) {
						Entity entity = GetEntityFromPosition(index, stream_index);
						EntityInfo info = m_entity_infos[index].stream[stream_index];
						if constexpr (early_exit) {
							if (functor(entity, info)) {
								should_continue = false;
								return true;
							}
							else {
								return false;
							}
						}
						else {
							functor(entity, info);
						}
					});
				}
			}
		}

		Entity GetEntityFromPosition(unsigned int chunk_index, unsigned int stream_index) const;

		EntityInfo GetInfo(Entity entity) const;

		EntityInfo GetInfoNoChecks(Entity entity) const;

		EntityInfo* GetInfoPtr(Entity entity);

		EntityInfo* GetInfoPtrNoChecks(Entity entity);

		// Returns how many entities are alive
		unsigned int GetCount() const;

		// The tag should be the bit position, not the actual value
		bool HasTag(Entity entity, unsigned char tag) const;

		// Checks to see if the given entity is valid in the current context
		bool IsValid(Entity entity) const;

		// Returns the generation count of the index at that index or -1 if it doesn't exist
		unsigned int IsEntityAt(unsigned int stream_index) const;

		// As if nothing is allocated
		void Reset();

		void SetEntityInfo(Entity entity, unsigned int archetype, unsigned int base_archetype, unsigned int stream_index);

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

