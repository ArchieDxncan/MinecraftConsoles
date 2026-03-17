#include "stdafx.h"
#include "SavannaTreeFeature.h"
#include "net.minecraft.world.level.tile.h"
#include "TreeTile2.h"
#include "LeafTile2.h"

SavannaTreeFeature::SavannaTreeFeature(bool doUpdate) : Feature(doUpdate) {

    this->baseHeight = 5;
}

bool SavannaTreeFeature::place(Level* level, Random* random, int x, int y, int z) {
    int height = random->nextInt(3) + baseHeight; 
    int ground = level->getTile(x, y - 1, z);
    if (ground != Tile::grass_Id && ground != Tile::dirt_Id) return false;

    int curX = x;
    int curZ = z;
    int curY = y;
    
    
    int dx = random->nextInt(3) - 1;
    int dz = random->nextInt(3) - 1;

    for (int h = 0; h < height; ++h) {
        
        if (h >= height / 2 && (dx != 0 || dz != 0)) {
            curX += dx;
            curZ += dz;
        }
        placeBlock(level, curX, curY + h, curZ, Tile::tree2Trunk_Id, TreeTile2::ACACIA_TRUNK);
        
        
        if (h == height - 1) {
            for (int lx = -2; lx <= 2; ++lx) {
                for (int lz = -2; lz <= 2; ++lz) {
                    if (abs(lx) == 2 && abs(lz) == 2) continue;
                    placeLeaf(level, curX + lx, curY + h, curZ + lz);
                }
            }
        }
    }
    return true;
}

void SavannaTreeFeature::placeLeaf(Level* level, int x, int y, int z) {
    int t = level->getTile(x, y, z);
    if (t == 0 || t == Tile::leaves_Id) {
        placeBlock(level, x, y, z, Tile::leaves2_Id, 0); 
    }
}