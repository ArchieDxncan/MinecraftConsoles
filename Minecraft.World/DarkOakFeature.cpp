#include "stdafx.h"
#include "DarkOakFeature.h"
#include "net.minecraft.world.level.h"
#include "net.minecraft.world.level.tile.h"
#include "TreeTile2.h"
#include "LeafTile2.h"
#include <cmath>
#include <algorithm>

DarkOakFeature::DarkOakFeature(bool doUpdate) : Feature(doUpdate) {
   
}

bool DarkOakFeature::place(Level *level, Random *rand, int x, int y, int z) {
    int treeHeight = rand->nextInt(3) + rand->nextInt(2) + 6; 
    int startX = x;
    int startY = y;
    int startZ = z;

    if (startY >= 1 && startY + treeHeight + 1 < Level::maxBuildHeight) {
        int belowTile = level->getTile(startX, startY - 1, startZ);
        bool isSoil = (belowTile == Tile::grass_Id || belowTile == Tile::dirt_Id);

        if (!(isSoil && startY < Level::maxBuildHeight - treeHeight - 1)) {
            return false;
        } else if (!this->checkSpace(level, startX, startY, startZ, treeHeight)) {
            return false;
        } else {
           
            if(app.getLevelGenerationOptions() != nullptr)
            {
                LevelGenerationOptions *levelGenOptions = app.getLevelGenerationOptions();
                int radius = 4; 
                bool intersects = levelGenOptions->checkIntersects(x - radius, y - 1, z - radius, x + radius + 1, y + treeHeight, z + radius + 1);
                if(intersects)
                {
                    return false;
                }
            }

            
            placeBlock(level, startX, startY - 1, startZ, Tile::dirt_Id);
            placeBlock(level, startX + 1, startY - 1, startZ, Tile::dirt_Id);
            placeBlock(level, startX, startY - 1, startZ + 1, Tile::dirt_Id);
            placeBlock(level, startX + 1, startY - 1, startZ + 1, Tile::dirt_Id);

            
            int face = rand->nextInt(4); // Direzione: 0=N, 1=E, 2=S, 3=W
            int dx = 0;
            int dz = 0;
            if (face == 0) dz = -1;
            else if (face == 1) dx = 1;
            else if (face == 2) dz = 1;
            else if (face == 3) dx = -1;

            int bendHeight = treeHeight - rand->nextInt(4);
            int bendLength = 2 - rand->nextInt(3);
            int trunkX = startX;
            int trunkZ = startZ;
            int topY = startY + treeHeight - 1;

            
            for (int j2 = 0; j2 < treeHeight; ++j2) {
                
                if (j2 >= bendHeight && bendLength > 0) {
                    trunkX += dx;
                    trunkZ += dz;
                    --bendLength;
                }

                int currentY = startY + j2;
                this->placeLog(level, trunkX, currentY, trunkZ);
                this->placeLog(level, trunkX + 1, currentY, trunkZ);
                this->placeLog(level, trunkX, currentY, trunkZ + 1);
                this->placeLog(level, trunkX + 1, currentY, trunkZ + 1);
            }

            
            for (int i3 = -2; i3 <= 0; ++i3) {
                for (int l3 = -2; l3 <= 0; ++l3) {
                    int k4 = -1;
                    this->placeLeaf(level, trunkX + i3, topY + k4, trunkZ + l3);
                    this->placeLeaf(level, 1 + trunkX - i3, topY + k4, trunkZ + l3);
                    this->placeLeaf(level, trunkX + i3, topY + k4, 1 + trunkZ - l3);
                    this->placeLeaf(level, 1 + trunkX - i3, topY + k4, 1 + trunkZ - l3);

                    if ((i3 > -2 || l3 > -1) && (i3 != -1 || l3 != -2)) {
                        k4 = 1;
                        this->placeLeaf(level, trunkX + i3, topY + k4, trunkZ + l3);
                        this->placeLeaf(level, 1 + trunkX - i3, topY + k4, trunkZ + l3);
                        this->placeLeaf(level, trunkX + i3, topY + k4, 1 + trunkZ - l3);
                        this->placeLeaf(level, 1 + trunkX - i3, topY + k4, 1 + trunkZ - l3);
                    }
                }
            }

            
            if (rand->nextInt(2) == 0) { 
                this->placeLeaf(level, trunkX, topY + 2, trunkZ);
                this->placeLeaf(level, trunkX + 1, topY + 2, trunkZ);
                this->placeLeaf(level, trunkX + 1, topY + 2, trunkZ + 1);
                this->placeLeaf(level, trunkX, topY + 2, trunkZ + 1);
            }
            
            
            for (int j3 = -3; j3 <= 4; ++j3) {
                for (int i4 = -3; i4 <= 4; ++i4) {
                    if ((j3 != -3 || i4 != -3) && (j3 != -3 || i4 != 4) && (j3 != 4 || i4 != -3) && (j3 != 4 || i4 != 4) && (std::abs(j3) < 3 || std::abs(i4) < 3)) {
                        this->placeLeaf(level, trunkX + j3, topY, trunkZ + i4);
                    }
                }
            }

            
            for (int k3 = -1; k3 <= 2; ++k3) {
                for (int j4 = -1; j4 <= 2; ++j4) {
                    if ((k3 < 0 || k3 > 1 || j4 < 0 || j4 > 1) && rand->nextInt(3) <= 0) {
                        int l4 = rand->nextInt(3) + 2;

                        
                        for (int i5 = 0; i5 < l4; ++i5) {
                            this->placeLog(level, startX + k3, topY - i5 - 1, startZ + j4);
                        }

                        
                        for (int j5 = -1; j5 <= 1; ++j5) {
                            for (int l2 = -1; l2 <= 1; ++l2) {
                                this->placeLeaf(level, trunkX + k3 + j5, topY, trunkZ + j4 + l2);
                            }
                        }

                        for (int k5 = -2; k5 <= 2; ++k5) {
                            for (int l5 = -2; l5 <= 2; ++l5) {
                                if (std::abs(k5) != 2 || std::abs(l5) != 2) {
                                    this->placeLeaf(level, trunkX + k3 + k5, topY - 1, trunkZ + j4 + l5);
                                }
                            }
                        }
                    }
                }
            }

            return true;
        }
    }
    return false;
}

bool DarkOakFeature::checkSpace(Level *worldIn, int x, int y, int z, int height) {
    for (int l = 0; l <= height + 1; ++l) {
        int i1 = 1;
        if (l == 0) i1 = 0;  
        if (l >= height - 1) i1 = 2;  

        for (int j1 = -i1; j1 <= i1; ++j1) {
            for (int k1 = -i1; k1 <= i1; ++k1) {
                if (l == 0 && j1 == 0 && k1 == 0) continue;
                
                int tile = worldIn->getTile(x + j1, y + l, z + k1);
                
                if (tile != 0 && 
                    tile != Tile::leaves_Id && tile != Tile::leaves2_Id && 
                    tile != Tile::tallgrass_Id && tile != Tile::sapling_Id &&
                    tile != Tile::grass_Id && tile != Tile::dirt_Id && 
                    tile != Tile::treeTrunk_Id && tile != Tile::tree2Trunk_Id) {
                    return false;  
                }
                
                if (tile == Tile::water_Id) return false;  
            }
        }
    }
    return true;
}

void DarkOakFeature::placeLog(Level *worldIn, int x, int y, int z) {
    int tile = worldIn->getTile(x, y, z);
    if (tile == 0 || tile == Tile::leaves_Id || tile == Tile::leaves2_Id || tile == Tile::tallgrass_Id) {
        placeBlock(worldIn, x, y, z, Tile::tree2Trunk_Id, TreeTile2::DARK_TRUNK);
    }
}

void DarkOakFeature::placeLeaf(Level *worldIn, int x, int y, int z) {
    int tile = worldIn->getTile(x, y, z);
    if (tile == 0) {
        
        placeBlock(worldIn, x, y, z, Tile::leaves2_Id, 1);
    }
}