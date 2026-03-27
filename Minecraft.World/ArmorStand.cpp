#include "stdafx.h"
#include "com.mojang.nbt.h"
#include "net.minecraft.world.entity.ai.attributes.h"
#include "net.minecraft.world.entity.ai.navigation.h"
#include "net.minecraft.world.entity.ai.goal.h"
#include "net.minecraft.world.level.tile.h"
#include "net.minecraft.world.level.h"
#include "net.minecraft.world.item.h"
#include "net.minecraft.world.entity.player.h"
#include "net.minecraft.world.damagesource.h"
#include "SynchedEntityData.h"
#include "SharedMonsterAttributes.h"
#include "ArmorStand.h"
#include "..\Minecraft.Client\Textures.h"
#include "MobCategory.h"
#include "ParticleTypes.h"

const Rotations ArmorStand::DEFAULT_HEAD_POSE      (  0,  0,  0);
const Rotations ArmorStand::DEFAULT_BODY_POSE      (  0,  0,  0);
const Rotations ArmorStand::DEFAULT_LEFT_ARM_POSE  (-10,  0,-10);
const Rotations ArmorStand::DEFAULT_RIGHT_ARM_POSE (-15,  0, 10);
const Rotations ArmorStand::DEFAULT_LEFT_LEG_POSE  ( -1,  0, -1);
const Rotations ArmorStand::DEFAULT_RIGHT_LEG_POSE (  1,  0,  1);

ArmorStand::ArmorStand(Level *level)
    : Mob(level)
{
    lastHit       = 0;
    disabledSlots = 0;
    invisible     = false;

    for (int i = 0; i < 5; i++) {
        equipment[i] = nullptr;
    }
   
    this->defineSynchedData();
    registerAttributes();
    setHealth(getMaxHealth());
    this->setSize(0.5f, 1.975f);
}

void ArmorStand::registerAttributes()
{
    LivingEntity::registerAttributes();
    getAttribute(SharedMonsterAttributes::MAX_HEALTH)->setBaseValue(20.0);
    getAttribute(SharedMonsterAttributes::MOVEMENT_SPEED)->setBaseValue(0.0);
    getAttribute(SharedMonsterAttributes::KNOCKBACK_RESISTANCE)->setBaseValue(1.0);
}

void ArmorStand::defineSynchedData()
{
    LivingEntity::defineSynchedData();
    entityData->define(DATA_CLIENT_FLAGS, static_cast<byte>(0));

    entityData->define(DATA_HEAD_POSE_X, DEFAULT_HEAD_POSE.x);
    entityData->define(DATA_HEAD_POSE_Y, DEFAULT_HEAD_POSE.y);
    entityData->define(DATA_HEAD_POSE_Z, DEFAULT_HEAD_POSE.z);

    entityData->define(DATA_BODY_POSE_X, DEFAULT_BODY_POSE.x);
    entityData->define(DATA_BODY_POSE_Y, DEFAULT_BODY_POSE.y);
    entityData->define(DATA_BODY_POSE_Z, DEFAULT_BODY_POSE.z);

    entityData->define(DATA_LEFT_ARM_X,  DEFAULT_LEFT_ARM_POSE.x);
    entityData->define(DATA_LEFT_ARM_Y,  DEFAULT_LEFT_ARM_POSE.y);
    entityData->define(DATA_LEFT_ARM_Z,  DEFAULT_LEFT_ARM_POSE.z);

    entityData->define(DATA_RIGHT_ARM_X, DEFAULT_RIGHT_ARM_POSE.x);
    entityData->define(DATA_RIGHT_ARM_Y, DEFAULT_RIGHT_ARM_POSE.y);
    entityData->define(DATA_RIGHT_ARM_Z, DEFAULT_RIGHT_ARM_POSE.z);

    entityData->define(DATA_LEFT_LEG_X,  DEFAULT_LEFT_LEG_POSE.x);
    entityData->define(DATA_LEFT_LEG_Y,  DEFAULT_LEFT_LEG_POSE.y);
    entityData->define(DATA_LEFT_LEG_Z,  DEFAULT_LEFT_LEG_POSE.z);

    entityData->define(DATA_RIGHT_LEG_X, DEFAULT_RIGHT_LEG_POSE.x);
    entityData->define(DATA_RIGHT_LEG_Y, DEFAULT_RIGHT_LEG_POSE.y);
    entityData->define(DATA_RIGHT_LEG_Z, DEFAULT_RIGHT_LEG_POSE.z);
}

void ArmorStand::tick()
{
    float lockedRot = this->yRot;
    if (this->isInWater()|| this->isInLava()) {
        this->yd -= 0.0392; 
    }
    LivingEntity::tick();
    this->yRot = lockedRot;
    this->yRotO = lockedRot;       
    this->yBodyRot = lockedRot;
    this->yBodyRotO = lockedRot;
    this->yHeadRot = lockedRot;
    this->yHeadRotO = lockedRot;

    
}

bool ArmorStand::interact(shared_ptr<Player> player)
{
    if (level->isClientSide) return true;
    if (isMarker()) return false;

    float dX = this->x - player->x;
    float dZ = this->z - player->z;
    float distHoriz = sqrt(dX * dX + dZ * dZ);
    float pitchRad = player->xRot * (3.14159265f / 180.0f);
    float lookYDiff = -tan(pitchRad) * distHoriz;
    float hitY = (player->y + 1.62f + lookYDiff) - this->y;

    
    int targetSlot = SLOT_WEAPON;
    if (hitY >= 0.1f && hitY < 0.55f) {
        targetSlot = SLOT_BOOTS;
    } else if (hitY >= 0.55f && hitY < 0.9f) {
        targetSlot = SLOT_LEGGINGS;
    } else if (hitY >= 0.9f && hitY < 1.6f) {
        targetSlot = SLOT_CHEST;
    } else if (hitY >= 1.6f) {
        targetSlot = SLOT_HELM;
    }

    shared_ptr<ItemInstance> playerItem = player->getCarriedItem();
    
  
    int slotToInteract = (playerItem != nullptr) ? getEquipmentSlotForItem(playerItem) : targetSlot;

    if (isSlotDisabled(slotToInteract)) return false;

    shared_ptr<ItemInstance> standItem = getItemInSlot(slotToInteract);

    
    if (playerItem != nullptr) {
        shared_ptr<ItemInstance> toPlace = ItemInstance::clone(playerItem);
        toPlace->count = 1;
        setEquippedSlot(slotToInteract, toPlace);

        if (standItem != nullptr) {
            if (playerItem->count > 1) {
                playerItem->remove(1);
                if (!player->inventory->add(standItem)) spawnAtLocation(standItem, 0.0f);
            } else {
                player->inventory->setItem(player->inventory->selected, standItem);
            }
        } else {
            playerItem->remove(1);
        }
    } 
    
    else if (standItem != nullptr) {
        player->inventory->setItem(player->inventory->selected, standItem);
        setEquippedSlot(slotToInteract, nullptr);
    }

    return true;
}
bool ArmorStand::hurt(DamageSource *source, float damage)
{
    if (isInvulnerable() || level->isClientSide || removed || isMarker()) return false;

    
    if (source != nullptr && source->getMsgId() == eEntityDamageType_Suffocate) return false;

    bool isFireDamage = source->isFire();

    
    if (dynamic_cast<EntityDamageSource *>(source) != nullptr) {
        shared_ptr<Entity> attacker = source->getEntity();
        if (attacker != nullptr && attacker->instanceof(eTYPE_PLAYER)) {
            if (dynamic_pointer_cast<Player>(attacker)->abilities.instabuild) {
                level->broadcastEntityEvent(shared_from_this(), (byte)31); 
                remove();
                return true;
            }
        }
    }

    
    long long now = (long long)tickCount;
    
    
    if (isFireDamage) {
        
        float currentHealth = this->getHealth() - 0.1f; 
        this->setHealth(currentHealth);

        if (currentHealth <= 0) {
            //level->broadcastEntityEvent(shared_from_this(), (byte)31);
            remove(); 
            return true;
        }
        
        
        return false; 
    }

    
    if (now - lastHit > 5) {
        level->broadcastEntityEvent(shared_from_this(), (byte)32);
        lastHit = now;
        return true;
    } else {
        
        level->broadcastEntityEvent(shared_from_this(), (byte)31); 
        remove();
        spawnAtLocation(Item::armor_stand_Id, 1); 
        for (int i = 0; i < 5; i++) {
            if (equipment[i] != nullptr) spawnAtLocation(equipment[i], 0.0f);
        }
        return true;
    }
}

bool ArmorStand::isPickable()
{
    return !removed && !isMarker();
}


byte ArmorStand::setBit(byte oldBit, int offset, bool value)
{
    if (value) oldBit = (byte)(oldBit | offset);
    else       oldBit = (byte)(oldBit & ~offset);
    return oldBit;
}

bool ArmorStand::isSmall()       const { return (entityData->getByte(DATA_CLIENT_FLAGS) & FLAG_SMALL)        != 0; }
bool ArmorStand::showArms()      const { return (entityData->getByte(DATA_CLIENT_FLAGS) & FLAG_SHOW_ARMS)    != 0; }
bool ArmorStand::showBasePlate() const { return (entityData->getByte(DATA_CLIENT_FLAGS) & FLAG_NO_BASEPLATE) == 0; }
bool ArmorStand::isMarker()      const { return (entityData->getByte(DATA_CLIENT_FLAGS) & FLAG_MARKER)       != 0; }

void ArmorStand::setSmall(bool v)       { entityData->set(DATA_CLIENT_FLAGS, setBit(entityData->getByte(DATA_CLIENT_FLAGS), FLAG_SMALL,        v)); }
void ArmorStand::setShowArms(bool v)    { entityData->set(DATA_CLIENT_FLAGS, setBit(entityData->getByte(DATA_CLIENT_FLAGS), FLAG_SHOW_ARMS,    v)); }
void ArmorStand::setNoBasePlate(bool v) { entityData->set(DATA_CLIENT_FLAGS, setBit(entityData->getByte(DATA_CLIENT_FLAGS), FLAG_NO_BASEPLATE, v)); }
void ArmorStand::setMarker(bool v)      { entityData->set(DATA_CLIENT_FLAGS, setBit(entityData->getByte(DATA_CLIENT_FLAGS), FLAG_MARKER,       v)); }

Rotations ArmorStand::readRotation(int slotX) const
{
    return Rotations(
        entityData->getFloat(slotX),
        entityData->getFloat(slotX + 1),
        entityData->getFloat(slotX + 2)
    );
}

void ArmorStand::writeRotation(int slotX, const Rotations &r)
{
    entityData->set(slotX,     r.x);
    entityData->set(slotX + 1, r.y);
    entityData->set(slotX + 2, r.z);
}

Rotations ArmorStand::getHeadPose()     const { return readRotation(DATA_HEAD_POSE_X); }
Rotations ArmorStand::getBodyPose()     const { return readRotation(DATA_BODY_POSE_X); }
Rotations ArmorStand::getLeftArmPose()  const { return readRotation(DATA_LEFT_ARM_X);  }
Rotations ArmorStand::getRightArmPose() const { return readRotation(DATA_RIGHT_ARM_X); }
Rotations ArmorStand::getLeftLegPose()  const { return readRotation(DATA_LEFT_LEG_X);  }
Rotations ArmorStand::getRightLegPose() const { return readRotation(DATA_RIGHT_LEG_X); }

void ArmorStand::setHeadPose    (const Rotations &r) { writeRotation(DATA_HEAD_POSE_X, r); }
void ArmorStand::setBodyPose    (const Rotations &r) { writeRotation(DATA_BODY_POSE_X, r); }
void ArmorStand::setLeftArmPose (const Rotations &r) { writeRotation(DATA_LEFT_ARM_X,  r); }
void ArmorStand::setRightArmPose(const Rotations &r) { writeRotation(DATA_RIGHT_ARM_X, r); }
void ArmorStand::setLeftLegPose (const Rotations &r) { writeRotation(DATA_LEFT_LEG_X,  r); }
void ArmorStand::setRightLegPose(const Rotations &r) { writeRotation(DATA_RIGHT_LEG_X, r); }

void ArmorStand::readAdditionalSaveData(CompoundTag *tag)
{
    LivingEntity::readAdditionalSaveData(tag);

    invisible     = tag->getBoolean(L"Invisible");
    disabledSlots = tag->getInt(L"DisabledSlots");
    setInvisible(invisible);
    setSmall      (tag->getBoolean(L"Small"));
    setShowArms   (tag->getBoolean(L"ShowArms"));
    setNoBasePlate(tag->getBoolean(L"NoBasePlate"));
    setMarker     (tag->getBoolean(L"Marker"));
    
    if (tag->contains(L"Pose"))
    {
        CompoundTag *pose = tag->getCompound(L"Pose");
        auto loadRot = [&](const wchar_t *key, const Rotations &def) -> Rotations {
            if (!pose->contains(key)) return def;
            ListTag<FloatTag> *list = (ListTag<FloatTag> *)pose->getList(key);
            if (!list || list->size() < 3) return def;
            return Rotations(list->get(0)->data, list->get(1)->data, list->get(2)->data);
        };
        setHeadPose    (loadRot(L"Head",     DEFAULT_HEAD_POSE));
        setBodyPose    (loadRot(L"Body",     DEFAULT_BODY_POSE));
        setLeftArmPose (loadRot(L"LeftArm",  DEFAULT_LEFT_ARM_POSE));
        setRightArmPose(loadRot(L"RightArm", DEFAULT_RIGHT_ARM_POSE));
        setLeftLegPose (loadRot(L"LeftLeg",  DEFAULT_LEFT_LEG_POSE));
        setRightLegPose(loadRot(L"RightLeg", DEFAULT_RIGHT_LEG_POSE));
    }

    if (tag->contains(L"ArmorStandEquipment")) 
    {
        ListTag<CompoundTag> *eqList = (ListTag<CompoundTag> *)tag->getList(L"ArmorStandEquipment");
        if (eqList) {
            for (int i = 0; i < 5 && i < eqList->size(); i++) {
                CompoundTag *itemTag = eqList->get(i);
                if (itemTag->contains(L"id")) {
                   equipment[i] = ItemInstance::fromTag(itemTag);
                } else {
                    equipment[i] = nullptr;
                }
            }
        }
    }
}

void ArmorStand::addAdditonalSaveData(CompoundTag *tag)
{
    LivingEntity::addAdditonalSaveData(tag);

    tag->putBoolean(L"Invisible",    isInvisible());
    tag->putBoolean(L"Small",        isSmall());
    tag->putBoolean(L"ShowArms",     showArms());
    tag->putBoolean(L"NoBasePlate",  !showBasePlate());
    tag->putInt    (L"DisabledSlots", disabledSlots);
    if (isMarker()) tag->putBoolean(L"Marker", true);

    auto saveRot = [](const Rotations &r) -> ListTag<FloatTag> * {
        ListTag<FloatTag> *list = new ListTag<FloatTag>();
        list->add(new FloatTag(L"", r.x));
        list->add(new FloatTag(L"", r.y));
        list->add(new FloatTag(L"", r.z));
        return list;
    };

    CompoundTag *pose = new CompoundTag();
    pose->put(L"Head",     saveRot(getHeadPose()));
    pose->put(L"Body",     saveRot(getBodyPose()));
    pose->put(L"LeftArm",  saveRot(getLeftArmPose()));
    pose->put(L"RightArm", saveRot(getRightArmPose()));
    pose->put(L"LeftLeg",  saveRot(getLeftLegPose()));
    pose->put(L"RightLeg", saveRot(getRightLegPose()));
    tag->put(L"Pose", pose);

    ListTag<CompoundTag> *eqList = new ListTag<CompoundTag>();
    for (int i = 0; i < 5; i++) {
        CompoundTag *itemTag = new CompoundTag();
        if (equipment[i] != nullptr) {
            equipment[i]->save(itemTag);
        }
        eqList->add(itemTag);
    }
    tag->put(L"ArmorStandEquipment", eqList);
}

shared_ptr<ItemInstance> ArmorStand::getCarriedItem()
{
    return equipment[SLOT_WEAPON];
}

shared_ptr<ItemInstance> ArmorStand::getCarried(int slot)
{
    if (slot == SLOT_WEAPON) return getCarriedItem();
    else if (slot >= SLOT_BOOTS && slot <= SLOT_HELM) return getArmor(slot - 1);
    return nullptr;
}

shared_ptr<ItemInstance> ArmorStand::getArmor(int pos)
{
    if (pos >= 0 && pos < 4) {
        return equipment[pos + 1];
    }
    return nullptr;
}

void ArmorStand::setEquippedSlot(int slot, shared_ptr<ItemInstance> item)
{
    if (slot >= 0 && slot < 5) {
        equipment[slot] = item;
    }
}

ItemInstanceArray ArmorStand::getEquipmentSlots()
{
    return equipment;
}

int ArmorStand::getEquipmentSlotForItem(shared_ptr<ItemInstance> item) const
{
    if (!item) return SLOT_WEAPON;
    
    int id = item->id;
    
    const int HELMET_IDS[] = {298, 302, 306, 310, 314};
    const int CHEST_IDS[]  = {299, 303, 307, 311, 315};
    const int LEGS_IDS[]   = {300, 304, 308, 312, 316};
    const int BOOTS_IDS[]  = {301, 305, 309, 313, 317};
    const int HEAD_IDS[]   = {397, 144, 145, 146,  86}; 
    
    for (int helm : HELMET_IDS) if (id == helm) return SLOT_HELM;
    for (int chest : CHEST_IDS) if (id == chest) return SLOT_CHEST;
    for (int legs : LEGS_IDS) if (id == legs) return SLOT_LEGGINGS;
    for (int boots : BOOTS_IDS) if (id == boots) return SLOT_BOOTS;
    for (int head : HEAD_IDS) if (id == head) return SLOT_HELM;
    
    return SLOT_WEAPON;
}

bool ArmorStand::isSlotDisabled(int slot) const
{
    if (slot == SLOT_WEAPON && !showArms()) return true;
    return false;
}

shared_ptr<ItemInstance> ArmorStand::getItemInSlot(int slot)
{
    if (slot == SLOT_WEAPON) {
        return getCarriedItem();
    } else if (slot >= SLOT_BOOTS && slot <= SLOT_HELM) {
        return getArmor(slot - 1);
    }
    return nullptr;
}


void ArmorStand::handleEntityEvent(byte id)
{
    if (id == 31) 
    {
        int woodId = (Tile::wood != nullptr) ? Tile::wood->id : 5;
        ePARTICLE_TYPE woodParticle = PARTICLE_TILECRACK(woodId, 0);

        for (int i = 0; i < 10; i++) 
    {
        
        double xa = random->nextGaussian() * 0.01;
        double ya = -0.05;
        double za = random->nextGaussian() * 0.01;

       
        double px = x + (random->nextFloat() * 0.1f - 0.05f);
        
        
        double py = y + (bbHeight * 0.6f) + (random->nextFloat() * 0.2f);
        
        double pz = z + (random->nextFloat() * 0.1f - 0.05f);

        level->addParticle(woodParticle, px, py, pz, xa, ya, za);
    }
        
        
        
    }
    else if (id == 32)
        lastHit = (long long)tickCount;
    else
        LivingEntity::handleEntityEvent(id);
}

bool ArmorStand::updateInWaterState()
{
    
    return Entity::updateInWaterState(); 
}

