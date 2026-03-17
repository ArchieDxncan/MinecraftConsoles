#pragma once
#include "Tile.h"

class SeaLanternTile : public Tile
{
public:
    SeaLanternTile(int id, Material* material);
    virtual void registerIcons(IconRegister* iconRegister);
    virtual Icon* getTexture(int face, int data);
    virtual int getResourceCountForLootBonus(int bonusLevel, Random* random);
    virtual int getResourceCount(Random* random);
    virtual int getResource(int data, Random* random, int playerBonusLevel);
private:
    Icon* icon;
};