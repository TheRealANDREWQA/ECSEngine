#pragma once
#include "InternalStructures.h"

namespace ECSEngine {

	struct ECSENGINE_API VectorComponentSignature {
		ECS_INLINE VectorComponentSignature() : value(0) {}
		ECS_INLINE VectorComponentSignature(Vec16us _value) : value(_value) {}

		ECS_INLINE VectorComponentSignature(ComponentSignature signature) { ConvertComponents(signature); }

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(VectorComponentSignature);

		ECS_INLINE bool ECS_VECTORCALL operator == (VectorComponentSignature other) const {
			return horizontal_and(value == other.value);
		}

		ECS_INLINE operator Vec16us() {
			return value;
		}

		void ConvertComponents(ComponentSignature signature);

		void ConvertInstances(const SharedInstance* instances, unsigned char count);

		bool ECS_VECTORCALL HasComponents(VectorComponentSignature components) const;

		bool ECS_VECTORCALL ExcludesComponents(VectorComponentSignature components) const;

		// Find the indices of the components inside the current selection
		// A value of UCHAR_MAX signals that the component is missing.
		void ECS_VECTORCALL Find(VectorComponentSignature components, unsigned char* values);

		// Returns how many components it contains
		unsigned char Count() const;

		void InitializeSharedComponent(SharedComponentSignature shared_signature);

		void InitializeSharedInstances(SharedComponentSignature shared_signature);

		// When no components are desired
		void Initialize();

		ComponentSignature ToNormalSignature(Component* components) const;

		// Returns how many components it can store at a maximum
		static size_t ComponentCount() {
			return sizeof(VectorComponentSignature) / sizeof(Component);
		}

		static size_t InstanceCount() {
			return sizeof(VectorComponentSignature) / sizeof(SharedInstance);
		}

		Vec16us value;
	};

	ECSENGINE_API bool ECS_VECTORCALL VectorComponentSignatureHasComponents(VectorComponentSignature components, VectorComponentSignature query, unsigned char query_count);

	ECSENGINE_API bool ECS_VECTORCALL VectorComponentSignatureExcludesComponents(VectorComponentSignature components, VectorComponentSignature query, unsigned char query_count);

	ECSENGINE_API bool ECS_VECTORCALL SharedComponentSignatureHasInstances(
		VectorComponentSignature archetype_components,
		VectorComponentSignature archetype_instances,
		VectorComponentSignature query_components,
		VectorComponentSignature query_instances
	);

	struct ECSENGINE_API ArchetypeQuery {
		ECS_INLINE ArchetypeQuery() {}
		ECS_INLINE ArchetypeQuery(VectorComponentSignature _unique, VectorComponentSignature _shared) : unique(_unique), shared(_shared) {}
		
		ECS_INLINE bool ECS_VECTORCALL operator == (ArchetypeQuery other) const {
			return unique == other.unique && shared == other.shared;
		}

		bool ECS_VECTORCALL Verifies(VectorComponentSignature unique, VectorComponentSignature shared) const;

		bool ECS_VECTORCALL VerifiesUnique(VectorComponentSignature unique) const;

		bool ECS_VECTORCALL VerifiesShared(VectorComponentSignature shared) const;

		VectorComponentSignature unique;
		VectorComponentSignature shared;
	};

	struct ECSENGINE_API ArchetypeQueryExclude {
		ECS_INLINE ArchetypeQueryExclude() {}
		ECS_INLINE ArchetypeQueryExclude(
			VectorComponentSignature _unique,
			VectorComponentSignature _shared,
			VectorComponentSignature _exclude_unique,
			VectorComponentSignature _exclude_shared
		) : unique(_unique), shared(_shared), unique_excluded(_exclude_unique), shared_excluded(_exclude_shared) {}

		ECS_INLINE bool ECS_VECTORCALL operator == (ArchetypeQueryExclude other) const {
			return unique == other.unique && shared == other.shared && unique_excluded == other.unique_excluded
				&& shared_excluded == other.shared_excluded;
		}

		bool ECS_VECTORCALL Verifies(VectorComponentSignature unique, VectorComponentSignature shared) const;

		bool ECS_VECTORCALL VerifiesUnique(VectorComponentSignature unique) const;

		bool ECS_VECTORCALL VerifiesShared(VectorComponentSignature shared) const;

		VectorComponentSignature unique;
		VectorComponentSignature shared;
		VectorComponentSignature unique_excluded;
		VectorComponentSignature shared_excluded;
	};

	struct ArchetypeQueryResult {
		Stream<unsigned int> archetypes;
		ArchetypeQuery components;
	};

}