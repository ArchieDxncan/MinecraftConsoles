#pragma once

#include "Biome.h"

class PlainsBiome : public Biome
{
	friend class Biome;
protected:
	PlainsBiome(int id);
	virtual Feature* getFlowerFeature(Random* random, int x, int y, int z) override;
};