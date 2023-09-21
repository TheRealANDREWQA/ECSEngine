#pragma once
#include "InternalStructures.h"
#include "../Containers/Stream.h"
#include "../Utilities/TreeIterator.h"

namespace ECSEngine {

	struct EntityHierarchyHash {
		static inline unsigned int Hash(Entity entity) {
			return (entity.value + entity.value) * entity.value;
		};
	};
	
	ECSENGINE_API MemoryManager DefaultEntityHierarchyAllocator(AllocatorPolymorphic global_memory);

#define ECS_ENTITY_HIERARCHY_STATIC_STORAGE (3)
#define ECS_ENTITY_HIERARCHY_MAX_CHILDREN (1 << 11)

	struct ECSENGINE_API EntityHierarchy {
		EntityHierarchy() = default;
		// children and parent table initial size must be a power of two
		EntityHierarchy(
			MemoryManager* memory_manager,
			unsigned int root_initial_size = -1, 
			unsigned int children_table_initial_size = -1,
			unsigned int parent_table_initial_size = -1
		);

		// If the parent doesn't exist it will insert it as well.
		// If the parent is -1, then the child will be placed as a root
		void AddEntry(Entity parent, Entity child);
		
		// The allocator should already be initialized
		void CopyOther(const EntityHierarchy* other);

		// Updates the parent of an entity to another parent
		void ChangeParent(Entity new_parent, Entity child);

		// Updates the parent of an entity if it already has a parent, else it parents that entity
		// to the specified parent
		void ChangeOrSetParent(Entity parent, Entity child);

		// Returns true if the entity is a root or a child of another entity
		bool Exists(Entity entity);

		// Return true in the functor to early exit.
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachImpl(Entity current_entity, Entity parent, Functor&& functor) {
			Stream<Entity> children = GetChildren(current_entity);
			if constexpr (early_exit) {
				if (functor(current_entity, parent, children)) {
					return true;
				}
			}
			else {
				functor(current_entity, parent, children);
			}

			for (size_t index = 0; index < children.size; index++) {
				bool should_early_exit = ForEachImpl<early_exit>(children[index], current_entity, functor);
				if constexpr (early_exit) {
					if (should_early_exit) {
						return true;
					}
				}
			}
			return false;
		}

		// Return true in the functor to early exit.
		// Top down traversal. The functor receives as parameters the current entity, its parent (-1 if it doesn't have one)
		// and the its children as a Stream<Entity> (size = 0 if it doesn't have any)
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			for (unsigned int index = 0; index < roots.size; index++) {
				bool should_early_exit = ForEachImpl<early_exit>(roots[index], Entity(-1), functor);
				if constexpr (early_exit) {
					if (should_early_exit) {
						return true;
					}
				}
			}
			return false;
		}

		unsigned int GetEntityCount() const;

		// It will alias the children from inside the table. Do not modify !!
		// If the static storage is provided and the stream would normally point to it,
		// then it will copy into the given buffer (useful if wanting to read the children
		// while doing operations like adding and removing)
		Stream<Entity> GetChildren(Entity parent, Entity* static_storage = nullptr) const;

		// It will copy into the children stream
		void GetChildrenCopy(Entity parent, CapacityStream<Entity>& children) const;

		// Fills in the children stream with all the children of that entity. It will recursively go into the children.
		// Can be used to enumerate all children and subchildren of a certain entity.
		// It does nothing if the entity doesn't exist in the hierarchy
		void GetAllChildrenFromEntity(Entity entity, CapacityStream<Entity>& children) const;

		// It returns an entity -1 in case the entity does not have a parent (doesn't exist, or it is a root)
		Entity GetParent(Entity entity) const;

		// Determines the root that contains that entity
		// It returns an entity -1 in case the entity doesn't exist in the hierarchy at all.
		// It can happen that if some disconnect happened inside the hierarchy it returns a valid entity
		// that is not the root of the given entity
		Entity GetRootFromChildren(Entity entity) const;

		// Returns true if the entity is a root, else false (can return false if the entity has not yet been inserted)
		bool IsRoot(Entity entity) const;

		// It will eliminate all the children as well
		void RemoveEntry(Entity entity);

		union Children {
			Children() {}

			ECS_INLINE Entity* Entities() {
				return IsPointer() ? entities : static_children;
			}

			ECS_INLINE const Entity* Entities() const {
				return IsPointer() ? entities : static_children;
			}

			ECS_INLINE bool IsPointer() const {
				return count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE;
			}

			// Unfortunately, some bits need to be wasted. Only 3 static children can be stored at a time
			Entity static_children[ECS_ENTITY_HIERARCHY_STATIC_STORAGE];
			struct {
				Entity* entities;
				// Needed such that the 3 entities don't overlap the count
				unsigned int padding;
				unsigned int count;
			};
		};

		MemoryManager* allocator;
		ResizableStream<Entity> roots;
		HashTable<Children, Entity, HashFunctionPowerOfTwo, EntityHierarchyHash> children_table;
		HashTable<Entity, Entity, HashFunctionPowerOfTwo, EntityHierarchyHash> parent_table;
	};

	// -----------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeEntityHierarchy(const EntityHierarchy* hierarchy, ECS_FILE_HANDLE file);

	// -----------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeEntityHierarchy(const EntityHierarchy* hierarchy, uintptr_t& ptr);

	// -----------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeEntityHierarchySize(const EntityHierarchy* hierarchy);

	// -----------------------------------------------------------------------------------------------------

	ECSENGINE_API bool DeserializeEntityHierarchy(EntityHierarchy* hierarchy, ECS_FILE_HANDLE file);

	// -----------------------------------------------------------------------------------------------------

	// The hierarchy must have its allocator set
	// Returns true if the data was valid, else false if the data was corrupted
	ECSENGINE_API bool DeserializeEntityHierarchy(EntityHierarchy* hierarchy, uintptr_t& ptr);

	// -----------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data required for the children table entries
	ECSENGINE_API size_t DeserializeEntityHierarchySize(uintptr_t ptr);

	// -----------------------------------------------------------------------------------------------------

	struct ECSENGINE_API EntityHierarchyIteratorImpl {
		using StorageType = Entity;
		using ReturnType = Entity;

		ECS_INLINE Stream<StorageType> GetChildren(StorageType value, AllocatorPolymorphic allocator) const {
			return hierarchy->GetChildren(value);
		}

		ECS_INLINE bool HasChildren(StorageType value) const {
			return hierarchy->GetChildren(value).size > 0;
		}

		ECS_INLINE Stream<StorageType> GetRoots(AllocatorPolymorphic allocator) const {
			return hierarchy->roots;
		}

		ECS_INLINE ReturnType GetReturnValue(StorageType value, AllocatorPolymorphic allocator) const {
			return value;
		}

		const EntityHierarchy* hierarchy;
	};

	typedef DFSIterator<EntityHierarchyIteratorImpl> DFSEntityHierarchyIterator;

	typedef BFSIterator<EntityHierarchyIteratorImpl> BFSEntityHierarchyIterator;

	// Preferably a temporary allocator. If nullptr it uses the internal allocator
	ECSENGINE_API BFSEntityHierarchyIterator GetEntityHierarchyBFSIterator(const EntityHierarchy* hierarchy, AllocatorPolymorphic allocator = { nullptr });

	// Preferably a temporary allocator. If nullptr it uses the internal allocator
	ECSENGINE_API DFSEntityHierarchyIterator GetEntityHierarchyDFSIterator(const EntityHierarchy* hierarchy, AllocatorPolymorphic allocator = { nullptr });

	// -----------------------------------------------------------------------------------------------------

}