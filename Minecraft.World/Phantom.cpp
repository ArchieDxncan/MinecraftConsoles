
#include "stdafx.h"
#include "net.minecraft.world.h"
#include "net.minecraft.world.phys.h"
#include "net.minecraft.world.level.h"
#include "net.minecraft.world.entity.h"
#include "net.minecraft.world.entity.ai.attributes.h"
#include "net.minecraft.world.entity.ai.control.h"
#include "net.minecraft.world.entity.ai.goal.h"
#include "net.minecraft.world.entity.ai.goal.target.h"
#include "net.minecraft.world.entity.monster.h"
#include "net.minecraft.world.entity.player.h"
#include "net.minecraft.world.damagesource.h"
#include "net.minecraft.world.level.tile.h"
#include "LevelEvent.h"
#include "SoundTypes.h"
#include "Phantom.h"



class PhantomMoveControl : public MoveControl {
    Phantom* phantom;
public:
    PhantomMoveControl(Phantom* p) : MoveControl(p), phantom(p) {}

    void tick() override {
        if (phantom->horizontalCollision) {
            phantom->yRot += 180.0f;
            phantom->flySpeed = 0.1f;
        }

        double d0 = phantom->moveTargetX - phantom->x;
        double d1 = phantom->moveTargetY - phantom->y;
        double d2 = phantom->moveTargetZ - phantom->z;
        double d3 = sqrt(d0 * d0 + d2 * d2);

        if (abs(d3) > 1.0e-5) {
            double d4 = 1.0 - abs(d1 * 0.7) / d3;
            d0 *= d4;
            d2 *= d4;
            d3 = sqrt(d0 * d0 + d2 * d2);
            double d5 = sqrt(d0 * d0 + d2 * d2 + d1 * d1);
            
            float f1 = (float)(atan2(d2, d0) * 180.0 / PI);
            float f2 = Mth::wrapDegrees(phantom->yRot + 90.0f);
            float f3 = Mth::wrapDegrees(f1);
            
            float diff = Mth::wrapDegrees(f3 - f2);
            phantom->yRot = f2 + Mth::clamp(diff, -4.0f, 4.0f) - 90.0f;
            phantom->yBodyRot = phantom->yRot;

            if (abs(diff) < 3.0f) {
                phantom->flySpeed += Mth::clamp(1.8f - phantom->flySpeed, -0.005f * (1.8f / max(phantom->flySpeed, 0.001f)), 0.005f * (1.8f / max(phantom->flySpeed, 0.001f)));
            } else {
                phantom->flySpeed += Mth::clamp(0.2f - phantom->flySpeed, -0.025f, 0.025f);
            }

            float f4 = (float)(-(atan2(-d1, d3) * 180.0 / PI));
            phantom->xRot = f4;
            
            float f5_rad = (phantom->yRot + 90.0f) * (float)(PI / 180.0);
            float f4_rad = f4 * (float)(PI / 180.0);
            
            double d6 = phantom->flySpeed * cos(f5_rad) * abs(d0 / d5);
            double d7 = phantom->flySpeed * sin(f5_rad) * abs(d2 / d5);
            double d8 = phantom->flySpeed * sin(f4_rad) * abs(d1 / d5);
            
            phantom->xd += (d6 - phantom->xd) * 0.2;
            phantom->yd += (d8 - phantom->yd) * 0.2;
            phantom->zd += (d7 - phantom->zd) * 0.2;
        }
    }
};

class PhantomAttackStrategyGoal : public Goal {
    Phantom* phantom;
    int nextSweepTick;
public:
    PhantomAttackStrategyGoal(Phantom* p) : phantom(p), nextSweepTick(0) {}

    bool canUse() override {
        shared_ptr<LivingEntity> target = phantom->getTarget();
        return target != nullptr && target->isAlive();
    }

    void start() override {
        nextSweepTick = 10 * 20; // About 10 seconds
        phantom->attackPhase = Phantom::AttackPhase::CIRCLE;
        setAnchorAboveTarget();
    }

    void stop() override {
        if (phantom->hasAnchor) {
            phantom->anchorY = phantom->level->getTopSolidBlock(phantom->anchorX, phantom->anchorZ) + 10 + phantom->random->nextInt(20);
        }
    }

    void tick() override {
        if (phantom->attackPhase == Phantom::AttackPhase::CIRCLE) {
            nextSweepTick--;
            if (nextSweepTick <= 0) {
                phantom->attackPhase = Phantom::AttackPhase::SWOOP;
                setAnchorAboveTarget();
                nextSweepTick = (8 + phantom->random->nextInt(4)) * 20;
                phantom->level->levelEvent(nullptr, LevelEvent::SOUND_BAT_LIFTOFF, (int)phantom->x, (int)phantom->y, (int)phantom->z, 0);
            }
        }
    }

private:
    void setAnchorAboveTarget() {
        shared_ptr<LivingEntity> target = phantom->getTarget();
        if (target != nullptr) {
            phantom->anchorX = Mth::floor(target->x);
            phantom->anchorY = Mth::floor(target->y) + 20 + phantom->random->nextInt(20);
            phantom->anchorZ = Mth::floor(target->z);
            if (phantom->anchorY < phantom->level->seaLevel) {
                phantom->anchorY = phantom->level->seaLevel + 1;
            }
            phantom->hasAnchor = true;
        }
    }
};

class PhantomSweepAttackGoal : public Goal {
    Phantom* phantom;
public:
    PhantomSweepAttackGoal(Phantom* p) : phantom(p) {}

    bool canUse() override {
        return phantom->getTarget() != nullptr && phantom->attackPhase == Phantom::AttackPhase::SWOOP;
    }

    bool canContinueToUse() override {
        shared_ptr<LivingEntity> target = phantom->getTarget();
        if (target == nullptr || !target->isAlive()) return false;
        return canUse();
    }

    void stop() override {
        phantom->setTarget(nullptr);
        phantom->attackPhase = Phantom::AttackPhase::CIRCLE;
    }

    void tick() override {
        shared_ptr<LivingEntity> target = phantom->getTarget();
        if (target != nullptr) {
            phantom->moveTargetX = target->x;
            phantom->moveTargetY = target->y + (target->bbHeight * 0.5);
            phantom->moveTargetZ = target->z;

            // Hit control
            double dx = phantom->x - target->x;
            double dy = phantom->y - target->y;
            double dz = phantom->z - target->z;
            if (sqrt(dx*dx + dy*dy + dz*dz) < 2.0) { // Close range
                phantom->doHurtTarget(target);
                phantom->playSound(eSoundType_MOB_PHANTOM_BITE, 1.0f, 1.0f);  
                phantom->attackPhase = Phantom::AttackPhase::CIRCLE;
                phantom->level->levelEvent(nullptr, LevelEvent::SOUND_BAT_LIFTOFF, (int)phantom->x, (int)phantom->y, (int)phantom->z, 0);
            } else if (phantom->horizontalCollision || phantom->hurtTime > 0) {
                phantom->attackPhase = Phantom::AttackPhase::CIRCLE;
            }
        }
    }
};

class PhantomCircleAroundAnchorGoal : public Goal {
    Phantom* phantom;
public:
    PhantomCircleAroundAnchorGoal(Phantom* p) : phantom(p) {}

    bool canUse() override {
        return phantom->getTarget() == nullptr || phantom->attackPhase == Phantom::AttackPhase::CIRCLE;
    }

    void start() override {
        phantom->circleDistance = 5.0f + phantom->random->nextFloat() * 10.0f;
        phantom->circleHeight = -4.0f + phantom->random->nextFloat() * 9.0f;
        phantom->circleClockwise = phantom->random->nextBoolean() ? 1.0f : -1.0f;
        selectNext();
    }

    void tick() override {
        if (phantom->random->nextInt(350) == 0) phantom->circleHeight = -4.0f + phantom->random->nextFloat() * 9.0f;
        
        if (phantom->random->nextInt(250) == 0) {
            phantom->circleDistance++;
            if (phantom->circleDistance > 15.0f) {
                phantom->circleDistance = 5.0f;
                phantom->circleClockwise = -phantom->circleClockwise;
            }
        }

        if (phantom->random->nextInt(450) == 0) {
            phantom->circleAngle = phantom->random->nextFloat() * 2.0f * (float)PI;
            selectNext();
        }

        if (phantom->isTouchingMoveTarget()) selectNext();

        // Height control to avoid crashing
        if (phantom->moveTargetY < phantom->y && !phantom->level->isEmptyTile(Mth::floor(phantom->x), Mth::floor(phantom->y) - 1, Mth::floor(phantom->z))) {
            phantom->circleHeight = max(1.0f, phantom->circleHeight);
            selectNext();
        }
        if (phantom->moveTargetY > phantom->y && !phantom->level->isEmptyTile(Mth::floor(phantom->x), Mth::floor(phantom->y) + 1, Mth::floor(phantom->z))) {
            phantom->circleHeight = min(-1.0f, phantom->circleHeight);
            selectNext();
        }
    }

private:
    void selectNext() {
        if (!phantom->hasAnchor) {
            phantom->anchorX = Mth::floor(phantom->x);
            phantom->anchorY = Mth::floor(phantom->y);
            phantom->anchorZ = Mth::floor(phantom->z);
            phantom->hasAnchor = true;
        }

        phantom->circleAngle += phantom->circleClockwise * 15.0f * (float)(PI / 180.0);
        phantom->moveTargetX = phantom->anchorX + 0.5 + phantom->circleDistance * cos(phantom->circleAngle);
        phantom->moveTargetY = phantom->anchorY - 4.0 + phantom->circleHeight;
        phantom->moveTargetZ = phantom->anchorZ + 0.5 + phantom->circleDistance * sin(phantom->circleAngle);
        phantom->moveTargetInitialized = true;
    }
};


class PhantomAttackPlayerTargetGoal : public Goal {
    Phantom* phantom;
    int nextScanTick;
public:
    PhantomAttackPlayerTargetGoal(Phantom* p) : phantom(p), nextScanTick(20) {}

    bool canUse() override {
        if (nextScanTick > 0) {
            nextScanTick--;
            return false;
        }
        nextScanTick = 60;
        
        
        shared_ptr<Player> player = phantom->level->getNearestAttackablePlayer(phantom->shared_from_this(), 64.0);
        if (player != nullptr) {
            phantom->setTarget(player);
            return true;
        }
        return false;
    }

    bool canContinueToUse() override {
        shared_ptr<LivingEntity> target = phantom->getTarget();
        return target != nullptr && target->isAlive();
    }
};



Phantom::Phantom(Level *level) : FlyingMob(level)
{
    this->defineSynchedData();
    registerAttributes();
    setHealth(getMaxHealth());

    xpReward = Enemy::XP_REWARD_SMALL;
    setSize(0.9f, 0.5f);

    moveTargetX = 0.0;
    moveTargetY = 0.0;
    moveTargetZ = 0.0;
    moveTargetInitialized = false;

    hasAnchor = false;
    anchorX = anchorY = anchorZ = 0;
    anchorHeight = 10 + random->nextInt(10);

    attackPhase = AttackPhase::CIRCLE;
    circleAngle = 0.0f;
    circleDistance = 5.0f;
    circleHeight = 0.0f;
    circleClockwise = 1.0f;

    flySpeed = 0.1f;
    phantomSize = 0;

    
    delete moveControl;
    moveControl = new PhantomMoveControl(this);

    
    goalSelector.addGoal(1, new PhantomAttackStrategyGoal(this));
    goalSelector.addGoal(2, new PhantomSweepAttackGoal(this));
    goalSelector.addGoal(3, new PhantomCircleAroundAnchorGoal(this));
    
    
    targetSelector.addGoal(1, new PhantomAttackPlayerTargetGoal(this));
}

void Phantom::setPhantomSize(int size)
{
    phantomSize = Mth::clamp(size, 0, 64);
    float scale = 1.0f + 0.15f * phantomSize;
    setSize(0.9f * scale, 0.5f * scale);
    getAttribute(SharedMonsterAttributes::ATTACK_DAMAGE)->setBaseValue(6.0 + phantomSize);
}

void Phantom::defineSynchedData()
{
    FlyingMob::defineSynchedData();
}

void Phantom::registerAttributes()
{
    FlyingMob::registerAttributes();
    getAttribute(SharedMonsterAttributes::MAX_HEALTH)->setBaseValue(20.0);
    getAttributes()->registerAttribute(SharedMonsterAttributes::ATTACK_DAMAGE)->setBaseValue(6.0);
}

float Phantom::getSoundVolume()  { return 1.0f; }
float Phantom::getVoicePitch()   { return FlyingMob::getVoicePitch(); }
int Phantom::getAmbientSound() { return eSoundType_MOB_PHANTOM_IDLE;  }
int Phantom::getHurtSound()    { return eSoundType_MOB_PHANTOM_HURT;  }
int Phantom::getDeathSound()   { return eSoundType_MOB_PHANTOM_DEATH; }

bool Phantom::isPushable()                              { return false; }
void Phantom::doPush(shared_ptr<Entity> e)              { }
void Phantom::pushEntities()                            { }
bool Phantom::makeStepSound()                           { return false; }
void Phantom::causeFallDamage(float distance)           { }
void Phantom::checkFallDamage(double ya, bool onGround) { }
bool Phantom::isIgnoringTileTriggers()                  { return true;  }
bool Phantom::onLadder()                                { return false; }

void Phantom::tick()
{

    if (!level->isClientSide && isAlive()) 
    {
        
        if (Calendar::GetMonth() + 1 != 4 || Calendar::GetDayOfMonth() != 1) 
        {
            
             remove(); 
            return; 

            
            hurt(DamageSource::magic, getMaxHealth()); 
        }
    }

    FlyingMob::tick();
    yBodyRot = yRot;

    if (!level->isClientSide && isAlive() && tickCount % 20 == 0)
    {
        playSound(eSoundType_MOB_PHANTOM_FLAP, 1.0f, 1.0f);
    }


    if (!level->isClientSide && isAlive())
    {

        if (level->isDay() && !level->isRaining())
        {
            float br = getBrightness(1.0f);
            if (br > 0.5f && level->canSeeSky(
                    Mth::floor(x), static_cast<int>(floor(y + 0.5)), Mth::floor(z)))
            {
                setOnFire(8);
            }
        }
    }
}

void Phantom::initAnchor(int bx, int by, int bz)
{
    anchorX = bx;
    anchorY = by;
    anchorZ = bz;
    hasAnchor = true;
}

bool Phantom::isTouchingMoveTarget() const
{
    double dx = moveTargetX - x;
    double dy = moveTargetY - y;
    double dz = moveTargetZ - z;
    return (dx * dx + dy * dy + dz * dz) < 4.0;
}

bool Phantom::hurt(DamageSource *source, float dmg)
{
    if (isInvulnerable()) return false;
    return FlyingMob::hurt(source, dmg);
}

bool Phantom::doHurtTarget(shared_ptr<Entity> target)
{
    float damage = (float)getAttribute(SharedMonsterAttributes::ATTACK_DAMAGE)->getValue();
    return target->hurt(DamageSource::mobAttack(dynamic_pointer_cast<LivingEntity>(shared_from_this())), damage);
}

void Phantom::readAdditionalSaveData(CompoundTag *tag)
{
    FlyingMob::readAdditionalSaveData(tag);
    setPhantomSize(tag->getInt(L"Size"));
    if (tag->contains(L"AnchorX"))
    {
        anchorX = tag->getInt(L"AnchorX");
        anchorY = tag->getInt(L"AnchorY");
        anchorZ = tag->getInt(L"AnchorZ");
        hasAnchor = true;
    }
    anchorHeight          = 10 + random->nextInt(10);
    moveTargetInitialized = false;
}

void Phantom::addAdditonalSaveData(CompoundTag *entityTag)
{
    FlyingMob::addAdditonalSaveData(entityTag);
    entityTag->putInt(L"Size", phantomSize);
    if (hasAnchor)
    {
        entityTag->putInt(L"AnchorX", anchorX);
        entityTag->putInt(L"AnchorY", anchorY);
        entityTag->putInt(L"AnchorZ", anchorZ);
    }
}

bool Phantom::canSpawn()
{
    if (Calendar::GetMonth() + 1 != 4 || Calendar::GetDayOfMonth() != 1) {
        return false;
    }
    int xt = Mth::floor(x);
    int yt = static_cast<int>(y);
    int zt = Mth::floor(z);

    //if (yt < level->seaLevel)                     return false;
    if (level->isDay())                           return false;
    //if (!level->canSeeSky(xt, yt, zt))            return false;
    if (level->getRawBrightness(xt, yt, zt) >= 8) return false;

    int topSolidY = level->getTopSolidBlock(xt, zt);
    if (topSolidY < level->seaLevel) {
        topSolidY = level->seaLevel;
    }
    if (yt <= topSolidY + 5) 
    {
        int heightBoost = 20 + random->nextInt(15); 
        yt = topSolidY + heightBoost;
        
        
        setPos(x, (double)yt, z); 
    }
    if (!level->canSeeSky(xt, yt, zt)) return false;

    return FlyingMob::canSpawn();
}

