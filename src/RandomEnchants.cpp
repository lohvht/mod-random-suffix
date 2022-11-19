/*
* Converted from the original LUA script to a module for Azerothcore(Sunwell) :D
*/
#include "ScriptMgr.h"
#include "Player.h"
#include "Configuration/Config.h"
#include "Chat.h"
#include "Item.h"

// DEFAULT VALUES

// Our random enchants can go up to 5 slots and they occupy the same slots as the stats given from
// random props. i.e. If there are 2 slots occupied from random properties, then we have a max of
// 5 - 2 = 3 slots. 
#define MAX_RAND_ENCHANT_SLOTS 5

double default_enchant_pcts[MAX_RAND_ENCHANT_SLOTS] = {30.0, 35.0, 40.0, 45.0, 50.0};

bool default_announce_on_log = true;
bool default_debug = false;
bool default_on_loot = true;
bool default_on_create = true;
bool default_on_quest_reward = true;
bool default_on_group_roll_reward_item = true;
bool default_on_vendor_purchase = true;
bool default_on_all_items_created = true;
bool default_use_new_random_enchant_system = true;
bool default_roll_player_class_preference = false;
std::string default_login_message ="This server is running a RandomEnchants Module.";

std::vector<EnchantmentSlot> default_allowed_rand_enchant_slots = {
    PROP_ENCHANTMENT_SLOT_4,
    PROP_ENCHANTMENT_SLOT_3,
    PROP_ENCHANTMENT_SLOT_2,
    PROP_ENCHANTMENT_SLOT_1,
    PROP_ENCHANTMENT_SLOT_0,
};

// CONFIGURATION

double config_enchant_pcts[MAX_RAND_ENCHANT_SLOTS] = {
    default_enchant_pcts[0],
    default_enchant_pcts[1],
    default_enchant_pcts[2],
    default_enchant_pcts[3],
    default_enchant_pcts[4],
};
bool config_announce_on_log = default_announce_on_log;
bool config_debug = default_debug;
bool config_on_loot = default_on_loot;
bool config_on_create = default_on_create;
bool config_on_quest_reward = default_on_quest_reward;
bool config_on_group_roll_reward_item = default_on_group_roll_reward_item;
bool config_on_vendor_purchase = default_on_vendor_purchase;
// bool config_on_all_items_created = default_on_all_items_created;
bool config_use_new_random_enchant_system = default_use_new_random_enchant_system;
bool config_roll_player_class_preference = default_roll_player_class_preference;
std::string config_login_message = default_login_message;

// UTILS
enum EnchantCategory
{
    ENCH_CAT_STRENGTH          = 0,  // TITLE strength users
    ENCH_CAT_AGILITY           = 1,  // TITLE agi users
    ENCH_CAT_INTELLECT         = 2,  // TITLE int users
    ENCH_CAT_TANK_DEFENSE      = 3,  // TITLE tanks with defense stats
    ENCH_CAT_TANK_SHIELD_BLOCK = 4,  // TITLE tanks that use shields // TODO: Maybe not needed with ENCH_CAT_TANK_DEFENSE if the above FIX for itemSubclass is done properly
    ENCH_CAT_MELEE             = 5,  // TITLE uses expertise, melee damage
    ENCH_CAT_RANGED            = 6,  // TITLE ranged dps (primarily hunter)
    ENCH_CAT_CASTER            = 7,  // TITLE casters, mainly spell power
    ENCH_CAT_HOLY_DMG          = 8,  // TITLE Holy damage
    ENCH_CAT_SHADOW_DMG        = 9,  // TITLE Shadow damage
    ENCH_CAT_FROST_DMG         = 10, // TITLE Frost damage
    ENCH_CAT_NATURE_DMG        = 11, // TITLE Nature damage
    ENCH_CAT_FIRE_DMG          = 12, // TITLE Fire damage
    ENCH_CAT_ARCANE_DMG        = 13, // TITLE Arcane damage
    ENCH_CAT_HEALER            = 14, // TITLE for healers, mainly Spirit
};

uint32 getEnchantCategoryMask(std::vector<EnchantCategory> enchCategories)
{
    uint32 r = 0;
    for (auto enchCat : enchCategories)
    {
        r |= 1 << enchCat;
    }
    return r;
}

uint32 getMask(std::vector<uint32> us)
{
    uint32 r = 0;
    for (auto u : us)
    {
        r |= 1 << u;
    }
    return r;
}

bool playerHasSkillRequirementForEnchant(const Player* player, uint32 enchantID)
{
    if (SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchantID))
    {
        if (enchantEntry->requiredSkill && player->GetSkillValue(enchantEntry->requiredSkill) < enchantEntry->requiredSkillValue)
        {
            return false;
        }
    }
    return true;
}

bool playerHasLevelRequirementForEnchant(const Player* player, uint32 enchantID)
{
    if (SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchantID))
    {
        if (player->getLevel() < enchantEntry->requiredLevel)
        {
            return false;
        }
    }
    return true;
}

bool isItemPlayerClassPreference(Player* player, Item* item)
{
    if (!player)
    {
        return false;
    }
    // If its misc items that everyone can use, it is preferred.
    switch (item->GetTemplate()->InventoryType)
    {
        case INVTYPE_NECK:
        case INVTYPE_FINGER:
        case INVTYPE_TRINKET:
        case INVTYPE_CLOAK:
        case INVTYPE_TABARD:
        case INVTYPE_BODY: // shirt
            return true;
    }
    if (item->GetTemplate()->Class == ITEM_CLASS_WEAPON)
    {
        return player->CanUseItem(item->GetTemplate()) == EQUIP_ERR_OK;
    }
    if (item->GetTemplate()->Class == ITEM_CLASS_ARMOR)
    {
        std::vector<uint32> armorSubclasses;
        switch (player->getClass())
        {
            case CLASS_PALADIN:
                armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_LIBRAM, ITEM_SUBCLASS_ARMOR_SHIELD});
                goto PLATE_USER_LABEL;
            case CLASS_DEATH_KNIGHT:
                armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_SIGIL});
                goto PLATE_USER_LABEL;
            case CLASS_WARRIOR:
                armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_SHIELD});
PLATE_USER_LABEL:
                if (player->HasSkill(SKILL_PLATE_MAIL)) {
                    armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_PLATE});
                } else {
                    armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_MAIL});
                }
                break;
            case CLASS_SHAMAN:
                armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_TOTEM, ITEM_SUBCLASS_ARMOR_SHIELD});
                goto MAIL_USER_LABEL;
            case CLASS_HUNTER:
MAIL_USER_LABEL:
                if (player->HasSkill(SKILL_MAIL)) {
                    armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_MAIL});
                } else {
                    armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_LEATHER});
                }
                break;
            case CLASS_DRUID:
                armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_IDOL});
                goto LEATHER_USER_LABEL;
            case CLASS_ROGUE:
LEATHER_USER_LABEL:
                armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_LEATHER});
                break;
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                armorSubclasses.insert(armorSubclasses.end(), {ITEM_SUBCLASS_ARMOR_CLOTH});
                break;
        }
        return (getMask(armorSubclasses) & (1 << item->GetTemplate()->SubClass)) > 0;
    }
    return false;
}

// getItemPlayerLevel retrieves an item's player required level
// It uses the item's required level if its not zero, otherwise it will rely on
// the average required level calculated from the item template table.
uint32 getItemPlayerLevel(Item* item)
{
    if (uint32 reqLevel = item->GetTemplate()->RequiredLevel)
    {
        return reqLevel;
    }
    QueryResult qr = WorldDatabase.Query(R"(select ceil(avg(RequiredLevel)) from item_template where
ItemLevel = {}
and class in ({},{})
and RequiredLevel != {}
and not (name like '%qa%' or name like '%test%' or name like '%debug%' or name like '%internal%' or name like '%demo%')
group by ItemLevel LIMIT 1)",
        item->GetTemplate()->ItemLevel,
        ITEM_CLASS_WEAPON, ITEM_CLASS_ARMOR,
        0);
    if (!qr)
    {
        // If unable to query, fallback to maxlevel (NOTE: maybe would be better to default to 1 instead of max)
        return sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
    }
    uint32 avgReqLevel = qr->Fetch()[0].Get<uint32>();
    return avgReqLevel;
}

int getLevelOffset(Item* item, Player* player = nullptr)
{
    int level = 1;
    if (player)
    {
        level = player->getLevel();
    }
    else
    {
        level = getItemPlayerLevel(item);
    }
    // level offset calculation below
    // current_level - 5 + item_quality
    level = level - ITEM_QUALITY_LEGENDARY + item->GetTemplate()->Quality;
    if (level <= 0)
    {
        level = 1;
    }
    return level;
}

uint32 getPlayerEnchantCategoryMask(Player* player)
{
    std::vector<EnchantCategory> plrEnchCats;
    switch (player->getClass())
    {
        case CLASS_WARRIOR:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_STRENGTH, ENCH_CAT_MELEE});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
            case TALENT_TREE_WARRIOR_ARMS:
            case TALENT_TREE_WARRIOR_FURY:
                break;
            case TALENT_TREE_WARRIOR_PROTECTION:
                plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_TANK_DEFENSE, ENCH_CAT_TANK_SHIELD_BLOCK});
                break;
            }
            break;
        case CLASS_PALADIN:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_CASTER, ENCH_CAT_HOLY_DMG});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_PALADIN_HOLY:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_INTELLECT, ENCH_CAT_HEALER});
                    break;
                case TALENT_TREE_PALADIN_PROTECTION:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_STRENGTH, ENCH_CAT_MELEE, ENCH_CAT_TANK_DEFENSE, ENCH_CAT_TANK_SHIELD_BLOCK});
                    break;
                case TALENT_TREE_PALADIN_RETRIBUTION:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_STRENGTH, ENCH_CAT_MELEE});
                    break;
            }
            break;
        case CLASS_HUNTER:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_AGILITY, ENCH_CAT_RANGED});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_HUNTER_BEAST_MASTERY:
                case TALENT_TREE_HUNTER_MARKSMANSHIP:
                case TALENT_TREE_HUNTER_SURVIVAL:
                    break;
            }
            break;
        case CLASS_ROGUE:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_AGILITY, ENCH_CAT_MELEE});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_ROGUE_ASSASSINATION:
                case TALENT_TREE_ROGUE_COMBAT:
                case TALENT_TREE_ROGUE_SUBTLETY:
                    break;
            }
            break;
        case CLASS_PRIEST:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_INTELLECT, ENCH_CAT_CASTER, ENCH_CAT_HEALER});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_PRIEST_DISCIPLINE:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_HOLY_DMG});
                    break;
                case TALENT_TREE_PRIEST_HOLY:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_HOLY_DMG});
                    break;
                case TALENT_TREE_PRIEST_SHADOW:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_SHADOW_DMG});
                    break;
            }
            break;
        case CLASS_DEATH_KNIGHT:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_STRENGTH, ENCH_CAT_MELEE, ENCH_CAT_CASTER, ENCH_CAT_SHADOW_DMG, ENCH_CAT_TANK_DEFENSE});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_DEATH_KNIGHT_BLOOD:
                    break;
                case TALENT_TREE_DEATH_KNIGHT_FROST:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_FROST_DMG});
                    break;
                case TALENT_TREE_DEATH_KNIGHT_UNHOLY:
                    break;
            }
            break;
        case CLASS_SHAMAN:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_NATURE_DMG, ENCH_CAT_CASTER});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_SHAMAN_ELEMENTAL:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_INTELLECT, ENCH_CAT_FIRE_DMG});
                    break;
                case TALENT_TREE_SHAMAN_ENHANCEMENT:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_AGILITY, ENCH_CAT_MELEE});
                    break;
                case TALENT_TREE_SHAMAN_RESTORATION:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_INTELLECT, ENCH_CAT_HEALER});
                    break;
            }
            break;
        case CLASS_MAGE:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_INTELLECT, ENCH_CAT_CASTER});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_MAGE_ARCANE:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_ARCANE_DMG});
                    break;
                case TALENT_TREE_MAGE_FIRE:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_FIRE_DMG});
                    break;
                case TALENT_TREE_MAGE_FROST:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_FROST_DMG});
                    break;
            }
            break;
        case CLASS_WARLOCK:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_INTELLECT, ENCH_CAT_CASTER, ENCH_CAT_SHADOW_DMG});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_WARLOCK_AFFLICTION:
                    break;
                case TALENT_TREE_WARLOCK_DEMONOLOGY:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_FIRE_DMG});
                    break;
                case TALENT_TREE_WARLOCK_DESTRUCTION:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_FIRE_DMG});
                    break;
            }
            break;
        case CLASS_DRUID:
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_DRUID_BALANCE:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_INTELLECT, ENCH_CAT_CASTER, ENCH_CAT_NATURE_DMG, ENCH_CAT_ARCANE_DMG});
                    break;
                case TALENT_TREE_DRUID_FERAL_COMBAT:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_AGILITY, ENCH_CAT_TANK_DEFENSE, ENCH_CAT_MELEE});
                    break;
                case TALENT_TREE_DRUID_RESTORATION:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_INTELLECT, ENCH_CAT_CASTER, ENCH_CAT_HEALER, ENCH_CAT_NATURE_DMG});
                    break;
            }
            break;
    }
    return getEnchantCategoryMask(plrEnchCats);
}

// gets the item enchant category mask for a given item
uint32 getItemEnchantCategoryMask(Item* item)
{
    // stat checks
    bool hasMainStatAgi = false;
    bool hasMainStatStr = false;
    bool hasMainStatInt = false;
    // role checks
    bool isHealer = false;
    bool isMelee = false;
    bool isRanged = false;
    bool isSpellDmg = false;
    bool isTank = false;
    bool isTankShielder = false;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        if (i >= item->GetTemplate()->StatsCount)
        {
            break;
        }
        switch (item->GetTemplate()->ItemStat[i].ItemStatType)
        {
            case ITEM_MOD_AGILITY:
                hasMainStatAgi = true;
                continue;
            case ITEM_MOD_STRENGTH:
                hasMainStatStr = true;
                continue;
            case ITEM_MOD_INTELLECT:
                hasMainStatInt = true;
                continue;
            case ITEM_MOD_SPIRIT:
                isHealer = true;
                continue;
            case ITEM_MOD_HIT_MELEE_RATING:
            case ITEM_MOD_CRIT_MELEE_RATING:
            case ITEM_MOD_HASTE_MELEE_RATING:
            case ITEM_MOD_EXPERTISE_RATING:
                isMelee = true;
                continue;
            case ITEM_MOD_HIT_RANGED_RATING:
            case ITEM_MOD_CRIT_RANGED_RATING:
            case ITEM_MOD_HASTE_RANGED_RATING:
            case ITEM_MOD_RANGED_ATTACK_POWER:
                isRanged = true;
                continue;
            case ITEM_MOD_HIT_SPELL_RATING:
            case ITEM_MOD_SPELL_DAMAGE_DONE:
            case ITEM_MOD_SPELL_PENETRATION:
                isSpellDmg = true;
                continue;
            case ITEM_MOD_CRIT_SPELL_RATING:
            case ITEM_MOD_HASTE_SPELL_RATING:
            case ITEM_MOD_SPELL_POWER:
            // TODO: Check proto->HasSpellPowerStat() for spellpower
            // TODO: Check SPELL_AURA_MOD_POWER_COST_SCHOOL for specific spell schools (e.g. shadow, holy etc)
                isSpellDmg = true;
                isHealer = true;
                continue;
            case ITEM_MOD_SPELL_HEALING_DONE:
            case ITEM_MOD_MANA_REGENERATION:
                isHealer = true;
                continue;
            case ITEM_MOD_ATTACK_POWER:
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                isMelee = true;
                isRanged = true;
                continue;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
            case ITEM_MOD_DODGE_RATING:
            case ITEM_MOD_PARRY_RATING:
                isTank = true;
                continue;
            case ITEM_MOD_BLOCK_RATING:
            case ITEM_MOD_BLOCK_VALUE:
                isTankShielder = true;
                continue;
            case ITEM_MOD_STAMINA:
            case ITEM_MOD_HEALTH:
            case ITEM_MOD_MANA:
            case ITEM_MOD_HEALTH_REGEN:
            case ITEM_MOD_HIT_RATING:
            case ITEM_MOD_CRIT_RATING:
            case ITEM_MOD_HASTE_RATING:
            case ITEM_MOD_HIT_TAKEN_RATING:         // Miss Related
            case ITEM_MOD_HIT_TAKEN_MELEE_RATING:   // Miss Related
            case ITEM_MOD_HIT_TAKEN_RANGED_RATING:  // Miss Related
            case ITEM_MOD_HIT_TAKEN_SPELL_RATING:   // Miss Related
            case ITEM_MOD_CRIT_TAKEN_RATING:        // Resilience related
            case ITEM_MOD_RESILIENCE_RATING:        // Resilience related
            case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:  // Resilience related
            case ITEM_MOD_CRIT_TAKEN_RANGED_RATING: // Resilience related
            case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:  // Resilience related
                continue;
        }
    }

    std::vector<EnchantCategory> enchCatsAgi;
    std::vector<EnchantCategory> enchCatsStr;
    std::vector<EnchantCategory> enchCatsInt;
    std::vector<EnchantCategory> enchCatsHealer;
    std::vector<EnchantCategory> enchCatsMelee;
    std::vector<EnchantCategory> enchCatsRanged;
    std::vector<EnchantCategory> enchCatsSpellDmg;
    std::vector<EnchantCategory> enchCatsTank;
    std::vector<EnchantCategory> enchCatsTankShielder;
    switch (item->GetTemplate()->Class)
    {
        case ITEM_CLASS_ARMOR:
            switch (item->GetTemplate()->SubClass)
            {
                case ITEM_SUBCLASS_ARMOR_MISC:
                    enchCatsAgi.insert(enchCatsAgi.end(), {ENCH_CAT_AGILITY});
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsRanged.insert(enchCatsRanged.end(), {ENCH_CAT_RANGED});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_HOLY_DMG,ENCH_CAT_SHADOW_DMG,ENCH_CAT_FROST_DMG,ENCH_CAT_NATURE_DMG,ENCH_CAT_FIRE_DMG,ENCH_CAT_ARCANE_DMG});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    enchCatsTankShielder.insert(enchCatsTankShielder.end(), {ENCH_CAT_TANK_SHIELD_BLOCK});
                    break;
                case ITEM_SUBCLASS_ARMOR_CLOTH:
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_HOLY_DMG,ENCH_CAT_SHADOW_DMG,ENCH_CAT_FROST_DMG,ENCH_CAT_FIRE_DMG,ENCH_CAT_ARCANE_DMG});
                    break;
                case ITEM_SUBCLASS_ARMOR_LEATHER:
                    enchCatsAgi.insert(enchCatsAgi.end(), {ENCH_CAT_AGILITY});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsRanged.insert(enchCatsRanged.end(), {ENCH_CAT_RANGED});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_FROST_DMG,ENCH_CAT_NATURE_DMG,ENCH_CAT_FIRE_DMG,ENCH_CAT_ARCANE_DMG});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    break;
                case ITEM_SUBCLASS_ARMOR_MAIL:
                    enchCatsAgi.insert(enchCatsAgi.end(), {ENCH_CAT_AGILITY});
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsRanged.insert(enchCatsRanged.end(), {ENCH_CAT_RANGED});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_HOLY_DMG,ENCH_CAT_FROST_DMG,ENCH_CAT_NATURE_DMG,ENCH_CAT_FIRE_DMG});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    break;
                case ITEM_SUBCLASS_ARMOR_PLATE:
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    enchCatsTankShielder.insert(enchCatsTankShielder.end(), {ENCH_CAT_TANK_SHIELD_BLOCK});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_HOLY_DMG,ENCH_CAT_SHADOW_DMG,ENCH_CAT_FROST_DMG});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    break;
                case ITEM_SUBCLASS_ARMOR_SHIELD:
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    enchCatsTankShielder.insert(enchCatsTankShielder.end(), {ENCH_CAT_TANK_SHIELD_BLOCK});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_HOLY_DMG,ENCH_CAT_NATURE_DMG});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    break;
                case ITEM_SUBCLASS_ARMOR_LIBRAM:
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    enchCatsTankShielder.insert(enchCatsTankShielder.end(), {ENCH_CAT_TANK_SHIELD_BLOCK});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_HOLY_DMG});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    break;
                case ITEM_SUBCLASS_ARMOR_IDOL:
                    enchCatsAgi.insert(enchCatsAgi.end(), {ENCH_CAT_AGILITY});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_NATURE_DMG,ENCH_CAT_ARCANE_DMG});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    break;
                case ITEM_SUBCLASS_ARMOR_TOTEM:
                    enchCatsAgi.insert(enchCatsAgi.end(), {ENCH_CAT_AGILITY});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_FROST_DMG,ENCH_CAT_NATURE_DMG,ENCH_CAT_FIRE_DMG});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    break;
                case ITEM_SUBCLASS_ARMOR_SIGIL:
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_SHADOW_DMG,ENCH_CAT_FROST_DMG});
                    break;
            }
            break;
        case ITEM_CLASS_WEAPON:
            switch (item->GetTemplate()->SubClass)
            {
                case ITEM_SUBCLASS_WEAPON_AXE:
                case ITEM_SUBCLASS_WEAPON_AXE2:
                case ITEM_SUBCLASS_WEAPON_MACE:
                case ITEM_SUBCLASS_WEAPON_MACE2:
                case ITEM_SUBCLASS_WEAPON_SWORD:
                case ITEM_SUBCLASS_WEAPON_SWORD2:
                case ITEM_SUBCLASS_WEAPON_POLEARM:
                case ITEM_SUBCLASS_WEAPON_DAGGER:
                    enchCatsAgi.insert(enchCatsAgi.end(), {ENCH_CAT_AGILITY});
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsRanged.insert(enchCatsRanged.end(), {ENCH_CAT_RANGED});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_HOLY_DMG,ENCH_CAT_SHADOW_DMG,ENCH_CAT_FROST_DMG,ENCH_CAT_NATURE_DMG,ENCH_CAT_FIRE_DMG,ENCH_CAT_ARCANE_DMG});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    break;
                case ITEM_SUBCLASS_WEAPON_BOW:
                case ITEM_SUBCLASS_WEAPON_GUN:
                case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                    enchCatsAgi.insert(enchCatsAgi.end(), {ENCH_CAT_AGILITY});
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsRanged.insert(enchCatsRanged.end(), {ENCH_CAT_RANGED});
                    break;
                case ITEM_SUBCLASS_WEAPON_FIST:
                    enchCatsAgi.insert(enchCatsAgi.end(), {ENCH_CAT_AGILITY});
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_FROST_DMG,ENCH_CAT_NATURE_DMG,ENCH_CAT_FIRE_DMG,ENCH_CAT_ARCANE_DMG});
                    break;
                case ITEM_SUBCLASS_WEAPON_THROWN:
                    enchCatsAgi.insert(enchCatsAgi.end(), {ENCH_CAT_AGILITY});
                    enchCatsStr.insert(enchCatsStr.end(), {ENCH_CAT_STRENGTH});
                    enchCatsTank.insert(enchCatsTank.end(), {ENCH_CAT_TANK_DEFENSE});
                    enchCatsMelee.insert(enchCatsMelee.end(), {ENCH_CAT_MELEE});
                    break;
                case ITEM_SUBCLASS_WEAPON_WAND:
                    enchCatsInt.insert(enchCatsInt.end(), {ENCH_CAT_INTELLECT});
                    enchCatsHealer.insert(enchCatsHealer.end(), {ENCH_CAT_HEALER});
                    enchCatsSpellDmg.insert(enchCatsSpellDmg.end(), {ENCH_CAT_CASTER,ENCH_CAT_HOLY_DMG,ENCH_CAT_SHADOW_DMG,ENCH_CAT_FROST_DMG,ENCH_CAT_FIRE_DMG,ENCH_CAT_ARCANE_DMG});
                    break;
                case ITEM_SUBCLASS_WEAPON_SPEAR:
                case ITEM_SUBCLASS_WEAPON_obsolete:
                case ITEM_SUBCLASS_WEAPON_STAFF:
                case ITEM_SUBCLASS_WEAPON_EXOTIC:
                case ITEM_SUBCLASS_WEAPON_EXOTIC2:
                case ITEM_SUBCLASS_WEAPON_MISC:
                case ITEM_SUBCLASS_WEAPON_FISHING_POLE:
                    break;
            }
            break;
    }
    std::vector<EnchantCategory> itmEnchCats;
    bool noStats = !hasMainStatAgi && !hasMainStatStr && !hasMainStatInt && !isHealer && !isMelee && !isRanged && !isSpellDmg && !isTank && !isTankShielder;
    bool hasOnlyMainStat = (hasMainStatAgi || hasMainStatStr || hasMainStatInt)  && !isHealer && !isMelee && !isRanged && !isSpellDmg && !isTank && !isTankShielder;
    bool hasOnlySubStat = !hasMainStatAgi && !hasMainStatStr && !hasMainStatInt && (isHealer || isMelee || isRanged || isSpellDmg || isTank || isTankShielder);
    if (config_debug)
    {
        LOG_INFO("module", "RANDOM_ENCHANT: Getting item enchant mask, checks below:");
        LOG_INFO("module", "       For item {}, Item ID is: {}", item->GetTemplate()->Name1, item->GetTemplate()->ItemId);
        LOG_INFO("module", "                hasMainStatAgi = {}", hasMainStatAgi);
        LOG_INFO("module", "                hasMainStatStr = {}", hasMainStatStr);
        LOG_INFO("module", "                hasMainStatInt = {}", hasMainStatInt);
        LOG_INFO("module", "                isHealer = {}", isHealer);
        LOG_INFO("module", "                isMelee = {}", isMelee);
        LOG_INFO("module", "                isRanged = {}", isRanged);
        LOG_INFO("module", "                isSpellDmg = {}", isSpellDmg);
        LOG_INFO("module", "                isTank = {}", isTank);
        LOG_INFO("module", "                isTankShielder = {}", isTankShielder);
        LOG_INFO("module", "                noStats = {}", noStats);
        LOG_INFO("module", "                hasOnlyMainStat = {}", hasOnlyMainStat);
        LOG_INFO("module", "                hasOnlySubStat = {}", hasOnlySubStat);
    }
    if (noStats) {
        // If no stats, we add every item category from the item classes (armour type, weapon weapon type etc etc).
        itmEnchCats.insert(itmEnchCats.end(), enchCatsAgi.begin(), enchCatsAgi.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsStr.begin(), enchCatsStr.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsInt.begin(), enchCatsInt.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsHealer.begin(), enchCatsHealer.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsMelee.begin(), enchCatsMelee.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsRanged.begin(), enchCatsRanged.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsSpellDmg.begin(), enchCatsSpellDmg.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsTank.begin(), enchCatsTank.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsTankShielder.begin(), enchCatsTankShielder.end());
        return getEnchantCategoryMask(itmEnchCats);
    }
    // Below, we ensure that item roles that only have either main stat or substat, if they're missing them.
    // this is going by a very simple heuristic, but may end up being not entirely useful after all
    if (hasOnlyMainStat)
    {
        if (hasMainStatAgi)
        {
            isMelee = true;
            isRanged = true;
            isTank = true;
        }
        if (hasMainStatStr)
        {
            isMelee = true;
            isTank = true;
            isTankShielder = true;
        }
        if (hasMainStatInt)
        {
            isHealer = true;
            isSpellDmg = true;
        }
    }
    else if (hasOnlySubStat)
    {
        if (isHealer || isSpellDmg)
        {
            hasMainStatInt = true;
        }
        if (isMelee || isRanged || isTank)
        {
            hasMainStatAgi = true;
            hasMainStatStr = true;
        }
        if (isTankShielder)
        {
            hasMainStatStr = true;
        }
    }
    // Add item categories by item's stats
    if (hasMainStatAgi) { itmEnchCats.insert(itmEnchCats.end(), enchCatsAgi.begin(), enchCatsAgi.end()); }
    if (hasMainStatStr) { itmEnchCats.insert(itmEnchCats.end(), enchCatsStr.begin(), enchCatsStr.end()); }
    if (hasMainStatInt) { itmEnchCats.insert(itmEnchCats.end(), enchCatsInt.begin(), enchCatsInt.end()); }
    if (isHealer) { itmEnchCats.insert(itmEnchCats.end(), enchCatsHealer.begin(), enchCatsHealer.end()); }
    if (isMelee) { itmEnchCats.insert(itmEnchCats.end(), enchCatsMelee.begin(), enchCatsMelee.end()); }
    if (isRanged) { itmEnchCats.insert(itmEnchCats.end(), enchCatsRanged.begin(), enchCatsRanged.end()); }
    if (isSpellDmg) { itmEnchCats.insert(itmEnchCats.end(), enchCatsSpellDmg.begin(), enchCatsSpellDmg.end()); }
    if (isTank) { itmEnchCats.insert(itmEnchCats.end(), enchCatsTank.begin(), enchCatsTank.end()); }
    if (isTankShielder) { itmEnchCats.insert(itmEnchCats.end(), enchCatsTankShielder.begin(), enchCatsTankShielder.end()); }
    return getEnchantCategoryMask(itmEnchCats);
}

uint32 getPlayerItemEnchantCategoryMask(Item* item, Player* player = nullptr)
{
    if(config_roll_player_class_preference && player && isItemPlayerClassPreference(player, item))
    {
        if (config_debug)
        {
            LOG_INFO("module", "RANDOM_ENCHANT: Getting player class preference");
        }
        return getPlayerEnchantCategoryMask(player);
    }
    if (config_debug)
    {
        LOG_INFO("module", "RANDOM_ENCHANT: Getting item enchant category");
    }
    return getItemEnchantCategoryMask(item);
}

// END UTILS

// MAIN GET ROLL ENCHANT FUNCTIONS

int getRandomEnchantment_New(Item* item, Player* player = nullptr)
{
    uint32 Class = item->GetTemplate()->Class;
    uint32 subclassMask = 1 << item->GetTemplate()->SubClass;
    int level = getLevelOffset(item, player);
    uint32 enchantCategoryMask = getPlayerItemEnchantCategoryMask(item, player);

    int maxCount = 50;
    while (maxCount > 0)
    {
    QueryResult qr = WorldDatabase.Query(R"(select ID from item_enchantment_random_tiers_NEW WHERE
MinLevel <= {} and {} <= MaxLevel AND
(
ItemClass is NULL OR
ItemClass = {} AND ItemSubClassMask is NULL OR
ItemClass = {} AND ItemSubClassMask & {} > 0
) AND
(
EnchantCategory is NULL OR
EnchantCategory & {} > 0
) ORDER BY RAND() LIMIT 1)", level, level, Class, Class, subclassMask, enchantCategoryMask);
        if (qr)
        {
            int enchID = qr->Fetch()[0].Get<uint32>();
            if (config_debug)
            {
                LOG_INFO("module", "RANDOM_ENCHANT: Query with the following params:");
                LOG_INFO("module", "                level {}, item_class {}, subclassmask {}, enchCatMask {}", level, Class, subclassMask, enchantCategoryMask);
                LOG_INFO("module", "                Return was: {}", enchID);
            }
            if (
                player &&
                !playerHasSkillRequirementForEnchant(player, enchID) &&
                !playerHasLevelRequirementForEnchant(player, enchID)
            )
            {
                // If a player is available, we check if the user can use the enchant
                continue;
            }
            return enchID;
        }
        // get rand enchant ID failed for some reason, should not happen, we still just continue and try to get
        // another one.
        maxCount--;
    }
    return -1;
}

int getRandEnchantment(Item* item)
{
    uint32 Class = item->GetTemplate()->Class;
    std::string ClassQueryString = "";
    switch (Class)
    {
    case 2:
        ClassQueryString = "WEAPON";
        break;
    case 4:
        ClassQueryString = "ARMOR";
        break;
    }
    if (ClassQueryString == "")
        return -1;
    uint32 Quality = item->GetTemplate()->Quality;
    int rarityRoll = -1;
    switch (Quality)
    {
    case 0://grey
        rarityRoll = rand_norm() * 25;
        break;
    case 1://white
        rarityRoll = rand_norm() * 50;
        break;
    case 2://green
        rarityRoll = 45 + (rand_norm() * 20);
        break;
    case 3://blue
        rarityRoll = 65 + (rand_norm() * 15);
        break;
    case 4://purple
        rarityRoll = 80 + (rand_norm() * 14);
        break;
    case 5://orange
        rarityRoll = 93;
        break;
    }
    if (rarityRoll < 0)
        return -1;
    int tier = 0;
    if (rarityRoll <= 44)
        tier = 1;
    else if (rarityRoll <= 64)
        tier = 2;
    else if (rarityRoll <= 79)
        tier = 3;
    else if (rarityRoll <= 92)
        tier = 4;
    else
        tier = 5;

    int maxCount = 20;
    while (maxCount > 0)
    {
        QueryResult qr = WorldDatabase.Query("SELECT enchantID FROM item_enchantment_random_tiers WHERE tier='{}' AND exclusiveSubClass=NULL AND class='{}' OR exclusiveSubClass='{}' OR class='ANY' ORDER BY RAND() LIMIT 1", tier, ClassQueryString.c_str(), item->GetTemplate()->SubClass);
        if (qr)
        {
            return qr->Fetch()[0].Get<uint32>();
        }
        // get rand enchant ID failed for some reason, should not happen, we still just continue and try to get
        // another one.
        maxCount--;
    }
    return -1;
}

std::vector<EnchantmentSlot> GetAvailableEnchantSlots(Item* item)
{
    std::vector<EnchantmentSlot> availableSlots;
    for (auto slot : default_allowed_rand_enchant_slots)
    {
        if (!item->GetEnchantmentId(slot))
        {
            // Add slots that dont have an enchantment slot
            availableSlots.push_back(slot);
        }
    }
    return availableSlots;
}

// // HasBeenTouchedByRandomEnchantMod is a hack-ish way to check if an item has possibly been touched by this mod
// // The usual blizzlike Azerothcore logic for random enchants using PROP_ENCHANTMENT_SLOT_{0,1,2,3,4} is that
// // PROP_ENCHANTMENT_SLOT_{0,1,2} will usually be touched by enchants from the `acore_world`.`item_enchantment_template`
// // table
// // This usually leaves PROP_ENCHANTMENT_SLOT_{3,4} empty, which is probably an indicator that the item has been touched
// // by this mod as this is the first enchant we'll populate.
// bool HasBeenTouchedByRandomEnchantMod(Item* item)
// {
//     return item->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_4);
// }

std::vector<std::pair<uint32, EnchantmentSlot>> GetRolledEnchants(Item* item, Player* player = nullptr)
{
    std::vector<EnchantmentSlot> availableSlots = GetAvailableEnchantSlots(item);
    std::vector<std::pair<uint32, EnchantmentSlot>> rolledEnchants;
    
    for (std::size_t i = 0; i < availableSlots.size(); ++i)
    {
        auto slot = availableSlots[i];
        float rollpct = config_enchant_pcts[i];
        float roll = (float)rand_chance();
        if (roll + rollpct < 100.0)
        {
            // If roll was not successful, we break, no more attempted rolls beyond this;
            break;
        }
        int randEnch = -1;
        if (config_use_new_random_enchant_system)
        {
            randEnch = getRandomEnchantment_New(item, player);
        } else {
            randEnch = getRandEnchantment(item);
        }
        if (randEnch > 0)
        {
            rolledEnchants.push_back(std::make_pair((uint32)randEnch, slot));
        }
    }
    return rolledEnchants;
}

void RollPossibleEnchant(Player* player, Item* item)
{
    uint32 Quality = item->GetTemplate()->Quality;
    uint32 Class = item->GetTemplate()->Class;

    if (
        (Quality > 5 || Quality < 1) /* eliminates enchanting anything that isn't a recognized quality */ ||
        (Class != 2 && Class != 4) /* eliminates enchanting anything but weapons/armor */)
    {
        return;
    }

    std::vector<std::pair<uint32, EnchantmentSlot>> rolledEnchants = GetRolledEnchants(item, player);
    int numActualEnchants = 0;
    for (auto const& [enchID, enchSlot] : rolledEnchants)
    {
        if (sSpellItemEnchantmentStore.LookupEntry(enchID))//Make sure enchantment id exists
        {
            player->ApplyEnchantment(item, enchSlot, false);
            item->SetEnchantment(enchSlot, enchID, 0, 0);
            player->ApplyEnchantment(item, enchSlot, true);
            numActualEnchants++;
        }
    }
    ChatHandler chathandle = ChatHandler(player->GetSession());
    if (numActualEnchants > 0)
    {
        chathandle.PSendSysMessage("Newly Acquired |cffFF0000 %s |rhas received|cffFF0000 %d |rrandom enchantments!", item->GetTemplate()->Name1.c_str(), numActualEnchants);
    }
}

// END MAIN GET ROLL ENCHANTS FUNCTIONS

class RandomEnchantsWorldScript : public WorldScript
{
public:
    RandomEnchantsWorldScript() : WorldScript("RandomEnchantsWorldScript") { }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        config_announce_on_log = sConfigMgr->GetOption<bool>("RandomEnchants.AnnounceOnLogin", default_announce_on_log);
        config_debug = sConfigMgr->GetOption<bool>("RandomEnchants.Debug", default_debug);
        config_on_loot = sConfigMgr->GetOption<bool>("RandomEnchants.OnLoot", default_on_loot);
        config_on_create = sConfigMgr->GetOption<bool>("RandomEnchants.OnCreate", default_on_create);
        config_on_quest_reward = sConfigMgr->GetOption<bool>("RandomEnchants.OnQuestReward", default_on_quest_reward);
        config_on_group_roll_reward_item = sConfigMgr->GetOption<bool>("RandomEnchants.OnGroupRollRewardItem", default_on_group_roll_reward_item);
        config_on_vendor_purchase = sConfigMgr->GetOption<bool>("RandomEnchants.OnVendorPurchase", default_on_vendor_purchase);
        // config_on_all_items_created = sConfigMgr->GetOption<bool>("RandomEnchants.OnAllItemsCreated", default_on_all_items_created);
        config_use_new_random_enchant_system = sConfigMgr->GetOption<bool>("RandomEnchants.UseNewRandomEnchantSystem", default_use_new_random_enchant_system);
        config_roll_player_class_preference =  sConfigMgr->GetOption<bool>("RandomEnchants.RollPlayerClassPreference", default_roll_player_class_preference);
        config_login_message = sConfigMgr->GetOption<std::string>("RandomEnchants.OnLoginMessage", default_login_message);
        config_enchant_pcts[0] = sConfigMgr->GetOption<float>("RandomEnchants.RollPercentage.1", default_enchant_pcts[0]);
        config_enchant_pcts[1] = sConfigMgr->GetOption<float>("RandomEnchants.RollPercentage.2", default_enchant_pcts[1]);
        config_enchant_pcts[2] = sConfigMgr->GetOption<float>("RandomEnchants.RollPercentage.3", default_enchant_pcts[2]);
        config_enchant_pcts[3] = sConfigMgr->GetOption<float>("RandomEnchants.RollPercentage.4", default_enchant_pcts[3]);
        config_enchant_pcts[4] = sConfigMgr->GetOption<float>("RandomEnchants.RollPercentage.5", default_enchant_pcts[4]);
    }
};

class RandomEnchantsPlayer : public PlayerScript{
public:

    RandomEnchantsPlayer() : PlayerScript("RandomEnchantsPlayer") { }

    void OnLogin(Player* player) override {
        if (config_announce_on_log)
            ChatHandler(player->GetSession()).SendSysMessage(config_login_message);
    }
    void OnStoreNewItem(Player* player, Item* item, uint32 /*count*/) override
    {
        if (/*!HasBeenTouchedByRandomEnchantMod(item) && */config_on_loot)

            RollPossibleEnchant(player, item);
    }
    void OnCreateItem(Player* player, Item* item, uint32 /*count*/) override
    {
        if (/*!HasBeenTouchedByRandomEnchantMod(item) && */config_on_create)
            RollPossibleEnchant(player, item);
    }
    void OnQuestRewardItem(Player* player, Item* item, uint32 /*count*/) override
    {
        if(/*!HasBeenTouchedByRandomEnchantMod(item) && */config_on_quest_reward)
            RollPossibleEnchant(player, item);
    }
    void OnGroupRollRewardItem(Player* player, Item* item, uint32 /*count*/, RollVote /*voteType*/, Roll* /*roll*/) override
    {
        if (/*!HasBeenTouchedByRandomEnchantMod(item) && */config_on_group_roll_reward_item)
        {
            RollPossibleEnchant(player, item);
        }
    }
    void OnAfterStoreOrEquipNewItem(Player* player, uint32 /*vendorslot*/, Item* item, uint8 /*count*/, uint8 /*bag*/, uint8 /*slot*/, ItemTemplate const* /*pProto*/, Creature* /*pVendor*/, VendorItem const* /*crItem*/, bool /*bStore*/) override
    {
        if (/*!HasBeenTouchedByRandomEnchantMod(item) && */config_on_vendor_purchase)
        {
            RollPossibleEnchant(player, item);
        }
    }
};

// class RandomEnchantsMisc : public MiscScript{
// public:

//     RandomEnchantsMisc() : MiscScript("RandomEnchantsMisc") { }

//     void OnItemCreate(Item* item, ItemTemplate const* /*itemProto*/, Player const* owner) override
//     {
//         if (config_on_all_items_created)
//         {
//             if (!owner) {
//                 return;
//             }
//             Player* player = const_cast<Player*>(owner);
//             if (!player || !player->FindMap())
//             {
//                 return;
//             }
//             RollPossibleEnchant(player, item);
//         }
//     }
// };

void AddRandomEnchantsScripts() {
    new RandomEnchantsWorldScript();
    new RandomEnchantsPlayer();
    // new RandomEnchantsMisc();
}
