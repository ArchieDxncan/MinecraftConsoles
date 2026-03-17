#pragma once

#include "LeafTile2.h"
#include "Bush.h"
#include "Sapling.h"

class Sapling2 : public Bush
{	
	friend class Tile;
public:
    
	static const int TYPE_ACACIA = 0;
	static const int TYPE_DARK_OAK = 1;

	static const int SAPLING_NAMES_SIZE = 2;
	static int SAPLING_NAMES[SAPLING_NAMES_SIZE];

private:
	static const wstring TEXTURE_NAMES[];
	Icon **icons;

	static const int TYPE_MASK = 7; 
	static const int AGE_BIT = 8;

protected:
	Sapling2(int id);

    bool isSapling(Level *level, int x, int y, int z, int type);
public:
	virtual void updateDefaultShape(); 
	virtual void tick(Level *level, int x, int y, int z, Random *random);
	virtual Icon *getTexture(int face, int data);
	void growTree(Level *level, int x, int y, int z, Random *random);
	virtual bool fertilize(Level *level, int x, int y, int z);

	virtual void registerIcons(IconRegister *iconRegister);
};