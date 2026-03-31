#pragma once
using namespace std;
#include "FlyingMob.h"
#include "Enemy.h"

class Level;
class CompoundTag;
class DamageSource;
class Player;

class Phantom : public FlyingMob, public Enemy
{
   
    friend class PhantomMoveControl;
    friend class PhantomAttackStrategyGoal;
    friend class PhantomCircleAroundAnchorGoal;
    friend class PhantomSweepAttackGoal;

public:
    eINSTANCEOF GetType() { return eTYPE_PHANTOM; }
    static Entity *create(Level *level) { return new Phantom(level); }
    enum class AttackPhase { CIRCLE, SWOOP };

private:
    int phantomSize;

   
    double moveTargetX, moveTargetY, moveTargetZ;
    bool   moveTargetInitialized;

    // Orbit anchor
    int  anchorX, anchorY, anchorZ;
    bool hasAnchor;
    int  anchorHeight;

    // Attack
    AttackPhase attackPhase;

    //Circle
    float circleAngle;
    float circleDistance;
    float circleHeight;
    float circleClockwise;

    float flySpeed;

public:
    Phantom(Level *level);

    int         getPhantomSize() const { return phantomSize; }
    void        setPhantomSize(int size);
    AttackPhase getAttackPhase() const { return attackPhase; }

    virtual bool  isPushable();
    virtual void  tick();
    virtual bool  hurt(DamageSource *source, float dmg);
    virtual void  readAdditionalSaveData(CompoundTag *tag);
    virtual void  addAdditonalSaveData(CompoundTag *entityTag);
    virtual bool  canSpawn();
    virtual bool  doHurtTarget(shared_ptr<Entity> target) override;

protected:
    virtual void  defineSynchedData();
    virtual void  registerAttributes();
    virtual float getSoundVolume();
    virtual float getVoicePitch();
    virtual int   getAmbientSound();
    virtual int   getHurtSound();
    virtual int   getDeathSound();
    
    
    virtual bool  useNewAi() override { return true; }

    virtual void  doPush(shared_ptr<Entity> e);
    virtual void  pushEntities();
    virtual bool  makeStepSound();
    virtual void  causeFallDamage(float distance);
    virtual void  checkFallDamage(double ya, bool onGround);
    virtual bool  isIgnoringTileTriggers();
    virtual bool  onLadder();

private:
    void  initAnchor(int bx, int by, int bz);
    bool  isTouchingMoveTarget() const;
};