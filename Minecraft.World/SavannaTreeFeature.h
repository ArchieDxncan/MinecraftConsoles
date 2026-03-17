#pragma once
#include "net.minecraft.world.level.levelgen.feature.h"
#include "Feature.h"

class SavannaTreeFeature : public Feature
{
public:
    
    SavannaTreeFeature(bool doUpdate);

    
    virtual bool place(Level* level, Random* random, int x, int y, int z) override;

private:
    int baseHeight;

    
    void generateBendingTree(Level* level, Random* random, int x, int y, int z, int height);

    
    void generateForkingTree(Level* level, Random* random, int x, int y, int z, int height);

    
    void generateLeafCap(Level* level, int x, int y, int z);

    
    void placeLeaf(Level* level, int x, int y, int z);
};