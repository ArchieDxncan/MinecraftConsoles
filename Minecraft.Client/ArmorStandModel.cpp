#include "stdafx.h"
#include "ModelPart.h"
#include "ArmorStandModel.h"
#include "../Minecraft.World/ArmorStand.h"

ArmorStandModel::ArmorStandModel() : HumanoidModel() 
{
    texWidth  = 64;
    texHeight = 64;

    
    head = new ModelPart(this, 0, 0);
    head->addBox(-1.0F, -7.0F, -1.0F, 2, 7, 2, 0.0F);
    head->setPos(0.0F, 1.0F, 0.0F);
    head->compile(1.0f / 16.0f);

    hair = new ModelPart(this, 0, 0); 

    body = new ModelPart(this, 0, 26);
    body->addBox(-6.0F, 0.0F, -1.5F, 12, 3, 3, 0.0F);
    body->setPos(0.0F, 0.0F, 0.0F);
    body->compile(1.0f / 16.0f);

    arm0 = new ModelPart(this, 24, 0); // right
    arm0->addBox(-2.0F, -2.0F, -1.0F, 2, 12, 2, 0.0F);
    arm0->setPos(-5.0F, 2.0F, 0.0F);
    arm0->compile(1.0f / 16.0f);

    arm1 = new ModelPart(this, 32, 16); // left
    arm1->addBox(0.0F, -2.0F, -1.0F, 2, 12, 2, 0.0F);
    arm1->setPos(5.0F, 2.0F, 0.0F);
    arm1->compile(1.0f / 16.0f);

    leg0 = new ModelPart(this, 8, 0); // right
    leg0->addBox(-1.0F, 0.0F, -1.0F, 2, 11, 2, 0.0F);
    leg0->setPos(-1.9F, 12.0F, 0.0F);
    leg0->compile(1.0f / 16.0f);

    leg1 = new ModelPart(this, 40, 16); // left
    leg1->addBox(-1.0F, 0.0F, -1.0F, 2, 11, 2, 0.0F);
    leg1->setPos(1.9F, 12.0F, 0.0F);
    leg1->compile(1.0f / 16.0f);

    //sticks
    rightBodyStick = new ModelPart(this, 16, 0);
    rightBodyStick->addBox(-3.0F, 3.0F, -1.0F, 2, 7, 2, 0.0F);
    rightBodyStick->setPos(0.0F, 0.0F, 0.0F);
    rightBodyStick->compile(1.0f / 16.0f);

    leftBodyStick = new ModelPart(this, 48, 16);
    leftBodyStick->addBox(1.0F, 3.0F, -1.0F, 2, 7, 2, 0.0F);
    leftBodyStick->setPos(0.0F, 0.0F, 0.0F);
    leftBodyStick->compile(1.0f / 16.0f);

    shoulderStick = new ModelPart(this, 0, 48); 
    shoulderStick->addBox(-4.0F, 10.0F, -1.0F, 8, 2, 2, 0.0F);
    shoulderStick->setPos(0.0F, 0.0F, 0.0F);
    shoulderStick->compile(1.0f / 16.0f);

    basePlate = new ModelPart(this, 0, 32);
    basePlate->addBox(-6.0F, 11.0F, -6.0F, 12, 1, 12, 0.0F);
    basePlate->setPos(0.0F, 12.0F, 0.0F);
    basePlate->compile(1.0f / 16.0f);
}

void ArmorStandModel::setupPose(float hX, float hY, float hZ,
               float bX, float bY, float bZ,
               float lAX, float lAY, float lAZ,
               float rAX, float rAY, float rAZ,
               float lLX, float lLY, float lLZ,
               float rLX, float rLY, float rLZ)
{
    head->xRot = hX;  head->yRot = hY;  head->zRot = hZ;

    body->xRot = bX;  body->yRot = bY;  body->zRot = bZ;
    rightBodyStick->xRot = bX; rightBodyStick->yRot = bY; rightBodyStick->zRot = bZ;
    leftBodyStick->xRot  = bX; leftBodyStick->yRot  = bY; leftBodyStick->zRot  = bZ;
    shoulderStick->xRot  = bX; shoulderStick->yRot  = bY; shoulderStick->zRot  = bZ;

    arm1->xRot  = lAX; arm1->yRot  = lAY; arm1->zRot  = lAZ; // left
    arm0->xRot = rAX; arm0->yRot = rAY; arm0->zRot = rAZ; // right

    leg1->xRot  = lLX; leg1->yRot  = lLY; leg1->zRot  = lLZ; // left
    leg0->xRot = rLX; leg0->yRot = rLY; leg0->zRot = rLZ; // right
}

void ArmorStandModel::render(shared_ptr<Entity> entity, float time, float r, float bob, float yRot, float xRot, float scale, bool usecompiled)
{
    shared_ptr<ArmorStand> stand = dynamic_pointer_cast<ArmorStand>(entity);
    if (stand) {
        
        bool armsVisible = stand->showArms();
        arm0->visible = armsVisible; // right
        arm1->visible = armsVisible; // left

        
        // basePlate->visible = stand->showBasePlate();
    }

    
    HumanoidModel::render(entity, time, r, bob, yRot, xRot, scale, usecompiled);

  
    rightBodyStick->render(scale, usecompiled);
    leftBodyStick->render(scale, usecompiled);
    shoulderStick->render(scale, usecompiled);
    
   
    basePlate->render(scale, usecompiled); 
}

void ArmorStandModel::setupAnim(float time, float r, float bob, float yRot, float xRot, float scale, shared_ptr<Entity> entity, unsigned int uiBitmaskOverrideAnim)
{
    
}