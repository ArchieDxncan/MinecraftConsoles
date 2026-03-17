#pragma once
#include "Feature.h"
#include "Level.h"

class SavannaTreeFeature : public Feature {
public:
    SavannaTreeFeature(bool doUpdate);
    virtual bool place(Level* level, Random* random, int x, int y, int z);
private:
    int baseHeight;
    void placeLeaf(Level* level, int x, int y, int z);
};  