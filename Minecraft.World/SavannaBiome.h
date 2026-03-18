#pragma once
#include "Biome.h"

class SavannaBiome : public Biome
{
public:
    SavannaBiome(int id);

    virtual Feature *getTreeFeature(Random *random);
};