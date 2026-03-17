#include "stdafx.h"
#include "net.minecraft.world.level.h"
#include "net.minecraft.world.level.tile.h"
#include "net.minecraft.world.level.levelgen.feature.h"
#include "net.minecraft.world.h"

#include "SavannaTreeFeature.h"
#include "DarkOakFeature.h"
#include "Sapling2.h"


int Sapling2::SAPLING_NAMES[SAPLING_NAMES_SIZE] = { 
    IDS_TILE_SAPLING_ACACIA, 
    IDS_TILE_SAPLING_DARK_OAK 
};

const wstring Sapling2::TEXTURE_NAMES[] = { L"sapling_acacia", L"sapling_dark_oak" };

Sapling2::Sapling2(int id) : Bush(id)
{
    this->updateDefaultShape();
    icons = nullptr;
}

void Sapling2::updateDefaultShape()
{
    float ss = 0.4f;
    this->setShape(0.5f - ss, 0, 0.5f - ss, 0.5f + ss, ss * 2, 0.5f + ss);
}

bool Sapling2::isSapling(Level *level, int x, int y, int z, int type)
{
    return level->getTile(x, y, z) == this->id && (level->getData(x, y, z) & TYPE_MASK) == type;
}

void Sapling2::tick(Level *level, int x, int y, int z, Random *random)
{
    if (level->isClientSide) return;
    Bush::tick(level, x, y, z, random);

    if (level->getRawBrightness(x, y + 1, z) >= 9 && random->nextInt(7) == 0)
    {
        this->growTree(level, x, y, z, random);
    }
}

Icon *Sapling2::getTexture(int face, int data)
{
    
    if (icons == nullptr) 
    {
        
        return nullptr; 
    }

    int type = data & TYPE_MASK;
    if (type < 0 || type >= SAPLING_NAMES_SIZE) type = 0;
    return icons[type];
}

void Sapling2::registerIcons(IconRegister *iconRegister)
{
    icons = new Icon*[SAPLING_NAMES_SIZE];
    for (int i = 0; i < SAPLING_NAMES_SIZE; ++i)
    {
        icons[i] = iconRegister->registerIcon(TEXTURE_NAMES[i]);
    }
}

void Sapling2::growTree(Level *level, int x, int y, int z, Random *random)
{
    int data = level->getData(x, y, z) & TYPE_MASK;
    Feature *f = nullptr;
    int ox = 0, oz = 0;
    bool multiblock = false;

    if (data == TYPE_ACACIA)
    {
        f = new SavannaTreeFeature(true); 
    }
    else if (data == TYPE_DARK_OAK)
    {
        
        for (ox = 0; ox >= -1; ox--)
        {
            for (oz = 0; oz >= -1; oz--)
            {
                if (isSapling(level, x + ox, y, z + oz, TYPE_DARK_OAK) && 
                    isSapling(level, x + ox + 1, y, z + oz, TYPE_DARK_OAK) && 
                    isSapling(level, x + ox, y, z + oz + 1, TYPE_DARK_OAK) && 
                    isSapling(level, x + ox + 1, y, z + oz + 1, TYPE_DARK_OAK))
                {
                    f = new DarkOakFeature(true);
                    multiblock = true;
                    break;
                }
            }
            if (f != nullptr) break;
        }
        if (f == nullptr) return; 
    }

    // Deletion
    if (multiblock)
    {
        level->setTileAndData(x + ox, y, z + oz, 0, 0, Tile::UPDATE_NONE);
        level->setTileAndData(x + ox + 1, y, z + oz, 0, 0, Tile::UPDATE_NONE);
        level->setTileAndData(x + ox, y, z + oz + 1, 0, 0, Tile::UPDATE_NONE);
        level->setTileAndData(x + ox + 1, y, z + oz + 1, 0, 0, Tile::UPDATE_NONE);
    }
    else
    {
        level->setTileAndData(x, y, z, 0, 0, Tile::UPDATE_NONE);
    }

    // Generation
    if (f != nullptr)
    {
        if (!f->place(level, random, x + ox, y, z + oz))
        {
            if (multiblock)
            {
                level->setTileAndData(x + ox, y, z + oz, id, data, Tile::UPDATE_NONE);
                level->setTileAndData(x + ox + 1, y, z + oz, id, data, Tile::UPDATE_NONE);
                level->setTileAndData(x + ox, y, z + oz + 1, id, data, Tile::UPDATE_NONE);
                level->setTileAndData(x + ox + 1, y, z + oz + 1, id, data, Tile::UPDATE_NONE);
            }
            else
            {
                level->setTileAndData(x, y, z, id, data, Tile::UPDATE_NONE);
            }
        }
        delete f;
    }
}

bool Sapling2::fertilize(Level *level, int x, int y, int z)
{
    this->growTree(level, x, y, z, level->random);
    return true;
}