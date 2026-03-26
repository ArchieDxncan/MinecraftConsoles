#include "stdafx.h"
#include "Color.h"
#include "..\Minecraft.Client\Minecraft.h"
#include "net.minecraft.world.level.levelgen.feature.h"
#include "net.minecraft.world.level.tile.h"
#include "net.minecraft.world.level.h"
#include "net.minecraft.world.entity.animal.h"
#include "net.minecraft.world.entity.monster.h"
#include "net.minecraft.world.entity.h"
#include "Biome.h"
#include "net.minecraft.world.level.biome.h"
#include "PerlinNoise.h"

Biome *Biome::biomes[256];

Biome *Biome::ocean = nullptr;
Biome *Biome::plains = nullptr;
Biome *Biome::desert = nullptr;
Biome *Biome::extremeHills = nullptr;
Biome *Biome::forest = nullptr;
Biome *Biome::taiga = nullptr;
Biome *Biome::swampland = nullptr;
Biome *Biome::river = nullptr;
Biome *Biome::hell = nullptr;
Biome *Biome::sky = nullptr;
Biome *Biome::frozenOcean = nullptr;
Biome *Biome::frozenRiver = nullptr;
Biome *Biome::iceFlats = nullptr;
Biome *Biome::iceMountains = nullptr;
Biome *Biome::mushroomIsland = nullptr;
Biome *Biome::mushroomIslandShore = nullptr;
Biome *Biome::beaches = nullptr;
Biome *Biome::desertHills = nullptr;
Biome *Biome::forestHills = nullptr;
Biome *Biome::taigaHills = nullptr;
Biome *Biome::smallerExtremeHills = nullptr;
Biome *Biome::jungle = nullptr;
Biome *Biome::jungleHills = nullptr;
Biome *Biome::jungleEdge = nullptr;
Biome *Biome::deepOcean = nullptr;
Biome *Biome::stoneBeach = nullptr;
Biome *Biome::coldBeach = nullptr;
Biome *Biome::birchForest = nullptr;
Biome *Biome::birchForestHills = nullptr;
Biome *Biome::roofedForest = nullptr;
Biome *Biome::coldTaiga = nullptr;
Biome *Biome::coldTaigaHills = nullptr;
Biome *Biome::megaTaiga = nullptr;
Biome *Biome::megaTaigaHills = nullptr;
Biome *Biome::extremeHills_plus = nullptr;
Biome *Biome::savanna = nullptr;
Biome *Biome::savannaPlateau = nullptr;
Biome *Biome::mesa = nullptr;
Biome *Biome::mesaPlateauF = nullptr;
Biome *Biome::mesaPlateau = nullptr;
Biome *Biome::theVoid = nullptr;
Biome *Biome::sunflowersPlains = nullptr;
Biome *Biome::desertM = nullptr;
Biome *Biome::extremeHillsM = nullptr;
Biome *Biome::flowerForest = nullptr;
Biome *Biome::taigaM = nullptr;
Biome *Biome::swamplandM = nullptr;
Biome *Biome::iceSpikes = nullptr;
Biome *Biome::jungleM = nullptr;
Biome *Biome::jungleEdgeM = nullptr;
Biome *Biome::birchForestM = nullptr;
Biome *Biome::birchForestHillsM = nullptr;
Biome *Biome::roofedForestM = nullptr;
Biome *Biome::coldTaigaM = nullptr;
Biome *Biome::redwoodTaiga = nullptr;
Biome *Biome::redwoodTaigaHills = nullptr;
Biome *Biome::extremeHills_plusM = nullptr;
Biome *Biome::savannaM = nullptr;
Biome *Biome::savannaPlateauM = nullptr;
Biome *Biome::mesaBryce = nullptr;
Biome *Biome::mesaPlateauFM = nullptr;
Biome *Biome::mesaPlateauM = nullptr;//167

void Biome::staticCtor()
{
    Biome::ocean = (new OceanBiome(0))->setColor(0x000070)->setName(L"Ocean")->setDepthAndScale(-1, 0.4f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Ocean, eMinecraftColour_Foliage_Ocean, eMinecraftColour_Water_Ocean,eMinecraftColour_Sky_Ocean);
    Biome::plains = (new PlainsBiome(1,false))->setColor(0x8db360)->setName(L"Plains")->setTemperatureAndDownfall(0.8f, 0.4f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Plains, eMinecraftColour_Foliage_Plains, eMinecraftColour_Water_Plains,eMinecraftColour_Sky_Plains);
    Biome::desert = (new DesertBiome(2))->setColor(0xFA9418)->setName(L"Desert")->setNoRain()->setTemperatureAndDownfall(2, 0)->setDepthAndScale(0.1f, 0.2f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Desert, eMinecraftColour_Foliage_Desert, eMinecraftColour_Water_Desert,eMinecraftColour_Sky_Desert);
    Biome::extremeHills = (new ExtremeHillsBiome(3))->setColor(0x606060)->setName(L"Extreme Hills")->setDepthAndScale(0.3f, 1.5f)->setTemperatureAndDownfall(0.2f, 0.3f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_ExtremeHills, eMinecraftColour_Foliage_ExtremeHills, eMinecraftColour_Water_ExtremeHills,eMinecraftColour_Sky_ExtremeHills);
    Biome::forest = (new ForestBiome(4,0))->setColor(0x056621)->setName(L"Forest")->setLeafColor(0x4EBA31)->setTemperatureAndDownfall(0.7f, 0.8f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Forest, eMinecraftColour_Foliage_Forest, eMinecraftColour_Water_Forest,eMinecraftColour_Sky_Forest);
    Biome::taiga = (new TaigaBiome(5))->setColor(0x0b6659)->setName(L"Taiga")->setLeafColor(0x4EBA31)->setTemperatureAndDownfall(0.25f, 0.8f)->setDepthAndScale(0.1f, 0.4f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Taiga, eMinecraftColour_Foliage_Taiga, eMinecraftColour_Water_Taiga,eMinecraftColour_Sky_Taiga);
    Biome::coldTaiga = (new TaigaBiome(30))->setColor(0x0b6659)->setName(L"Cold Taiga")->setLeafColor(0x4EBA31)->setSnowCovered()->setTemperatureAndDownfall(-0.5f, 0.4f)->setDepthAndScale(0.1f, 0.4f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Taiga, eMinecraftColour_Foliage_Taiga, eMinecraftColour_Water_Taiga,eMinecraftColour_Sky_Taiga);
    Biome::swampland = (new SwampBiome(6))->setColor(0x07F9B2)->setName(L"Swampland")->setLeafColor(0x8BAF48)->setDepthAndScale(-0.2f, 0.1f)->setTemperatureAndDownfall(0.8f, 0.9f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Swampland, eMinecraftColour_Foliage_Swampland, eMinecraftColour_Water_Swampland,eMinecraftColour_Sky_Swampland);
    Biome::river = (new RiverBiome(7))->setColor(0x0000ff)->setName(L"River")->setDepthAndScale(-0.5f, 0)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_River, eMinecraftColour_Foliage_River, eMinecraftColour_Water_River,eMinecraftColour_Sky_River);
    Biome::hell = (new HellBiome(8))->setColor(0xff0000)->setName(L"Hell")->setNoRain()->setTemperatureAndDownfall(2, 0)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Hell, eMinecraftColour_Foliage_Hell, eMinecraftColour_Water_Hell,eMinecraftColour_Sky_Hell);
    Biome::sky = (new TheEndBiome(9))->setColor(0x8080ff)->setName(L"Sky")->setNoRain()->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Sky, eMinecraftColour_Foliage_Sky, eMinecraftColour_Water_Sky,eMinecraftColour_Sky_Sky);
    Biome::frozenOcean = (new OceanBiome(10))->setColor(0x9090a0)->setName(L"Frozen Ocean")->setSnowCovered()->setDepthAndScale(-1, 0.5f)->setTemperatureAndDownfall(0, 0.5f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_FrozenOcean, eMinecraftColour_Foliage_FrozenOcean, eMinecraftColour_Water_FrozenOcean,eMinecraftColour_Sky_FrozenOcean);
    Biome::frozenRiver = (new RiverBiome(11))->setColor(0xa0a0ff)->setName(L"Frozen River")->setSnowCovered()->setDepthAndScale(-0.5f, 0)->setTemperatureAndDownfall(0, 0.5f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_FrozenRiver, eMinecraftColour_Foliage_FrozenRiver, eMinecraftColour_Water_FrozenRiver,eMinecraftColour_Sky_FrozenRiver);
    Biome::iceFlats = (new IceBiome(12))->setColor(0xffffff)->setName(L"Ice Plains")->setSnowCovered()->setTemperatureAndDownfall(0, 0.5f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_IcePlains, eMinecraftColour_Foliage_IcePlains, eMinecraftColour_Water_IcePlains,eMinecraftColour_Sky_IcePlains);
    Biome::iceMountains = (new IceBiome(13))->setColor(0xa0a0a0)->setName(L"Ice Mountains")->setSnowCovered()->setDepthAndScale(0.3f, 1.3f)->setTemperatureAndDownfall(0, 0.5f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_IceMountains, eMinecraftColour_Foliage_IceMountains, eMinecraftColour_Water_IceMountains,eMinecraftColour_Sky_IceMountains);
    Biome::mushroomIsland = (new MushroomIslandBiome(14))->setColor(0xff00ff)->setName(L"Mushroom Island")->setTemperatureAndDownfall(0.9f, 1.0f)->setDepthAndScale(0.2f, 1.0f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_MushroomIsland, eMinecraftColour_Foliage_MushroomIsland, eMinecraftColour_Water_MushroomIsland,eMinecraftColour_Sky_MushroomIsland);
    Biome::mushroomIslandShore = (new MushroomIslandBiome(15))->setColor(0xa000ff)->setName(L"Mushroom Island Shore")->setTemperatureAndDownfall(0.9f, 1.0f)->setDepthAndScale(-1, 0.1f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_MushroomIslandShore, eMinecraftColour_Foliage_MushroomIslandShore, eMinecraftColour_Water_MushroomIslandShore,eMinecraftColour_Sky_MushroomIslandShore);
    Biome::beaches = (new BeachBiome(16))->setColor(0xfade55)->setName(L"Beach")->setTemperatureAndDownfall(0.8f, 0.4f)->setDepthAndScale(0.0f, 0.1f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Beach, eMinecraftColour_Foliage_Beach, eMinecraftColour_Water_Beach,eMinecraftColour_Sky_Beach);
    Biome::desertHills = (new DesertBiome(17))->setColor(0xd25f12)->setName(L"Desert Hills")->setNoRain()->setTemperatureAndDownfall(2, 0)->setDepthAndScale(0.3f, 0.8f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_DesertHills, eMinecraftColour_Foliage_DesertHills, eMinecraftColour_Water_DesertHills,eMinecraftColour_Sky_DesertHills);
    Biome::forestHills = (new ForestBiome(18,0))->setColor(0x22551c)->setName(L"Forest Hills")->setLeafColor(0x4EBA31)->setTemperatureAndDownfall(0.7f, 0.8f)->setDepthAndScale(0.3f, 0.7f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_ForestHills, eMinecraftColour_Foliage_ForestHills, eMinecraftColour_Water_ForestHills,eMinecraftColour_Sky_ForestHills);
    Biome::taigaHills = (new TaigaBiome(19))->setColor(0x163933)->setName(L"Taiga Hills")->setLeafColor(0x4EBA31)->setTemperatureAndDownfall(0.25f, 0.8f)->setDepthAndScale(0.3f, 0.8f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_TaigaHills, eMinecraftColour_Foliage_TaigaHills, eMinecraftColour_Water_TaigaHills,eMinecraftColour_Sky_TaigaHills);
    
    Biome::smallerExtremeHills = (new ExtremeHillsBiome(20))->setColor(0x72789a)->setName(L"Extreme Hills Edge")->setDepthAndScale(0.2f, 0.8f)->setTemperatureAndDownfall(0.2f, 0.3f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_ExtremeHillsEdge, eMinecraftColour_Foliage_ExtremeHillsEdge, eMinecraftColour_Water_ExtremeHillsEdge,eMinecraftColour_Sky_ExtremeHillsEdge);
    Biome::jungle = (new JungleBiome(21, false))->setColor(0x537b09)->setName(L"Jungle")->setLeafColor(0x537b09)->setTemperatureAndDownfall(1.2f, 0.9f)->setDepthAndScale(0.2f, 0.4f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Jungle, eMinecraftColour_Foliage_Jungle, eMinecraftColour_Water_Jungle,eMinecraftColour_Sky_Jungle);
    Biome::jungleHills = (new JungleBiome(22, false))->setColor(0x2c4205)->setName(L"Jungle Hills")->setLeafColor(0x537b09)->setTemperatureAndDownfall(1.2f, 0.9f)->setDepthAndScale(1.8f, 0.5f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_JungleHills, eMinecraftColour_Foliage_JungleHills, eMinecraftColour_Water_JungleHills,eMinecraftColour_Sky_JungleHills);
    //23
    Biome::deepOcean= (new OceanBiome(24))->setName(L"Deep Ocean")->setDepthAndScale(-1.8,0.1f)->setColor(0x000070)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Ocean, eMinecraftColour_Foliage_Ocean, eMinecraftColour_Water_Ocean,eMinecraftColour_Sky_Ocean);;
    //25
    //26
    Biome::birchForest=(new ForestBiome(27, 2))->setColor(0x307444)->setName(L"Birch Forest")->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Forest, eMinecraftColour_Foliage_Birch, eMinecraftColour_Water_Forest, eMinecraftColour_Sky_Forest);
    Biome::birchForestHills=(new ForestBiome(28, 2))->setColor(0x1f5f32)->setName(L"Birch Forest Hills")->setDepthAndScale(0.45f, 0.3f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_ForestHills, eMinecraftColour_Foliage_Birch, eMinecraftColour_Water_Forest, eMinecraftColour_Sky_ForestHills);
    Biome::roofedForest = (new ForestBiome(29, 3))->setColor(0x056621)->setName(L"Roofed Forest")->setTemperatureAndDownfall(0.7f, 0.8f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_RoofedForest, eMinecraftColour_Foliage_RoofedForest, eMinecraftColour_Water_Forest, eMinecraftColour_Sky_Forest);
    //30
    //31
    Biome::coldTaigaHills = (new TaigaBiome(31))->setColor(0x163933)->setName(L"Cold Taiga Hills")->setLeafColor(0x4EBA31)->setSnowCovered()->setTemperatureAndDownfall(-0.5f, 0.4f)->setDepthAndScale(0.3f, 0.8f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_TaigaHills, eMinecraftColour_Foliage_TaigaHills, eMinecraftColour_Water_TaigaHills,eMinecraftColour_Sky_TaigaHills);
    Biome::megaTaiga = (new TaigaBiome(32,1))->setColor(0x0b6659)->setName(L"Mega Taiga")->setLeafColor(0x4EBA31)->setTemperatureAndDownfall(0.3f, 0.8f)->setDepthAndScale(0.1f, 0.4f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Taiga, eMinecraftColour_Foliage_Taiga, eMinecraftColour_Water_Taiga,eMinecraftColour_Sky_Taiga);
    Biome::megaTaigaHills = (new TaigaBiome(33,2))->setColor(0x0b6659)->setName(L"Mega Taiga Hills")->setLeafColor(0x4EBA31)->setTemperatureAndDownfall(0.3f, 0.8f)->setDepthAndScale(0.3f, 0.8f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_TaigaHills, eMinecraftColour_Foliage_TaigaHills, eMinecraftColour_Water_TaigaHills,eMinecraftColour_Sky_TaigaHills);
    Biome::savanna = (new SavannaBiome(35))->setColor(0xbda235)->setName(L"Savanna")->setNoRain()->setTemperatureAndDownfall(1.2f, 0.0f) ->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Savanna, eMinecraftColour_Foliage_Savanna, eMinecraftColour_Sky_Desert, eMinecraftColour_Sky_Desert);
    
    Biome::sunflowersPlains = (new PlainsBiome(129,true))->setColor(0x8db360)->setName(L"Sunflowers Plains")->setTemperatureAndDownfall(0.8f, 0.4f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Plains, eMinecraftColour_Foliage_Plains, eMinecraftColour_Water_Plains,eMinecraftColour_Sky_Plains);
    Biome::flowerForest = (new ForestBiome(132, 1))->setColor(0x056621)->setName(L"Flower Forest")->setTemperatureAndDownfall(0.7f, 0.8f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Forest, eMinecraftColour_Foliage_Forest, eMinecraftColour_Water_Forest, eMinecraftColour_Sky_Forest);
    Biome::iceSpikes = (new IceBiome(140,true))->setColor(0xffffff)->setName(L"Ice Spikes")->setSnowCovered()->setTemperatureAndDownfall(0, 0.5f)->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_IcePlains, eMinecraftColour_Foliage_IcePlains, eMinecraftColour_Water_IcePlains,eMinecraftColour_Sky_IcePlains);
    Biome::birchForestM=(new ForestBiome::MutatedBirchForestBiome(155, biomes[27]))->setColor(0x47875a)->setName(L"Birch Forest M")->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_Forest, eMinecraftColour_Foliage_Birch, eMinecraftColour_Water_Forest, eMinecraftColour_Sky_Forest);
    Biome::birchForestHillsM=(new ForestBiome::MutatedBirchForestBiome(156, biomes[28]))->setColor(0x47875a)->setName(L"Birch Forest Hills M")->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_ForestHills, eMinecraftColour_Foliage_Birch, eMinecraftColour_Water_Forest, eMinecraftColour_Sky_ForestHills);
    Biome::roofedForestM=(new ForestBiome::MutatedForestBiome(157, biomes[29]))->setColor(0x177a35)->setName(L"Roofed Forest M")->setLeafFoliageWaterSkyColor(eMinecraftColour_Grass_RoofedForest, eMinecraftColour_Foliage_RoofedForest, eMinecraftColour_Water_Forest, eMinecraftColour_Sky_Forest);
    

}
Biome::Biome(int id) : id(id)
{
    color = 0;
    topMaterial = static_cast<byte>(Tile::grass_Id);
    material = static_cast<byte>(Tile::dirt_Id);
    leafColor = 0x4EE031;
    _hasRain = true;
    depth = 0.1f;
    scale = 0.3f;
    temperature = 0.5f;
    downfall = 0.5f;
    decorator = nullptr;
    m_temperatureNoise = nullptr;
    GRASS_COLOR_NOISE = new PerlinNoise(new Random(2345L), 1);
    DOUBLE_PLANT_GENERATOR = new DoublePlantFeature(false);
    m_grassColor = eMinecraftColour_NOT_SET;
    m_foliageColor = eMinecraftColour_NOT_SET;
    m_waterColor = eMinecraftColour_NOT_SET;

    biomes[id] = this;
    decorator = createDecorator();

    // -- FIX FIORI ED ERBA GLOBALI --
    decorator->flowerCount = 2; // Valore base di fiori (esclusi biomi speciali che ne sovrascrivono il numero)
    decorator->grassCount = 1;  // Valore base di erba alta

    friendlies.push_back(new MobSpawnerData(eTYPE_SHEEP, 12, 4, 4));
    friendlies.push_back(new MobSpawnerData(eTYPE_PIG, 10, 4, 4));
    friendlies_chicken.push_back(new MobSpawnerData(eTYPE_CHICKEN, 10, 4, 4));
    friendlies.push_back(new MobSpawnerData(eTYPE_COW, 8, 4, 4));

    enemies.push_back(new MobSpawnerData(eTYPE_SPIDER, 10, 4, 4));
    enemies.push_back(new MobSpawnerData(eTYPE_ZOMBIE, 10, 4, 4));
    enemies.push_back(new MobSpawnerData(eTYPE_SKELETON, 10, 4, 4));
    enemies.push_back(new MobSpawnerData(eTYPE_CREEPER, 10, 4, 4));
    enemies.push_back(new MobSpawnerData(eTYPE_SLIME, 10, 4, 4));
    enemies.push_back(new MobSpawnerData(eTYPE_ENDERMAN, 1, 1, 4));
    enemies.push_back(new MobSpawnerData(eTYPE_WITCH, 1, 1, 1));

    waterFriendlies.push_back(new MobSpawnerData(eTYPE_SQUID, 10, 4, 4));
    ambientFriendlies.push_back(new MobSpawnerData(eTYPE_BAT, 10, 8, 8));
}

Biome::~Biome()
{
    if(decorator != nullptr) delete decorator;
    if(DOUBLE_PLANT_GENERATOR != nullptr) delete DOUBLE_PLANT_GENERATOR;
}

BiomeDecorator *Biome::createDecorator()
{
    return new BiomeDecorator(this);
}

Biome *Biome::setLeafFoliageWaterSkyColor(eMinecraftColour grassColor, eMinecraftColour foliageColor, eMinecraftColour waterColour, eMinecraftColour skyColour)
{
    m_grassColor = grassColor;
    m_foliageColor = foliageColor;
    m_waterColor = waterColour;
    m_skyColor = skyColour;
    return this;
}

Biome *Biome::setTemperatureAndDownfall(float temp, float downfall)
{
    temperature = temp;
    this->downfall = downfall;
    return this;
}

Biome *Biome::setDepthAndScale(float depth, float scale)
{
    this->depth = depth;
    this->scale = scale;
    return this;
}

Biome *Biome::setNoRain()
{
    _hasRain = false;
    return this;
}

Feature *Biome::getTreeFeature(Random *random)
{
    if (random->nextInt(10) == 0)
    {
        return new BasicTree(false);
    }
    return new TreeFeature(false);
}

Feature *Biome::getGrassFeature(Random *random)
{
    return new TallGrassFeature(Tile::tallgrass_Id, TallGrass::TALL_GRASS);
}

Biome *Biome::setSnowCovered()
{
    snowCovered = true;
    return this;
}

Biome *Biome::setName(const wstring &name)
{
    this->m_name = name;
    return this;
}

Biome *Biome::setLeafColor(int leafColor)
{
    this->leafColor = leafColor;
    return this;
}

Biome* Biome::setColor(int color, bool b) 
{
    this->color = color;
    return this;
}

int Biome::getSkyColor(float temp)
{
    return Minecraft::GetInstance()->getColourTable()->getColor( m_skyColor );
}

vector<Biome::MobSpawnerData *> *Biome::getMobs(MobCategory *category)
{
    if (category == MobCategory::monster) return &enemies;
    if (category == MobCategory::creature) return &friendlies;
    if (category == MobCategory::waterCreature) return &waterFriendlies;
    if (category == MobCategory::creature_chicken) return &friendlies_chicken;
    if (category == MobCategory::creature_wolf) return &friendlies_wolf;
    if (category == MobCategory::creature_mushroomcow) return &friendlies_mushroomcow;
    if (category == MobCategory::ambient) return &ambientFriendlies;
    return nullptr;
}

bool Biome::hasSnow()
{
    if( !_hasRain ) return false;
    if( getTemperature() >= 0.15f ) return false;
    return true;
}

bool Biome::hasRain()
{
    if( hasSnow() ) return false;
    return _hasRain;
}

bool Biome::isHumid()
{
    return downfall > .85f;
}

float Biome::getCreatureProbability() const
{
    return 0.1f;
}

int Biome::getDownfallInt()
{
    return static_cast<int>(downfall * 65536);
}

int Biome::getTemperatureInt()
{
    return static_cast<int>(temperature * 65536);
}

float Biome::getDownfall()
{
    return downfall;
}

float Biome::getTemperature()
{
    return temperature;
}

void Biome::decorate(Level *level, Random *random, int xo, int zo)
{
    decorator->decorate(level, random, xo, zo);
}

int Biome::getGrassColor() const
{
    return Minecraft::GetInstance()->getColourTable()->getColor( m_grassColor );
}

int Biome::getFoliageColor() const
{
    return Minecraft::GetInstance()->getColourTable()->getColor( m_foliageColor );
}

int Biome::getWaterColor()
{
    return Minecraft::GetInstance()->getColourTable()->getColor( m_waterColor );
}

float Biome::getTemperature(int x, int y, int z)
{
    if (y > 64)
    {
        float noiseTemp = 0.0f;
        if (m_temperatureNoise != nullptr) 
        {
            noiseTemp = (float)(m_temperatureNoise->getValue((double)x / 8.0, (double)z / 8.0) * 4.0);
        }
        return temperature - (noiseTemp + (float)(y - 64)) * 0.05f / 30.0f; 
    }
    
    return temperature;
}

void Biome::buildSurfaceAtDefault(Level *level, Random *random, byte* chunkBlocks, int x, int z, double noiseVal)
{
    byte topState = this->topMaterial;
    byte fillerState = this->material;
    
    int runDepth = -1;
    int noiseDepth = (int)(noiseVal / 3.0 + 3.0 + random->nextDouble() * 0.25);
    int localX = x & 15;
    int localZ = z & 15;

    for (int y = 255; y >= 0; --y)
    {
        int index = (localX * 16 + localZ) * 256 + y;

        if (y <= random->nextInt(5))
        {
            chunkBlocks[index] = static_cast<byte>(Tile::unbreakable_Id); 
            continue;
        }

        byte currentBlockId = chunkBlocks[index];

        if (currentBlockId == 0) 
        {
            runDepth = -1;
        }
        else if (currentBlockId == static_cast<byte>(Tile::stone_Id))
        {
            if (runDepth == -1)
            {
                if (noiseDepth <= 0)
                {
                    topState = 0; 
                    fillerState = static_cast<byte>(Tile::stone_Id);
                }
                else if (y >= 59 && y <= 64) 
                {
                    topState = this->topMaterial;
                    fillerState = this->material;
                }

                if (y < 63 && topState == 0)
                {
                    if (this->getTemperature(x, y, z) < 0.15f) {
                        topState = static_cast<byte>(Tile::ice_Id);
                    } else {
                        topState = static_cast<byte>(Tile::water_Id); 
                    }
                }

                runDepth = noiseDepth;

                if (y >= 62)
                {
                    chunkBlocks[index] = topState;
                }
                else if (y < 56 - noiseDepth)
                {
                    topState = 0;
                    fillerState = static_cast<byte>(Tile::stone_Id);
                    chunkBlocks[index] = static_cast<byte>(Tile::gravel_Id);
                }
                else
                {
                    chunkBlocks[index] = fillerState;
                }
            }
            else if (runDepth > 0)
            {
                --runDepth;
                chunkBlocks[index] = fillerState;

                if (runDepth == 0 && fillerState == static_cast<byte>(Tile::sand_Id))
                {
                    runDepth = random->nextInt(4) + (y - 63 > 0 ? y - 63 : 0);
                    fillerState = static_cast<byte>(Tile::sandStone_Id);
                }
            }
        }
    }
}

float Biome::getTemperature(const BlockPos& pos)
{
    return getTemperature(pos.getX(), pos.getY(), pos.getZ());
}


void Biome::buildSurfaceAtDefault(Level *level, Random *random, byte* chunkBlocks, const BlockPos& pos, double noiseVal)
{
    buildSurfaceAtDefault(level, random, chunkBlocks, pos.getX(), pos.getZ(), noiseVal);
}

bool Biome::isSame(const Biome* other) const
{
    if (!other) return false;
    return id == other->id;  
}

int Biome::getTemperatureCategory() const
{
    if (temperature < 0.2f) return 1;   
    if (temperature > 1.0f) return 3;   
    return 2;                           
}

void Biome::buildSurfaceAt(Level* level, Random* random, ChunkPrimer* primer, int x, int z, double noiseVal)
{
    buildSurfaceAtDefault(level, random, primer->getBlockIds(), x, z, noiseVal);
}

Feature *Biome::getFlowerFeature(Random *random, int x, int y, int z)
{
    
    if (random->nextInt(3) > 0)
    {
        return new FlowerFeature(Tile::flower_Id); 
    }
    return new FlowerFeature(Tile::rose_Id); 
}

int Biome::getRandomDoublePlantType(Random *random)
{
    
    int type = random->nextInt(10);
    if (type < 7) return 0; 
    if (type == 7) return 3; 
    if (type == 8) return 4; 
    return 2; 
}