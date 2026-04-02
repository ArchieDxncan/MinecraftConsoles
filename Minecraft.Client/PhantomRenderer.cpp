#include "stdafx.h"
#include "..\Minecraft.World\net.minecraft.world.entity.monster.h"
#include "PhantomRenderer.h"
#include "PhantomModel.h"
#include "../Minecraft.World/Phantom.h"
#include "Textures.h"



ResourceLocation PhantomRenderer::PHANTOM_LOCATION = ResourceLocation(TN_MOB_PHANTOM);
ResourceLocation PhantomRenderer::PHANTOM_EYES_LOCATION = ResourceLocation(TN_MOB_PHANTOM_EYES);

PhantomRenderer::PhantomRenderer() : MobRenderer(new PhantomModel(), 0.5f)
{
    modelVersion = static_cast<PhantomModel *>(model)->modelVersion();
    this->setArmor(model);
}

void PhantomRenderer::render(shared_ptr<Entity> _mob,
                             double x, double y, double z,
                             float rot, float a)
{
    
    int currentVersion = static_cast<PhantomModel *>(model)->modelVersion();
    if (currentVersion != modelVersion)
    {
        modelVersion = currentVersion;
        model = new PhantomModel();
        this->setArmor(model);
    }

    MobRenderer::render(_mob, x, y, z, rot, a);
}

ResourceLocation *PhantomRenderer::getTextureLocation(shared_ptr<Entity> mob)
{
    return &PHANTOM_LOCATION;
}

void PhantomRenderer::scale(shared_ptr<LivingEntity> mob, float a)
{
    
     shared_ptr<Phantom> phantom = dynamic_pointer_cast<Phantom>(mob);
     if (phantom)
     {
         float s = 1.0f + 0.15f * phantom->getPhantomSize();
         glScalef(s, s, s);
     }

    //glScalef(0.35f, 0.35f, 0.35f);
}

void PhantomRenderer::setupPosition(shared_ptr<LivingEntity> mob,
                                    double x, double y, double z)
{
    MobRenderer::setupPosition(mob, x, y-0.5, z ); 
}

void PhantomRenderer::setupRotations(shared_ptr<LivingEntity> _mob,
                                     float bob, float bodyRot, float a)
{
    glTranslatef(0.0f, -0.5f, 0.0f);  
    glTranslatef(0.0f, cos(bob * 0.3f) * 0.1f, 0.0f);
    MobRenderer::setupRotations(_mob, bob, bodyRot, a);
}
int PhantomRenderer::prepareArmor(shared_ptr<LivingEntity> _mob, int layer, float a)
{
    shared_ptr<Phantom> mob = dynamic_pointer_cast<Phantom>(_mob);

    
    if (layer != 0) return -1;

    bindTexture(&PHANTOM_EYES_LOCATION);
    float br = 1;
    glEnable(GL_BLEND);
    
#ifdef _XBOX
    glDisable(GL_ALPHA_TEST);
#endif

    
    glBlendFunc(GL_ONE, GL_ONE); 
    glDisable(GL_LIGHTING); 

    if (mob->isInvisible())
    {
        glDepthMask(false);
    }
    else
    {
        glDepthMask(true);
    }

    if (SharedConstants::TEXTURE_LIGHTING)
    {
        int col = 0xf0f0; 
        int u = col % 65536;
        int v = col / 65536;

        glMultiTexCoord2f(GL_TEXTURE1, u / 1.0f, v / 1.0f);
        glColor4f(1, 1, 1, 1);
    }

    glEnable(GL_LIGHTING);
    glColor4f(1, 1, 1, br);
    
    return 1; 
}