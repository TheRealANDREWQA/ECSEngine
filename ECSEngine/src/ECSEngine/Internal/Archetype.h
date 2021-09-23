#pragma once
#include "StaticArchetypeBase.h"
#include "ArchetypeBase.h"

namespace ECSEngine {

	template<typename ArchetypeType>
	class Archetype
	{
	public:
		Archetype(
			unsigned int max_base_archetypes,
			MemoryManager* memory_manager,
			const Stream<ComponentInfo>& unique_components,
			const Stream<ComponentInfo>& shared_components
		) : m_maximum_base_archetypes(max_base_archetypes), m_memory_manager(memory_manager)
		{
			void* allocation = memory_manager->Allocate(
				MemoryOf(max_base_archetypes, unique_components.size, shared_components.size),
				8
			);
			uintptr_t buffer = (uintptr_t)allocation;

			m_unique_components.buffer = (ComponentInfo*)allocation;
			m_unique_components.size = unique_components.size;
			memcpy(m_unique_components.buffer, unique_components.buffer, sizeof(ComponentInfo) * unique_components.size);

			buffer += sizeof(ComponentInfo) * unique_components.size;
			m_shared_components.buffer = (ComponentInfo*)buffer;
			m_shared_components.size = shared_components.size;
			memcpy(m_shared_components.buffer, shared_components.buffer, sizeof(ComponentInfo) * shared_components.size);

			buffer += sizeof(ComponentInfo) * shared_components.size;
			m_base_archetypes.buffer = (ArchetypeType*)buffer;
			m_base_archetypes.size = 0;

			buffer += sizeof(ArchetypeType) * max_base_archetypes;
			m_shared_component_subindex = (unsigned short*)buffer;

			m_default_descriptor.chunk_size = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_CHUNK_SIZE;
			m_default_descriptor.initial_chunk_count = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_INITIAL_CHUNK_COUNT;
			m_default_descriptor.max_chunk_count = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_CHUNK_COUNT;
			m_default_descriptor.max_sequence_count = ECS_DEFAULT_ARCHETYPE_BASE_DESCRIPTOR_SEQUENCE_COUNT;
		}

		Archetype(
			unsigned int max_base_archetypes,
			MemoryManager* memory_manager,
			const Stream<ComponentInfo>& unique_components,
			const Stream<ComponentInfo>& shared_components,
			ArchetypeBaseDescriptor descriptor
		) : m_default_descriptor(descriptor)
		{
			Archetype(max_base_archetypes, memory_manager, unique_components, shared_components);
		}

		Archetype& operator = (const Archetype& other) = default;

		Archetype& operator = (Archetype&& other) = default;

		bool CheckArchetypeQuery(const ArchetypeQuery* query) const {
			unsigned char unique_components[32];
			unsigned char shared_components[32];
			for (size_t index = 0; index < m_unique_components.size; index++) {
				unique_components[index + 1] = m_unique_components[index].index;
			}
			unique_components[0] = m_unique_components.size;
			for (size_t index = 0; index < m_shared_components.size; index++) {
				shared_components[index + 1] = m_shared_components[index].index;
			}
			shared_components[0] = m_shared_components.size;
			VectorComponentSignature unique_signature(unique_components);
			VectorComponentSignature shared_signature(shared_components);
			return unique_signature.HasComponents(query->unique) && unique_signature.HasComponents(query->shared);
		}

		bool CheckArchetypeQuery(const ArchetypeQueryExclude* query) const {
			unsigned char unique_components[32];
			unsigned char shared_components[32];
			for (size_t index = 0; index < m_unique_components.size; index++) {
				unique_components[index + 1] = m_unique_components[index].index;
			}
			unique_components[0] = m_unique_components.size;
			for (size_t index = 0; index < m_shared_components.size; index++) {
				shared_components[index + 1] = m_shared_components[index].index;
			}
			shared_components[0] = m_shared_components.size;
			VectorComponentSignature unique_signature(unique_components);
			VectorComponentSignature shared_signature(shared_components);
			// zero initialized
			VectorComponentSignature zero_signature;
			return unique_signature.HasComponents(query->unique) && unique_signature.ExcludesComponents(query->unique_excluded, zero_signature)
				&& shared_signature.HasComponents(query->shared) && shared_signature.ExcludesComponents(query->shared_excluded, zero_signature);
		}

		ArchetypeType* CreateBaseArchetype(const unsigned short* shared_subindices) {
			ECS_ASSERT(m_base_archetypes.size < m_maximum_base_archetypes, "Too many base archetypes!");
			m_base_archetypes[m_base_archetypes.size] = ArchetypeType(m_default_descriptor, m_unique_components, m_memory_manager);
			memcpy(
				m_shared_component_subindex + m_base_archetypes.size * m_shared_components.size,
				shared_subindices,
				sizeof(unsigned short) * m_shared_components.size
			);
			m_base_archetypes.size++;
			return m_base_archetypes.buffer + m_base_archetypes.size - 1;
		}

		ArchetypeType* CreateBaseArchetype(const unsigned char* shared_components, const unsigned short* shared_subindices) {
			ECS_ASSERT(m_base_archetypes.size < m_maximum_base_archetypes, "Too many base archetypes!");
			m_base_archetypes[m_base_archetypes.size] = ArchetypeType(m_default_descriptor, m_unique_components, m_memory_manager);
			size_t offset = m_base_archetypes.size * m_shared_components.size;
			for (size_t index = 0; index < m_shared_components.size; index++) {
				for (size_t subindex = 0; subindex < m_shared_components.size; subindex++) {
					if (shared_components[index] == m_shared_components[subindex].index) {
						m_shared_component_subindex[offset + subindex] = shared_subindices[index];
						break;
					}
				}
			}
			m_base_archetypes.size++;
			return m_base_archetypes.buffer + m_base_archetypes.size - 1;
		}

		ArchetypeType* CreateBaseArchetype(ArchetypeBaseDescriptor descriptor, const unsigned short* shared_subindices) {
			ECS_ASSERT(m_base_archetypes.size < m_maximum_base_archetypes, "Too many base archetypes!");
			m_base_archetypes[m_base_archetypes.size] = ArchetypeType(descriptor, m_unique_components, m_memory_manager);
			size_t offset = m_base_archetypes.size * m_shared_components.size;
			memcpy(
				m_shared_component_subindex + m_base_archetypes.size * m_shared_components.size,
				shared_subindices,
				sizeof(unsigned short) * m_shared_components.size
			);
			m_base_archetypes.size++;
			return m_base_archetypes.buffer + m_base_archetypes.size - 1;
		}

		ArchetypeType* CreateBaseArchetype(const unsigned char* shared_components, ArchetypeBaseDescriptor descriptor, const unsigned short* shared_subindices) {
			ECS_ASSERT(m_base_archetypes.size < m_maximum_base_archetypes, "Too many base archetypes!");
			m_base_archetypes[m_base_archetypes.size] = ArchetypeType(descriptor, m_unique_components, m_memory_manager);
			size_t offset = m_base_archetypes.size * m_shared_components.size;
			for (size_t index = 1; index <= m_shared_components.size; index++) {
				for (size_t subindex = 0; subindex < m_shared_components.size; subindex++) {
					if (shared_components[index] == m_shared_components[subindex].index) {
						m_shared_component_subindex[offset + subindex] = shared_subindices[index];
						break;
					}
				}
			}
			m_base_archetypes.size++;
			return m_base_archetypes.buffer + m_base_archetypes.size - 1;
		}

		// it will deallocate all of its base archetypes then itself
		void Deallocate() {
			for (size_t index = 0; index < m_base_archetypes.size; index++) {
				m_base_archetypes[index].Deallocate();
			}
			m_memory_manager->Deallocate(m_unique_components.buffer);
		}

		// it will deallocate that archetype's buffer and replace it with the last one
		void DestroyBase(unsigned int archetype_index) {
			m_base_archetypes[archetype_index].Deallocate();
			m_base_archetypes.size--;
			m_base_archetypes[archetype_index] = m_base_archetypes[m_base_archetypes.size];
		}

		ArchetypeType* FindArchetypeBase(const unsigned short* shared_component_subindices) const {
			for (size_t index = 0; index < m_base_archetypes.size; index++) {
				if (memcmp(
					m_shared_component_subindex + index * m_shared_components.size,
					shared_component_subindices,
					sizeof(unsigned short) * m_shared_components.size)
					) {
					return m_base_archetypes.buffer + index;
				}
			}
			return nullptr;
		}

		ArchetypeType* FindArchetypeBase(const unsigned short* shared_component_subindices, unsigned int& subindex) const {
			for (size_t index = 0; index < m_base_archetypes.size; index++) {
				if (memcmp(
					m_shared_component_subindex + index * m_shared_components.size,
					shared_component_subindices,
					sizeof(unsigned short) * m_shared_components.size)
					) {
					subindex = index;
					return m_base_archetypes.buffer + index;
				}
			}
			return nullptr;
		}

		ArchetypeType* FindArchetypeBase(const unsigned short* shared_component_subindices, const unsigned char* shared_components) const {
			unsigned char order[32];
			for (size_t parameter_index = 0; parameter_index < m_shared_components.size; parameter_index++) {
				for (size_t archetype_index = 0; archetype_index < m_shared_components.size; archetype_index++) {
					if (m_shared_components[archetype_index].index == shared_components[parameter_index]) {
						order[parameter_index] = archetype_index;
						break;
					}
				}
			}
			for (size_t index = 0; index < m_base_archetypes.size; index++) {
				size_t offset = index * m_shared_components.size;
				size_t subindex = 0;
				for (; subindex < m_shared_components.size; subindex++) {
					if (m_shared_component_subindex[order[subindex] + offset] != shared_component_subindices[subindex]) {
						break;
					}
				}
				if (subindex == m_shared_components.size) {
					return m_base_archetypes.buffer + index;
				}
			}
			return nullptr;
		}

		ArchetypeType* FindArchetypeBase(const unsigned short* shared_component_subindices, const unsigned char* shared_components, unsigned int& archetype_base_index) const {
			unsigned char order[32];
			for (size_t parameter_index = 0; parameter_index < m_shared_components.size; parameter_index++) {
				for (size_t archetype_index = 0; archetype_index < m_shared_components.size; archetype_index++) {
					if (m_shared_components[archetype_index].index == shared_components[parameter_index]) {
						order[parameter_index] = archetype_index;
						break;
					}
				}
			}
			for (size_t index = 0; index < m_base_archetypes.size; index++) {
				size_t offset = index * m_shared_components.size;
				size_t subindex = 0;
				for (; subindex < m_shared_components.size; subindex++) {
					if (m_shared_component_subindex[order[subindex] + offset] != shared_component_subindices[subindex]) {
						break;
					}
				}
				if (subindex == m_shared_components.size) {
					archetype_base_index = subindex;
					return m_base_archetypes.buffer + index;
				}
			}
			return nullptr;
		}

		ArchetypeType* GetArchetypeBase(unsigned int index) const {
			return m_base_archetypes.buffer + index;
		}

		void GetArchetypeSharedSubindices(unsigned int archetype_index, const unsigned char* order, unsigned short* indices) const {
			size_t offset = archetype_index * m_shared_components.size;
			for (size_t index = 1; index <= order[0]; index++) {
				for (size_t shared_index = 0; shared_index < m_shared_components.size; shared_index++) {
					if (m_shared_components[shared_index] == order[index]) {
						indices[index - 1] = m_shared_component_subindex[offset + shared_index];
						break;
					}
				}
			}
		}

		void GetArchetypeSharedSubindices(unsigned int archetype_index, unsigned short* indices) const {
			memcpy(
				indices,
				m_shared_component_subindex + archetype_index * m_shared_components.size,
				sizeof(unsigned short) * m_shared_components.size
			);
		}

		void GetArchetypeStream(Stream<ArchetypeType>& archetypes) const {
			archetypes = m_base_archetypes;
		}

		unsigned int GetArchetypeBaseCount() const {
			return m_base_archetypes.size;
		}

		void GetComponentInfo(Stream<ComponentInfo>& infos) const {
			infos = m_unique_components;
		}

		void GetSharedComponentInfo(Stream<ComponentInfo>& infos) const {
			infos = m_shared_components;
		}

		VectorComponentSignature GetComponentSignature() const {
			unsigned char unique_components[32];
			for (size_t index = 0; index < m_unique_components.size; index++) {
				unique_components[index + 1] = m_unique_components[index].index;
			}
			unique_components[0] = m_unique_components.size;
			return VectorComponentSignature(unique_components);
		}

		VectorComponentSignature GetSharedComponentSignature() const {
			unsigned char shared_components[32];
			for (size_t index = 0; index < m_shared_components.size; index++) {
				shared_components[index + 1] = m_shared_components[index].index;
			}
			shared_components[0] = m_shared_components.size;
			return VectorComponentSignature(shared_components);
		}

		size_t MemoryOf(unsigned int max_base_archetypes, unsigned int component_count, unsigned int shared_component_count) {
			return sizeof(ArchetypeType) * max_base_archetypes + sizeof(unsigned short) * max_base_archetypes 
				* shared_component_count + sizeof(ComponentInfo) * (component_count + shared_component_count) + 8;
		}

	private:
		Stream<ArchetypeType> m_base_archetypes;
		Stream<ComponentInfo> m_unique_components;
		Stream<ComponentInfo> m_shared_components;
		MemoryManager* m_memory_manager;
		unsigned short* m_shared_component_subindex;
		ArchetypeBaseDescriptor m_default_descriptor;
		unsigned int m_maximum_base_archetypes;
	};

}

