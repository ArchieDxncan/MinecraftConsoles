#pragma once

#include "Biome.h"

class ExtremeHillsBiome : public Biome
{
	friend class Biome;
private:
	static const bool GENERATE_EMERALD_ORE = true;
	Feature *silverfishFeature;
	int type;
	Feature *taigaFeature;

protected:
	ExtremeHillsBiome(int id, bool extraTrees = false);
	~ExtremeHillsBiome();

public:
	virtual void decorate(Level *level, Random *random, int xo, int zo) override;
	virtual void buildSurfaceAtDefault(Level *level, Random *random, byte* chunkBlocks, int x, int z, double noiseVal) override;
	virtual Feature *getTreeFeature(Random *random) override;
	Biome* mutateHills(Biome* baseBiome);
};