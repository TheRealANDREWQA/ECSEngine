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
#include "../Utilities/StringUtilities.h"

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

		ECS_INLINE unsigned int Hash() const {
			// A use case must be found to warrant a more advanced hash function
			return value;
		}

		// If the extended string is specified, it will write the value and in parentheses the index and generation count
		// Else just the value
		void ToString(CapacityStream<char>& string, bool extended_string = false) const;

		union {
			struct {
				unsigned int index : 26;
				unsigned int generation_count : 6;
			};
			unsigned int value;
		};
	};

	struct ECSENGINE_API EntityPair {
		ECS_INLINE EntityPair() {}
		ECS_INLINE EntityPair(Entity _first, Entity _second) : first(_first), second(_second) {}

		ECS_INLINE bool operator == (EntityPair other) const {
			return (first == other.first && second == other.second) || (first == other.second && second == other.first);
		}

		ECS_INLINE unsigned int Hash() const {
			// Use CantorPair hashing such that reversed pairs will be considered the same
			return CantorPair(first.value, second.value);
		}

		void ToString(CapacityStream<char>& string, bool extended_string = false) const;

		union {
			struct {
				Entity first;
				Entity second;
			};
			struct {
				Entity parent;
				Entity child;
			};
		};
	};

	// Returns the entity from that string
	ECSENGINE_API Entity StringToEntity(Stream<char> string);

#define ECS_ENTITY_TO_STRING(string_name, entity) ECS_STACK_CAPACITY_STREAM(char, string_name, 128); entity.ToString(string_name)
#define ECS_ENTITY_TO_STRING_EXTENDED(string_name, entity) ECS_STACK_CAPACITY_STREAM(char, string_name, 128); entity.ToString(string_name, true)

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

		ECS_INLINE void ToString(CapacityStream<char>& string) const {
			ConvertIntToChars(string, value);
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

		ECS_INLINE void ToString(CapacityStream<char>& string) const {
			ConvertIntToChars(string, value);
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
			functions.data = data;
			return functions;
		}

		// It does not create the allocator
		ECS_INLINE void SetComponentFunctions(const ComponentFunctions* component_functions, AllocatorPolymorphic allocator) {
			copy_function = component_functions->copy_function;
			deallocate_function = component_functions->deallocate_function;
			data = CopyableCopy(component_functions->data, allocator);
		}

		ECS_INLINE void ResetComponentFunctions() {
			copy_function = nullptr;
			deallocate_function = nullptr;
			data = nullptr;
		}

		MemoryArena* allocator;
		unsigned int size;
		ComponentCopyFunction copy_function;
		ComponentDeallocateFunction deallocate_function;
		Copyable* data;
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

