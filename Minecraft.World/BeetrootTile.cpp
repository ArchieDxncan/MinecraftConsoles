#include "stdafx.h"
#include "IconRegister.h"
#include "net.minecraft.world.item.h"
#include "net.minecraft.world.level.h"
#include "BeetrootTile.h"

BeetrootTile::BeetrootTile(int id) : CropTile(id)
{
}

Icon *BeetrootTile::getTexture(int face, int data)
{
	if (data < 7)
	{
		if (data == 6)
		{
			data = 5;
		}
		return icons[data >> 1];
	}
	return icons[3];
}

int BeetrootTile::getBaseSeedId()
{
	return Item::beetroot_seeds_Id;
}

int BeetrootTile::getBasePlantId()
{
	return Item::beetroot_Id;
}

void BeetrootTile::registerIcons(IconRegister *iconRegister)
{
	for (int i = 0; i < 4; i++)
	{
		icons[i] = iconRegister->registerIcon(getIconName() + L"_stage_" + std::to_wstring(i));
	}
}
