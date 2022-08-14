#include "ecspch.h"
#include "EntityHierarchy.h"
#include "../Utilities/Crash.h"
#include "../Utilities/Serialization/SerializationHelpers.h"

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
    };

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
                // Need to reallocate the buffer
                children->count++;
                Entity* new_allocation = (Entity*)hierarchy->allocator->Allocate(sizeof(Entity) * children->count);
                memcpy(new_allocation, children->entities, sizeof(Entity) * (children->count - 1));
                hierarchy->allocator->Deallocate(children->entities);

                children->entities = new_allocation;
                children->entities[children->count - 1] = child;
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
                InsertIntoDynamicTable(hierarchy->children_table, hierarchy->allocator, children, parent);
            }
            else {
                unsigned int stream_index = hierarchy->roots.Add(parent);
                EntityHierarchy::Children children;
                children.is_root = 1;
                children.root_index = stream_index;
                children.count = 1;
                children.static_children[0] = child;
                InsertIntoDynamicTable(hierarchy->children_table, hierarchy->allocator, children, parent);
            }
        }
    }

    void EntityHierarchy::AddEntry(Entity parent, Entity child)
    {
        AddEntryImplementation(this, parent, child);

        // Insert the child into the parent table aswell
        InsertIntoDynamicTable(parent_table, allocator, parent, child);
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void EntityHierarchy::CopyOther(const EntityHierarchy* other)
    {
        // The roots can be just memcpy'ed
        roots.FreeBuffer();
        roots.Initialize(GetAllocatorPolymorphic(allocator), other->roots.size);

        other->roots.CopyTo(roots.buffer);

        // Copy the parent table - this can be blitted
        const void* allocated_buffer = parent_table.GetAllocatedBuffer();
        if (allocated_buffer != nullptr) {
            allocator->Deallocate(allocated_buffer);
        }

        parent_table.Copy(GetAllocatorPolymorphic(allocator), &other->parent_table);

        // The children cannot be memcpy'ed because of the children allocations
        allocated_buffer = children_table.GetAllocatedBuffer();
        if (allocated_buffer != nullptr) {
            allocator->Deallocate(allocated_buffer);
        }

        size_t blit_size = other->children_table.MemoryOf(other->children_table.GetCapacity());
        void* allocation = allocator->Allocate(blit_size);
        children_table.SetBuffers(allocation, other->children_table.GetCapacity());

        // Blit the entire table, and then resolve the allocated pieces
        memcpy(allocation, other->children_table.GetAllocatedBuffer(), blit_size);

        other->children_table.ForEachIndexConst([&](unsigned int index) {
            Children other_children = other->children_table.GetValueFromIndex(index);

            Children* current_children = children_table.GetValuePtrFromIndex(index);
            current_children->count = other_children.count;
            current_children->is_root = other_children.is_root;
            current_children->root_index = other_children.root_index;
            if (other_children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                // This is allocated from the allocator - must do the same
                void* children_allocation = allocator->Allocate(sizeof(Entity) * other_children.count);
                memcpy(children_allocation, other_children.entities, sizeof(Entity) * other_children.count);

                current_children->entities = (Entity*)children_allocation;
            }
            else {
                current_children->entities = nullptr;
                memcpy(current_children->static_children, other_children.static_children, sizeof(Entity) * other_children.count);
            }
        });
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

    bool SerializeEntityHierarchy(const EntityHierarchy* hierarchy, ECS_FILE_HANDLE file)
    {
        // Write the header first
        SerializeEntityHierarchyHeader header;
        header.version = SERIALIZE_VERSION;
        header.root_count = hierarchy->roots.size;
        header.parent_count = hierarchy->parent_table.GetCount();
        header.children_count = hierarchy->children_table.GetCount();

        bool success = WriteFile(file, { &header, sizeof(header) });
        if (!success) {
            return false;
        }

        // Now write the roots
        success = WriteFile(file, { hierarchy->roots.buffer, hierarchy->roots.size });
        if (!success) {
            return false;
        }

        // Now the children table
        void* temp_buffer = ECS_STACK_ALLOC(sizeof(unsigned char) * ECS_KB * 64);
        uintptr_t temp_ptr = (uintptr_t)temp_buffer;

        hierarchy->children_table.ForEachConst([&](const auto children, const auto parent) {
            temp_ptr = (uintptr_t)temp_buffer;

            Write<true>(&temp_ptr, &parent, sizeof(Entity));
            // The 32 bits with the count, root index and is_root
            Write<true>(&temp_ptr, function::OffsetPointer(&children.padding, sizeof(unsigned int)), sizeof(unsigned int));
            Write<true>(&temp_ptr, children.Entities(), sizeof(Entity) * children.count);

            success = WriteFile(file, { temp_buffer, temp_ptr - (uintptr_t)temp_buffer });
            if (!success) {
                return false;
            }
        });

        // The parent table can be deduced from the children table
        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    void SerializeEntityHierarchy(const EntityHierarchy* hierarchy, uintptr_t* ptr)
    {
        // Write the header first
        SerializeEntityHierarchyHeader header;
        header.version = SERIALIZE_VERSION;
        header.root_count = hierarchy->roots.size;
        header.parent_count = hierarchy->parent_table.GetCount();
        header.children_count = hierarchy->children_table.GetCount();

        Write<true>(ptr, &header, sizeof(header));

        // Now write the roots
        Write<true>(ptr, hierarchy->roots.buffer, hierarchy->roots.size);

        // Now the children table
        unsigned int children_capacity = hierarchy->children_table.GetExtendedCapacity();
        hierarchy->children_table.ForEachConst([&](const auto children, const auto parent) {
            Write<true>(ptr, &parent, sizeof(Entity));
            Write<true>(ptr, function::OffsetPointer(&children.padding, sizeof(unsigned int)), sizeof(unsigned int));
            Write<true>(ptr, children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? children.entities : children.static_children, sizeof(Entity) * children.count);
        });
        // The parent table can be deduced from the children table
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    size_t SerializeEntityHierarchySize(const EntityHierarchy* hierarchy)
    {
        // The roots need to be written
        size_t size = sizeof(Entity) * hierarchy->roots.size + sizeof(SerializeEntityHierarchyHeader);

        // First determine how many entries in the children are and how much memory they require
        unsigned int children_capacity = hierarchy->children_table.GetExtendedCapacity();
        hierarchy->children_table.ForEachConst([&](const auto children, const auto parent) {
            // The entities must be written aswell. The final entity represents the key which will be used
            // when reinserting the values back on deserialization
            // The unsigned int contains all the necessary extra data
            size += children.count * sizeof(Entity) + sizeof(unsigned int) + sizeof(Entity);
        });

        // The parent table can be deduced from the children table
        return size;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool DeserializeEntityHierarchy(EntityHierarchy* hierarchy, ECS_FILE_HANDLE file)
    {
        // Firstly read the header
        SerializeEntityHierarchyHeader header;
        bool success = ReadFile(file, { &header, sizeof(header) });
        if (!success) {
            return false;
        }

        if (header.version != SERIALIZE_VERSION || header.root_count > header.children_count) {
            return false;
        }

        if (header.root_count > ECS_MB_10 || header.children_count > ECS_MB_10 || header.parent_count > ECS_MB_10) {
            return false;
        }

        if (hierarchy->children_table.GetCapacity() != 0) {
            hierarchy->allocator->Deallocate(hierarchy->children_table.GetAllocatedBuffer());
        }

        if (hierarchy->parent_table.GetCapacity() != 0) {
            hierarchy->allocator->Deallocate(hierarchy->parent_table.GetAllocatedBuffer());
        }

        if (header.children_count > 0 || header.parent_count > 0) {
            unsigned int power_of_two_children_table_size = function::PowerOfTwoGreater((float)header.children_count * 100.0f / ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR + 1).x;
            unsigned int power_of_two_parent_table_size = function::PowerOfTwoGreater((float)header.parent_count * 100.0f / ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR + 1).x;
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

        // Read the roots now
        hierarchy->roots.Resize(header.root_count);
        success = ReadFile(file, { hierarchy->roots.buffer, sizeof(Entity) * header.root_count });
        hierarchy->roots.size = header.root_count;
        if (!success) {
            return false;
        }

        void* temp_buffer = ECS_STACK_ALLOC(sizeof(char) * ECS_KB * 64);
        uintptr_t temp_ptr = (uintptr_t)temp_buffer;

        // Read the children table now
        for (unsigned int index = 0; index < header.children_count; index++) {
            EntityHierarchy::Children children;
            success = ReadFile(file, { &children.padding, sizeof(unsigned int) + sizeof(Entity) });
            if (!success) {
                return false;
            }
            Entity key_entity = { children.padding };

            if (children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                Entity* children_allocation = (Entity*)hierarchy->allocator->Allocate(sizeof(Entity) * children.count);
                success = ReadFile(file, { children_allocation, sizeof(Entity) * children.count });
                children.entities = children_allocation;
            }
            else {
                success = ReadFile(file, { children.static_children, sizeof(Entity) * children.count });
            }
            if (!success) {
                return false;
            }

            // Insert the value into the hash table now
            hierarchy->children_table.Insert(children, key_entity);

            // Iterate through the list of children and insert them into the parent table aswell
            const Entity* children_entities = children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? children.entities : children.static_children;
            for (unsigned int child_index = 0; child_index < children.count; child_index++) {
                hierarchy->parent_table.Insert(key_entity, children_entities[child_index]);
            }
        }

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    bool DeserializeEntityHierarchy(EntityHierarchy* hierarchy, uintptr_t* ptr)
    {
        // Firstly read the header
        SerializeEntityHierarchyHeader header;
        Read<true>(ptr, &header, sizeof(header));
        if (header.version != SERIALIZE_VERSION || header.root_count > header.children_count) {
            return false;
        }

        if (header.root_count > ECS_MB_10 || header.children_count > ECS_MB_10 || header.parent_count > ECS_MB_10) {
            return false;
        }

        if (hierarchy->children_table.GetCapacity() != 0) {
            hierarchy->allocator->Deallocate(hierarchy->children_table.GetAllocatedBuffer());
        }

        if (hierarchy->parent_table.GetCapacity() != 0) {
            hierarchy->allocator->Deallocate(hierarchy->parent_table.GetAllocatedBuffer());
        }

        if (header.children_count > 0 || header.parent_count > 0) {
            unsigned int power_of_two_children_table_size = function::PowerOfTwoGreater((float)header.children_count * 100.0f / ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR + 1).x;
            unsigned int power_of_two_parent_table_size = function::PowerOfTwoGreater((float)header.parent_count * 100.0f / ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR + 1).x;
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

        // Read the roots now
        hierarchy->roots.Resize(header.root_count);
        Read<true>(ptr, hierarchy->roots.buffer, sizeof(Entity) * header.root_count);
        hierarchy->roots.size = header.root_count;

        // Read the children table now
        for (unsigned int index = 0; index < header.children_count; index++) {
            EntityHierarchy::Children children;
            Read<true>(ptr, function::OffsetPointer(&children.padding, sizeof(unsigned int)), sizeof(unsigned int));
            Entity key_entity;
            Read<true>(ptr, &key_entity, sizeof(key_entity));

            if (children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE) {
                Entity* children_allocation = (Entity*)hierarchy->allocator->Allocate(sizeof(Entity) * children.count);
                Read<true>(ptr, children_allocation, sizeof(Entity) * children.count);
                children.entities = children_allocation;
            }
            else {
                Read<true>(ptr, children.static_children, sizeof(Entity) * children.count);
            }

            // Insert the value into the hash table now
            hierarchy->children_table.Insert(children, key_entity);

            // Iterate through the list of children and insert them into the parent table aswell
            const Entity* children_entities = children.count > ECS_ENTITY_HIERARCHY_STATIC_STORAGE ? children.entities : children.static_children;
            for (unsigned int child_index = 0; child_index < children.count; child_index++) {
                hierarchy->parent_table.Insert(key_entity, children_entities[child_index]);
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------

    size_t DeserializeEntityHierarchySize(uintptr_t ptr)
    {
        SerializeEntityHierarchyHeader header;
        Read<true>(&ptr, &header, sizeof(header));
        if (header.version != SERIALIZE_VERSION || header.root_count > header.children_count) {
            return -1;
        }

        if (header.root_count > ECS_MB_10 || header.children_count > ECS_MB_10 || header.parent_count > ECS_MB_10) {
            return -1;
        }

        ptr += sizeof(Entity) * header.root_count;
        size_t size = 0;

        for (unsigned int index = 0; index < header.children_count; index++) {
            unsigned int data_int;
            Read<true>(&ptr, &data_int, sizeof(data_int));

            EntityHierarchy::Children child;
            memcpy(function::OffsetPointer(&child.padding, sizeof(unsigned int)), &data_int, sizeof(data_int));
            ptr += sizeof(Entity) * (child.count + 1);
            size += sizeof(Entity) * child.count;
        }

        return size;
    }

    // -----------------------------------------------------------------------------------------------------------------------------

}