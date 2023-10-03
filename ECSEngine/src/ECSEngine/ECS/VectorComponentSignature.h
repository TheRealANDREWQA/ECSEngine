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

	struct ArchetypeQueryOptional {
		ECS_INLINE bool ECS_VECTORCALL Verifies(VectorComponentSignature unique, VectorComponentSignature shared) const {
			return base_query.Verifies(unique, shared);
		}

		ECS_INLINE bool ECS_VECTORCALL VerifiesUnique(VectorComponentSignature unique) const {
			return base_query.VerifiesUnique(unique);
		}

		ECS_INLINE bool ECS_VECTORCALL VerifiesShared(VectorComponentSignature shared) const {
			return base_query.VerifiesShared(shared);
		}

		ArchetypeQuery base_query;
		ComponentSignature unique_optional;
		ComponentSignature shared_optional;
	};

	struct ArchetypeQueryExcludeOptional {
		ECS_INLINE bool ECS_VECTORCALL Verifies(VectorComponentSignature unique, VectorComponentSignature shared) const {
			return base_query.Verifies(unique, shared);
		}

		ECS_INLINE bool ECS_VECTORCALL VerifiesUnique(VectorComponentSignature unique) const {
			return base_query.VerifiesUnique(unique);
		}

		ECS_INLINE bool ECS_VECTORCALL VerifiesShared(VectorComponentSignature shared) const {
			return base_query.VerifiesShared(shared);
		}

		ArchetypeQueryExclude base_query;
		ComponentSignature unique_optional;
		ComponentSignature shared_optiona;
	};

	struct ArchetypeQueryResult {
		Stream<unsigned int> archetypes;
		ArchetypeQuery components;
	};

	enum ECS_ARCHETYPE_QUERY_DESCRIPTOR_TYPE : unsigned char {
		ECS_ARCHETYPE_QUERY_SIMPLE,				// No exclude or optional components
		ECS_ARCHETYPE_QUERY_EXCLUDE,			// Has exclude but no optional components
		ECS_ARCHETYPE_QUERY_OPTIONAL,			// No exclude but has optional components
		ECS_ARCHETYPE_QUERY_EXCLUDE_OPTIONAL	// Has exclude and has optional components
	};

	struct ECSENGINE_API ArchetypeQueryDescriptor {
		// Returns true if there are only unique and shared components specified
		// No exclude or optional components
		bool IsSimple() const;

		// Returns true if it has exclude components and no optional components
		bool IsExclude() const;

		// Returns true if it has optional components but no exclude
		bool IsOptional() const;

		// Returns true if it has optional components and exclude components
		bool IsExcludeOptional() const;

		ECS_ARCHETYPE_QUERY_DESCRIPTOR_TYPE GetType() const;

		ECS_INLINE ArchetypeQuery AsSimple() const {
			return { unique, shared };
		}

		ECS_INLINE ArchetypeQueryExclude AsExclude() const {
			return { unique, shared, unique_exclude, shared_exclude };
		}

		// The optional reference the same component values as here (no copy is performed)
		ECS_INLINE ArchetypeQueryOptional AsOptional() const {
			return { AsSimple(), unique_optional, shared_optional };
		}

		// The optional reference the same component values as here (no copy is performed)
		ECS_INLINE ArchetypeQueryExcludeOptional AsExcludeOptional() const {
			return { AsExclude(), unique_optional, shared_optional };
		}

		// Writes the mandatory unique and then the optional uniques and returns the new signature
		ComponentSignature AggregateUnique(Component* components) const;

		// Writes the mandatory shared and then the optional shareds and returns the new signature
		ComponentSignature AggregateShared(Component* components) const;

		ComponentSignature unique;
		ComponentSignature shared;
		ComponentSignature unique_exclude = {};
		ComponentSignature shared_exclude = {};
		ComponentSignature unique_optional = {};
		ComponentSignature shared_optional = {};
	};

}