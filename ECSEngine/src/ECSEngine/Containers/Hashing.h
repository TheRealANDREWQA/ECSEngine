#pragma once
#include "../Core.h"
#include "Stream.h"
#include "../Math/VCLExtensions.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

#ifndef ECS_HASHTABLE_DYNAMIC_GROW_FACTOR
#define ECS_HASHTABLE_DYNAMIC_GROW_FACTOR 1.5f
#endif

	ECSENGINE_API Vec32cb ECS_VECTORCALL HashTableFindSIMDKernel(unsigned int index, unsigned char* m_metadata, unsigned char key_hash_bits, unsigned char hash_bits_mask);

	// filename can be used as a general purpose pointer if other identifier than the filename is used
	// Compare function uses AVX2 32 byte SIMD char compare
	struct ECSENGINE_API ResourceIdentifier {
		ResourceIdentifier();
		ResourceIdentifier(const char* filename);
		ResourceIdentifier(const wchar_t* filename);
		// if the identifier is something other than a LPCWSTR path
		ResourceIdentifier(const void* id, unsigned int size);
		ResourceIdentifier(Stream<void> identifier);
		ResourceIdentifier(Stream<char> identifier);
		ResourceIdentifier(Stream<wchar_t> identifier);

		ResourceIdentifier(const ResourceIdentifier& other) = default;
		ResourceIdentifier& operator = (const ResourceIdentifier& other) = default;

		bool operator == (const ResourceIdentifier& other) const;

		bool Compare(const ResourceIdentifier& other) const;

		unsigned int Hash() const;

		const void* ptr;
		unsigned int size;
	};

	struct ECSENGINE_API HashFunctionPowerOfTwo {
	public:
		HashFunctionPowerOfTwo() {}
		HashFunctionPowerOfTwo(size_t additional_info) {}
		unsigned int operator ()(unsigned int key, unsigned int capacity) const {
			return key & (capacity - 1);
		}

		static unsigned int Next(unsigned int capacity) {
			// If the value is really small make it to 16
			if (capacity < 16) {
				return 16;
			}
			return function::PowerOfTwoGreater(capacity);
		}
	};

	struct ECSENGINE_API HashFunctionPrimeNumber {
	public:
		HashFunctionPrimeNumber() {}
		HashFunctionPrimeNumber(size_t additional_info) {}

		unsigned int operator () (unsigned int key, unsigned int capacity) const {
			switch (capacity) {
			case 11:
				return key % 11u;
			case 23:
				return key % 23u;
			case 41:
				return key % 41u;
			case 67:
				return key % 67u;
			case 97:
				return key % 97u;
			case 179:
				return key % 179u;
			case 331:
				return key % 331u;
			case 503:
				return key % 503u;
			case 617:
				return key % 617u;
			case 773:
				return key % 773u;
			case 919:
				return key % 919u;
			case 1063:
				return key % 1063u;
			case 1237:
				return key % 1237u;
			case 1511:
				return key % 1511u;
			case 1777:
				return key % 1777u;
			case 2003:
				return key % 2003u;
			case 2381:
				return key % 2381u;
			case 2707:
				return key % 2707u;
			case 3049:
				return key % 3049u;
			case 3463:
				return key % 3463u;
			case 3911:
				return key % 3911u;
			case 4603:
				return key % 4603u;
			case 5231:
				return key % 5231u;
			case 6007:
				return key % 6007u;
			case 7333:
				return key % 7333u;
			case 8731:
				return key % 8731u;
			case 9973:
				return key % 9973u;
			case 12203:
				return key % 12203u;
			case 16339:
				return key % 16339u;
			case 20521:
				return key % 20521u;
			case 27127:
				return key % 27127u;
			case 33587:
				return key % 33587u;
			case 40187:
				return key % 40187u;
			case 48611:
				return key % 48611u;
			case 58169:
				return key % 58169u;
			case 71707:
				return key % 71707u;
			case 82601:
				return key % 82601u;
			case 94777:
				return key % 94777u;
			case 115153:
				return key % 115153u;
			case 127453:
				return key % 127453u;
			case 149011:
				return key % 149011u;
			case 181889:
				return key % 181889u;
			case 219533:
				return key % 219533u;
			case 260959:
				return key % 260959u;
			case 299993:
				return key % 299993u;
			case 354779:
				return key % 354779u;
			case 435263:
				return key % 435263u;
			case 511087:
				return key % 511087u;
			case 587731:
				return key % 587731u;
			case 668201:
				return key % 668201u;
			case 781087:
				return key % 781807u;
			case 900001:
				return key % 900001u;
			case 999983:
				return key % 999983u;
			}

			ECS_ASSERT(false);
			return key % capacity;
		}

		static unsigned int Next(unsigned int capacity) {
			switch (capacity) {
			case 0:
				return 11;
			case 11:
				return 23;
			case 23:
				return 41;
			case 41:
				return 67;
			case 67:
				return 97;
			case 97:
				return 179;
			case 179:
				return 331;
			case 331:
				return 503;
			case 503:
				return 617;
			case 617:
				return 773;
			case 773:
				return 919;
			case 919:
				return 1063;
			case 1063:
				return 1237;
			case 1237:
				return 1511;
			case 1511:
				return 1777;
			case 1777:
				return 2003;
			case 2003:
				return 2381;
			case 2381:
				return 2707;
			case 2707:
				return 3049;
			case 3049:
				return 3463;
			case 3463:
				return 3911;
			case 3911:
				return 4603;
			case 4603:
				return 5231;
			case 5231:
				return 6007;
			case 6007:
				return 7333;
			case 7333:
				return 8731;
			case 8731:
				return 9973;
			case 9973:
				return 12203;
			case 12203:
				return 16339;
			case 16339:
				return 20521;
			case 20521:
				return 27127;
			case 27127:
				return 33587;
			case 33587:
				return 40187;
			case 40187:
				return 48611;
			case 48611:
				return 58169;
			case 58169:
				return 71707;
			case 71707:
				return 82601;
			case 82601:
				return 94777;
			case 94777:
				return 115153;
			case 115153:
				return 127453;
			case 127453:
				return 149011;
			case 149011:
				return 181889;
			case 181889:
				return 219533;
			case 219533:
				return 260959;
			case 260959:
				return 299993;
			case 299993:
				return 354779;
			case 354779:
				return 435263;
			case 435263:
				return 511087;
			case 511087:
				return 587731;
			case 587731:
				return 668201;
			case 668201:
				return 781087;
			case 781087:
				return 900001;
			case 900001:
				return 999983;
			default:
				ECS_ASSERT(false);
			}

			return -1;
		}

		static size_t PrimeCapacity(size_t index) {
			switch (index) {
			case 1:
				return 11;
			case 2:
				return 23;
			case 3:
				return 41;
			case 4:
				return 67;
			case 5:
				return 97;
			case 6:
				return 179;
			case 7:
				return 331;
			case 8:
				return 503;
			case 9:
				return 617;
			case 10:
				return 773;
			case 11:
				return 919;
			case 12:
				return 1063;
			case 13:
				return 1237;
			case 14:
				return 1511;
			case 15:
				return 1777;
			case 16:
				return 2003;
			case 17:
				return 2381;
			case 18:
				return 2707;
			case 19:
				return 3049;
			case 20:
				return 3463;
			case 21:
				return 3911;
			case 22:
				return 4603;
			case 23:
				return 5231;
			case 24:
				return 6007;
			case 25:
				return 7333;
			case 26:
				return 8731;
			case 27:
				return 9973;
			case 28:
				return 12203;
			case 29:
				return 16339;
			case 30:
				return 20521;
			case 31:
				return 27127;
			case 32:
				return 33587;
			case 33:
				return 40187;
			case 34:
				return 48611;
			case 35:
				return 58169;
			case 36:
				return 71707;
			case 37:
				return 82601;
			case 38:
				return 94777;
			case 39:
				return 115153;
			case 40:
				return 127453;
			case 41:
				return 149011;
			case 42:
				return 181889;
			case 43:
				return 219533;
			case 44:
				return 260959;
			case 45:
				return 299993;
			case 46:
				return 354779;
			case 47:
				return 435263;
			case 48:
				return 511087;
			case 49:
				return 587731;
			case 50:
				return 668201;
			case 51:
				return 781087;
			case 52:
				return 900001;
			case 53:
				return 999983;
			}

			ECS_ASSERT(false);
			return 999983;
		}
	};

	struct ECSENGINE_API HashFunctionFibonacci {
	public:
		HashFunctionFibonacci() : m_shift_amount(64) {}
		HashFunctionFibonacci(size_t additional_info) : m_shift_amount(64 - additional_info) {}
		unsigned int operator () (unsigned int key, unsigned int capacity) const {
			return (key * 11400714819323198485llu) >> m_shift_amount;
		}

		static unsigned int Next(unsigned int capacity) {
			return (unsigned int)((float)capacity * ECS_HASHTABLE_DYNAMIC_GROW_FACTOR + 2);
		}

		size_t m_shift_amount;
	};

	struct ECSENGINE_API HashFunctionXORFibonacci {
	public:
		HashFunctionXORFibonacci() : m_shift_amount(64) {}
		HashFunctionXORFibonacci(size_t additional_info) : m_shift_amount(64 - additional_info) {}
		unsigned int operator() (unsigned int key, unsigned int capacity) const {
			key ^= key >> m_shift_amount;
			return (key * 11400714819323198485llu) >> m_shift_amount;
		}

		static unsigned int Next(unsigned int capacity) {
			return (unsigned int)((float)capacity * ECS_HASHTABLE_DYNAMIC_GROW_FACTOR + 2);
		}

		size_t m_shift_amount;
	};

	struct ECSENGINE_API HashFunctionFolding {
	public:
		HashFunctionFolding() {}
		HashFunctionFolding(size_t additional_info) {}
		unsigned int operator() (unsigned int key, unsigned int capacity) const {
			return (key & 0x0000FFFF + (key & 0xFFFF0000) >> 16) & (capacity - 1);
		}

		static unsigned int Next(unsigned int capacity) {
			// If the value is really small, make it 16
			if (capacity < 16) {
				return 16;
			}
			return function::PowerOfTwoGreater(capacity);
		}
	};

	struct ObjectHashFallthrough {
		template<typename T>
		static inline unsigned int Hash(T identifier) {
			if constexpr (std::is_arithmetic_v<T> || std::is_pointer_v<T>) {
				return (unsigned int)identifier;
			}
			else {
				return identifier.Hash();
			}
		}
	};

}