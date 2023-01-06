/*
* Converted from the original LUA script to a module for Azerothcore(Sunwell) :D
*/
#include "ScriptMgr.h"
#include "Player.h"
#include "Configuration/Config.h"
#include "Chat.h"
#include "Item.h"

// DEFAULT VALUES

// Our suffix tiers can go up to 4.
#define MAX_RAND_ENCHANT_TIERS 4

double default_enchant_pcts[MAX_RAND_ENCHANT_TIERS] = {30.0, 35.0, 40.0, 45.0};

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

// CONFIGURATION

double config_enchant_pcts[MAX_RAND_ENCHANT_TIERS] = {
    default_enchant_pcts[0],
    default_enchant_pcts[1],
    default_enchant_pcts[2],
    default_enchant_pcts[3],
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

enum Attributes
{
    ATTRIBUTE_STRENGTH      = 0,  
    ATTRIBUTE_AGILITY       = 1,  
    ATTRIBUTE_INTELLECT     = 2,  
    ATTRIBUTE_SPIRIT        = 3,  
    ATTRIBUTE_STAMINA       = 4,  
    ATTRIBUTE_ATTACKPOWER   = 5,  
    ATTRIBUTE_SPELLPOWER    = 6,  
    ATTRIBUTE_HASTE         = 7,  
    ATTRIBUTE_HIT           = 8,  
    ATTRIBUTE_CRIT          = 9,  
    ATTRIBUTE_EXPERTISE     = 10,  
    ATTRIBUTE_DEFENSERATING = 11,  
    ATTRIBUTE_DODGE         = 12,  
    ATTRIBUTE_PARRY         = 13,  
};

// UTILS
enum EnchantCategory
{
    ENCH_CAT_MELEE_STR_DPS  = 0,
    ENCH_CAT_MELEE_STR_TANK = 1,
    ENCH_CAT_MELEE_AGI_DPS  = 2,
    ENCH_CAT_MELEE_AGI_TANK = 3,
    ENCH_CAT_RANGED_AGI     = 4,
    ENCH_CAT_CASTER         = 5,
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

uint32 getAttributeMask(std::vector<Attributes> attributes)
{
    uint32 r = 0;
    for (auto enchCat : attributes)
    {
        r |= 1 << enchCat;
    }
    return r;
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

auto getPlayerEnchantCategoryMask(Player* player)
{
    std::vector<EnchantCategory> plrEnchCats;
    std::vector<Attributes> plrAttrs;
    switch (player->getClass())
    {
        case CLASS_WARRIOR:
            switch (player->GetSpec(player->GetActiveSpec()))
            {
            case TALENT_TREE_WARRIOR_ARMS:
            case TALENT_TREE_WARRIOR_FURY:
                plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_MELEE_STR_DPS});
                break;
            case TALENT_TREE_WARRIOR_PROTECTION:
                plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_MELEE_STR_TANK});
                break;
            }
            break;
        case CLASS_PALADIN:
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_PALADIN_HOLY:
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_CASTER});
                    break;
                case TALENT_TREE_PALADIN_PROTECTION:
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_MELEE_STR_TANK});
                    break;
                case TALENT_TREE_PALADIN_RETRIBUTION:
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_MELEE_STR_DPS});
                    break;
            }
            break;
        case CLASS_HUNTER:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_RANGED_AGI});
            plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_HUNTER_BEAST_MASTERY:
                case TALENT_TREE_HUNTER_MARKSMANSHIP:
                case TALENT_TREE_HUNTER_SURVIVAL:
                    break;
            }
            break;
        case CLASS_ROGUE:
            plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_ROGUE_ASSASSINATION:
                case TALENT_TREE_ROGUE_COMBAT:
                case TALENT_TREE_ROGUE_SUBTLETY:
                    break;
            }
            break;
        case CLASS_PRIEST:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_CASTER});
            plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPIRIT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_CRIT});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_PRIEST_DISCIPLINE:
                case TALENT_TREE_PRIEST_HOLY:
                    break;
                case TALENT_TREE_PRIEST_SHADOW:
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_HIT});
                    break;
            }
            break;
        case CLASS_DEATH_KNIGHT:
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_DEATH_KNIGHT_BLOOD:
                case TALENT_TREE_DEATH_KNIGHT_FROST:
                case TALENT_TREE_DEATH_KNIGHT_UNHOLY:
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_MELEE_STR_DPS, ENCH_CAT_MELEE_STR_TANK});
                    break;
            }
            break;
        case CLASS_SHAMAN:
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_SHAMAN_ELEMENTAL:
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_CASTER});
                    break;
                case TALENT_TREE_SHAMAN_ENHANCEMENT:
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_STAMINA,ATTRIBUTE_STRENGTH,ATTRIBUTE_AGILITY,ATTRIBUTE_INTELLECT,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_MELEE_AGI_DPS});
                    break;
                case TALENT_TREE_SHAMAN_RESTORATION:
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_CRIT});
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_CASTER});
                    break;
            }
            break;
        case CLASS_MAGE:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_CASTER});
            plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPIRIT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_MAGE_ARCANE:
                case TALENT_TREE_MAGE_FIRE:
                case TALENT_TREE_MAGE_FROST:
                    break;
            }
            break;
        case CLASS_WARLOCK:
            plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_CASTER});
            plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPIRIT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_WARLOCK_AFFLICTION:
                case TALENT_TREE_WARLOCK_DEMONOLOGY:
                case TALENT_TREE_WARLOCK_DESTRUCTION:
                    break;
            }
            break;
        case CLASS_DRUID:
            switch (player->GetSpec(player->GetActiveSpec()))
            {
                case TALENT_TREE_DRUID_BALANCE:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_CASTER});
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPIRIT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    break;
                case TALENT_TREE_DRUID_FERAL_COMBAT:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_MELEE_AGI_DPS});
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_AGILITY,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE});
                    break;
                case TALENT_TREE_DRUID_RESTORATION:
                    plrEnchCats.insert(plrEnchCats.end(), {ENCH_CAT_CASTER});
                    plrAttrs.insert(plrAttrs.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPIRIT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_CRIT});
                    break;
            }
            break;
    }
    struct retVals {
        uint32 enchCatMask, attrMask;
    };
    return retVals{getEnchantCategoryMask(plrEnchCats), getAttributeMask(plrAttrs)};
}

// gets the item enchant category mask for a given item
auto getItemEnchantCategoryMask(Item* item)
{
    // role checks
    bool isRanged = false;
    bool isMelee = false;
    bool isPhysDPS = false;
    bool isStr = false;
    bool isAgi = false;
    bool isTank = false;
    bool isCaster = false;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        if (i >= item->GetTemplate()->StatsCount)
        {
            break;
        }
        switch (item->GetTemplate()->ItemStat[i].ItemStatType)
        {
            case ITEM_MOD_AGILITY:
                isAgi = true;
                continue;
            case ITEM_MOD_STRENGTH:
                isStr = true;
                continue;
            case ITEM_MOD_INTELLECT:
            case ITEM_MOD_SPIRIT:
                isCaster = true;
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
                isCaster = true;
                continue;
            case ITEM_MOD_CRIT_SPELL_RATING:
            case ITEM_MOD_HASTE_SPELL_RATING:
            case ITEM_MOD_SPELL_POWER:
            // TODO: Check proto->HasSpellPowerStat() for spellpower
            // TODO: Check SPELL_AURA_MOD_POWER_COST_SCHOOL for specific spell schools (e.g. shadow, holy etc)
                isCaster = true;
                continue;
            case ITEM_MOD_SPELL_HEALING_DONE:
            case ITEM_MOD_MANA_REGENERATION:
                isCaster = true;
                continue;
            case ITEM_MOD_ATTACK_POWER:
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                isPhysDPS = true;
                continue;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
            case ITEM_MOD_DODGE_RATING:
            case ITEM_MOD_PARRY_RATING:
                isTank = true;
                continue;
            case ITEM_MOD_BLOCK_RATING:
            case ITEM_MOD_BLOCK_VALUE:
                isTank = true;
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
    std::vector<Attributes> attributesMeleeStrDps;
    std::vector<Attributes> attributesMeleeStrTank;
    std::vector<Attributes> attributesMeleeAgiDps;
    std::vector<Attributes> attributesMeleeAgiTank;
    std::vector<Attributes> attributesRangedAgi;
    std::vector<Attributes> attributesCaster;
    std::vector<EnchantCategory> enchCatsMeleeStrDps;
    std::vector<EnchantCategory> enchCatsMeleeStrTank;
    std::vector<EnchantCategory> enchCatsMeleeAgiDps;
    std::vector<EnchantCategory> enchCatsMeleeAgiTank;
    std::vector<EnchantCategory> enchCatsRangedAgi;
    std::vector<EnchantCategory> enchCatsCaster;

    auto itemPlayerLevel = getItemPlayerLevel(item);
    switch (item->GetTemplate()->Class)
    {
        case ITEM_CLASS_ARMOR:
            switch (item->GetTemplate()->SubClass)
            {
                case ITEM_SUBCLASS_ARMOR_CLOTH:
                    attributesCaster.insert(attributesCaster.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPIRIT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    enchCatsCaster.insert(enchCatsCaster.end(), {ENCH_CAT_CASTER});
                    break;
                case ITEM_SUBCLASS_ARMOR_LEATHER:
                    attributesMeleeAgiDps.insert(attributesMeleeAgiDps.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeAgiTank.insert(attributesMeleeAgiTank.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_AGILITY,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE});
                    attributesCaster.insert(attributesCaster.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPIRIT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT});
                    enchCatsMeleeAgiDps.insert(enchCatsMeleeAgiDps.end(), {ENCH_CAT_MELEE_AGI_DPS});
                    enchCatsMeleeAgiTank.insert(enchCatsMeleeAgiTank.end(), {ENCH_CAT_MELEE_AGI_TANK});
                    if (itemPlayerLevel < 40)
                    {
                        attributesMeleeAgiDps.insert(attributesMeleeAgiDps.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPELLPOWER});
                        attributesRangedAgi.insert(attributesRangedAgi.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                        enchCatsRangedAgi.insert(enchCatsRangedAgi.end(), {ENCH_CAT_RANGED_AGI});
                    }
                    enchCatsCaster.insert(enchCatsCaster.end(), {ENCH_CAT_CASTER});
                    break;
                case ITEM_SUBCLASS_ARMOR_MAIL:
                    if (itemPlayerLevel < 40)
                    {
                        attributesMeleeStrDps.insert(attributesMeleeStrDps.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                        attributesMeleeStrTank.insert(attributesMeleeStrTank.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                        enchCatsMeleeStrDps.insert(enchCatsMeleeStrDps.end(), {ENCH_CAT_MELEE_STR_DPS});
                        enchCatsMeleeStrTank.insert(enchCatsMeleeStrTank.end(), {ENCH_CAT_MELEE_STR_TANK});
                    }
                    else
                    {
                        attributesMeleeAgiDps.insert(attributesMeleeAgiDps.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_INTELLECT,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_SPELLPOWER});
                        attributesRangedAgi.insert(attributesRangedAgi.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                        enchCatsMeleeAgiDps.insert(enchCatsMeleeAgiDps.end(), {ENCH_CAT_MELEE_AGI_DPS});
                        enchCatsRangedAgi.insert(enchCatsRangedAgi.end(), {ENCH_CAT_RANGED_AGI});
                    }
                    attributesCaster.insert(attributesCaster.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    enchCatsCaster.insert(enchCatsCaster.end(), {ENCH_CAT_CASTER});
                    break;
                case ITEM_SUBCLASS_ARMOR_PLATE:
                    attributesMeleeStrDps.insert(attributesMeleeStrDps.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeStrTank.insert(attributesMeleeStrTank.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                    attributesCaster.insert(attributesCaster.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    enchCatsMeleeStrDps.insert(enchCatsMeleeStrDps.end(), {ENCH_CAT_MELEE_STR_DPS});
                    enchCatsMeleeStrTank.insert(enchCatsMeleeStrTank.end(), {ENCH_CAT_MELEE_STR_TANK});
                    enchCatsCaster.insert(enchCatsCaster.end(), {ENCH_CAT_CASTER});
                    break;
                case ITEM_SUBCLASS_ARMOR_SHIELD:
                    attributesMeleeStrTank.insert(attributesMeleeStrTank.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                    attributesCaster.insert(attributesCaster.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    enchCatsMeleeStrTank.insert(enchCatsMeleeStrTank.end(), {ENCH_CAT_MELEE_STR_TANK});
                    enchCatsCaster.insert(enchCatsCaster.end(), {ENCH_CAT_CASTER});
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
                    attributesMeleeStrDps.insert(attributesMeleeStrDps.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeStrTank.insert(attributesMeleeStrTank.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                    attributesMeleeAgiDps.insert(attributesMeleeAgiDps.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeAgiTank.insert(attributesMeleeAgiTank.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE});
                    attributesRangedAgi.insert(attributesRangedAgi.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    attributesCaster.insert(attributesCaster.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPIRIT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    enchCatsMeleeStrDps.insert(enchCatsMeleeStrDps.end(), {ENCH_CAT_MELEE_STR_DPS});
                    enchCatsMeleeStrTank.insert(enchCatsMeleeStrTank.end(), {ENCH_CAT_MELEE_STR_TANK});
                    enchCatsMeleeAgiDps.insert(enchCatsMeleeAgiDps.end(), {ENCH_CAT_MELEE_AGI_DPS});
                    enchCatsMeleeAgiTank.insert(enchCatsMeleeAgiTank.end(), {ENCH_CAT_MELEE_AGI_TANK});
                    enchCatsRangedAgi.insert(enchCatsRangedAgi.end(), {ENCH_CAT_RANGED_AGI});
                    enchCatsCaster.insert(enchCatsCaster.end(), {ENCH_CAT_CASTER});
                    break;
                case ITEM_SUBCLASS_WEAPON_BOW:
                case ITEM_SUBCLASS_WEAPON_GUN:
                case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                    attributesMeleeStrDps.insert(attributesMeleeStrDps.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeStrTank.insert(attributesMeleeStrTank.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                    attributesMeleeAgiDps.insert(attributesMeleeAgiDps.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeAgiTank.insert(attributesMeleeAgiTank.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE});
                    attributesRangedAgi.insert(attributesRangedAgi.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    enchCatsMeleeStrDps.insert(enchCatsMeleeStrDps.end(), {ENCH_CAT_MELEE_STR_DPS});
                    enchCatsMeleeStrTank.insert(enchCatsMeleeStrTank.end(), {ENCH_CAT_MELEE_STR_TANK});
                    enchCatsMeleeAgiDps.insert(enchCatsMeleeAgiDps.end(), {ENCH_CAT_MELEE_AGI_DPS});
                    enchCatsMeleeAgiTank.insert(enchCatsMeleeAgiTank.end(), {ENCH_CAT_MELEE_AGI_TANK});
                    enchCatsRangedAgi.insert(enchCatsRangedAgi.end(), {ENCH_CAT_RANGED_AGI});
                    break;
                case ITEM_SUBCLASS_WEAPON_FIST:
                    attributesMeleeStrDps.insert(attributesMeleeStrDps.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeStrTank.insert(attributesMeleeStrTank.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                    attributesMeleeAgiDps.insert(attributesMeleeAgiDps.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeAgiTank.insert(attributesMeleeAgiTank.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE});
                    enchCatsMeleeStrDps.insert(enchCatsMeleeStrDps.end(), {ENCH_CAT_MELEE_STR_DPS});
                    enchCatsMeleeStrTank.insert(enchCatsMeleeStrTank.end(), {ENCH_CAT_MELEE_STR_TANK});
                    enchCatsMeleeAgiDps.insert(enchCatsMeleeAgiDps.end(), {ENCH_CAT_MELEE_AGI_DPS});
                    enchCatsMeleeAgiTank.insert(enchCatsMeleeAgiTank.end(), {ENCH_CAT_MELEE_AGI_TANK});
                    break;
                case ITEM_SUBCLASS_WEAPON_THROWN:
                    attributesMeleeStrDps.insert(attributesMeleeStrDps.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeStrTank.insert(attributesMeleeStrTank.end(), {ATTRIBUTE_STRENGTH,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE,ATTRIBUTE_PARRY});
                    attributesMeleeAgiDps.insert(attributesMeleeAgiDps.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_ATTACKPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT,ATTRIBUTE_EXPERTISE});
                    attributesMeleeAgiTank.insert(attributesMeleeAgiTank.end(), {ATTRIBUTE_AGILITY,ATTRIBUTE_STAMINA,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_EXPERTISE,ATTRIBUTE_DEFENSERATING,ATTRIBUTE_DODGE});
                    enchCatsMeleeStrDps.insert(enchCatsMeleeStrDps.end(), {ENCH_CAT_MELEE_STR_DPS});
                    enchCatsMeleeStrTank.insert(enchCatsMeleeStrTank.end(), {ENCH_CAT_MELEE_STR_TANK});
                    enchCatsMeleeAgiDps.insert(enchCatsMeleeAgiDps.end(), {ENCH_CAT_MELEE_AGI_DPS});
                    enchCatsMeleeAgiTank.insert(enchCatsMeleeAgiTank.end(), {ENCH_CAT_MELEE_AGI_TANK});
                    break;
                case ITEM_SUBCLASS_WEAPON_WAND:
                    attributesCaster.insert(attributesCaster.end(), {ATTRIBUTE_INTELLECT,ATTRIBUTE_SPIRIT,ATTRIBUTE_STAMINA,ATTRIBUTE_SPELLPOWER,ATTRIBUTE_HASTE,ATTRIBUTE_HIT,ATTRIBUTE_CRIT});
                    enchCatsCaster.insert(enchCatsCaster.end(), {ENCH_CAT_CASTER});
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
    std::vector<Attributes> itemAttrs;
    struct retVals {
        uint32 enchCatMask, attrMask;
    };
    bool noStats = !isRanged && !isMelee && !isPhysDPS && !isStr && !isAgi && !isTank && !isCaster;
    if (config_debug)
    {
        LOG_INFO("module", "RANDOM_ENCHANT: Getting item enchant mask, checks below:");
        LOG_INFO("module", "       For item {}, Item ID is: {}", item->GetTemplate()->Name1, item->GetTemplate()->ItemId);
        LOG_INFO("module", "                isRanged = {}", isRanged); 
        LOG_INFO("module", "                isMelee = {}", isMelee); 
        LOG_INFO("module", "                isPhysDPS = {}", isPhysDPS); 
        LOG_INFO("module", "                isStr = {}", isStr); 
        LOG_INFO("module", "                isAgi = {}", isAgi); 
        LOG_INFO("module", "                isTank = {}", isTank); 
        LOG_INFO("module", "                isCaster = {}", isCaster); 
        LOG_INFO("module", "                noStats = {}", noStats);
    }
    if (noStats) {
        // If no stats, we add every item category from the item classes (armour type, weapon weapon type etc etc).
        itemAttrs.insert(itemAttrs.end(), attributesMeleeStrDps.begin(), attributesMeleeStrDps.end());
        itemAttrs.insert(itemAttrs.end(), attributesMeleeStrTank.begin(), attributesMeleeStrTank.end());
        itemAttrs.insert(itemAttrs.end(), attributesMeleeAgiDps.begin(), attributesMeleeAgiDps.end());
        itemAttrs.insert(itemAttrs.end(), attributesMeleeAgiTank.begin(), attributesMeleeAgiTank.end());
        itemAttrs.insert(itemAttrs.end(), attributesRangedAgi.begin(), attributesRangedAgi.end());
        itemAttrs.insert(itemAttrs.end(), attributesCaster.begin(), attributesCaster.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsMeleeStrDps.begin(), enchCatsMeleeStrDps.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsMeleeStrTank.begin(), enchCatsMeleeStrTank.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsMeleeAgiDps.begin(), enchCatsMeleeAgiDps.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsMeleeAgiTank.begin(), enchCatsMeleeAgiTank.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsRangedAgi.begin(), enchCatsRangedAgi.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsCaster.begin(), enchCatsCaster.end());
        return retVals{getEnchantCategoryMask(itmEnchCats), getAttributeMask(itemAttrs)};
    }

    bool isMeleeStrDPS = (isMelee || isPhysDPS) && isStr;
    bool isMeleeStrTank = isTank && isStr;
    bool isMeleeAgiDPS = (isMelee || isPhysDPS) && isAgi;
    bool isMeleeAgiTank = isTank && isAgi;
    bool isRangedAgi = (isRanged || isPhysDPS) && isAgi;
    if (isStr && !isMeleeStrDPS && !isMeleeStrTank)
    {
        // Has str but no general melee or tank stats, set both
        isMeleeStrDPS = true;
        isMeleeStrTank = true;
    }
    if (isAgi && !isMeleeAgiDPS && !isMeleeAgiTank)
    {
        // Has agi but no general melee or tank stats, set both
        isMeleeAgiDPS = true;
        isMeleeAgiTank = true;
    }
    if (!isStr && !isAgi)
    {
        // No main stat for str and agi
        if (isMelee)
        {
            isMeleeStrDPS = true;
            isMeleeAgiDPS = true;
        }
        if (isTank)
        {
            isMeleeStrTank = true;
            isMeleeAgiTank = true;
        }
        if (isPhysDPS)
        {
            isMeleeStrDPS = true;
            isMeleeAgiDPS = true;
            isRangedAgi = true;
        }
        if (isRanged)
        {
            isRangedAgi = true;
        }
    }

    if (isCaster)
    {
        itemAttrs.insert(itemAttrs.end(), attributesCaster.begin(), attributesCaster.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsCaster.begin(), enchCatsCaster.end());
    }
    if (isMeleeStrDPS)
    {
        itemAttrs.insert(itemAttrs.end(), attributesMeleeStrDps.begin(), attributesMeleeStrDps.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsMeleeStrDps.begin(), enchCatsMeleeStrDps.end());
    }
    if (isMeleeStrTank)
    {
        itemAttrs.insert(itemAttrs.end(), attributesMeleeStrTank.begin(), attributesMeleeStrTank.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsMeleeStrTank.begin(), enchCatsMeleeStrTank.end());
    }
    if (isMeleeAgiDPS)
    {
        itemAttrs.insert(itemAttrs.end(), attributesMeleeAgiDps.begin(), attributesMeleeAgiDps.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsMeleeAgiDps.begin(), enchCatsMeleeAgiDps.end());
    }
    if (isMeleeAgiTank)
    {
        itemAttrs.insert(itemAttrs.end(), attributesMeleeAgiTank.begin(), attributesMeleeAgiTank.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsMeleeAgiTank.begin(), enchCatsMeleeAgiTank.end());
    }
    if (isRangedAgi)
    {
        itemAttrs.insert(itemAttrs.end(), attributesRangedAgi.begin(), attributesRangedAgi.end());
        itmEnchCats.insert(itmEnchCats.end(), enchCatsRangedAgi.begin(), enchCatsRangedAgi.end());
    }
    return retVals{getEnchantCategoryMask(itmEnchCats), getAttributeMask(itemAttrs)};
}

auto getPlayerItemEnchantCategoryMask(Item* item, Player* player = nullptr)
{
    struct retVals {
        uint32 enchCatMask, attrMask;
    };
    if (config_roll_player_class_preference && player->CanUseItem(item, false) == EQUIP_ERR_OK)
    {
        if (config_debug)
        {
            LOG_INFO("module", "RANDOM_ENCHANT: Getting player class preference for enchant category");
        }
        auto [enchMask, attrMask] = getPlayerEnchantCategoryMask(player);
        return retVals{enchMask, attrMask};
    }
    if (config_debug)
    {
        LOG_INFO("module", "RANDOM_ENCHANT: Getting item enchant category");
    }
    auto [enchMask, attrMask] = getItemEnchantCategoryMask(item);
    return retVals{enchMask, attrMask};
}

// END UTILS

// MAIN GET ROLL ENCHANT FUNCTIONS

int32 getCustomRandomSuffix(int enchantQuality, Item* item, Player* player = nullptr)
{
    uint32 Class = item->GetTemplate()->Class;
    uint32 subclassMask = 1 << item->GetTemplate()->SubClass;
    // int level = getLevelOffset(item, player);
    int level = getItemPlayerLevel(item);
    auto [enchantCategoryMask, attrMask] = getPlayerItemEnchantCategoryMask(item, player);

    int maxCount = 50;
    while (maxCount > 0)
    {
    QueryResult qr = WorldDatabase.Query(R"(SELECT ID FROM item_enchantment_random_suffixes
INNER JOIN itemrandomsuffix_dbc ON
item_enchantment_random_suffixes.SuffixID = itemrandomsuffix_dbc.ID
WHERE
((MinLevel <= {} AND {} <= MaxLevel) OR (MinLevel = 0 AND MaxLevel = 0))
AND (
(ItemClass = 0) OR
(ItemClass = {} AND ItemSubClassMask = 0) OR
(ItemClass = {} AND ItemSubClassMask & {} > 0)
)
AND EnchantQuality = {}
AND (
  (AttributeMask = 0) OR
  ((AttributeMask & {} > 0) AND (AttributeMask & ~{} = 0))
)
AND (
  (EnchantCategoryMask = 0) OR
  (EnchantCategoryMask & {} > 0)
) ORDER BY RAND() LIMIT 1)", level, level, Class, Class, subclassMask, enchantQuality, attrMask, attrMask, enchantCategoryMask);
        if (qr)
        {
            int suffixID = qr->Fetch()[0].Get<uint32>();
            ItemRandomSuffixEntry const* item_rand = sItemRandomSuffixStore.LookupEntry(suffixID);
            if (!item_rand)
            {
                LOG_INFO("module", "Suffix ID does not exist to be enchanted, getting a new one: {}", suffixID);
                // get suffixID failed for some reason, should not happen, we still just continue and try to get
                // another one.
                maxCount--;
                continue;
            }
            uint32 minAllocPct = 3567587328;
            for (uint8 k = 0; k != MAX_ITEM_ENCHANTMENT_EFFECTS; ++k)
            {
                if (minAllocPct > item_rand->AllocationPct[k])
                {
                    minAllocPct = item_rand->AllocationPct[k];
                }
            }
            auto suffFactor = GenerateEnchSuffixFactor(item->GetTemplate()->ItemId); 
            int32 basepoints = int32(minAllocPct * suffFactor / 10000);
            if (basepoints < 1)
            {
                // Suffix ID should ideally be above 1 after suffix factor calculations
                // This is so that when presented on the client we dont get some weird looking values
                LOG_INFO("module", "Suffix min alloc pct calculation is below one, getting a new one: suffID: {}, suffFactor: {}, minAllocPct: {}", suffixID, suffFactor, minAllocPct);
                maxCount--;
                continue;
            }
            if (config_debug)
            {
                LOG_INFO("module", "RANDOM_ENCHANT: Query with the following params:");
                LOG_INFO("module", "                level {}, enchantQuality {}, item_class {}, subclassmask {}, enchCatMask {}, attrMask {}", level, enchantQuality, Class, subclassMask, enchantCategoryMask, attrMask);
                LOG_INFO("module", "                Return was: {}", suffixID);
            }
            return suffixID;
        }
    }
    LOG_INFO("module", "rerolled rolls a max number of times already times, but no candidate enchants, returning without a suffix");
    return -1;
}

int GetRolledEnchantLevel()
{
    int currentTier = -1;
    for (auto rollpct: config_enchant_pcts)
    {
        double roll = (float)rand_chance();
        if (roll + rollpct < 100.0)
        {
            // If roll was not successful, we break, no more attempted rolls beyond this;
            break;
        }
        currentTier++;
    }
    return currentTier;
}

void RollPossibleEnchant(Player* player, Item* item)
{
    uint32 Quality = item->GetTemplate()->Quality;
    uint32 Class = item->GetTemplate()->Class;

    switch (item->GetTemplate()->InventoryType)
    {
        // Dont roll if its of these types (Taken from GenerateEnchSuffixFactor)
        // Items of that type don`t have points
        case INVTYPE_NON_EQUIP:
        case INVTYPE_BAG:
        case INVTYPE_TABARD:
        case INVTYPE_AMMO:
        case INVTYPE_QUIVER:
        case INVTYPE_RELIC:
            return;
    }
    if (
        (Quality > ITEM_QUALITY_LEGENDARY || Quality < ITEM_QUALITY_UNCOMMON) /* eliminates enchanting anything that isn't a recognized quality */ ||
        (Class != ITEM_CLASS_WEAPON && Class != ITEM_CLASS_ARMOR) /* eliminates enchanting anything but weapons/armor */)
    {
        return;
    }

    if (item->GetItemRandomPropertyId() != 0)
    {
        // If already have enchant, we shouldnt apply a new one
        return;
    }

    auto rolledEnchantLevel = GetRolledEnchantLevel();
    if (rolledEnchantLevel < 0)
    {
        // Failed roll
        return;
    }
    auto suffixID = getCustomRandomSuffix(rolledEnchantLevel, item, player);
    if (suffixID < 0)
    {
        return;
    }
    // Apply the suffix to the item
    ItemRandomSuffixEntry const* item_rand = sItemRandomSuffixStore.LookupEntry(suffixID);
    if (!item_rand)
    {
        return;
    }
    item->SetItemRandomProperties(-suffixID);
    ChatHandler chathandle = ChatHandler(player->GetSession());
    uint32 loc = player->GetSession()->GetSessionDbLocaleIndex();
    std::string suffixName = item_rand->Name[loc];
    chathandle.PSendSysMessage("|cffFF0000 %s |rhas rolled the suffix|cffFF0000 %s |r!", item->GetTemplate()->Name1.c_str(), suffixName);
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
        config_roll_player_class_preference =  sConfigMgr->GetOption<bool>("RandomEnchants.RollPlayerClassPreference", default_roll_player_class_preference);
        config_login_message = sConfigMgr->GetOption<std::string>("RandomEnchants.OnLoginMessage", default_login_message);
        config_enchant_pcts[0] = sConfigMgr->GetOption<float>("RandomEnchants.RollPercentage.1", default_enchant_pcts[0]);
        config_enchant_pcts[1] = sConfigMgr->GetOption<float>("RandomEnchants.RollPercentage.2", default_enchant_pcts[1]);
        config_enchant_pcts[2] = sConfigMgr->GetOption<float>("RandomEnchants.RollPercentage.3", default_enchant_pcts[2]);
        config_enchant_pcts[3] = sConfigMgr->GetOption<float>("RandomEnchants.RollPercentage.4", default_enchant_pcts[3]);
    }
};

class RandomEnchantsPlayer : public PlayerScript{
public:

    RandomEnchantsPlayer() : PlayerScript("RandomEnchantsPlayer") { }

    void OnLogin(Player* player) override {
        if (config_announce_on_log)
        {
            ChatHandler(player->GetSession()).SendSysMessage(config_login_message);
        }
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


using namespace Acore::ChatCommands;

class RandomEnchantCommands : public CommandScript
{
public:
    RandomEnchantCommands() : CommandScript("RandomEnchantCommands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "additemwsuffix",           HandleAddItemCommand,           SEC_GAMEMASTER,         Console::No  },
        };
        return commandTable;
    }
    static bool HandleAddItemCommand(ChatHandler* handler, ItemTemplate const* itemTemplate, Optional<int32> _count, Optional<int32> _suffID)
    {
        if (!sObjectMgr->GetItemTemplate(itemTemplate->ItemId))
        {
            handler->PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, itemTemplate->ItemId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = itemTemplate->ItemId;
        int32 count = 1;

        if (_count)
        {
            count = *_count;
        }

        if (!count)
        {
            count = 1;
        }
        int32 suffID = 0;
        if (_suffID)
        {
            suffID = *_suffID;
        }

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();

        if (!playerTarget)
        {
            playerTarget = player;
        }

        // Subtract
        if (count < 0)
        {
            // Only have scam check on player accounts
            if (playerTarget->GetSession()->GetSecurity() == SEC_PLAYER)
            {
                if (!playerTarget->HasItemCount(itemId, 0))
                {
                    // output that player don't have any items to destroy
                    handler->PSendSysMessage(LANG_REMOVEITEM_FAILURE, handler->GetNameLink(playerTarget).c_str(), itemId);
                    handler->SetSentErrorMessage(true);
                    return false;
                }

                if (!playerTarget->HasItemCount(itemId, -count))
                {
                    // output that player don't have as many items that you want to destroy
                    handler->PSendSysMessage(LANG_REMOVEITEM_ERROR, handler->GetNameLink(playerTarget).c_str(), itemId);
                    handler->SetSentErrorMessage(true);
                    return false;
                }
            }

            // output successful amount of destroyed items
            playerTarget->DestroyItemCount(itemId, -count, true, false);
            handler->PSendSysMessage(LANG_REMOVEITEM, itemId, -count, handler->GetNameLink(playerTarget).c_str());
            return true;
        }

        // Adding items
        uint32 noSpaceForCount = 0;

        // check space and find places
        ItemPosCountVec dest;
        InventoryResult msg = playerTarget->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count, &noSpaceForCount);

        if (msg != EQUIP_ERR_OK) // convert to possible store amount
        {
            count -= noSpaceForCount;
        }

        if (!count || dest.empty()) // can't add any
        {
            handler->PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itemId, noSpaceForCount);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Item* item = playerTarget->StoreNewItem(dest, itemId, true);

        // remove binding (let GM give it to another player later)
        if (player == playerTarget)
        {
            for (auto const& itemPos : dest)
            {
                if (Item* item1 = player->GetItemByPos(itemPos.pos))
                {
                    item1->SetBinding(false);
                }
            }
        }

        if (count && item)
        {
            item->SetItemRandomProperties(suffID);
            player->SendNewItem(item, count, false, true);

            if (player != playerTarget)
            {
                playerTarget->SendNewItem(item, count, true, false);
            }
        }

        if (noSpaceForCount)
        {
            handler->PSendSysMessage(LANG_ITEM_CANNOT_CREATE, itemId, noSpaceForCount);
        }

        return true;
    }
};

void AddRandomEnchantsScripts() {
    new RandomEnchantsWorldScript();
    new RandomEnchantsPlayer();
    new RandomEnchantCommands();
    // new RandomEnchantsMisc();
}
