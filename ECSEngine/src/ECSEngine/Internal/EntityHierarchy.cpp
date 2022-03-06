#include "ecspch.h"
#include "EntityHierarchy.h"
#include "../Utilities/Crash.h"

#define ROOT_STARTING_SIZE ECS_KB
#define CHILDREN_TABLE_INITIAL_SIZE ECS_KB
#define PARENT_TABLE_INITIAL_SIZE ECS_KB * 4

#define ENTITY_HIERARCHY_ALLOCATOR_SIZE ECS_MB * 2
#define ENTITY_HIERARCHY_ALLOCATOR_CHUNKS 2048

namespace ECSEngine {

    void RemoveEntityFromParent(EntityHierarchy* hierarchy, Entity entity, Entity parent) {
        // Remove the entity from its parent and the entry from the parent table
        unsigned int parent_children_index = hierarchy->children_table.Find<false>(parent);
        ECS_CRASH_RETURN(parent_children_index != -1, "Could not the parent {#} children data when trying to remove entity {#} as its child.", parent.value, entity.value);
        EntityHierarchy::Children* children = hierarchy->children_table.GetValuePtrFromIndex(parent_children_index);
        if (children->count == 1) {
            // The node becomes childless, can be removed from the children table
            // Check to see if it is a root to remove it aswell from the root stream
            if (children->is_root) {
                unsigned int remove_index = children->root_index;
                hierarchy->roots.RemoveSwapBack(remove_index);
                if (remove_index != hierarchy->roots.size) {
                    Entity replaced_root = hierarchy->roots[remove_index];
                    EntityHierarchy::Children* replaced_root_children = hierarchy->children_table.GetValuePtr<false>(replaced_root);
                    ECS_CRASH_RETURN(replaced_root_children->is_root, "The swapped back root {#} does not have as its children data the is_root flag set.", replaced_root.value);
                    replaced_root_children->root_index = remove_index;
                }
            }

            // Remove the entry now
            hierarchy->children_table.EraseFromIndex(parent_children_index);
        }
        else {
            Entity* entities_to_search = children->count <= ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? children->static_children : children->entities;
            children->count--;
            for (size_t index = 0; index < children->count; index++) {
                if (entities_to_search[index] == entity) {
                    entities_to_search[index] = entities_to_search[children->count];
                    break;
                }
            }

            // Move from pointer to static storage
            if (children->count == ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                memcpy(children->static_children, entities_to_search, sizeof(Entity) * children->count);
                // Deallocate the pointer
                hierarchy->allocator->Deallocate(entities_to_search);
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    EntityHierarchy::EntityHierarchy(MemoryManager* memory_manager, unsigned int root_initial_size, unsigned int children_table_initial_size, unsigned int parent_table_initial_size)
    {
        root_initial_size = root_initial_size == -1 ? ROOT_STARTING_SIZE : root_initial_size;
        children_table_initial_size = children_table_initial_size == -1 ? CHILDREN_TABLE_INITIAL_SIZE : children_table_initial_size;
        parent_table_initial_size = parent_table_initial_size == -1 ? PARENT_TABLE_INITIAL_SIZE : parent_table_initial_size;

        allocator = memory_manager;
        roots.Initialize(GetAllocatorPolymorphic(memory_manager), root_initial_size);
        children_table.Initialize(memory_manager, children_table_initial_size);
        parent_table.Initialize(memory_manager, parent_table_initial_size);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    // The only difference between this and the member function is that this one does not update the children table
    // It can be used inside the ChangeParent for example
    void AddEntryImplementation(EntityHierarchy* hierarchy, Entity parent, Entity child) {
        unsigned int children_index = hierarchy->children_table.Find<false>(parent);
        if (children_index != -1) {
            EntityHierarchy::Children* children = hierarchy->children_table.GetValuePtrFromIndex(children_index);
            if (children->count == ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                // Allocate a new buffer - from the allocator - the static storage has been exhausted
                Entity temp_entities[ECS_ENTITY_HIERARCHY_STATIC_STORAGE];
                for (size_t index = 0; index < ECS_ENTITY_HIERARCHY_STATIC_STORAGE; index++) {
                    temp_entities[index] = children->static_children[index];
                }
                children->entities = (Entity*)hierarchy->allocator->Allocate(sizeof(Entity) * (ECS_ENTITY_HIERARCHY_STATIC_STORAGE + 1), alignof(Entity));
                memcpy(children->entities, temp_entities, sizeof(Entity) * ECS_ENTITY_HIERARCHY_STATIC_STORAGE);
                children->entities[ECS_ENTITY_HIERARCHY_STATIC_STORAGE] = child;
                children->count = ECS_ENTITY_HIERARCHY_STATIC_STORAGE + 1;
            }
            else if (children->count < ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                children->static_children[children->count++] = child;
            }
            else {
                children->entities[children->count++] = child;
            }
        }
        else {
            unsigned int parents_parent_index = hierarchy->parent_table.Find<false>(parent);
            if (parents_parent_index != -1) {
                EntityHierarchy::Children children;
                children.is_root = 0;
                children.root_index = 0;
                children.count = 1;
                children.static_children[0] = child;
                InsertToDynamicTable(hierarchy->children_table, hierarchy->allocator, children, parent);
            }
            else {
                unsigned int stream_index = hierarchy->roots.Add(parent);
                EntityHierarchy::Children children;
                children.is_root = 1;
                children.root_index = stream_index;
                children.count = 1;
                children.static_children[0] = child;
                InsertToDynamicTable(hierarchy->children_table, hierarchy->allocator, children, parent);
            }
        }
    }

    void EntityHierarchy::AddEntry(Entity parent, Entity child)
    {
        AddEntryImplementation(this, parent, child);

        // Insert the child into the parent table aswell
        InsertToDynamicTable(parent_table, allocator, parent, child);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::ChangeParent(Entity new_parent, Entity child)
    {
        unsigned int parent_index = parent_table.Find<false>(child);
        ECS_CRASH_RETURN(parent_index == -1, "Could not change the parent of entity {#}. It doesn't have a parent.", child.value);

        Entity* current_parent = parent_table.GetValuePtrFromIndex(parent_index);
        RemoveEntityFromParent(this, child, *current_parent);

        *current_parent = child;

        // Add the child to the parent's list
        AddEntryImplementation(this, new_parent, child);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::ChangeOrSetParent(Entity parent, Entity child)
    {
        unsigned int parent_index = parent_table.Find<false>(child);
        if (parent_index == -1) {
            AddEntry(parent, child);
        }
        else {
            ChangeParent(parent, child);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool EntityHierarchy::Exists(Entity entity)
    {
        return parent_table.Find<false>(entity) != -1 || children_table.Find<false>(entity) != -1;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::RemoveEntry(Entity entity)
    {
        // Determine if it is a root or not
        unsigned int children_index = children_table.Find<false>(entity);
        Children children = {};
        children.is_root = 0;
        if (children_index != -1) {
            // Destroy every children
            children = children_table.GetValueFromIndex(children_index);
            Stream<Entity> children_entities = { children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? children.entities : children.static_children, children.count};

            for (size_t index = 0; index < children_entities.size; index++) {
                RemoveEntry(children_entities[index]);
            }
        }

        unsigned int parent_index = parent_table.Find<false>(entity);
        // Do this only if it is not a root. If it is a root, it must be eliminated from the root
        if (parent_index != -1) {
            Entity parent = parent_table.GetValueFromIndex(parent_index);
            RemoveEntityFromParent(this, entity, parent);
        }
        else {
            ECS_CRASH_RETURN(!children.is_root, "Trying to remove root {#} from an entity hierarchy but it doesn't have children data.", entity.value);
            unsigned int swap_index = children.root_index;
            roots.RemoveSwapBack(swap_index);
            if (swap_index != roots.size) {
                Entity replaced_root = roots[swap_index];
                EntityHierarchy::Children* replaced_root_children = children_table.GetValuePtr<false>(replaced_root);
                ECS_CRASH_RETURN(replaced_root_children->is_root, "The swapped back root {#} does not have as its children data the is_root flag set.", replaced_root.value);
                replaced_root_children->root_index = swap_index;
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    Stream<Entity> EntityHierarchy::GetChildren(Entity parent) const {
        Children children;
        if (children_table.TryGetValue<false>(parent, children)) {
            return { children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? children.entities : children.static_children, children.count };
        }
        return { nullptr, 0 };
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::GetChildrenCopy(Entity parent, CapacityStream<Entity>& children) const
    {
        Stream<Entity> parent_children = GetChildren(parent);
        children.AddStreamSafe(parent_children);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::GetAllChildrenFromEntity(Entity entity, CapacityStream<Entity>& children) const
    {
        unsigned int initial_children_index = children_table.Find<false>(entity);
        if (initial_children_index == -1) {
            return;
        }

        Children initial_children = children_table.GetValueFromIndex(initial_children_index);
        Stream<Entity> initial_children_entities = { 
            initial_children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? initial_children.entities : initial_children.static_children,
            initial_children.count 
        };
        
        size_t first_children_to_search = children.size;
        ECS_CRASH_RETURN(initial_children_entities.size <= children.capacity - children.size, "The given capacity stream for retrieving entity's {#} children is too small.");
        children.AddStream(initial_children_entities);

        while (first_children_to_search != children.size) {
            size_t loop_children_size = children.size;
            for (size_t index = first_children_to_search; index < loop_children_size; index++) {
                unsigned int current_index = children_table.Find<false>(children[index]);
                if (current_index != -1) {
                    Children current_children = children_table.GetValueFromIndex(current_index);
                    Stream<Entity> current_children_entities = { 
                        current_children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? current_children.entities : current_children.static_children, 
                        current_children.count 
                    };
                    ECS_CRASH_RETURN(current_children_entities.size <= children.capacity - children.size, "The given capacity stream for retrieving entity's {#} children is too small.");
                    children.AddStream(current_children_entities);
                }
            }

            first_children_to_search = loop_children_size;
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    Entity EntityHierarchy::GetParent(Entity entity) const
    {
        Entity parent;
        parent.value = -1;
        parent_table.TryGetValue<false>(entity, parent);
        return parent;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    Entity EntityHierarchy::GetRootFromChildren(Entity entity) const
    {
        Entity parent;
        parent.value = -1;
        while (parent_table.TryGetValue<false>(entity, parent)) {
            entity = parent;
        }

        // There was no parent, either it is already the root or it doesn't exist
        if (parent.value == -1) {
            // The entity does not have children - it doesn't exist at all
            if (children_table.Find<false>(entity) == -1) {
                return parent;
            }
            else {
                return entity;
            }
        }

        return parent;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    MemoryManager DefaultEntityHierarchyAllocator(GlobalMemoryManager* global_memory)
    {
        return MemoryManager(ENTITY_HIERARCHY_ALLOCATOR_SIZE, ENTITY_HIERARCHY_ALLOCATOR_CHUNKS, ENTITY_HIERARCHY_ALLOCATOR_SIZE, global_memory);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

}