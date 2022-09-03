#pragma once
#include "../Containers/Stream.h"
#include "InternalStructures.h"
#include "../Utilities/TreeIterator.h"

namespace ECSEngine {

	struct ECSENGINE_API EntityHierarchyHash {
		static unsigned int Hash(Entity entity) {
			return (entity.value + entity.value) * entity.value;
		};
	};
	
	ECSENGINE_API MemoryManager DefaultEntityHierarchyAllocator(GlobalMemoryManager* global_memory);

#define ECS_ENTITY_HIERARCHY_STATIC_STORAGE (3)
#define ECS_ENTITY_HIERARCHY_MAX_CHILDREN (1 << 11)

	struct EntityHierarchy;

	struct ECSENGINE_API EntityHierarchyIteratorImpl {
		using storage_type = Entity;
		using return_type = Entity;

		Stream<storage_type> GetChildren(storage_type value, AllocatorPolymorphic allocator) const;

		bool HasChildren(storage_type value) const;

		Stream<storage_type> GetRoots(AllocatorPolymorphic allocator) const;

		return_type GetReturnValue(storage_type value, AllocatorPolymorphic allocator) const;

		const EntityHierarchy* hierarchy;
	};

	typedef DFSIterator<EntityHierarchyIteratorImpl> DFSEntityHierarchyIterator;

	typedef BFSIterator<EntityHierarchyIteratorImpl> BFSEntityHierarchyIterator;

	struct ECSENGINE_API EntityHierarchy {
		EntityHierarchy() = default;
		// children and parent table initial size must be a power of two
		EntityHierarchy(
			MemoryManager* memory_manager,
			unsigned int root_initial_size = -1, 
			unsigned int children_table_initial_size = -1,
			unsigned int parent_table_initial_size = -1
		);

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

		template<typename Functor>
		void ForEachImpl(Entity current_entity, Entity parent, Functor&& functor) {
			Stream<Entity> children = GetChildren(current_entity);
			functor(current_entity, parent, children);

			for (size_t index = 0; index < children.size; index++) {
				ForEachImpl(children[index], current_entity, functor);
			}
		}

		// Top down traversal. The functor receives as parameters the current entity, its parent (-1 if it doesn't have one)
		// and the its children as a Stream<Entity> (size = 0 if it doesn't have any)
		template<typename Functor>
		void ForEach(Functor&& functor) {
			for (unsigned int index = 0; index < roots.size; index++) {
				ForEachImpl(roots[index], Entity(-1), functor);
			}
		}

		// Preferably a temporary allocator. If nullptr it uses the internal allocator
		BFSEntityHierarchyIterator GetBFSIterator(AllocatorPolymorphic allocator = { nullptr }) const;

		// Preferably a temporary allocator. If nullptr it uses the internal allocator
		DFSEntityHierarchyIterator GetDFSIterator(AllocatorPolymorphic allocator = { nullptr }) const;

		unsigned int GetEntityCount() const;

		// It will alias the children from inside the table. Do not modify !!
		Stream<Entity> GetChildren(Entity parent) const;

		// It will copy into the children stream
		void GetChildrenCopy(Entity parent, CapacityStream<Entity>& children) const;

		// Fills in the children stream with all the children of that entity. It will recursively go into the children.
		// Can be used to enumerate all children and subchildren of a certain entity.
		// It does nothing if the entity doesn't exist in the hierarchy
		void GetAllChildrenFromEntity(Entity entity, CapacityStream<Entity>& children) const;

		// It returns an entity full of 1's (can verify with -1) in case the entity does not have a parent (doesn't exist, or it is a root)
		Entity GetParent(Entity entity) const;

		// Determines the root that contains that entity
		// It returns an entity full of 1's in case the entity doesn't exist in the hierarchy at all
		Entity GetRootFromChildren(Entity entity) const;

		// It will eliminate all the children as well
		void RemoveEntry(Entity entity);

		union Children {
			Children() {}

			inline Entity* Entities() {
				return IsPointer() ? entities : static_children;
			}

			inline const Entity* Entities() const {
				return IsPointer() ? entities : static_children;
			}

			inline bool IsPointer() const {
				return count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE;
			}

			// Unfortunately, some bits need to be wasted. Only 3 static children can be stored at a time
			Entity static_children[ECS_ENTITY_HIERARCHY_STATIC_STORAGE];
			struct {
				Entity* entities;
				unsigned int padding;

				// Used to tell if this is a root or not
				unsigned int is_root : 1;
				// Indexes into the roots stream and can be used to quickly remove roots
				unsigned int root_index : 20;
				unsigned int count : 11;
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

	ECSENGINE_API void SerializeEntityHierarchy(const EntityHierarchy* hierarchy, uintptr_t* ptr);

	// -----------------------------------------------------------------------------------------------------

	ECSENGINE_API size_t SerializeEntityHierarchySize(const EntityHierarchy* hierarchy);

	// -----------------------------------------------------------------------------------------------------

	ECSENGINE_API bool DeserializeEntityHierarchy(EntityHierarchy* hierarchy, ECS_FILE_HANDLE file);

	// -----------------------------------------------------------------------------------------------------

	// The hierarchy must have its allocator set
	// Returns true if the data was valid, else false if the data was corrupted
	ECSENGINE_API bool DeserializeEntityHierarchy(EntityHierarchy* hierarchy, uintptr_t* ptr);

	// -----------------------------------------------------------------------------------------------------

	// Returns the amount of pointer data required for the children table entries
	ECSENGINE_API size_t DeserializeEntityHierarchySize(uintptr_t ptr);

	// -----------------------------------------------------------------------------------------------------

}