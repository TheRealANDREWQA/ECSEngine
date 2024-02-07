#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Allocators/MemoryManager.h"
#include "../Allocators/MemoryArena.h"
#include "../Allocators/LinearAllocator.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "../Containers/StableReferenceStream.h"
#include "../Containers/ResizableStableReferenceStream.h"
#include "../Utilities/File.h"
#include "../Containers/DataPointer.h"
#include "ComponentFunctions.h"

#define ECS_ARCHETYPE_MAX_COMPONENTS 15
#define ECS_ARCHETYPE_MAX_SHARED_COMPONENTS 15
#define ECS_SHARED_INSTANCE_MAX_VALUE SHORT_MAX
#define ECS_COMPONENT_MAX_BYTE_SIZE 2048

#define ECS_COMPONENT_BUFFER_SOA_COUNT 8

namespace ECSEngine {

#define ECS_ENTITY_MAX_COUNT (1 << 26)

	struct ECSENGINE_API Entity {
		ECS_INLINE Entity() : value(0) {}
		ECS_INLINE Entity(unsigned int _index, unsigned int _generation_count) : index(_index), generation_count(_generation_count) {}
		ECS_INLINE Entity(unsigned int _value) : value(_value) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(Entity);

		ECS_INLINE bool operator == (const Entity& other) const {
			return value == other.value;
		}

		// This just returns true if the value is different from -1,
		// And not if the entity is valid in the context of an entity manager
		ECS_INLINE bool IsValid() const {
			return value != -1;
		}

		ECS_INLINE static Entity Invalid() {
			return { (unsigned int)-1 };
		}

		ECS_INLINE operator unsigned int() const {
			return value;
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
#define ECS_ARCHETYPE_MAX_USER_DEFINED_COMPONENT_FUNCTIONS 5

	struct ArchetypeUserDefinedComponents {
		ECS_INLINE unsigned char& operator[](size_t index) {
			return signature_indices[index];
		}

		ECS_INLINE const unsigned char& operator[](size_t index) const {
			return signature_indices[index];
		}

		// These indices refer to the index inside the component signature
		unsigned char signature_indices[ECS_ARCHETYPE_MAX_USER_DEFINED_COMPONENT_FUNCTIONS];
		unsigned char count;
	};

	struct ECSENGINE_API EntityInfo {
		ECS_INLINE EntityInfo() : main_archetype(0), base_archetype(0), stream_index(0) {}
		ECS_INLINE EntityInfo(unsigned int _archetype_index, unsigned int _subarchetype_index, unsigned int _index) :
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
		ECS_INLINE Component() : value(-1) {}
		ECS_INLINE Component(short _value) : value(_value) {}

		ECS_INLINE Component& operator = (short _value) {
			value = _value;
			return *this;
		}

		ECS_INLINE bool operator == (Component other) const {
			return value == other.value;
		}

		ECS_INLINE bool operator != (Component other) const {
			return value != other.value;
		}

		ECS_INLINE unsigned int Hash() const {
			return value;
		}

		ECS_INLINE operator short() {
			return value;
		}

		// This just returns true if the value is different from -1,
		// And not if the entity is valid in the context of an entity manager
		ECS_INLINE bool Valid() const {
			return value != -1;
		}

		short value;
	};

	// Here we implement a SFINAE to check whether or not a component has the AllocatorSize function
	template<typename T, typename = void>
	struct has_AllocatorSize_t : std::false_type {};

	template<typename T>
	struct has_AllocatorSize_t<T, std::void_t<decltype(std::declval<T>().AllocatorSize())>> : std::true_type {};

	template<typename T, typename std::enable_if_t<has_AllocatorSize_t<T>::value, bool>::type = false>
	constexpr ECS_INLINE size_t ComponentAllocatorSize() {
		return 0;
	}

	template<typename T, typename std::enable_if<has_AllocatorSize_t<T>::value, bool>::type = true>
	constexpr ECS_INLINE size_t ComponentAllocatorSize()
	{
		return T::AllocatorSize();
	};

	enum ECS_COMPONENT_TYPE : unsigned char {
		ECS_COMPONENT_UNIQUE,
		ECS_COMPONENT_SHARED,
		ECS_COMPONENT_GLOBAL,
		ECS_COMPONENT_TYPE_COUNT
	};

	static Stream<char> ComponentTypeToString(ECS_COMPONENT_TYPE type) {
		switch (type) {
		case ECS_COMPONENT_UNIQUE:
			return "Unique";
		case ECS_COMPONENT_SHARED:
			return "Shared";
		case ECS_COMPONENT_GLOBAL:
			return "Global";
		}

		// Shouldn't reach here
		ECS_ASSERT(false, "Invalid component type value");
		return "";
	}

	struct ComponentWithType {
		ECS_INLINE bool operator == (ComponentWithType other) const {
			return component == other.component && type == other.type;
		}

		Component component;
		ECS_COMPONENT_TYPE type;
	};

	struct SharedInstance {
		// This just returns true if the value is different from -1,
		// And not if the entity is valid in the context of an entity manager
		ECS_INLINE bool IsValid() const {
			return value != -1;
		}

		short value;
	};

	ECS_INLINE bool operator == (const SharedInstance& lhs, const SharedInstance& rhs) {
		return lhs.value == rhs.value;
	}

	ECS_INLINE bool operator != (const SharedInstance& lhs, const SharedInstance& rhs) {
		return !(lhs == rhs);
	}

	typedef MemoryArena ComponentBufferAllocator;

	// Both offsets (for the pointer and for the size) need to be specified relative to the base
	// of the struct
	struct ComponentBuffer {
		// We need this field for the SoA pointers which can have
		// Sizes of different byte width
		unsigned char size_int_type : 2;
		// This indicates whether or not this is a SoA pointer
		unsigned char is_soa_pointer : 1;
		unsigned char is_data_pointer : 1;
		// This entry details how many soa entries there are, excluding the current one
		// So for an SoA of 3 pointers (x, y, z), this value will be 2
		unsigned char soa_pointer_count;
		// Embed the data for SoA pointers directly, such that we don't have to
		// Reference some other ComponentBuffer's. This adds some extra storage
		// For all the other cases, but we can aford it.
		unsigned char soa_pointer_element_byte_sizes[ECS_COMPONENT_BUFFER_SOA_COUNT];
		// The pointer offsets are relative from the beginning of the type. They are
		// At the moment unsigned char to not waste too much space
		unsigned char soa_pointer_offsets[ECS_COMPONENT_BUFFER_SOA_COUNT];
		unsigned short pointer_offset;
		unsigned short size_offset;
		unsigned short element_byte_size;
	};

	ECSENGINE_API unsigned short ComponentBufferPerEntryByteSize(const ComponentBuffer& component_buffer);

	// This works for all types except data pointers
	ECSENGINE_API void ComponentBufferSetSizeValue(const ComponentBuffer& component_buffer, void* target, size_t capacity);

	ECSENGINE_API size_t ComponentBufferGetSizeValue(const ComponentBuffer& component_buffer, const void* target);

	ECSENGINE_API size_t ComponentBufferAlignment(const ComponentBuffer& component_buffer);

	ECSENGINE_API void ComponentBufferSetSoAPointers(const ComponentBuffer& component_buffer, void* target, void* assign_pointer, size_t capacity);

	// It handles the case where the component buffers are aliased (as would be the case for reallocation)
	ECSENGINE_API void ComponentBufferSetAndCopySoAPointers(
		const ComponentBuffer& component_buffer, 
		void* target, 
		void* assign_pointer, 
		size_t capacity, 
		const void* source_data,
		size_t source_data_size,
		size_t source_data_capacity
	);

	ECSENGINE_API void ComponentBufferCopy(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination);

	ECSENGINE_API void ComponentBufferCopyDataPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination);

	ECSENGINE_API void ComponentBufferCopyStream(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination);

	// The source will still be offsetted
	ECSENGINE_API void ComponentBufferCopy(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, Stream<void> data, void* destination);

	// The source will still be offsetted
	ECSENGINE_API void ComponentBufferCopyStream(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, Stream<void> data, void* destination);

	// The source will still be offsetted
	ECSENGINE_API void ComponentBufferCopyDataPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, Stream<void> data, void* destination);

	// Only does the allocation and the copy if the current data is different from the given data
	ECSENGINE_API void ComponentBufferCopyStreamChecked(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, Stream<void> data, void* destination);

	ECSENGINE_API void ComponentBufferDeallocate(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, void* source);

	ECSENGINE_API void ComponentBufferDeallocateDataPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, void* source);

	ECSENGINE_API void ComponentBufferDeallocateNormalPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, void* source);

	// If the destination already has data, it will use reallocation. If the source is empty, it will deallocate directly
	ECSENGINE_API void ComponentBufferReallocate(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination);

	// If the destination already has data, it will use reallocation. If the source is empty, it will deallocate directly
	ECSENGINE_API void ComponentBufferReallocateDataPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination);

	// If the destination already has data, it will use reallocation. If the source is empty, it will deallocate directly
	ECSENGINE_API void ComponentBufferReallocateNormalPointer(const ComponentBuffer& component_buffer, ComponentBufferAllocator* allocator, const void* source, void* destination);

	ECSENGINE_API Stream<void> ComponentBufferGetStream(const ComponentBuffer& component_buffer, const void* source);

	ECSENGINE_API Stream<void> ComponentBufferGetStreamDataPointer(const ComponentBuffer& component_buffer, const void* source);

	ECSENGINE_API Stream<void> ComponentBufferGetStreamNormalPointer(const ComponentBuffer& component_buffer, const void* source);

	// It will assigns, it does not allocate
	ECSENGINE_API void ComponentBufferSetStream(const ComponentBuffer& component_buffer, void* destination, Stream<void> data);

	// It will assigns, it does not allocate
	ECSENGINE_API void ComponentBufferSetStreamDataPointer(const ComponentBuffer& component_buffer, void* destination, Stream<void> data);

	// It will assigns, it does not allocate
	ECSENGINE_API void ComponentBufferSetStreamNormalPointer(const ComponentBuffer& component_buffer, void* destination, Stream<void> data);

	struct ECSENGINE_API ComponentInfo {
		ECS_INLINE ComponentInfo() : size(0) {}
		ECS_INLINE ComponentInfo(unsigned int _size) {
			memset(this, 0, sizeof(*this));
			size = _size;
		}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ComponentInfo);

		// It will call the copy function. It assumes that there is one
		void CallCopyFunction(void* destination, const void* source, bool deallocate_previous) const;

		// It will call the deallocate function. It assumes that there is one
		void CallDeallocateFunction(void* data) const;

		// It will call this function only if it is set
		void TryCallCopyFunction(void* destination, const void* source, bool deallocate_previous) const;

		// It will call this function only if it is set
		void TryCallDeallocateFunction(void* data) const;

		ECS_INLINE ComponentFunctions GetComponentFunctions() const {
			ComponentFunctions functions;
			functions.copy_function = copy_function;
			functions.deallocate_function = deallocate_function;
			functions.allocator_size = allocator != nullptr ? allocator->InitialArenaCapacity() : 0;
			functions.data = copy_deallocate_data;
			return functions;
		}

		// It does not create the allocator
		ECS_INLINE void SetComponentFunctions(const ComponentFunctions* component_functions, AllocatorPolymorphic allocator) {
			copy_function = component_functions->copy_function;
			deallocate_function = component_functions->deallocate_function;
			if (component_functions->data.size == 0) {
				copy_deallocate_data = component_functions->data;
			}
			else {
				copy_deallocate_data = component_functions->data.Copy(allocator);
			}
		}

		ECS_INLINE void ResetComponentFunctions() {
			copy_function = nullptr;
			deallocate_function = nullptr;
			copy_deallocate_data = {};
		}

		MemoryArena* allocator;
		unsigned int size;
		ComponentCopyFunction copy_function;
		ComponentDeallocateFunction deallocate_function;
		Stream<void> copy_deallocate_data;
		Stream<char> name;
	};

	struct SharedComponentInfo {
		ComponentInfo info;
		ResizableStableReferenceStream<void*> instances;
		HashTableDefault<SharedInstance> named_instances;

		SharedComponentCompareEntry compare_entry;
	};

	struct ECSENGINE_API ComponentSignature {
		ECS_INLINE ComponentSignature() : indices(nullptr), count(0) {}
		ECS_INLINE ComponentSignature(Component* _indices, unsigned char _count) : indices(_indices), count(_count) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ComponentSignature);

		ECS_INLINE bool operator == (ComponentSignature other) const {
			return count == other.count && memcmp(indices, other.indices, sizeof(Component) * count) == 0;
		}

		ComponentSignature Copy(uintptr_t& ptr) const;

		// Returns UCHAR_MAX when the component is not found
		ECS_INLINE unsigned char Find(Component component) const {
			for (unsigned char index = 0; index < count; index++) {
				if (indices[index] == component) {
					return index;
				}
			}
			return UCHAR_MAX;
		}

		ECS_INLINE void WriteTo(Component* components) const {
			memcpy(components, indices, sizeof(Component) * count);
		}

		// Writes into the components of the given parameter signature
		ECS_INLINE ComponentSignature CombineInto(ComponentSignature write_to) const {
			WriteTo(write_to.indices + write_to.count);
			return { write_to.indices, (unsigned char)(write_to.count + count) };
		}

		ECS_INLINE Component& operator[](size_t index) {
			return indices[index];
		}

		ECS_INLINE const Component& operator[](size_t index) const {
			return indices[index];
		}

		Component* indices;
		unsigned char count;
	};

	ECSENGINE_API bool HasComponents(ComponentSignature query, ComponentSignature archetype_component);

	ECSENGINE_API bool ExcludesComponents(ComponentSignature query, ComponentSignature archetype_component);

	struct ECSENGINE_API SharedComponentSignature {
		ECS_INLINE SharedComponentSignature() : indices(nullptr), instances(nullptr), count(0) {}
		ECS_INLINE SharedComponentSignature(Component* _indices, SharedInstance* _instances, unsigned char _count) : indices(_indices), instances(_instances), count(_count) {}

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(SharedComponentSignature);

		ECS_INLINE ComponentSignature ComponentSignature() const {
			return { indices, count };
		}

		// Returns UCHAR_MAX when the component is not found
		ECS_INLINE unsigned char Find(Component component) const {
			for (unsigned char index = 0; index < count; index++) {
				if (indices[index] == component) {
					return index;
				}
			}
			return UCHAR_MAX;
		}

		Component* indices;
		SharedInstance* instances;
		unsigned char count;
	};

	struct DeferredAction {
		DataPointer data_and_type;
		DebugInfo debug_info;
	};

	typedef CapacityStream<DeferredAction> EntityManagerCommandStream;

	struct ECSENGINE_API EntityPool {
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
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			bool should_continue = true;
			for (unsigned int index = 0; index < m_entity_infos.size && should_continue; index++) {
				if (m_entity_infos[index].is_in_use) {
					should_continue = !m_entity_infos[index].stream.ForEachIndex<early_exit>([&](unsigned int stream_index) {
						Entity entity = GetEntityFromPosition(index, stream_index);
						EntityInfo info = m_entity_infos[index].stream[stream_index];
						entity.generation_count = info.generation_count;
						if constexpr (early_exit) {
							if (functor(entity, info)) {
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
			return !should_continue;
		}

		Entity GetEntityFromPosition(unsigned int chunk_index, unsigned int stream_index) const;

		EntityInfo GetInfo(Entity entity) const;

		EntityInfo GetInfoNoChecks(Entity entity) const;

		EntityInfo* GetInfoPtr(Entity entity);

		EntityInfo* GetInfoPtrNoChecks(Entity entity);

		// Returns how many entities are alive
		unsigned int GetCount() const;

		// Returns an Entity identifier that is not in use at this moment
		// The bit count limits the amount of bits that the entity can have
		// Returns -1 if there is no empty entity
		Entity GetVirtualEntity(unsigned int bit_count = 32) const;

		// Fills in entity identifiers that are not in use at this moment
		// The bit count limits the amount of bits that the entity can have
		// Returns true if there were enough entities, else false
		bool GetVirtualEntities(Stream<Entity> entities, unsigned int bit_count = 32) const;

		// Returns an Entity identifier that is not in use at this moment.
		// The bit count limits the amount of bits that the entity can have
		// Additionally, this version takes in a stream of already used slots from previous calls
		// to omit them. Returns -1 if there is no empty entity
		Entity GetVirtualEntity(Stream<Entity> excluded_entities, unsigned int bit_count = 32) const;

		// Fills in entity identifiers that are not in use at this moment.
		// The bit count limits the amount of bits that the entity can have
		// Additionally, this version takes in a stream of already used slots from previous calls
		// to omit them. Returns true if there were enough entities, else false
		bool GetVirtualEntities(Stream<Entity> entities, Stream<Entity> excluded_entities, unsigned int bit_count = 32) const;

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

	ECSENGINE_API void SerializeEntityPool(const EntityPool* entity_pool, uintptr_t& stream);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeEntityPoolSize(const EntityPool* entity_pool);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool DeserializeEntityPool(EntityPool* entity_pool, ECS_FILE_HANDLE file);

	// -------------------------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool DeserializeEntityPool(EntityPool* entity_pool, uintptr_t& stream);

	// -------------------------------------------------------------------------------------------------------------------------------------

	// Returns the number of entities in the data
	// If the version is incorrect, it will return -1
	ECSENGINE_API size_t DeserializeEntityPoolSize(uintptr_t stream);

	// -------------------------------------------------------------------------------------------------------------------------------------

}

