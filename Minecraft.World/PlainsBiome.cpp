#include "stdafx.h"
#include "net.minecraft.world.level.biome.h"

PlainsBiome::PlainsBiome(int id) : Biome(id)
{
	friendlies.push_back(new MobSpawnerData(eTYPE_HORSE, 5, 2, 6));
	friendlies.push_back(new MobSpawnerData(eTYPE_RABBIT, 4, 2, 3));

	decorator->treeCount = -999;
	decorator->flowerCount = 4;
	decorator->grassCount = 10;
}

Feature* PlainsBiome::getFlowerFeature(Random* random, int x, int y, int z)
{
    int fType = random->nextInt(3);
    if (fType == 0) return new FlowerFeature(Tile::rose_Id, Rose::OXEYE_DAISY);
    if (fType == 1) return new FlowerFeature(Tile::rose_Id, Rose::AZURE_BLUET);
    
    // Tulipani (qualsiasi colore)
    int tulipColor = random->nextInt(4);
    switch (tulipColor) {
        case 0: return new FlowerFeature(Tile::rose_Id, Rose::RED_TULIP);
        case 1: return new FlowerFeature(Tile::rose_Id, Rose::ORANGE_TULIP);
        case 2: return new FlowerFeature(Tile::rose_Id, Rose::WHITE_TULIP);
        case 3: return new FlowerFeature(Tile::rose_Id, Rose::PINK_TULIP);
    }
    
    return Biome::getFlowerFeature(random, x, y, z);
}