#pragma once
#include "Model.h"

class PhantomModel : public Model
{
private:
    ModelPart *body;
    ModelPart *head;
    ModelPart *leftWing;
    ModelPart *leftWingTip;
    ModelPart *rightWing;
    ModelPart *rightWingTip;
    ModelPart *tailBase;
    ModelPart *tailTip;

public:
    PhantomModel();
    int modelVersion();
    virtual void render(shared_ptr<Entity> entity,
                        float time, float r, float bob,
                        float yRot, float xRot,
                        float scale, bool usecompiled);
};
