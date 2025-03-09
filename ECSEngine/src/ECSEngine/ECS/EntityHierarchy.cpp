#include "ecspch.h"
#include "EntityHierarchy.h"
#include "../Utilities/Crash.h"
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "../Utilities/ReaderWriterInterface.h"

#define ROOT_STARTING_SIZE ECS_KB
#define CHILDREN_TABLE_INITIAL_SIZE ECS_KB
#define PARENT_TABLE_INITIAL_SIZE ECS_KB * 4

#define ENTITY_HIERARCHY_ALLOCATOR_SIZE ECS_MB * 2
#define ENTITY_HIERARCHY_ALLOCATOR_CHUNKS 2048

#define SERIALIZE_VERSION 1

namespace ECSEngine {
    
    struct SerializeEntityHierarchyHeader {
        unsigned int version;
        unsigned int root_count;
        unsigned int children_count;
        unsigned int parent_count;

        // The number of entities serialized contained in the children table
        // such that on deserialization they can be read in bulk
        unsigned int children_data_size;
    };

    static void RemoveEntityFromParent(EntityHierarchy* hierarchy, Entity entity, Entity parent) {
        // Remove the entity from its parent and the entry from the parent table
        unsigned int parent_children_index = hierarchy->children_table.Find(parent);
        ECS_CRASH_CONDITION(parent_children_index != -1, "EntityHierarchy: Could not find the parent's {#} children data when trying to remove entity {#} as its child.", parent.value, entity.value);
        EntityHierarchy::Children* children = hierarchy->children_table.GetValuePtrFromIndex(parent_children_index);
        if (children->count == 1) {
            // The node becomes childless, can be removed from the children table
            hierarchy->children_table.EraseFromIndex(parent_children_index);
        }
        else {
            Entity* entities_to_search = children->Entities();
            unsigned int before_count = children->count;
            for (size_t index = 0; index < children->count; index++) {
                if (entities_to_search[index] == entity) {
                    children->count--;
                    entities_to_search[index] = entities_to_search[children->count];

                    // Move from pointer to static storage
                    if (children->count == ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                        memcpy(children->static_children, entities_to_search, sizeof(Entity) * children->count);
                        // Deallocate the pointer
                        hierarchy->allocator->Deallocate(entities_to_search);
                    }
                    else if (children->count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                        // The buffer must be relocated in order to keep up with the count
                        Entity* new_allocation = (Entity*)hierarchy->allocator->Allocate(sizeof(Entity) * children->count);
                        memcpy(new_allocation, entities_to_search, sizeof(Entity) * children->count);
                        hierarchy->allocator->Deallocate(entities_to_search);

                        children->entities = new_allocation;
                    }
                    break;
                }
            }
            ECS_CRASH_CONDITION(before_count > children->count, "EntityHierarchy: Could not find the child {#} inside the parent's {#} children data. The hierarchy is corrupted.", parent.value, entity.value);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    EntityHierarchy::EntityHierarchy(MemoryManager* memory_manager, unsigned int root_initial_size, unsigned int children_table_initial_size, unsigned int parent_table_initial_size)
    {
        root_initial_size = root_initial_size == -1 ? ROOT_STARTING_SIZE : root_initial_size;
        children_table_initial_size = children_table_initial_size == -1 ? CHILDREN_TABLE_INITIAL_SIZE : children_table_initial_size;
        parent_table_initial_size = parent_table_initial_size == -1 ? PARENT_TABLE_INITIAL_SIZE : parent_table_initial_size;

        allocator = memory_manager;
        roots.Initialize(memory_manager, root_initial_size);
        children_table.Initialize(memory_manager, children_table_initial_size);
        parent_table.Initialize(memory_manager, parent_table_initial_size);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::AddEntry(Entity parent, Entity child)
    {
        if (parent.value == -1) {
            roots.Add(child);
            parent_table.InsertDynamic(allocator, parent, child);
            return;
        }

        unsigned int children_index = children_table.Find(parent);
        if (children_index != -1) {
            EntityHierarchy::Children* children = children_table.GetValuePtrFromIndex(children_index);
            if (children->count == ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                // Allocate a new buffer - from the allocator - the static storage has been exhausted
                Entity temp_entities[ECS_ENTITY_HIERARCHY_STATIC_STORAGE];
                for (size_t index = 0; index < ECS_ENTITY_HIERARCHY_STATIC_STORAGE; index++) {
                    temp_entities[index] = children->static_children[index];
                }
                children->entities = (Entity*)allocator->Allocate(sizeof(Entity) * (ECS_ENTITY_HIERARCHY_STATIC_STORAGE + 1), alignof(Entity));
                memcpy(children->entities, temp_entities, sizeof(Entity) * ECS_ENTITY_HIERARCHY_STATIC_STORAGE);
                children->entities[ECS_ENTITY_HIERARCHY_STATIC_STORAGE] = child;
                children->count = ECS_ENTITY_HIERARCHY_STATIC_STORAGE + 1;
            }
            else if (children->count < ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                children->static_children[children->count++] = child;
            }
            else {
                // Need to reallocate the buffer
                children->count++;
                Entity* new_allocation = (Entity*)allocator->Allocate(sizeof(Entity) * children->count);
                memcpy(new_allocation, children->entities, sizeof(Entity) * (children->count - 1));
                allocator->Deallocate(children->entities);

                children->entities = new_allocation;
                children->entities[children->count - 1] = child;
            }
        }
        else {
            EntityHierarchy::Children children;
            children.count = 1;
            children.static_children[0] = child;

            children_table.InsertDynamic(allocator, children, parent);
            unsigned int parent_index = parent_table.Find(parent);
            if (parent_index == -1) {
                // It doesn't exist, so add it to the parent table and root buffer
                roots.Add(parent);
                Entity root_parent = { (unsigned int)-1 };
                parent_table.InsertDynamic(allocator, root_parent, parent);
            }
        }

        unsigned int child_parent_index = parent_table.Find(child);
        if (child_parent_index == -1) {
            parent_table.InsertDynamic(allocator, parent, child);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::CopyOther(const EntityHierarchy* other)
    {
        // The roots can be just memcpy'ed
        roots.FreeBuffer();
        roots.Initialize(allocator, other->roots.size);

        roots.CopyOther(other->roots);

        // Copy the parent table - this can be blitted
        const void* allocated_buffer = parent_table.GetAllocatedBuffer();
        if (allocated_buffer != nullptr) {
            allocator->Deallocate(allocated_buffer);
        }
        parent_table.Copy(allocator, &other->parent_table);

        // The children cannot be memcpy'ed because of the children allocations
        allocated_buffer = children_table.GetAllocatedBuffer();
        if (allocated_buffer != nullptr) {
            allocator->Deallocate(allocated_buffer);
        }
        children_table.Copy(allocator, &other->children_table);

        other->children_table.ForEachIndexConst([&](unsigned int index) {
            Children other_children = other->children_table.GetValueFromIndex(index);

            Children* current_children = children_table.GetValuePtrFromIndex(index);
            current_children->count = other_children.count;
            if (other_children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                // This is allocated from the allocator - must do the same
                void* children_allocation = allocator->Allocate(sizeof(Entity) * other_children.count);
                memcpy(children_allocation, other_children.entities, sizeof(Entity) * other_children.count);

                current_children->entities = (Entity*)children_allocation;
            }
        });
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void ChangeParentImpl(EntityHierarchy* hierarchy, Entity new_parent, Entity child, unsigned int old_parent_table_index) {
        // Can take a pointer to it since there are no modifications done on the parent table in the RemoveEntityFromParent function
        Entity* current_parent = hierarchy->parent_table.GetValuePtrFromIndex(old_parent_table_index);
        if (current_parent->value != -1) {
            RemoveEntityFromParent(hierarchy, child, *current_parent);
        }
        else {
            // It is a root. Need to remove it from the buffer
            unsigned int root_index = SearchBytes(hierarchy->roots.ToStream(), child);
            ECS_CRASH_CONDITION(root_index != -1, "EntityHierarchy: Fatal internal error. Parent table says {#} is a root but the root buffer doesn't contain it.", child.value);
            hierarchy->roots.RemoveSwapBack(root_index);
        }
        *current_parent = new_parent;

        // Add the child to the parent's list
        hierarchy->AddEntry(new_parent, child);
    }

    void EntityHierarchy::ChangeParent(Entity new_parent, Entity child)
    {
        unsigned int parent_index = parent_table.Find(child);
        ECS_CRASH_CONDITION(parent_index == -1, "EntityHierarchy: Could not change the parent of entity {#}. It doesn't exist in the hierarchy.", child.value);

        ChangeParentImpl(this, new_parent, child, parent_index);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::ChangeOrSetParent(Entity parent, Entity child)
    {
        unsigned int parent_index = parent_table.Find(child);
        if (parent_index == -1) {
            // The entity doesn't exist yet, insert it
            AddEntry(parent, child);
        }
        else {
            ChangeParentImpl(this, parent, child, parent_index);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool EntityHierarchy::Exists(Entity entity)
    {
        return parent_table.Find(entity) != -1;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::RemoveEntry(Entity entity)
    {
        ECS_CRASH_CONDITION(roots.size > 0, "EntityHierarchy: There are no entities inserted when trying to remove {#}.", entity.value);

        // Determine if it has children or not
        unsigned int children_index = children_table.Find(entity);
        if (children_index != -1) {
            // Destroy every children
            Children children = children_table.GetValueFromIndex(children_index);
            Stream<Entity> children_entities = { children.Entities(), children.count};

            for (size_t index = 0; index < children_entities.size; index++) {
                RemoveEntry(children_entities[index]);
            }
        }

        unsigned int parent_index = parent_table.Find(entity);
        ECS_CRASH_CONDITION(parent_index != -1, "EntityHierarchy: The entity {#} doesn't exist in the hierarchy when trying to remove it.", entity.value);
        Entity parent = parent_table.GetValueFromIndex(parent_index);
        if (parent.value == -1) {
            // It is a root, it must be eliminated from the
            unsigned int root_parent_index = SearchBytes(roots.ToStream(), entity);
            ECS_CRASH_CONDITION(root_parent_index != -1, "EntityHierarchy: The entity {#} doesn't exist in the root buffer but the parent table says it's a root.", entity.value);
            roots.RemoveSwapBack(root_parent_index);
            parent_table.EraseFromIndex(parent_index);
        }
        else {
            RemoveEntityFromParent(this, entity, parent);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    unsigned int EntityHierarchy::GetEntityCount() const
    {
        unsigned int count = 0;
        // Go through the children table and add the children counts in it
        // At the end add the root count
        children_table.ForEachConst([&](const Children& children, Entity parent) {
            count += children.count;
        });

        return count + roots.size;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    Stream<Entity> EntityHierarchy::GetChildren(Entity parent, Entity* static_storage) const {
        const Children* children = children_table.TryGetValuePtr(parent);
        if (children != nullptr) {
            if (children->IsPointer()) {
                return { children->entities, children->count };
            }
            else {
                if (static_storage != nullptr) {
                    memcpy(static_storage, children->static_children, children->count * sizeof(Entity));
                    return { static_storage, children->count };
                }
                else {
                    return { children->static_children, children->count };
                }
            }
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
        unsigned int initial_children_index = children_table.Find(entity);
        if (initial_children_index == -1) {
            return;
        }

        Children initial_children = children_table.GetValueFromIndex(initial_children_index);
        Stream<Entity> initial_children_entities = { 
            initial_children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? initial_children.entities : initial_children.static_children,
            initial_children.count 
        };
        
        size_t first_children_to_search = children.size;
        ECS_CRASH_CONDITION(initial_children_entities.size <= children.capacity - children.size, "EntityHierarchy: The given capacity stream for retrieving entity's {#} children is too small.");
        children.AddStream(initial_children_entities);

        while (first_children_to_search != children.size) {
            size_t loop_children_size = children.size;
            for (size_t index = first_children_to_search; index < loop_children_size; index++) {
                unsigned int current_index = children_table.Find(children[index]);
                if (current_index != -1) {
                    Children current_children = children_table.GetValueFromIndex(current_index);
                    Stream<Entity> current_children_entities = { 
                        current_children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? current_children.entities : current_children.static_children, 
                        current_children.count 
                    };
                    ECS_CRASH_CONDITION(current_children_entities.size <= children.capacity - children.size, "EntityHierarchy: The given capacity stream for retrieving entity's {#} children is too small.");
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
        parent_table.TryGetValue(entity, parent);
        return parent;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    Entity EntityHierarchy::GetRootFromChildren(Entity entity) const
    {
        Entity parent;
        parent.value = 0;
        parent_table.TryGetValue(entity, parent);
        while (parent.value != -1) {
            entity = parent;
            if (!parent_table.TryGetValue(entity, parent)) {
                break;
            }
        }

        return entity;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool EntityHierarchy::IsRoot(Entity entity) const
    {
        unsigned int entity_index = parent_table.Find(entity);
        if (entity_index == -1) {
            return false;
        }
        return parent_table.GetValueFromIndex(entity_index).value == -1;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    MemoryManager DefaultEntityHierarchyAllocator(AllocatorPolymorphic global_memory)
    {
        return MemoryManager(ENTITY_HIERARCHY_ALLOCATOR_SIZE, ENTITY_HIERARCHY_ALLOCATOR_CHUNKS, ENTITY_HIERARCHY_ALLOCATOR_SIZE, global_memory);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool SerializeEntityHierarchy(const EntityHierarchy* hierarchy, WriteInstrument* write_instrument)
    {
        // Write the header first
        SerializeEntityHierarchyHeader header;
        header.version = SERIALIZE_VERSION;
        header.root_count = hierarchy->roots.size;
        header.parent_count = hierarchy->parent_table.GetCount();
        header.children_count = hierarchy->children_table.GetCount();
        header.children_data_size = (sizeof(Entity) + sizeof(unsigned int)) * header.children_count;

        hierarchy->children_table.ForEachConst([&](const auto children, const auto parent) {
            header.children_data_size += children.count * sizeof(Entity);
        });

        if (!write_instrument->Write(&header)) {
            return false;
        }

        // Now write the roots
        if (!write_instrument->Write(hierarchy->roots.buffer, hierarchy->roots.CopySize())) {
            return false;
        }

        // Now the children table
        if (hierarchy->children_table.ForEachConst<true>([&](const auto children, const auto parent) {
            bool success = true;
            success &= write_instrument->Write(&parent, sizeof(Entity));
            success &= write_instrument->Write(&children.count, sizeof(unsigned int));
            success &= write_instrument->Write(children.Entities(), sizeof(Entity) * children.count);
            return !success;
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

        if (hierarchy->children_table.GetCapacity() != 0) {
            hierarchy->allocator->Deallocate(hierarchy->children_table.GetAllocatedBuffer());
        }

        if (hierarchy->parent_table.GetCapacity() != 0) {
            hierarchy->allocator->Deallocate(hierarchy->parent_table.GetAllocatedBuffer());
        }

        if (header->children_count > 0 || header->parent_count > 0) {
            unsigned int power_of_two_children_table_size = PowerOfTwoGreater(HashTableCapacityForElements(header->children_count));
            unsigned int power_of_two_parent_table_size = PowerOfTwoGreater(HashTableCapacityForElements(header->parent_count));
            size_t children_table_allocation_size = hierarchy->children_table.MemoryOf(power_of_two_children_table_size);
            size_t parent_table_allocation_size = hierarchy->parent_table.MemoryOf(power_of_two_parent_table_size);

            void* children_table_allocation = hierarchy->allocator->Allocate(children_table_allocation_size);
            void* parent_table_allocation = hierarchy->allocator->Allocate(parent_table_allocation_size);

            hierarchy->children_table.InitializeFromBuffer(children_table_allocation, power_of_two_children_table_size);
            hierarchy->parent_table.InitializeFromBuffer(parent_table_allocation, power_of_two_parent_table_size);
        }
        else {
            hierarchy->children_table.m_capacity = 0;
            hierarchy->parent_table.m_capacity = 0;
        }

        hierarchy->roots.Resize(header->root_count);
        return true;
    }
    
    // Returns true if the data is valid
    static bool DeserializeEntityHierarchyChildTable(EntityHierarchy* hierarchy, unsigned int children_count, ReadInstrument* read_instrument) {
        // Read the children table now
        for (unsigned int index = 0; index < children_count; index++) {
            EntityHierarchy::Children children;
            Entity current_parent;
            unsigned int count = 0;
            if (!read_instrument->Read(&current_parent)) {
                return false;
            }
            if (!read_instrument->Read(&count)) {
                return false;
            }
            children.count = count;

            Entity* children_entities = nullptr;
            if (count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                Entity* children_allocation = (Entity*)hierarchy->allocator->Allocate(sizeof(Entity) * children.count);
                children.entities = children_allocation;
                children_entities = children.entities;
            }
            else {
                children_entities = children.static_children;
            }
            if (!read_instrument->Read(children_entities, sizeof(Entity) * children.count)) {
                return false;
            }

            if (hierarchy->children_table.Find(current_parent) != -1) {
                return false;
            }
            // Insert the value into the hash table now
            hierarchy->children_table.Insert(children, current_parent);

            // Iterate through the list of children and insert them into the parent table as well
            for (unsigned int child_index = 0; child_index < children.count; child_index++) {
                if (hierarchy->parent_table.Find(children_entities[child_index]) != -1) {
                    return false;
                }
                hierarchy->parent_table.Insert(current_parent, children_entities[child_index]);
            }
        }

        // Now go through the roots and insert them into the parent table with a parent of -1
        Entity null_parent = { (unsigned int)-1 };
        for (unsigned int index = 0; index < hierarchy->roots.size; index++) {
            if (hierarchy->parent_table.Find(hierarchy->roots[index]) != -1) {
                return false;
            }
            hierarchy->parent_table.Insert(null_parent, hierarchy->roots[index]);
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

        if (!read_instrument->Read(hierarchy->roots.buffer, sizeof(Entity) * header.root_count)) {
            return false;
        }
        hierarchy->roots.size = header.root_count;

        return DeserializeEntityHierarchyChildTable(hierarchy, header.children_count, read_instrument);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    BFSEntityHierarchyIterator GetEntityHierarchyBFSIterator(const EntityHierarchy* hierarchy, AllocatorPolymorphic allocator)
    {
        if (allocator.allocator == nullptr) {
            allocator = hierarchy->allocator;
        }

        // Use a default capacity of the number of entries in the children table
        return BFSEntityHierarchyIterator(allocator, { hierarchy }, hierarchy->children_table.GetCount());
    }

    DFSEntityHierarchyIterator GetEntityHierarchyDFSIterator(const EntityHierarchy* hierarchy, AllocatorPolymorphic allocator)
    {
        if (allocator.allocator == nullptr) {
            allocator = hierarchy->allocator;
        }

        // Use a default capacity of the number of entries in the children table
        return DFSEntityHierarchyIterator(allocator, { hierarchy }, hierarchy->children_table.GetCount());
    }

    // -----------------------------------------------------------------------------------------------------------------------------

}