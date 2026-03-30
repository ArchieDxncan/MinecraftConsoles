#pragma once

#include "CropTile.h"

class BeetrootTile : public CropTile
{
	friend class ChunkRebuildData;

private:
	Icon *icons[4];

public:
	BeetrootTile(int id);

	Icon *getTexture(int face, int data);

protected:
	int getBaseSeedId();
	int getBasePlantId();

public:
	void registerIcons(IconRegister *iconRegister);
};
