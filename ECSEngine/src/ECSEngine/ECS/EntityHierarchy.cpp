#include "ecspch.h"
#include "EntityHierarchy.h"
#include "../Utilities/Crash.h"
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "../Utilities/ReaderWriterInterface.h"

#define ROOT_STARTING_SIZE ECS_KB
#define NODE_TABLE_INITIAL_SIZE ECS_KB * 4

#define ENTITY_HIERARCHY_ALLOCATOR_SIZE ECS_MB * 10
#define ENTITY_HIERARCHY_BACKUP_ALLOCATOR_SIZE ECS_MB * 50
#define ENTITY_HIERARCHY_ALLOCATOR_CHUNKS 2048

#define SERIALIZE_VERSION 1

namespace ECSEngine {
    
    struct SerializeEntityHierarchyHeader {
        unsigned int version;
        unsigned int node_count;
        // Store this extra field such that we can initialize the roots hash table directly with the appropriate amount
        unsigned int root_count;
    };

    // This is the header that is serialized for each node before its children entities
    struct SerializeNodeHeader {
        Entity entity;
        Entity parent_entity;
        unsigned int child_count;
    };

    typedef EntityHierarchy::Node Node;

    // -----------------------------------------------------------------------------------------------------------------------------

    EntityHierarchy::EntityHierarchy(MemoryManager* memory_manager, unsigned int root_initial_size, unsigned int node_table_initial_size)
    {
        root_initial_size = root_initial_size == -1 ? ROOT_STARTING_SIZE : root_initial_size;
        node_table_initial_size = node_table_initial_size == -1 ? NODE_TABLE_INITIAL_SIZE : node_table_initial_size;
        ECS_ASSERT(IsPowerOfTwo(root_initial_size) && IsPowerOfTwo(node_table_initial_size));

        allocator = memory_manager;
        roots.Initialize(memory_manager, root_initial_size);
        node_table.Initialize(memory_manager, node_table_initial_size);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::AddChildToNode(Node* node, Node* child) {
        if (node->child_count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
            Stream<Node*> nodes = { node->allocated_children, node->child_count };
            nodes.AddResize(child, allocator, true);
            node->allocated_children = nodes.buffer;
        }
        else if (node->child_count == ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
            // Transition to allocated nodes
            node->allocated_children = (Node**)allocator->Allocate(sizeof(Node*) * (node->child_count + 1));
            memcpy(node->allocated_children, node->static_children, sizeof(node->static_children));
            node->allocated_children[node->child_count] = child;
        }
        else {
            // Add to the static storage
            node->static_children[node->child_count] = child;
        }

        // Also, set the child's parent to this node
        child->parent = node;
        node->child_count++;

        // At last, check to see if the child node is a root. If it is a root, we must eliminate it
        unsigned int child_root_index = roots.Find(child->entity);
        if (child_root_index != -1) {
            roots.EraseFromIndex(child_root_index);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::RemoveChildFromNode(Node* node, Node* child) {
        size_t child_index = node->ChildrenStream().Find(child);
        ECS_CRASH_CONDITION_RETURN_VOID(child_index != -1, "EntityHierarchy: Trying to remove child {#} from entity {#}, but it is not parented to that entity.", child->entity.value, node->entity.value);
        
        if (node->child_count <= ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
            node->static_children[child_index] = node->static_children[node->child_count - 1];
        }
        else if (node->child_count == ECS_ENTITY_HIERARCHY_STATIC_STORAGE + 1) {
            // Transition from allocated buffer to static storage
            memcpy(node->static_children, node->allocated_children, sizeof(node->static_children));
            allocator->Deallocate(node->allocated_children);
        }
        else {
            node->allocated_children[child_index] = node->allocated_children[node->child_count - 1];
        }
        node->child_count--;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::AddEntry(Entity parent, Entity child)
    {
        Node* node = (Node*)allocator->Allocate(sizeof(Node));
        node->entity = child;
        node->child_count = 0;
        // The parent can be set later on, since the pointer is stored in the hash table
        node_table.InsertDynamic(allocator, node, child);

        if (parent.value == Entity::Invalid()) {
            node->parent = nullptr;
            roots.Insert(child);
        }
        else {
            unsigned int parent_node_table_index = node_table.Find(parent);
            ECS_CRASH_CONDITION_RETURN_VOID(parent_node_table_index != -1, "EntityHierarchy: Trying to add a child to a parent that wasn't added to the hierarchy.");
            node->parent = node_table.GetValueFromIndex(parent_node_table_index);
            AddChildToNode(node->parent, node);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::CopyOther(const EntityHierarchy* other)
    {
        // Deallocate the current data and copy the new data.

        roots.Deallocate(allocator);
        HashTableCopy<false, false>(other->roots, roots, allocator);

        // To deallocate the node table, we can do that using the generic function, but to copy it,
        // We can't simply use the generic function due to the referencing nature of the nodes.
        HashTableDeallocate<true, false>(node_table, allocator);
        
        node_table.Initialize(allocator, other->node_table.GetCapacity());
        
        // Do the copy manually. In a first pass create all nodes, and then in a second pass
        // Assign the parents and then children, since all nodes have been created at that point.
        other->node_table.ForEachConst([&](const Node* other_node, Entity entity) {
            Node* node = (Node*)allocator->Allocate(sizeof(Node));
            node->entity = entity;
            node->parent = nullptr;
            node->child_count = 0;
            node_table.Insert(node, entity);
        });

        other->node_table.ForEachConst([&](const Node* other_node, Entity entity) {
            Node* node = node_table.GetValue(entity);
            
            if (other_node->parent != nullptr) {
                node->parent = node_table.GetValue(other_node->parent->entity);
            }
            if (other_node->child_count > 0) {
                // For assigning the children, we can simply memcpy, we need to look up the nodes inside the node table
                if (other_node->child_count <= ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                    for (unsigned int index = 0; index < other_node->child_count; index++) {
                        node->static_children[index] = node_table.GetValue(other_node->static_children[index]->entity);
                    }
                }
                else {
                    node->allocated_children = (Node**)allocator->Allocate(sizeof(*node->allocated_children) * other_node->child_count);
                    for (unsigned int index = 0; index < other_node->child_count; index++) {
                        node->allocated_children[index] = node_table.GetValue(other_node->allocated_children[index]->entity);
                    }
                }
            }
            node->child_count = other_node->child_count;
        });
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::ChangeParentTo(Node* new_parent, Node* child) {
        if (child->parent != nullptr) {
            RemoveChildFromNode(child->parent, child);
        }
        child->parent = new_parent;
        if (new_parent != nullptr) {
            AddChildToNode(new_parent, child);
        }
    }

    void EntityHierarchy::ChangeParent(Entity new_parent, Entity child)
    {
        unsigned int child_index = node_table.Find(child);
        ECS_CRASH_CONDITION(child_index != -1, "EntityHierarchy: Could not change the parent of entity {#}. It doesn't exist in the hierarchy.", child.value);

        unsigned int new_parent_index = node_table.Find(new_parent);
        ECS_CRASH_CONDITION(new_parent_index != -1, "EntityHierarchy: Could not change the parent of entity {#} to {#}. The new parent doesn't exist.", new_parent.value);

        ChangeParentTo(node_table.GetValueFromIndex(new_parent_index), node_table.GetValueFromIndex(child_index));
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool EntityHierarchy::Exists(Entity entity)
    {
        return node_table.Find(entity) != -1;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::RemoveEntry(Entity entity)
    {
        unsigned int node_index = node_table.Find(entity);
        ECS_CRASH_CONDITION(node_index != -1, "EntityHierarchy: The provided entity {#} doesn't belong to the hierarchy when trying to remove it.", entity.value);
        Node* node = node_table.GetValueFromIndex(node_index);

        // If it has a parent, remove it from it
        if (node->parent != nullptr) {
            RemoveChildFromNode(node->parent, node);
        }

        // Remove the node from the roots, if it is one
        unsigned int root_index = roots.Find(entity);
        if (root_index != -1) {
            roots.EraseFromIndex(root_index);
        }
        
        // Destroy the children as well
        Stream<Node*> children = node->ChildrenStream();
        for (size_t index = 0; index < children.size; index++) {
            RemoveEntry(children[index]->entity);
        }

        // At last, deallocate the node and remove it from the node table
        if (children.size > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
            allocator->Deallocate(children.buffer);
        }
        allocator->Deallocate(node);
        node_table.EraseFromIndex(node_index);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    Stream<Node*> EntityHierarchy::GetChildrenNodes(Entity parent) const {
        Node* parent_node = nullptr;
        if (node_table.TryGetValue(parent, parent_node)) {
            return parent_node->ChildrenStream();
        }
        return {};
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::GetChildren(Entity parent, CapacityStream<Entity>& children) const {
        Node* parent_node = nullptr;
        if (node_table.TryGetValue(parent, parent_node)) {
            Stream<Node*> parent_children = parent_node->ChildrenStream();
            children.AssertCapacity(parent_children.size);
            for (size_t index = 0; index < parent_children.size; index++) {
                children.Add(parent_children[index]->entity);
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    Stream<Entity> EntityHierarchy::GetChildren(Entity parent, AllocatorPolymorphic allocator) const {
        Node* parent_node = nullptr;
        if (node_table.TryGetValue(parent, parent_node)) {
            Stream<Node*> parent_children = parent_node->ChildrenStream();
            Stream<Entity> entity_children;
            entity_children.Initialize(allocator, parent_children.size);

            for (size_t index = 0; index < parent_children.size; index++) {
                entity_children[index] = parent_children[index]->entity;
            }
            return entity_children;
        }

        return {};
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::GetAllChildren(Entity entity, CapacityStream<Entity>& children) const {
        // Use an index to keep track of the entities which have been added. We don't need
        // External storage for this to work.
        size_t children_to_search = children.size;
        GetChildren(entity, children);

        while (children_to_search < children.size) {
            GetChildren(children[children_to_search++], children);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    Entity EntityHierarchy::GetParent(Entity entity) const
    {
        Entity parent;
        parent.value = Entity::Invalid();
        Node* node;
        if (node_table.TryGetValue(entity, node)) {
            // If it doesn't have a parent, return invalid.
            parent = node->parent != nullptr ? node->parent->entity : Entity::Invalid();
        }
        return parent;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    Entity EntityHierarchy::GetRootFromEntity(Entity entity) const
    {
        unsigned int node_index = node_table.Find(entity);
        if (node_index == -1) {
            return Entity::Invalid();
        }

        const Node* node = node_table.GetValueFromIndex(node_index);
        while (node->parent != nullptr)
        {
            node = node->parent;
        }
        return node->entity;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool EntityHierarchy::IsRoot(Entity entity) const
    {
        // Use the roots table, it is faster since there is one less indirection to read
        return roots.Find(entity) != -1;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void DefaultEntityHierarchyAllocator(MemoryManager* allocator, AllocatorPolymorphic global_memory)
    {
        new (allocator) MemoryManager(ENTITY_HIERARCHY_ALLOCATOR_SIZE, ENTITY_HIERARCHY_ALLOCATOR_CHUNKS, ENTITY_HIERARCHY_BACKUP_ALLOCATOR_SIZE, global_memory);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool SerializeEntityHierarchy(const EntityHierarchy* hierarchy, WriteInstrument* write_instrument)
    {
        // Write the header first
        SerializeEntityHierarchyHeader header;
        header.version = SERIALIZE_VERSION;
        header.node_count = hierarchy->node_table.GetCount();
        header.root_count = hierarchy->roots.GetCount();

        if (!write_instrument->Write(&header)) {
            return false;
        }

        // Write the nodes now. The root table does not have to be serialized, it can be deduced from the nodes themselves.
        // Before each node write the header, then the children entities. This design allows for not using temporary buffers upon
        // Deserialization, since they could become consistent
        if (hierarchy->node_table.ForEachConst<true>([&](const Node* node, Entity entity) {
            SerializeNodeHeader node_header;
            node_header.child_count = node->child_count;
            node_header.parent_entity = node->parent == nullptr ? Entity::Invalid() : node->parent->entity;
            node_header.entity = entity;

            if (!write_instrument->Write(&node_header)) {
                return true;
            }

            // Write the children now
            Stream<Node*> child_nodes = node->ChildrenStream();
            for (size_t index = 0; index < child_nodes.size; index++) {
                if (!write_instrument->Write(&child_nodes[index]->entity)) {
                    return true;
                }
            }

            return false;
        })) {
            return false;
        }

        // The parent table can be deduced from the children table
        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    // Returns false if the header contains invalid values
    static bool DeserializeEntityHierarchyPostHeaderOps(EntityHierarchy* hierarchy, const SerializeEntityHierarchyHeader* header) {
        if (header->version != SERIALIZE_VERSION) {
            return false;
        }

        if (hierarchy->node_table.GetCapacity() != 0) {
            hierarchy->node_table.Deallocate(hierarchy->allocator);
        }

        if (hierarchy->roots.GetCapacity() != 0) {
            hierarchy->roots.Deallocate(hierarchy->allocator);
        }

        if (header->node_count > 0) {
            hierarchy->node_table.Initialize(hierarchy->allocator, HashTablePowerOfTwoCapacityForElements(header->node_count));
        }
        else {
            hierarchy->node_table.Reset();
        }

        if (header->root_count > 0) {
            hierarchy->roots.Initialize(hierarchy->allocator, HashTablePowerOfTwoCapacityForElements(header->root_count));
        }
        else {
            hierarchy->roots.Reset();
        }

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool DeserializeEntityHierarchy(EntityHierarchy* hierarchy, ReadInstrument* read_instrument)
    {
        // Don't allow size determination instruments for now
        if (read_instrument->IsSizeDetermination()) {
            return false;
        }

        // Firstly read the header
        SerializeEntityHierarchyHeader header;
        if (!read_instrument->ReadAlways(&header)) {
            return false;
        }
       
        if (!DeserializeEntityHierarchyPostHeaderOps(hierarchy, &header)) {
            return false;
        }

        // Read the nodes now. We will have to do a separate prepass later on to patch some references.
        for (unsigned int index = 0; index < header.node_count; index++) {
            SerializeNodeHeader node_header;
            if (!read_instrument->Read(&node_header)) {
                return false;
            }

            // Create the nodes and insert them into the node table, and into the root table
            // If it doesn't have a parent. Because the node might not be available until
            // All nodes have been written, store in the parent pointer the actual parent entity
            // And do the node assignment later on.
            Node* node = (Node*)hierarchy->allocator->Allocate(sizeof(Node));
            node->entity = node_header.entity;
            node->parent = node_header.parent_entity == Entity::Invalid() ? nullptr : (Node*)node_header.parent_entity.value;
            node->child_count = node_header.child_count;

            // Allocate the children, if they exceed the static storage. Do the same as the parent pointer,
            // Instead of writing the nodes they belong, write into the node pointers the entity that is referencing
            if (node->child_count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                node->allocated_children = (Node**)hierarchy->allocator->Allocate(sizeof(Node*) * node->child_count);   
            }
            Stream<Node*> node_children = node->ChildrenStream();
            for (size_t child_index = 0; child_index < node_children.size; child_index++) {
                Entity child_entity;
                if (!read_instrument->Read(&child_entity)) {
                    return false;
                }
                node_children[child_index] = (Node*)child_entity.value;
            }

            hierarchy->node_table.Insert(node, node->entity);
            if (node->parent == nullptr) {
                hierarchy->roots.Insert(node->entity);
            }
        }

        // Patch the parent references of each node - both parents and children
        hierarchy->node_table.ForEachConst([&](Node* node, Entity entity) {
            Entity parent_entity = Entity((unsigned int)node->parent);
            if (parent_entity != Entity::Invalid()) {
                node->parent = hierarchy->node_table.GetValue(parent_entity);
            }

            Stream<Node*> children = node->ChildrenStream();
            for (size_t index = 0; index < children.size; index++) {
                Entity child_entity = Entity((unsigned int)children[index]);
                children[index] = hierarchy->node_table.GetValue(child_entity);
            }
        });

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    EntityHierarchyChangeSet DetermineEntityHierarchyChangeSet(
        const EntityHierarchy* previous_hierarchy,
        const EntityHierarchy* new_hierarchy,
        AllocatorPolymorphic temporary_allocator
    ) {
        EntityHierarchyChangeSet change_set;
        change_set.removed_entities.Initialize(temporary_allocator, 128);
        change_set.changed_parents.Initialize(temporary_allocator, 128);

        // For the removed entities, these are those that appeared in the previous hierarchy but not in the new hierarchy
        previous_hierarchy->node_table.ForEachConst([&](Node* node, Entity entity) {
            if (new_hierarchy->node_table.Find(entity) == -1) {
                change_set.removed_entities.Add(entity);
            }
        });

        // The additions and changes can be determined by iterating over the new table and looking up the previous entry.
        new_hierarchy->node_table.ForEachConst([&](Node* node, Entity entity) {
            Node* previous_node = nullptr;
            bool add_entry = false;
            if (previous_hierarchy->node_table.TryGetValue(entity, previous_node)) {
                if ((previous_node->parent == nullptr && node->parent != nullptr) || (previous_node->parent != nullptr && node->parent == nullptr)) {
                    add_entry = true;
                }
                else if (previous_node->parent != nullptr && node->parent != nullptr) {
                    if (previous_node->parent->entity != node->parent->entity) {
                        add_entry = true;
                    }
                }
            }
            else {
                // New addition, add it
                add_entry = true;
            }

            if (add_entry) {
                change_set.changed_parents.Add({ entity, node->parent != nullptr ? node->parent->entity : Entity::Invalid() });
            }
        });

        return change_set;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void ApplyEntityHierarchyChangeSet(EntityHierarchy* hierarchy, const EntityHierarchyChangeSet& change_set) {
        // Start by removing the roots.
        for (unsigned int index = 0; index < change_set.removed_entities.size; index++) {
            hierarchy->RemoveEntry(change_set.removed_entities[index]);
        }

        // Change the parents or create the new entities
        for (unsigned int index = 0; index < change_set.changed_parents.size; index++) {
            if (change_set.changed_parents[index].parent != Entity::Invalid() && hierarchy->node_table.TryGetValuePtr(change_set.changed_parents[index].parent) == nullptr) {
                // Create the parent node as a root for the time being
                hierarchy->AddEntry(Entity::Invalid(), change_set.changed_parents[index].parent);
            }

            if (hierarchy->node_table.TryGetValuePtr(change_set.changed_parents[index].child) == nullptr) {
                // Add this new entry
                hierarchy->AddEntry(change_set.changed_parents[index].parent, change_set.changed_parents[index].child);
            }
            else {
                // Call the change parent function
                hierarchy->ChangeParent(change_set.changed_parents[index].parent, change_set.changed_parents[index].child);
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    BFSEntityHierarchyIterator GetEntityHierarchyBFSIterator(const EntityHierarchy* hierarchy, AllocatorPolymorphic allocator)
    {
        if (allocator.allocator == nullptr) {
            allocator = hierarchy->allocator;
        }
        return BFSEntityHierarchyIterator(allocator, { hierarchy }, hierarchy->node_table.GetCount());
    }

    DFSEntityHierarchyIterator GetEntityHierarchyDFSIterator(const EntityHierarchy* hierarchy, AllocatorPolymorphic allocator)
    {
        if (allocator.allocator == nullptr) {
            allocator = hierarchy->allocator;
        }
        return DFSEntityHierarchyIterator(allocator, { hierarchy }, hierarchy->node_table.GetCount());
    }

    // -----------------------------------------------------------------------------------------------------------------------------

}