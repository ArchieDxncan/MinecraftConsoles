#pragma once
#include "Mob.h"
#include "SynchedEntityData.h"

struct Rotations
{
    float x, y, z;
    Rotations() : x(0), y(0), z(0) {}
    Rotations(float x, float y, float z) : x(x), y(y), z(z) {}
};

class ArmorStand : public Mob
{
public:
    eINSTANCEOF GetType() { return eTYPE_ARMORSTAND; }
    static Entity *create(Level *level) { return new ArmorStand(level); }

    static const Rotations DEFAULT_HEAD_POSE;
    static const Rotations DEFAULT_BODY_POSE;
    static const Rotations DEFAULT_LEFT_ARM_POSE;
    static const Rotations DEFAULT_RIGHT_ARM_POSE;
    static const Rotations DEFAULT_LEFT_LEG_POSE;
    static const Rotations DEFAULT_RIGHT_LEG_POSE;

private:
    static const int DATA_CLIENT_FLAGS  = 10;
    static const int DATA_HEAD_POSE_X   = 11;
    static const int DATA_HEAD_POSE_Y   = 12;
    static const int DATA_HEAD_POSE_Z   = 13;
    static const int DATA_BODY_POSE_X   = 14;
    static const int DATA_BODY_POSE_Y   = 15;
    static const int DATA_BODY_POSE_Z   = 16;
    static const int DATA_LEFT_ARM_X    = 17;
    static const int DATA_LEFT_ARM_Y    = 18;
    static const int DATA_LEFT_ARM_Z    = 19;
    static const int DATA_RIGHT_ARM_X   = 20;
    static const int DATA_RIGHT_ARM_Y   = 21;
    static const int DATA_RIGHT_ARM_Z   = 22;
    static const int DATA_LEFT_LEG_X    = 23;
    static const int DATA_LEFT_LEG_Y    = 24;
    static const int DATA_LEFT_LEG_Z    = 25;
    static const int DATA_RIGHT_LEG_X   = 26;
    static const int DATA_RIGHT_LEG_Y   = 27;
    static const int DATA_RIGHT_LEG_Z   = 28;

    static const int FLAG_SMALL        = 1;
    static const int FLAG_SHOW_ARMS    = 4;
    static const int FLAG_NO_BASEPLATE = 8;
    static const int FLAG_MARKER       = 16;

public:
    long long lastHit;

private:
    int  disabledSlots;
    bool invisible;

public:
    ArmorStand(Level *level);
    virtual ~ArmorStand() {}

    bool isSmall()       const;
    bool showArms()      const;
    bool showBasePlate() const;
    bool isMarker()      const;
    void setSmall(bool v);
    void setShowArms(bool v);
    void setNoBasePlate(bool v);
    void setMarker(bool v);

    Rotations getHeadPose()     const;
    Rotations getBodyPose()     const;
    Rotations getLeftArmPose()  const;
    Rotations getRightArmPose() const;
    Rotations getLeftLegPose()  const;
    Rotations getRightLegPose() const;

    void setHeadPose    (const Rotations &r);
    void setBodyPose    (const Rotations &r);
    void setLeftArmPose (const Rotations &r);
    void setRightArmPose(const Rotations &r);
    void setLeftLegPose (const Rotations &r);
    void setRightLegPose(const Rotations &r);

    virtual bool useNewAi() override { return false; }
    virtual void tick() override;
    virtual bool hurt(DamageSource *source, float damage) override;
    virtual bool interact(shared_ptr<Player> player) override;

    virtual bool isPickable() override;
    virtual bool isPushable() override { return false; }
    virtual void push(shared_ptr<Entity> entity) override {}
    virtual void push(double xa, double ya, double za) override {}
    virtual bool isAttackable() override { return true; }
    virtual bool isBaby() override { return isSmall(); }
    virtual bool shouldShowName() override { return false; }
    virtual wstring getAName() override { return L""; }
    virtual wstring getDisplayName() override { return L""; }
    virtual wstring getNetworkName() override { return L""; }

    virtual shared_ptr<ItemInstance> getCarriedItem() override;
    virtual shared_ptr<ItemInstance> getCarried(int slot) override;
    virtual shared_ptr<ItemInstance> getArmor(int pos) override;
    virtual void setEquippedSlot(int slot, shared_ptr<ItemInstance> item) override;
    virtual ItemInstanceArray getEquipmentSlots() override;

    virtual void readAdditionalSaveData(CompoundTag *tag) override;
    virtual void addAdditonalSaveData(CompoundTag *tag)   override;
    virtual void handleEntityEvent(byte eventId) override;

protected:
    virtual void defineSynchedData() override;
    virtual void registerAttributes()  override;

    virtual int   getHurtSound()  override { return -1; }
    virtual int   getDeathSound() override { return -1; }
    virtual float getSoundVolume() override { return 0.0f; }
    virtual bool  makeStepSound() override { return false; }

private:
    byte setBit(byte oldBit, int offset, bool value);
    Rotations readRotation(int slotX) const;
    void      writeRotation(int slotX, const Rotations &r);

    int getEquipmentSlotForItem(shared_ptr<ItemInstance> item) const;
    bool isSlotDisabled(int slot) const;
    shared_ptr<ItemInstance> getItemInSlot(int slot);
};