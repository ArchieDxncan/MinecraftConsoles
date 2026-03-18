#pragma once
#include "Tile.h"

class PrismarineTile : public Tile
{
public:
    PrismarineTile(int id, Material* material);
    virtual void registerIcons(IconRegister* iconRegister);
    virtual Icon* getTexture(int face, int data);
private:
    Icon* icon;
};