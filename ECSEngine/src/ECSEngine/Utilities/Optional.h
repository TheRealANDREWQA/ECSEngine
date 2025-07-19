#pragma once
#include "../Core.h"
#include <type_traits>

namespace ECSEngine {

	// In order to not pull the <optional> header
	template<typename T>
	struct Optional {
		template<typename = std::enable_if_t<std::is_default_constructible_v<T>>>
		ECS_INLINE Optional() : has_value(false) {}
		ECS_INLINE Optional(Optional<T>&& other) : value(std::move(other.value)), has_value(other.has_value) {
			other.has_value = false;
		}
		ECS_INLINE Optional(T&& _value) : value(std::move(_value)), has_value(true) {}
		ECS_INLINE Optional(const T& _value) : value(_value), has_value(true) {}

		ECS_INLINE Optional<T>& operator= (const T& other) {
			value = other;
			has_value = true;
			return *this;
		}

		ECS_INLINE Optional<T>& operator= (T&& other) {
			value = std::move(other);
			has_value = true;
			return *this;
		}

		ECS_INLINE Optional<T>& operator=(const Optional<T>& other) {
			value = other.value;
			has_value = other.has_value;
			return *this;
		}

		ECS_INLINE Optional<T>& operator= (Optional<T>&& other) {
			value = std::move(other.value);
			has_value = other.has_value;
			other.has_value = false;
			return *this;
		}

		ECS_INLINE explicit operator bool() const {
			return has_value;
		}

		T value;
		bool has_value;
	};

}