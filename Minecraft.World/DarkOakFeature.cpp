#include "stdafx.h"
#include "DarkOakFeature.h"
#include "net.minecraft.world.level.h"
#include "net.minecraft.world.level.tile.h"
#include "TreeTile2.h"
#include "LeafTile2.h"

DarkOakFeature::DarkOakFeature(bool doUpdate) : Feature(doUpdate)
{
    this->baseHeight = 6; 
}

bool DarkOakFeature::place(Level *level, Random *random, int x, int y, int z)
{
    int treeHeight = random->nextInt(3) + baseHeight;
    
    
    if (y < 1 || y + treeHeight + 1 > Level::maxBuildHeight) return false;

    for (int ix = 0; ix <= 1; ix++) {
        for (int iz = 0; iz <= 1; iz++) {
            int below = level->getTile(x + ix, y - 1, z + iz);
            if (below != Tile::grass_Id && below != Tile::dirt_Id) return false;
        }
    }

    
    int dx = random->nextInt(3) - 1;
    int dz = random->nextInt(3) - 1;
    int bendStart = treeHeight - random->nextInt(4);
    int bendLength = 2 - random->nextInt(3);

    int curX = x;
    int curZ = z;

    
    for (int h = 0; h < treeHeight; h++)
    {
        if (h >= bendStart && bendLength > 0)
        {
            curX += dx;
            curZ += dz;
            bendLength--;
        }

        placeTrunk2x2(level, curX, y + h, curZ);
    }

    
    int topY = y + treeHeight;
    for (int lx = -2; lx <= 3; lx++)
    {
        for (int lz = -2; lz <= 3; lz++)
        {
            for (int ly = -1; ly <= 1; ly++)
            {
                
                if ((lx == -2 && lz == -2) || (lx == 3 && lz == -2) || (lx == -2 && lz == 3) || (lx == 3 && lz == 3)) 
                    if (ly != 0) continue;

                placeLeaf(level, curX + lx, topY + ly, curZ + lz);
            }
        }
    }

    
    for (int rx = -1; rx <= 2; rx++)
    {
        for (int rz = -1; rz <= 2; rz++)
        {
            if ((rx < 0 || rx > 1 || rz < 0 || rz > 1) && random->nextInt(3) == 0)
            {
                int branchLen = random->nextInt(3) + 2;
                for (int bh = 0; bh < branchLen; bh++)
                {
                    placeBlock(level, x + rx, topY - bh - 1, z + rz, Tile::tree2Trunk_Id, TreeTile2::DARK_TRUNK);
                }
            }
        }
    }

    return true;
}

void DarkOakFeature::placeTrunk2x2(Level *level, int x, int y, int z)
{
    
    placeBlock(level, x, y, z, Tile::tree2Trunk_Id, TreeTile2::DARK_TRUNK);
    placeBlock(level, x + 1, y, z, Tile::tree2Trunk_Id, TreeTile2::DARK_TRUNK);
    placeBlock(level, x, y, z + 1, Tile::tree2Trunk_Id, TreeTile2::DARK_TRUNK);
    placeBlock(level, x + 1, y, z + 1, Tile::tree2Trunk_Id, TreeTile2::DARK_TRUNK);
}

void DarkOakFeature::placeLeaf(Level *level, int x, int y, int z)
{
    int tile = level->getTile(x, y, z);
    if (tile == 0 || tile == Tile::leaves_Id || tile == Tile::leaves2_Id)
    {
        
        placeBlock(level, x, y, z, Tile::leaves2_Id, 1);
    }
}