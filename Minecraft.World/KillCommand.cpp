#include "stdafx.h"
#include "net.minecraft.commands.h"
#include "net.minecraft.world.entity.player.h"
#include "net.minecraft.world.damagesource.h"
#include "KillCommand.h"

EGameCommand KillCommand::getId()
{
	return eGameCommand_Kill;
}

void KillCommand::execute(std::shared_ptr<CommandSender> source, byteArray commandData)
{
	std::shared_ptr<Player> player = std::dynamic_pointer_cast<Player>(source);

	player->hurt(DamageSource::outOfWorld, 1000);

	source->sendMessage(L"Ouch. That look like it hurt.");
}