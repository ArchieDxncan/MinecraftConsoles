#include "stdafx.h"
#include "ArmorStandRenderer.h"
#include "Textures.h"
#include "HumanoidModel.h" 
#include "ArmorStandModel.h"
#include "..\Minecraft.World\ArmorStand.h"



class ArmorStandArmorModel : public HumanoidModel {
public:
    ArmorStandArmorModel(float scale) : HumanoidModel(scale) {}
    
    
    virtual void setupAnim(float time, float r, float bob, float yRot, float xRot, float scale, shared_ptr<Entity> entity, unsigned int uiBitmaskOverrideAnim = 0) override {
        
    }
};


static const float DEG_TO_RAD = 3.14159265f / 180.0f;

ResourceLocation ArmorStandRenderer::LOC_ARMOR_STAND = ResourceLocation(TN_MOB_ARMORSTAND);

ArmorStandRenderer::ArmorStandRenderer()
    : HumanoidMobRenderer(new ArmorStandModel(), 0.0f, 1.0f)
{
    createArmorParts();
}


ResourceLocation *ArmorStandRenderer::getTextureLocation(shared_ptr<Entity> entity)
{
    return &LOC_ARMOR_STAND;
}

void ArmorStandRenderer::createArmorParts()
{
    
    armorParts1 = new ArmorStandArmorModel(1.0f);
    armorParts2 = new ArmorStandArmorModel(0.5f);
}

void ArmorStandRenderer::render(shared_ptr<Entity> entity, double x, double y, double z, float rot, float a)
{
    HumanoidMobRenderer::render(entity, x, y, z, rot, a);
}

void ArmorStandRenderer::renderModel(shared_ptr<LivingEntity> mob, float wp, float ws, float bob,
                                     float headRotMinusBodyRot, float headRotx, float scale)
{
    
    shared_ptr<ArmorStand> stand = dynamic_pointer_cast<ArmorStand>(mob);
    if (!stand) return;

    ArmorStandModel *m = static_cast<ArmorStandModel *>(model);
    if (!m) return;
    
   
    Rotations h  = stand->getHeadPose();
    Rotations b  = stand->getBodyPose();
    Rotations la = stand->getLeftArmPose();
    Rotations ra = stand->getRightArmPose();
    Rotations ll = stand->getLeftLegPose();
    Rotations rl = stand->getRightLegPose();

    
    m->setupPose(
        h.x  * DEG_TO_RAD, h.y  * DEG_TO_RAD, h.z  * DEG_TO_RAD,
        b.x  * DEG_TO_RAD, b.y  * DEG_TO_RAD, b.z  * DEG_TO_RAD,
        la.x * DEG_TO_RAD, la.y * DEG_TO_RAD, la.z * DEG_TO_RAD,
        ra.x * DEG_TO_RAD, ra.y * DEG_TO_RAD, ra.z * DEG_TO_RAD,
        ll.x * DEG_TO_RAD, ll.y * DEG_TO_RAD, ll.z * DEG_TO_RAD,
        rl.x * DEG_TO_RAD, rl.y * DEG_TO_RAD, rl.z * DEG_TO_RAD
    );

   
   
    HumanoidModel *a1 = static_cast<HumanoidModel*>(armorParts1);
    if (a1) {
        a1->head->xRot = h.x * DEG_TO_RAD; a1->head->yRot = h.y * DEG_TO_RAD; a1->head->zRot = h.z * DEG_TO_RAD;
        if (a1->hair) { a1->hair->xRot = h.x * DEG_TO_RAD; a1->hair->yRot = h.y * DEG_TO_RAD; a1->hair->zRot = h.z * DEG_TO_RAD; }
        
        a1->body->xRot = b.x * DEG_TO_RAD; a1->body->yRot = b.y * DEG_TO_RAD; a1->body->zRot = b.z * DEG_TO_RAD;
        
        a1->arm0->xRot = ra.x * DEG_TO_RAD; a1->arm0->yRot = ra.y * DEG_TO_RAD; a1->arm0->zRot = ra.z * DEG_TO_RAD; // right
        a1->arm1->xRot = la.x * DEG_TO_RAD; a1->arm1->yRot = la.y * DEG_TO_RAD; a1->arm1->zRot = la.z * DEG_TO_RAD; // left
        
        a1->leg0->xRot = rl.x * DEG_TO_RAD; a1->leg0->yRot = rl.y * DEG_TO_RAD; a1->leg0->zRot = rl.z * DEG_TO_RAD; // right
        a1->leg1->xRot = ll.x * DEG_TO_RAD; a1->leg1->yRot = ll.y * DEG_TO_RAD; a1->leg1->zRot = ll.z * DEG_TO_RAD; // left
    }

    HumanoidModel *a2 = static_cast<HumanoidModel*>(armorParts2);
    if (a2) {
        a2->head->xRot = h.x * DEG_TO_RAD; a2->head->yRot = h.y * DEG_TO_RAD; a2->head->zRot = h.z * DEG_TO_RAD;
        if (a2->hair) { a2->hair->xRot = h.x * DEG_TO_RAD; a2->hair->yRot = h.y * DEG_TO_RAD; a2->hair->zRot = h.z * DEG_TO_RAD; }
        
        a2->body->xRot = b.x * DEG_TO_RAD; a2->body->yRot = b.y * DEG_TO_RAD; a2->body->zRot = b.z * DEG_TO_RAD;
        
        a2->arm0->xRot = ra.x * DEG_TO_RAD; a2->arm0->yRot = ra.y * DEG_TO_RAD; a2->arm0->zRot = ra.z * DEG_TO_RAD;
        a2->arm1->xRot = la.x * DEG_TO_RAD; a2->arm1->yRot = la.y * DEG_TO_RAD; a2->arm1->zRot = la.z * DEG_TO_RAD;
        
        a2->leg0->xRot = rl.x * DEG_TO_RAD; a2->leg0->yRot = rl.y * DEG_TO_RAD; a2->leg0->zRot = rl.z * DEG_TO_RAD;
        a2->leg1->xRot = ll.x * DEG_TO_RAD; a2->leg1->yRot = ll.y * DEG_TO_RAD; a2->leg1->zRot = ll.z * DEG_TO_RAD;
    }

    
    HumanoidMobRenderer::renderModel(mob, wp, ws, bob, headRotMinusBodyRot, headRotx, scale);
}


