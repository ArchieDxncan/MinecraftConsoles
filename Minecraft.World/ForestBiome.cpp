#include "stdafx.h"
#include "ForestBiome.h"
#include "net.minecraft.world.level.levelgen.feature.h"
#include "net.minecraft.world.level.biome.h"
#include "DarkOakFeature.h" 

ForestBiome::ForestBiome(int id, int type) : Biome(id)
{
    this->biomeType = type; 
    
    
    this->setLeafColor(0x4EBA31); 
    this->setTemperatureAndDownfall(0.7f, 0.8f);

    if (this->biomeType == 1) //flowerforest
    {
        this->decorator->treeCount = 10;
    this->decorator->grassCount = 2;
    this->decorator->flowerCount = 100;
    this->decorator->alliumCount     = 2;
    this->decorator->azureBluetCount = 2;
    this->decorator->oxeyeDaisyCount = 2;
    this->decorator->blueOrchidCount = 0;
    this->setTemperatureAndDownfall(0.6f, 0.6f);
    }
    else if (this->biomeType == 2) //birchforest
    {
        this->decorator->treeCount = 10;
        this->decorator->grassCount = 2;
    }
    else if (this->biomeType == 3) //roofedforest
    {
        
        this->decorator->treeCount = 50;      
        this->decorator->grassCount = 2;
        this->decorator->flowerCount = 1;
        this->decorator->hugeMushrooms = 2; 
    }
    else 
    {
        this->decorator->treeCount = 6;  
        this->decorator->flowerCount = 4; 
        this->decorator->grassCount = 1;  
        
        MobSpawnerData* wolfSpawn = new MobSpawnerData(eTYPE_WOLF, 5, 4, 4);
        this->friendlies.push_back(wolfSpawn);
    }

    if (this->biomeType == 0)
    {
        MobSpawnerData* rabbitSpawn = new MobSpawnerData(eTYPE_RABBIT, 4, 2, 3);
        this->friendlies.push_back(rabbitSpawn);
    }
}

Feature* ForestBiome::getTreeFeature(Random* random)
{
    if (this->biomeType == 3) // Roofed Forest
    {
        
        if (random->nextInt(3) > 0)
        {
            return new DarkOakFeature(false);
        }
    }
    
   
    if (this->biomeType == 2 || random->nextInt(5) == 0)
    {
        return new BirchFeature(false, false); 
    }
    
    
    return new TreeFeature(false); 
}


Biome* ForestBiome::setColor(int color, bool b) 
{
    
    Biome::setColor(color, b);
    
    
    return this;
}