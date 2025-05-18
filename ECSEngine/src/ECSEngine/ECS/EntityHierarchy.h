// ECS_REFLECT
#pragma once
#include "InternalStructures.h"
#include "../Containers/Stream.h"
#include "../Utilities/TreeIterator.h"
#include "../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	struct EntityHierarchyHash {
		ECS_INLINE static unsigned int Hash(Entity entity) {
			return entity.value;
		};
	};
	
	// Initializes the first parameter with memory from the second parameter
	ECSENGINE_API void DefaultEntityHierarchyAllocator(MemoryManager* allocator, AllocatorPolymorphic global_memory);

#define ECS_ENTITY_HIERARCHY_STATIC_STORAGE (2)


	struct ECSENGINE_API EntityHierarchy {
		struct Node;

		struct ChildIterator : IteratorInterface<Entity> {
			ECS_INLINE ChildIterator() : IteratorInterface<Entity>(0), child_node_index(0) {}
			ECS_INLINE ChildIterator(const Node* node) : IteratorInterface<Entity>(node->child_count), child_nodes(node->ChildrenStream()), child_node_index(0) {}

			ECS_ITERATOR_COPY_AND_ASSIGNMENT_OPERATORS(ChildIterator);

			virtual bool IsContiguous() const override {
				// The nodes are contiguous, but not the entities
				return false;
			}

			virtual IteratorInterface<Entity>* Clone(AllocatorPolymorphic allocator) override { return CloneHelper<ChildIterator>(allocator); }

			virtual IteratorInterface<Entity>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
				ChildIterator* subiterator = AllocateAndConstruct<ChildIterator>(allocator);
				subiterator->child_nodes = child_nodes;
				subiterator->child_node_index = child_node_index;
				subiterator->remaining_count = count;

				child_node_index += count;
				return subiterator;
			}

			virtual Entity* GetImpl() override {
				if (child_node_index >= child_nodes.size) {
					return nullptr;
				}

				return &child_nodes[child_node_index++]->entity;
			}

			Stream<Node*> child_nodes;
			size_t child_node_index;
		};

		// It doesn't include the parent node into the iteration
		struct ECSENGINE_API NestedChildIterator : IteratorInterface<Entity> {
			struct Level {
				Node* node;
				size_t node_child_index;
			};

			ECS_INLINE NestedChildIterator() : IteratorInterface<Entity>(0), level_index(0), is_unbounded(true) {}
			NestedChildIterator(Node* node);

			ECS_ITERATOR_COPY_AND_ASSIGNMENT_OPERATORS(NestedChildIterator);

			virtual bool IsContiguous() const override {
				return false;
			}

			virtual bool IsUnbounded() const override {
				return is_unbounded;
			}

			virtual void ComputeRemainingCount() override;

			virtual IteratorInterface<Entity>* Clone(AllocatorPolymorphic allocator) override { return CloneHelper<NestedChildIterator>(allocator); }

			virtual IteratorInterface<Entity>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override;

			virtual Entity* GetImpl() override;

			void Advance();

			// Allow a maximum depth - should be hard to reach in practice
			Level levels[64];
			size_t level_index;
			bool is_unbounded;
		};

		EntityHierarchy() = default;
		// Root initialize size and node table initial size must be a power of two
		EntityHierarchy(
			MemoryManager* memory_manager,
			unsigned int root_initial_size = -1, 
			unsigned int node_table_initialize_size = -1
		);

		// If the parent is -1, then the child will be placed as a root
		void AddEntry(Entity parent, Entity child);
		
		// TODO: Implement this - much faster than adding each child invidually
		// If the parent is -1, the children will be placed at the root
		void AddChildren(Entity parent, Stream<Entity> children);

		// The allocator should already be initialized
		void CopyOther(const EntityHierarchy* other);

		// Updates the parent of an entity to another parent. It assumes that the new_parent and the child exist in the hierarchy
		void ChangeParent(Entity new_parent, Entity child);

		// Returns true if the entity is a root or a child of another entity
		bool Exists(Entity entity) const;

		// It will alias the children from inside the table. Do not modify !!
		Stream<Node*> GetChildrenNodes(Entity parent) const;

		// Fills in the children of the given parent entity
		void GetChildren(Entity parent, AdditionStream<Entity> children) const;

		// Returns the children of the entity, allocated from the given allocator
		Stream<Entity> GetChildren(Entity parent, AllocatorPolymorphic allocator) const;

		// Fills in the children stream with all the children of that entity. It will recursively go into the children.
		// Can be used to enumerate all children and subchildren of a certain entity.
		// It does nothing if the entity doesn't exist in the hierarchy.
		void GetAllChildren(Entity entity, AdditionStream<Entity> children) const;

		// The iterator doesn't contain the entity itself
		ChildIterator GetChildIterator(Entity entity) const;

		// The iterator doesn't contain the entity itself
		NestedChildIterator GetNestedChildIterator(Entity entity) const;

		// It returns an invalid entity in case the entity does not have a parent (doesn't exist, or it is a root)
		Entity GetParent(Entity entity) const;

		// Determines the root that contains that entity. If the entity doesn't exist in the hierarchy,
		// It returns an invalid entity. If the given entity is already a root, it returns itself.
		Entity GetRootFromEntity(Entity entity) const;

		// Returns true if the entity is a root, else false (can return false if the entity has not yet been inserted)
		bool IsRoot(Entity entity) const;

		// It will eliminate all the children as well
		void RemoveEntry(Entity entity);

	private:
		void AddChildToNode(Node* node, Node* child);

		// Changes the child's parent to be the new parent
		void ChangeParentTo(Node* new_parent, Node* child);

		void RemoveChildFromNode(Node* node, Node* child);

	public:

		struct Node {
			ECS_INLINE Node** Children() {
				return IsPointer() ? allocated_children : static_children;
			}

			ECS_INLINE const Node** Children() const {
				return (const Node**)(IsPointer() ? allocated_children : static_children);
			}

			ECS_INLINE Stream<Node*> ChildrenStream() const {
				return { Children(), child_count };
			}

			ECS_INLINE bool IsPointer() const {
				return child_count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE;
			}

			ECS_INLINE void Deallocate(AllocatorPolymorphic allocator) {
				if (child_count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
					ECSEngine::Deallocate(allocator, allocated_children);
				}
			}

			// The parent will be nullptr for a root node.
			Node* parent;
			Entity entity;

			// If an entity has few children, store them in-place elements here
			// To avoid another indirection
			unsigned int child_count;
			union {
				Node** allocated_children;
				Node* static_children[ECS_ENTITY_HIERARCHY_STATIC_STORAGE];
			};
		};

		MemoryManager* allocator;
		// Performance explanation: Using HashTableEmpty because it can accelerate
		// The removals, since if there are many roots, this can become searching through an array can become expensive.
		// That does reduce the iteration performance, but the hash table should be reasonably packed, so there shouldn't
		// Be too much performance lost.
		// It also helps in answering IsRoot() calls faster, since the node data doesn't need to be accessed, resulting
		// In fewer cache lines being touched.
		HashTableEmpty<Entity, HashFunctionPowerOfTwo> roots;
		HashTable<Node*, Entity, HashFunctionPowerOfTwo, EntityHierarchyHash> node_table;
	};

	struct WriteInstrument;
	struct ReadInstrument;

	// -----------------------------------------------------------------------------------------------------

	// Returns true if it succeeded, else false
	ECSENGINE_API bool SerializeEntityHierarchy(const EntityHierarchy* hierarchy, WriteInstrument* write_instrument);

	// -----------------------------------------------------------------------------------------------------

	// The hierarchy must have its allocator set
	// Returns true if the data was valid, else false if the data was corrupted
	ECSENGINE_API bool DeserializeEntityHierarchy(EntityHierarchy* hierarchy, ReadInstrument* read_instrument);
	
	// -----------------------------------------------------------------------------------------------------

	// The serialization and deserialization of this change set can be done using the reflection manager
	struct ECS_REFLECT EntityHierarchyChangeSet {
		ResizableStream<Entity> removed_entities;
		// This array includes entities which were newly created, but also those that already existed.
		ResizableStream<EntityPair> changed_parents;
	};

	// Determines the change set that can be applied to an entity hierarchy such that
	// A delta can be performed later on. Allocates all buffers from the given temporary allocator
	ECSENGINE_API EntityHierarchyChangeSet DetermineEntityHierarchyChangeSet(
		const EntityHierarchy* previous_hierarchy, 
		const EntityHierarchy* new_hierarchy, 
		AllocatorPolymorphic temporary_allocator
	);

	// Applies a previously computed change set to a hierarchy.
	ECSENGINE_API void ApplyEntityHierarchyChangeSet(EntityHierarchy* hierarchy, const EntityHierarchyChangeSet& change_set);

	// -----------------------------------------------------------------------------------------------------

	struct ECSENGINE_API EntityHierarchyIteratorImpl {
		using StorageType = Entity;
		using ReturnType = Entity;

		ECS_INLINE Stream<StorageType> GetChildren(StorageType value, AllocatorPolymorphic allocator) const {
			return hierarchy->GetChildren(value, allocator);
		}

		ECS_INLINE bool HasChildren(StorageType value) const {
			return hierarchy->GetChildrenNodes(value).size > 0;
		}

		ECS_INLINE IteratorInterface<const StorageType>* GetRoots(AllocatorPolymorphic allocator) const {
			using RootIteratorType = decltype(hierarchy->roots.ConstIdentifierIterator());
			return AllocateAndConstruct<RootIteratorType>(allocator, hierarchy->roots.ConstIdentifierIterator());
		}

		ECS_INLINE ReturnType GetReturnValue(StorageType value, AllocatorPolymorphic allocator) const {
			return value;
		}

		const EntityHierarchy* hierarchy;
	};

	typedef DFSIterator<EntityHierarchyIteratorImpl> DFSEntityHierarchyIterator;

	typedef BFSIterator<EntityHierarchyIteratorImpl> BFSEntityHierarchyIterator;

	// Preferably a temporary allocator. If nullptr it uses the internal allocator
	ECSENGINE_API BFSEntityHierarchyIterator GetEntityHierarchyBFSIterator(const EntityHierarchy* hierarchy, AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR);

	// Preferably a temporary allocator. If nullptr it uses the internal allocator
	ECSENGINE_API DFSEntityHierarchyIterator GetEntityHierarchyDFSIterator(const EntityHierarchy* hierarchy, AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR);

	// -----------------------------------------------------------------------------------------------------

}