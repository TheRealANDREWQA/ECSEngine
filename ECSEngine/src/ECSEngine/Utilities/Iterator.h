#pragma once

namespace ECSEngine {

	// Helper structure that provides that allows functions to receive an iterable collection
	// ValueType must be fully qualified for const types, meaning the const should be added to the template parameter
	template<typename ValueType>
	struct IteratorInterface
	{
		typedef void (*Functor)(ValueType* value, void* data);

		typedef bool (*ExitFunctor)(ValueType* value, void* data);

		// Must return false when the elements have been exhausted
		virtual ValueType* Get() = 0;

		void ForEach(Functor functor, void* data) {
			ValueType* value = Get();
			while (value != nullptr) {
				functor(*value, data);
				value = Get();
			}
		}

		// Returns true if it early exited, else false
		bool ForEachExit(ExitFunctor functor, void* data) {
			bool should_exit = false;
			ValueType* value = Get();
			while (value != nullptr && !should_exit) {
				should_exit = functor(*value, data);
				value = Get();
			}
			return should_exit;
		}

		// Helper function that allows using a lambda instead of creating a manual function
		// And its set of data. The functor receives as argument (Value& value);
		template<typename FunctorType>
		void ForEach(FunctorType&& functor) {
			auto wrapper = [](ValueType* value, void* data) {
				FunctorType* functor = (FunctorType*)data;
				(*functor)(value);
			};
			ForEach(wrapper, &functor);
		}

		// Helper function that allows using a lambda instead of creating a manual function
		// And its set of data. The functor receives as argument (Value& value);
		template<typename FunctorType>
		bool ForEachExit(FunctorType&& functor) {
			auto wrapper = [](ValueType* value, void* data) {
				FunctorType* functor = (FunctorType*)data;
				return (*functor)(value);
			};
			return ForEachExit(wrapper, &functor);
		}
	};

}