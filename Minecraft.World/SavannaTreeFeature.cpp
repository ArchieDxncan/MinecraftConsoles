#include "stdafx.h"
#include "SavannaTreeFeature.h"
#include "net.minecraft.world.level.tile.h"
#include "TreeTile2.h"
#include "LeafTile2.h"
#include "Level.h"

SavannaTreeFeature::SavannaTreeFeature(bool doUpdate) : Feature(doUpdate) {
    this->baseHeight = 5;
}

bool SavannaTreeFeature::place(Level* level, Random* random, int x, int y, int z) {
    int height = random->nextInt(3) + baseHeight;
    int ground = level->getTile(x, y - 1, z);
    if (ground != Tile::grass_Id && ground != Tile::dirt_Id) return false;

    
    if (random->nextBoolean()) {
        generateForkingTree(level, random, x, y, z, height);
    } else {
        generateBendingTree(level, random, x, y, z, height);
    }

    return true;
}


void SavannaTreeFeature::generateForkingTree(Level* level, Random* random, int x, int y, int z, int height) {
    int curX = x;
    int curZ = z;
    int forkH = height - random->nextInt(4) - 1; 
    
    
    int dx1 = random->nextInt(3) - 1;
    int dz1 = (dx1 == 0) ? (random->nextBoolean() ? 1 : -1) : (random->nextInt(3) - 1);

    for (int h = 0; h < height; h++) {
        if (h >= forkH) { curX += dx1; curZ += dz1; }
        placeBlock(level, curX, y + h, curZ, Tile::tree2Trunk_Id, TreeTile2::ACACIA_TRUNK);
        if (h == height - 1) generateLeafCap(level, curX, y + h, curZ);
    }

    
    int dx2 = random->nextInt(3) - 1;
    int dz2 = random->nextInt(3) - 1;
    if (dx2 != dx1 || dz2 != dz1) {
        int curX2 = x;
        int curZ2 = z;
        int forkH2 = forkH - random->nextInt(2) - 1;
        for (int h = forkH2; h < height; h++) {
            if (h >= 1) {
                curX2 += dx2; curZ2 += dz2;
                placeBlock(level, curX2, y + h, curZ2, Tile::tree2Trunk_Id, TreeTile2::ACACIA_TRUNK);
                if (h == height - 1) generateLeafCap(level, curX2, y + h, curZ2);
            }
        }
    }
}



void SavannaTreeFeature::generateBendingTree(Level* level, Random* random, int x, int y, int z, int height) {
    int curX = x;
    int curZ = z;
    int curY = y;

    
    int dirX = 0, dirZ = 0;
    int roll = random->nextInt(4);
    if (roll == 0) dirX = 1; else if (roll == 1) dirX = -1; else if (roll == 2) dirZ = 1; else dirZ = -1;

    
    int straightHeight = 2 + random->nextInt(3); 
    for (int i = 0; i < straightHeight; i++) {
        placeBlock(level, curX, curY, curZ, Tile::tree2Trunk_Id, TreeTile2::ACACIA_TRUNK);
        curY++;
    }

    
    int diagonalSteps = 2 + random->nextInt(2); 
    for (int i = 0; i < diagonalSteps; i++) {
        curX += dirX;
        curZ += dirZ;
        
        placeBlock(level, curX, curY, curZ, Tile::tree2Trunk_Id, TreeTile2::ACACIA_TRUNK);
        
       
        if (i < diagonalSteps - 1) {
            curY++;
        }
    }

   
    generateLeafCap(level, curX, curY, curZ);
}


void SavannaTreeFeature::generateLeafCap(Level* level, int x, int y, int z) {
    
    for (int lx = -3; lx <= 3; lx++) {
        for (int lz = -3; lz <= 3; lz++) {
            
            if (abs(lx) == 3 && abs(lz) == 3) continue;
            if (abs(lx) + abs(lz) > 4) continue; 
            placeLeaf(level, x + lx, y, z + lz);
        }
    }

    
    for (int lx = -2; lx <= 2; lx++) {
        for (int lz = -2; lz <= 2; lz++) {
            if (abs(lx) == 2 && abs(lz) == 2) continue;
            placeLeaf(level, x + lx, y + 1, z + lz);
        }
    }
}

void SavannaTreeFeature::placeLeaf(Level* level, int x, int y, int z) {
    if (level->getTile(x, y, z) == 0) { 
        placeBlock(level, x, y, z, Tile::leaves2_Id, 0); 
    }
}