#include "stdafx.h"
#include "..\Minecraft.World\net.minecraft.world.entity.monster.h"
#include "PhantomModel.h"
#include "ModelPart.h"
#include "../Minecraft.World/Phantom.h"

PhantomModel::PhantomModel() : Model()
{
    texWidth  = 64;
    texHeight = 64;

    
    body = new ModelPart(this, 0, 8);
    body->addBox(-3.0f, -2.0f, -8.0f, 5, 3, 9);

  
    head = new ModelPart(this, 0, 0);
    head->addBox(-4.0f, -2.0f, -5.0f, 7, 3, 5);

    
    leftWing = new ModelPart(this, 23, 12);
    leftWing->addBox(0.0f, 0.0f, 0.0f, 6, 2, 9);

    
    leftWingTip = new ModelPart(this, 16, 24);
    leftWingTip->addBox(0.0f, 0.0f, 0.0f, 13, 1, 9);

    
    rightWing = new ModelPart(this, 23, 12);
    rightWing->bMirror = true;
    rightWing->addBox(-6.0f, 0.0f, 0.0f, 6, 2, 9);

    
    rightWingTip = new ModelPart(this, 16, 24);
    rightWingTip->bMirror = true;
    rightWingTip->addBox(-13.0f, 0.0f, 0.0f, 13, 1, 9);

    
    tailBase = new ModelPart(this, 3, 20);
    tailBase->addBox(-2.0f, 0.0f, 0.0f, 3, 2, 6);

    
    tailTip = new ModelPart(this, 4, 29);
    tailTip->addBox(-1.0f, 0.0f, 0.0f, 1, 1, 6);

    
    body->addChild(head);
    body->addChild(leftWing);
    body->addChild(rightWing);
    body->addChild(tailBase);
    leftWing->addChild(leftWingTip);
    rightWing->addChild(rightWingTip);
    tailBase->addChild(tailTip);

    
    const float s = 1.0f / 16.0f;
    body->compile(s);
    head->compile(s);
    leftWing->compile(s);
    rightWing->compile(s);
    leftWingTip->compile(s);
    rightWingTip->compile(s);
    tailBase->compile(s);
    tailTip->compile(s);
}

int PhantomModel::modelVersion()
{
    return 3;
}

void PhantomModel::render(shared_ptr<Entity> entity,
                          float time, float r, float bob,
                          float yRot, float xRot,
                          float scale, bool usecompiled)
{
    shared_ptr<Phantom> phantom = dynamic_pointer_cast<Phantom>(entity);
    if (!phantom) return;

    
    float flapTime = (float)phantom->tickCount;

    
    const float rad = (float)(PI / 180.0);
    float f = flapTime * 7.448451f * rad;

    
    body->xRot = -0.1f;
    body->yRot =  0.0f;
    body->zRot =  0.0f;
    body->setPos(0.0f, 0.0f, 0.0f);

    
    head->xRot =  0.2f;
    head->yRot =  0.0f;
    head->zRot =  0.0f;
    head->setPos(0.0f, 1.0f, -7.0f);

    
    leftWing->setPos(2.0f, -2.0f, -8.0f);
    leftWing->xRot = 0.0f;
    leftWing->yRot = 0.0f;
    leftWing->zRot = 0.1f;   

    rightWing->setPos(-3.0f, -2.0f, -8.0f);
    rightWing->xRot = 0.0f;
    rightWing->yRot = 0.0f;
    rightWing->zRot = -0.1f;  

    leftWingTip->setPos(6.0f, 0.0f, 0.0f);
    leftWingTip->zRot = 0.1f;

    rightWingTip->setPos(-6.0f, 0.0f, 0.0f);
    rightWingTip->zRot = -0.1f;

    
    tailBase->setPos(0.0f, -2.0f, 1.0f);
    tailTip->setPos(0.0f, 0.5f, 6.0f);

    
    float flapAngle = cos(f) * 16.0f * rad;
    leftWing->zRot  += flapAngle;
    leftWingTip->zRot += flapAngle;
    rightWing->zRot  -= flapAngle;
    rightWingTip->zRot -= flapAngle;

    
    float tailAngle = -(5.0f + cos(f * 2.0f) * 5.0f) * rad;
    tailBase->xRot = tailAngle;
    tailTip->xRot  = tailAngle;

    
    body->render(scale, usecompiled);
}