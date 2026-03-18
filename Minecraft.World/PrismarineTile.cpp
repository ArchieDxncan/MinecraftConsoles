#include "stdafx.h"
#include "PrismarineTile.h"
#include "net.minecraft.world.item.h"
#include "IconRegister.h"

PrismarineTile::PrismarineTile(int id, Material* material) : Tile(id, material)
{
}

void PrismarineTile::registerIcons(IconRegister* iconRegister)
{
    icon = iconRegister->registerIcon(L"prismarine_rough");
}

Icon* PrismarineTile::getTexture(int face, int data)
{
    return icon;
}