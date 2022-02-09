/*
* Converted from the original LUA script to a module for Azerothcore(Sunwell) :D
*/
#include "ScriptMgr.h"
#include "Player.h"
#include "Configuration/Config.h"
#include "Chat.h"
#include "Item.h"

// Our random enchants can go up to 5 slots and they occupy the same slots as the stats given from
// random props. i.e. If there are 2 slots occupied from random properties, then we have a max of
// 5 - 2 = 3 slots. 
#define MAX_RAND_ENCHANT_SLOTS 5

double default_enchant_pcts[MAX_RAND_ENCHANT_SLOTS] = {30.0, 35.0, 40.0, 45.0, 50.0};

uint32 default_allowed_rand_enchant_slots[MAX_RAND_ENCHANT_SLOTS] = {
	PROP_ENCHANTMENT_SLOT_4,
	PROP_ENCHANTMENT_SLOT_3,
	PROP_ENCHANTMENT_SLOT_2,
	PROP_ENCHANTMENT_SLOT_1,
	PROP_ENCHANTMENT_SLOT_0,
};

class RandomEnchantsPlayer : public PlayerScript{
public:

    RandomEnchantsPlayer() : PlayerScript("RandomEnchantsPlayer") { }

    void OnLogin(Player* player) override {
		if (sConfigMgr->GetOption<bool>("RandomEnchants.AnnounceOnLogin", true))
            ChatHandler(player->GetSession()).SendSysMessage(sConfigMgr->GetOption<std::string>("RandomEnchants.OnLoginMessage", "This server is running a RandomEnchants Module.").c_str());
    }
	void OnLootItem(Player* player, Item* item, uint32 /*count*/, ObjectGuid /*lootguid*/) override
	{
		if (sConfigMgr->GetOption<bool>("RandomEnchants.OnLoot", true))
			RollPossibleEnchant(player, item);
	}
	void OnCreateItem(Player* player, Item* item, uint32 /*count*/) override
	{
		if (sConfigMgr->GetOption<bool>("RandomEnchants.OnCreate", true))
			RollPossibleEnchant(player, item);
	}
	void OnQuestRewardItem(Player* player, Item* item, uint32 /*count*/) override
	{
		if(sConfigMgr->GetOption<bool>("RandomEnchants.OnQuestReward", true))
			RollPossibleEnchant(player, item);
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

		std::vector<std::pair<uint32, EnchantmentSlot>> rolledEnchants = GetRolledEnchants(item);
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

		QueryResult qr = WorldDatabase.Query("SELECT enchantID FROM item_enchantment_random_tiers WHERE tier='{}' AND exclusiveSubClass=NULL AND class='{}' OR exclusiveSubClass='{}' OR class='ANY' ORDER BY RAND() LIMIT 1", tier, ClassQueryString, item->GetTemplate()->SubClass);
		return qr->Fetch()[0].Get<uint32>();
	}
	std::vector<std::pair<uint32, EnchantmentSlot>> GetRolledEnchants(Item* item)
	{
		std::vector<EnchantmentSlot> availableSlots = GetAvailableEnchantSlots(item);
		std::vector<std::pair<uint32, EnchantmentSlot>> rolledEnchants;
		std::size_t i = 0;
		std::vector<std::string> keys = sConfigMgr->GetKeysByString("RandomEnchants.RollPercentage.");
		for (std::string const& rollPctKey : keys)
		{
			if (i >= MAX_RAND_ENCHANT_SLOTS || i >= availableSlots.size())
			{
				// cannot go beyond the maximum random enchant slots or the number of available slots available
				break;
			}
			float rollpct = sConfigMgr->GetOption<float>(rollPctKey, default_enchant_pcts[i]);
			float roll = (float)rand_chance();
			if (roll + rollpct < 100.0)
			{
				// If roll was not successful, we break, no more attempted rolls beyond this;
				break;
			}
			int randEnch = getRandEnchantment(item);
			while (randEnch <= 0)
			{
				// get rand enchant ID failed for some reason, should not happen, we still just continue and try to get
				// another one.
                                randEnch = getRandEnchantment(item);
				continue;
			}
			rolledEnchants.push_back(std::make_pair((uint32)randEnch, availableSlots[i]));
			i++;
		}
		return rolledEnchants;
	}
	std::vector<EnchantmentSlot> GetAvailableEnchantSlots(Item* item)
	{
		std::vector<EnchantmentSlot> availableSlots;
		for (int i = 0; i < MAX_RAND_ENCHANT_SLOTS; i++)
		{
			EnchantmentSlot slot = EnchantmentSlot(default_allowed_rand_enchant_slots[i]);
			if (uint32 enchId = item->GetEnchantmentId(slot))
			{
				// Do not add slots that have an enchantment slot
				continue;
			}
			availableSlots.push_back(slot);
		}
		return availableSlots;
	}
};

void AddRandomEnchantsScripts() {
    new RandomEnchantsPlayer();
}

