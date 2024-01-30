#include "ecspch.h"
#include "SpatialGrid.h"

namespace ECSEngine {

	unsigned int Djb2Hash(unsigned int x, unsigned int y, unsigned int z)
	{
		unsigned int hash = 5381;
		hash = ((hash << 5) + hash) + x;
		hash = ((hash << 5) + hash) + y;
		hash = ((hash << 5) + hash) + z;
		return hash;
	}

}