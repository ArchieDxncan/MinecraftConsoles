#pragma once
#include "MobRenderer.h"

class PhantomModel;

class PhantomRenderer : public MobRenderer
{
    static ResourceLocation PHANTOM_LOCATION;
    static ResourceLocation PHANTOM_EYES_LOCATION;
    int modelVersion;

public:
    PhantomRenderer();

    virtual void render(shared_ptr<Entity> mob,
                        double x, double y, double z,
                        float rot, float a);

protected:
    virtual ResourceLocation *getTextureLocation(shared_ptr<Entity> mob);
    virtual void scale(shared_ptr<LivingEntity> mob, float a);
    virtual void setupPosition(shared_ptr<LivingEntity> mob,
                               double x, double y, double z);
    virtual void setupRotations(shared_ptr<LivingEntity> mob,
                                float bob, float bodyRot, float a);
    virtual int prepareArmor(shared_ptr<LivingEntity> mob, int layer, float a);
};