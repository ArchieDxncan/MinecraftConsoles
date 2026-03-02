#include "stdafx.h"
#include "net.minecraft.world.entity.player.h"
#include "net.minecraft.world.entity.projectile.h"
#include "net.minecraft.world.item.h"
#include "net.minecraft.world.level.h"
#include "SnowballItem.h"
#include "SoundTypes.h"

SnowballItem::SnowballItem(int id) : Item(id)
{
    this->maxStackSize = 16;
}

std::shared_ptr<ItemInstance> SnowballItem::use(std::shared_ptr<ItemInstance> instance, Level *level, std::shared_ptr<Player> player)
{
	if (!player->abilities.instabuild)
	{
		instance->count--;
	}
    level->playSound((std::shared_ptr<Entity> ) player, eSoundType_RANDOM_BOW, 0.5f, 0.4f / (random->nextFloat() * 0.4f + 0.8f));
    if (!level->isClientSide) level->addEntity( std::shared_ptr<Snowball>( new Snowball(level, player) ) );
    return instance;
}