#pragma once
#include "Item.h"

class ArmorStandItem : public Item
{
public:
    ArmorStandItem(int id) : Item(id) {}
    virtual ~ArmorStandItem() {}

    virtual bool useOn(shared_ptr<ItemInstance> itemInstance, shared_ptr<Player> player,
                       Level *level, int x, int y, int z, int face,
                       float clickX, float clickY, float clickZ,
                       bool bTestUseOnOnly = false) override;
};