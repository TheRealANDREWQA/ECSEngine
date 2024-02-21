#include "ecspch.h"
#include "SpatialGrid.h"

namespace ECSEngine {

	static unsigned int Cantor(unsigned int a, unsigned int b) {
		return ((((a + b + 1) * (a + b)) >> 1) + b) * 0x8da6b343;
	}

	unsigned int Cantor(unsigned int a, unsigned int b, unsigned int c) {
		return Cantor(a, Cantor(b, c));
	}

	unsigned int Djb2Hash(unsigned int x, unsigned int y, unsigned int z)
	{
		// This function doesn't seem to work that well, since for linear
		// Probing it can create long chains of consecutive values since
		// The z value is added last
		unsigned int hash = 5381;
		hash = ((hash << 5) + hash) + x;
		hash = ((hash << 5) + hash) + y;
		hash = ((hash << 5) + hash) + z;
		return hash;
	}

}