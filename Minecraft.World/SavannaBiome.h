#pragma once
#include "Biome.h"

class SavannaBiome : public Biome
{
public:
    SavannaBiome(int id);

    virtual Feature *getTreeFeature(Random *random);
    virtual int getFoliageColor() const override;
    virtual int getGrassColor() const override;
    //virtual int getWaterColor() override;
    virtual Feature *getFlowerFeature(Random *random, int x, int y, int z) override;
    virtual int getRandomDoublePlantType(Random *random) override;
};