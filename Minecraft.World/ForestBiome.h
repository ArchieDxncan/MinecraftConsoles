#pragma once
#include "Biome.h"

class ForestBiome : public Biome
{
private:
    int biomeType;

public:
    ForestBiome(int id, int type = 0); 

    virtual Feature *getTreeFeature(Random *random) override;
    
    
    virtual Biome *setColor(int color, bool b = false) override; 
};