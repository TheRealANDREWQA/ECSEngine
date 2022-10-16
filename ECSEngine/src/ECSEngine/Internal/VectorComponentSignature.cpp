#include "ecspch.h"
#include "VectorComponentSignature.h"

/* 
	This file is separate from internal structures because the compiler generates horrible code for debug and release builds
	That is insanely slow. This file is compiled with full optimizations for all build types.
*/

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void VectorComponentSignature::ConvertComponents(ComponentSignature signature)
	{
		// Increment the component indices so as to not have 0 as a valid component index
		alignas(32) uint16_t increments[sizeof(__m256i) / sizeof(uint16_t)] = { 0 };
		for (size_t index = 0; index < signature.count; index++) {
			increments[index] = 1;
		}
		Vec16us increment;
		increment.load_a(increments);
		value.load_partial(signature.count, signature.indices);
		value += increment;
	}

	// ------------------------------------------------------------------------------------------------------------

	void VectorComponentSignature::ConvertInstances(const SharedInstance* instances, unsigned char count)
	{
		ConvertComponents({ (Component*)instances, count });
	}

	// ------------------------------------------------------------------------------------------------------------

	typedef bool (ECS_VECTORCALL* HasComponentFunctor)(VectorComponentSignature components, VectorComponentSignature query);

	template<int count>
	bool ECS_VECTORCALL HasComponentsN(VectorComponentSignature components, VectorComponentSignature query) {
		Vec16us splatted_component;
		Vec16sb match;

		if constexpr (count > 0) {
			splatted_component = Broadcast16<0>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 1) {
			splatted_component = Broadcast16<1>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 2) {
			splatted_component = Broadcast16<2>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 3) {
			splatted_component = Broadcast16<3>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 4) {
			splatted_component = Broadcast16<4>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 5) {
			splatted_component = Broadcast16<5>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 6) {
			splatted_component = Broadcast16<6>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 7) {
			splatted_component = Broadcast16<7>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 8) {
			splatted_component = Broadcast16<8>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 9) {
			splatted_component = Broadcast16<9>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 10) {
			splatted_component = Broadcast16<10>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 11) {
			splatted_component = Broadcast16<11>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 12) {
			splatted_component = Broadcast16<12>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 13) {
			splatted_component = Broadcast16<13>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 14) {
			splatted_component = Broadcast16<14>(query.value);
			match = splatted_component == components.value;
			if (!horizontal_or(match)) {
				return false;
			}
		}

		splatted_component = Broadcast16<count>(query.value);
		match = splatted_component == components.value;
		return horizontal_or(match);
	}

	HasComponentFunctor HAS_COMPONENT_FUNCTOR[] = {
		HasComponentsN<0>,
		HasComponentsN<1>,
		HasComponentsN<2>,
		HasComponentsN<3>,
		HasComponentsN<4>,
		HasComponentsN<5>,
		HasComponentsN<6>,
		HasComponentsN<7>,
		HasComponentsN<8>,
		HasComponentsN<9>,
		HasComponentsN<10>,
		HasComponentsN<11>,
		HasComponentsN<12>,
		HasComponentsN<13>,
		HasComponentsN<14>,
		HasComponentsN<15>,
	};

	typedef bool (ECS_VECTORCALL* ExcludesComponentsFunctor)(VectorComponentSignature components, VectorComponentSignature query);

	template<int count>
	bool ECS_VECTORCALL ExcludesComponentsN(VectorComponentSignature components, VectorComponentSignature query) {
		Vec16us splatted_component;
		Vec16sb match;

		if constexpr (count > 0) {
			splatted_component = Broadcast16<0>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 1) {
			splatted_component = Broadcast16<1>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 2) {
			splatted_component = Broadcast16<2>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 3) {
			splatted_component = Broadcast16<3>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 4) {
			splatted_component = Broadcast16<4>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 5) {
			splatted_component = Broadcast16<5>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 6) {
			splatted_component = Broadcast16<6>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 7) {
			splatted_component = Broadcast16<7>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 8) {
			splatted_component = Broadcast16<8>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 9) {
			splatted_component = Broadcast16<9>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 10) {
			splatted_component = Broadcast16<10>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 11) {
			splatted_component = Broadcast16<11>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 12) {
			splatted_component = Broadcast16<12>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 13) {
			splatted_component = Broadcast16<13>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		if constexpr (count > 14) {
			splatted_component = Broadcast16<14>(query.value);
			match = splatted_component == components.value;
			if (horizontal_or(match)) {
				return false;
			}
		}

		splatted_component = Broadcast16<count>(query.value);
		match = splatted_component == components.value; 
		return !horizontal_or(match);
	}

	ExcludesComponentsFunctor EXCLUDES_COMPONENTS[] = {
		ExcludesComponentsN<0>,
		ExcludesComponentsN<1>,
		ExcludesComponentsN<2>,
		ExcludesComponentsN<3>,
		ExcludesComponentsN<4>,
		ExcludesComponentsN<5>,
		ExcludesComponentsN<6>,
		ExcludesComponentsN<7>,
		ExcludesComponentsN<8>,
		ExcludesComponentsN<9>,
		ExcludesComponentsN<10>,
		ExcludesComponentsN<11>,
		ExcludesComponentsN<12>,
		ExcludesComponentsN<13>,
		ExcludesComponentsN<14>,
		ExcludesComponentsN<15>,
	};

	bool ECS_VECTORCALL VectorComponentSignatureHasComponents(VectorComponentSignature components, VectorComponentSignature query, unsigned char query_count)
	{
		return HAS_COMPONENT_FUNCTOR[query_count - 1](components, query);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL VectorComponentSignatureExcludesComponents(VectorComponentSignature components, VectorComponentSignature query, unsigned char query_count)
	{
		return EXCLUDES_COMPONENTS[query_count - 1](components, query);
	}

	bool ECS_VECTORCALL VectorComponentSignature::HasComponents(VectorComponentSignature components) const
	{
		Vec16us splatted_component;
		Vec16us zero_vector = ZeroVectorInteger();
		Vec16sb is_zero;
		Vec16sb match;

#define LOOP_ITERATION(iteration)   splatted_component = Broadcast16<iteration>(components.value); \
									/* Test to see if the current element is zero */ \
									is_zero = splatted_component == zero_vector; \
									/* If we get to an element which is 0, that means all components have been matched */ \
									if (horizontal_and(is_zero)) { \
										return true; \
									} \
									match = splatted_component == value; \
									if (!horizontal_or(match)) { \
										return false; \
									} 

		// In debug and release modes in seems fine. No notable performance difference between the manual unrolled version
		// and this macro
		LOOP_UNROLL(16, LOOP_ITERATION);

#undef LOOP_ITERATION

		//splatted_component = Broadcast16<0>(components.value); 
		//	/* Test to see if the current element is zero */ 
		//	is_zero = splatted_component == zero_vector; 
		//	/* If we get to an element which is 0, that means all components have been matched */ 
		//	if (horizontal_and(is_zero)) {
		//		
		//			return true; 
		//	} 
		//		match = splatted_component == value; 
		//			if (!horizontal_or(match)) {
		//				
		//					return false; 
		//			}

		//splatted_component = Broadcast16<1>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<2>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<3>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<4>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<5>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<6>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<7>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<8>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<9>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<10>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<11>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<12>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<13>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<14>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		//splatted_component = Broadcast16<15>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {

		//	return true;
		//}
		//match = splatted_component == value;
		//if (!horizontal_or(match)) {

		//	return false;
		//}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL VectorComponentSignature::ExcludesComponents(VectorComponentSignature components) const
	{
		Vec16us splatted_component;
		Vec16us zero_vector = ZeroVectorInteger();
		Vec16sb is_zero;
		Vec16sb match;

#define LOOP_ITERATION(iteration)   splatted_component = Broadcast16<iteration>(components.value); \
									/* Test to see if the current element is zero */ \
									is_zero = splatted_component == zero_vector; \
									/* If we get to an element which is 0, that means all components have been matched */ \
									if (horizontal_and(is_zero)) { \
										return true; \
									} \
									match = splatted_component == value; \
									if (horizontal_or(match)) { \
										return false; \
									} 

		// It seems like in debug and release mode this is fine. No notable performance difference when using the 
		// unrolled version by hand
		LOOP_UNROLL(16, LOOP_ITERATION);

#undef LOOP_ITERATION

		//splatted_component = Broadcast16<0>(components.value); 
		///* Test to see if the current element is zero */ 
		//is_zero = splatted_component == zero_vector; 
		///* If we get to an element which is 0, that means all components have been matched */ 
		//if (horizontal_and(is_zero)) {	
		//	return true; 
		//} 
		//match = splatted_component == value; 
		//if (horizontal_or(match)) {			
		//	return false; 
		//}

		//splatted_component = Broadcast16<1>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<2>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<3>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<4>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<5>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<6>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<7>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<8>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<9>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<10>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<11>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<12>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<13>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<14>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		//splatted_component = Broadcast16<15>(components.value);
		///* Test to see if the current element is zero */
		//is_zero = splatted_component == zero_vector;
		///* If we get to an element which is 0, that means all components have been matched */
		//if (horizontal_and(is_zero)) {
		//	return true;
		//}
		//match = splatted_component == value;
		//if (horizontal_or(match)) {
		//	return false;
		//}

		return true;

	}

	// ------------------------------------------------------------------------------------------------------------

	void ECS_VECTORCALL VectorComponentSignature::Find(VectorComponentSignature components, unsigned char* values)
	{
		Vec16us splatted_component;
		Vec16us zero_vector = ZeroVectorInteger();
		Vec16sb is_zero;
		Vec16sb match;

#define LOOP_ITERATION(iteration)   splatted_component = Broadcast16<iteration>(components.value); \
									/* Test to see if the current element is zero */ \
									is_zero = splatted_component == zero_vector; \
									/* If we get to an element which is 0, that means all components have been matched */ \
									if (horizontal_and(is_zero)) { \
										return; \
									} \
									match = splatted_component == value; \
									values[iteration] = horizontal_find_first(match);

		LOOP_UNROLL(16, LOOP_ITERATION);

		splatted_component = Broadcast16<0>(components.value); 
		/* Test to see if the current element is zero */ 
		is_zero = splatted_component == zero_vector;
		/* If we get to an element which is 0, that means all components have been matched */
		if (horizontal_and(is_zero)) {
				return;
		}
		match = splatted_component == value;
		values[0] = horizontal_find_first(match);



#undef LOOP_ITERATION
	}

	// ------------------------------------------------------------------------------------------------------------

	unsigned char VectorComponentSignature::Count() const
	{
		// Match with zero
		Vec16us zero_vector = ZeroVectorInteger();
		Vec16sb zeroes = value == zero_vector;
		// +1 must be added to give the count
		int first_index = horizontal_find_first(zeroes);
		return first_index == -1 ? 16 : first_index + 1;
	}

	// ------------------------------------------------------------------------------------------------------------

	void VectorComponentSignature::InitializeSharedComponent(SharedComponentSignature shared_signature)
	{
		ConvertComponents(ComponentSignature(shared_signature.indices, shared_signature.count));
	}

	// ------------------------------------------------------------------------------------------------------------

	void VectorComponentSignature::InitializeSharedInstances(SharedComponentSignature shared_signature)
	{
		ConvertInstances(shared_signature.instances, shared_signature.count);
	}

	// ------------------------------------------------------------------------------------------------------------

	void VectorComponentSignature::Initialize()
	{
		value = Vec16us(0);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ECS_VECTORCALL SharedComponentSignatureHasInstances(
		VectorComponentSignature archetype_components,
		VectorComponentSignature archetype_instances,
		VectorComponentSignature query_components,
		VectorComponentSignature query_instances
	)
	{
		Vec16us splatted_component;
		Vec16us splatted_instance;
		Vec16us zero_vector = ZeroVectorInteger();
		Vec16sb is_zero;
		Vec16sb match;
		Vec16sb instance_match;

#define LOOP_ITERATION(iteration)	splatted_component = Broadcast16<iteration>(query_components.value); \
									/* Test to see if the current element is zero */ \
									is_zero = splatted_component == zero_vector; \
									/* If we get to an element which is 0, that means all components have been matched */ \
									if (horizontal_and(is_zero)) { \
										return true; \
									} \
									match = splatted_component == archetype_components.value; \
									if (!horizontal_or(match)) { \
										return false; \
									} \
									/* Test now the instance - splat the instance and keep the position where only the component was matched */ \
									splatted_instance = Broadcast16<iteration>(query_instances.value); \
									instance_match = splatted_instance == archetype_instances.value; \
									match &= instance_match; \
									if (!horizontal_or(match)) { \
										return false; \
									}


		LOOP_UNROLL(16, LOOP_ITERATION);

		return true;

#undef LOOP_ITERATION
	}

	// ------------------------------------------------------------------------------------------------------------

	ArchetypeQuery::ArchetypeQuery() {}

	ArchetypeQuery::ArchetypeQuery(VectorComponentSignature _unique, VectorComponentSignature _shared) : unique(_unique), shared(_shared) {}

	bool ECS_VECTORCALL ArchetypeQuery::Verifies(VectorComponentSignature unique_to_compare, VectorComponentSignature shared_to_compare) const
	{
		return VerifiesUnique(unique_to_compare) && VerifiesShared(shared_to_compare);
	}

	bool ECS_VECTORCALL ArchetypeQuery::VerifiesUnique(VectorComponentSignature unique_to_compare) const
	{
		return unique_to_compare.HasComponents(unique);
	}

	bool ECS_VECTORCALL ArchetypeQuery::VerifiesShared(VectorComponentSignature shared_to_compare) const
	{
		return shared_to_compare.HasComponents(shared);
	}

	// ------------------------------------------------------------------------------------------------------------

	ArchetypeQueryExclude::ArchetypeQueryExclude() {}

	ArchetypeQueryExclude::ArchetypeQueryExclude(
		VectorComponentSignature _unique,
		VectorComponentSignature _shared,
		VectorComponentSignature _exclude_unique,
		VectorComponentSignature _exclude_shared
	) : unique(_unique), shared(_shared), unique_excluded(_exclude_unique), shared_excluded(_exclude_shared) {}

	bool ECS_VECTORCALL ArchetypeQueryExclude::Verifies(VectorComponentSignature unique_to_compare, VectorComponentSignature shared_to_compare) const
	{
		return VerifiesUnique(unique_to_compare) && VerifiesShared(shared_to_compare);
	}

	bool ECS_VECTORCALL ArchetypeQueryExclude::VerifiesUnique(VectorComponentSignature unique_to_compare) const
	{
		return unique_to_compare.HasComponents(unique) && unique_to_compare.ExcludesComponents(unique_excluded);
	}

	bool ECS_VECTORCALL ArchetypeQueryExclude::VerifiesShared(VectorComponentSignature shared_to_compare) const
	{
		return shared_to_compare.HasComponents(shared) && shared_to_compare.ExcludesComponents(shared_excluded);
	}

	// ------------------------------------------------------------------------------------------------------------

}