#pragma once
#include "HumanoidMobRenderer.h" 
#include "ArmorStandModel.h"
#include "..\Minecraft.World\ArmorStand.h"

class ArmorStandRenderer : public HumanoidMobRenderer
{
private:
    static ResourceLocation LOC_ARMOR_STAND;

public:
    ArmorStandRenderer();
    virtual ~ArmorStandRenderer() {}
    virtual void render(shared_ptr<Entity> entity, double x, double y, double z, float rot, float a) override;
    virtual ResourceLocation *getTextureLocation(shared_ptr<Entity> entity) override;

protected:
    
    virtual void createArmorParts() override;

    
    virtual void renderModel(shared_ptr<LivingEntity> mob, float wp, float ws, float bob,
                             float headRotMinusBodyRot, float headRotx, float scale) override;
};