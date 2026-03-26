#include "stdafx.h"
#include "TaigaBiome.h"
#include "net.minecraft.world.entity.animal.h"
#include "net.minecraft.world.level.levelgen.feature.h"
#include "net.minecraft.world.level.biome.h"

#include "net.minecraft.world.level.tile.h"
#include "DoublePlantFeature.h"
#include "TallGrass2.h"
#include "Level.h"
#include "Random.h"
#include "MegaPineTreeFeature.h"

TaigaBiome::TaigaBiome(int id, int type) : Biome(id)
{
	this->type = type;
	friendlies_wolf.push_back(new MobSpawnerData(eTYPE_WOLF, 8, 4, 4));	// 4J - moved to their own category
	friendlies.push_back(new MobSpawnerData(eTYPE_RABBIT, 4, 2, 3));

    decorator->treeCount = 10;
    
	if (type != 1 && type != 2)
	{
		decorator->grassCount = 1;
		decorator->mushroomCount = 1;
	}
	else
	{
		decorator->grassCount = 7;
		decorator->deadBushCount = 1;
		decorator->mushroomCount = 3;
	}
}

Feature *TaigaBiome::getTreeFeature(Random *random)
{
	if (type == 1 || type == 2)
	{
		if (random->nextInt(3) == 0)
		{
			return new MegaPineTreeFeature(false, random->nextBoolean());
		}
	}
	if (random->nextInt(3) == 0)
	{
		return new PineFeature();
	}
	return new SpruceFeature(false);
}

void TaigaBiome::decorate(Level *level, Random *random, int xo, int zo)
{
    if (type == 1 || type == 2)
    {
        int count = random->nextInt(3);
        for (int i = 0; i < count; ++i)
        {
            int x = xo + random->nextInt(16) + 8;
            int z = zo + random->nextInt(16) + 8;
            int y = level->getHeightmap(x, z);
            
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dz = -1; dz <= 1; ++dz) {
                        int tileAt = level->getTile(x + dx, y + dy, z + dz);
                        if (random->nextInt(4) != 0 && (tileAt == 0 || tileAt == Tile::grass_Id)) {
                            level->setTileAndData(x + dx, y + dy, z + dz, Tile::mossyCobblestone_Id, 0, Tile::UPDATE_CLIENTS);
                        }
                    }
                }
            }
        }
    }

    DOUBLE_PLANT_GENERATOR->setPlantType(TallGrass2::LARGE_FERN);
    for (int i = 0; i < 7; ++i)
    {
        int x = xo + random->nextInt(16) + 8;
        int z = zo + random->nextInt(16) + 8;
        int y = random->nextInt(level->getHeightmap(x, z) + 32);
        DOUBLE_PLANT_GENERATOR->place(level, random, x, y, z);
    }

    Biome::decorate(level, random, xo, zo);
}

void TaigaBiome::buildSurfaceAtDefault(Level *level, Random *random, byte* chunkBlocks, int x, int z, double noiseVal)
{
    if (type == 1 || type == 2)
    {
        topMaterial = static_cast<byte>(Tile::grass_Id);
        material = static_cast<byte>(Tile::dirt_Id);
        if (noiseVal > 1.75)
        {
            topMaterial = static_cast<byte>(Tile::dirt_Id); 
        }
        else if (noiseVal > -0.95)
        {
            topMaterial = static_cast<byte>(Tile::dirt_Id); 
        }
    }
    Biome::buildSurfaceAtDefault(level, random, chunkBlocks, x, z, noiseVal);
}