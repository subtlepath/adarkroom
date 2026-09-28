// items.h — everything that can be counted in A Dark Room: the stores (and what the path can
// carry), the village's buildings, its workers and the wanderer's perks. The names are the game's
// own keys (script/room.js, script/fabricator.js, script/world.js); the event tables generated from
// script/events (tools/extract_events.cjs) refer to these same ids.
#pragma once

#include <cstdint>

namespace adr {

// How an item is shown and carried: the original's craftable "type" (room.js Craftables,
// TradeGoods, MiscItems, fabricator.js Craftables). Resources have none of their own.
enum class ItemType : uint8_t { Resource, Good, Tool, Weapon, Upgrade, Special, Blueprint, Pseudo };

// ITEM(id, key, type, weight x10)
#define ADR_ITEMS(ITEM)                                         \
  ITEM(WOOD, "wood", Resource, 10)                              \
  ITEM(FUR, "fur", Resource, 10)                                \
  ITEM(MEAT, "meat", Resource, 10)                              \
  ITEM(SCALES, "scales", Good, 10)                              \
  ITEM(TEETH, "teeth", Good, 10)                                \
  ITEM(CLOTH, "cloth", Resource, 10)                            \
  ITEM(CHARM, "charm", Resource, 10)                            \
  ITEM(BAIT, "bait", Resource, 10)                              \
  ITEM(LEATHER, "leather", Resource, 10)                        \
  ITEM(CURED_MEAT, "cured meat", Resource, 10)                  \
  ITEM(IRON, "iron", Good, 10)                                  \
  ITEM(COAL, "coal", Good, 10)                                  \
  ITEM(SULPHUR, "sulphur", Resource, 10)                        \
  ITEM(STEEL, "steel", Good, 10)                                \
  ITEM(BULLETS, "bullets", Good, 1)                             \
  ITEM(MEDICINE, "medicine", Good, 10)                          \
  ITEM(ENERGY_CELL, "energy cell", Good, 2)                     \
  ITEM(ALIEN_ALLOY, "alien alloy", Good, 10)                    \
  ITEM(FLEET_BEACON, "fleet beacon", Resource, 10)              \
  ITEM(COMPASS, "compass", Special, 10)                         \
  ITEM(TORCH, "torch", Tool, 10)                                \
  ITEM(HYPO, "hypo", Tool, 10)                                  \
  ITEM(STIM, "stim", Tool, 10)                                  \
  ITEM(GLOWSTONE, "glowstone", Tool, 10)                        \
  ITEM(BONE_SPEAR, "bone spear", Weapon, 20)                    \
  ITEM(IRON_SWORD, "iron sword", Weapon, 30)                    \
  ITEM(STEEL_SWORD, "steel sword", Weapon, 50)                  \
  ITEM(BAYONET, "bayonet", Weapon, 10)                          \
  ITEM(RIFLE, "rifle", Weapon, 50)                              \
  ITEM(LASER_RIFLE, "laser rifle", Weapon, 50)                  \
  ITEM(PLASMA_RIFLE, "plasma rifle", Weapon, 50)                \
  ITEM(ENERGY_BLADE, "energy blade", Weapon, 10)                \
  ITEM(DISRUPTOR, "disruptor", Weapon, 10)                      \
  ITEM(GRENADE, "grenade", Weapon, 10)                          \
  ITEM(BOLAS, "bolas", Weapon, 5)                               \
  ITEM(WATERSKIN, "waterskin", Upgrade, 10)                     \
  ITEM(CASK, "cask", Upgrade, 10)                               \
  ITEM(WATER_TANK, "water tank", Upgrade, 10)                   \
  ITEM(RUCKSACK, "rucksack", Upgrade, 10)                       \
  ITEM(WAGON, "wagon", Upgrade, 10)                             \
  ITEM(CONVOY, "convoy", Upgrade, 10)                           \
  ITEM(L_ARMOUR, "l armour", Upgrade, 10)                       \
  ITEM(I_ARMOUR, "i armour", Upgrade, 10)                       \
  ITEM(S_ARMOUR, "s armour", Upgrade, 10)                       \
  ITEM(FLUID_RECYCLER, "fluid recycler", Upgrade, 10)           \
  ITEM(CARGO_DRONE, "cargo drone", Upgrade, 10)                 \
  ITEM(KINETIC_ARMOUR, "kinetic armour", Upgrade, 10)           \
  ITEM(HYPO_BLUEPRINT, "hypo blueprint", Blueprint, 10)         \
  ITEM(KINETIC_ARMOUR_BLUEPRINT, "kinetic armour blueprint", Blueprint, 10) \
  ITEM(DISRUPTOR_BLUEPRINT, "disruptor blueprint", Blueprint, 10) \
  ITEM(PLASMA_RIFLE_BLUEPRINT, "plasma rifle blueprint", Blueprint, 10) \
  ITEM(STIM_BLUEPRINT, "stim blueprint", Blueprint, 10)         \
  ITEM(GLOWSTONE_BLUEPRINT, "glowstone blueprint", Blueprint, 10) \
  ITEM(WATER, "water", Pseudo, 0)                               \
  ITEM(HP, "hp", Pseudo, 0)

enum Item : uint8_t {
#define ADR_ITEM_ENUM(id, key, type, w) IT_##id,
  ADR_ITEMS(ADR_ITEM_ENUM)
#undef ADR_ITEM_ENUM
  IT_COUNT,
  IT_NONE = 0xFF
};
// The pseudo items (an event's cost in water or hp) are not stores.
constexpr int kStoreCount = IT_WATER;

struct ItemInfo {
  const char* key;
  ItemType type;
  uint8_t weight10;  // Path.Weight, in tenths (bullets weigh 0.1)
};
extern const ItemInfo kItems[IT_COUNT];
inline const char* itemName(Item i) { return i < IT_COUNT ? kItems[i].key : "?"; }
inline ItemType itemType(Item i) { return kItems[i].type; }
// What the glass calls it: the game's key, save that the fabricator's glowstone is "glow stone".
const char* itemLabel(Item i);

// BUILDING(id, key)
#define ADR_BUILDINGS(B)                 \
  B(TRAP, "trap")                        \
  B(CART, "cart")                        \
  B(HUT, "hut")                          \
  B(LODGE, "lodge")                      \
  B(TRADING_POST, "trading post")        \
  B(TANNERY, "tannery")                  \
  B(SMOKEHOUSE, "smokehouse")            \
  B(WORKSHOP, "workshop")                \
  B(STEELWORKS, "steelworks")            \
  B(ARMOURY, "armoury")                  \
  B(IRON_MINE, "iron mine")              \
  B(COAL_MINE, "coal mine")              \
  B(SULPHUR_MINE, "sulphur mine")

enum Building : uint8_t {
#define ADR_B_ENUM(id, key) BD_##id,
  ADR_BUILDINGS(ADR_B_ENUM)
#undef ADR_B_ENUM
  BD_COUNT
};
extern const char* const kBuildingNames[BD_COUNT];

// The workers, in outside.js _INCOME order. The gatherer is whoever is left.
#define ADR_WORKERS(W)                                    \
  W(GATHERER, "gatherer", BD_COUNT)                       \
  W(HUNTER, "hunter", BD_LODGE)                           \
  W(TRAPPER, "trapper", BD_LODGE)                         \
  W(TANNER, "tanner", BD_TANNERY)                         \
  W(CHARCUTIER, "charcutier", BD_SMOKEHOUSE)              \
  W(IRON_MINER, "iron miner", BD_IRON_MINE)               \
  W(COAL_MINER, "coal miner", BD_COAL_MINE)               \
  W(SULPHUR_MINER, "sulphur miner", BD_SULPHUR_MINE)      \
  W(STEELWORKER, "steelworker", BD_STEELWORKS)            \
  W(ARMOURER, "armourer", BD_ARMOURY)

enum Worker : uint8_t {
#define ADR_W_ENUM(id, key, b) WK_##id,
  ADR_WORKERS(ADR_W_ENUM)
#undef ADR_W_ENUM
  WK_COUNT
};
struct WorkerInfo {
  const char* key;
  Building building;
  uint8_t delay;           // seconds between collections
  struct { Item item; int8_t n2; } stores[3];  // per worker, in halves (a hunter brings 0.5 fur)
};
extern const WorkerInfo kWorkers[WK_COUNT];

// Income sources: the workers, then the builder and the thieves.
enum IncomeSource : uint8_t { SRC_BUILDER = WK_COUNT, SRC_THIEVES, SRC_COUNT };
const char* incomeName(uint8_t src);

#define ADR_PERKS(P)                                                                             \
  P(BOXER, "boxer", "punches do more damage", "learned to throw punches with purpose")          \
  P(MARTIAL_ARTIST, "martial artist", "punches do even more damage.",                          \
    "learned to fight quite effectively without weapons")                                       \
  P(UNARMED_MASTER, "unarmed master", "punch twice as fast, and with even more force",         \
    "learned to strike faster without weapons")                                                 \
  P(BARBARIAN, "barbarian", "melee weapons deal more damage", "learned to swing weapons with force") \
  P(SLOW_METABOLISM, "slow metabolism", "go twice as far without eating",                      \
    "learned how to ignore the hunger")                                                         \
  P(DESERT_RAT, "desert rat", "go twice as far without drinking", "learned to love the dry air") \
  P(EVASIVE, "evasive", "dodge attacks more effectively", "learned to be where they're not")    \
  P(PRECISE, "precise", "land blows more often", "learned to predict their movement")           \
  P(SCOUT, "scout", "see farther", "learned to look ahead")                                     \
  P(STEALTHY, "stealthy", "better avoid conflict in the wild", "learned how not to be seen")    \
  P(GASTRONOME, "gastronome", "restore more health when eating", "learned to make the most of food")

enum Perk : uint8_t {
#define ADR_P_ENUM(id, key, desc, notify) PK_##id,
  ADR_PERKS(ADR_P_ENUM)
#undef ADR_P_ENUM
  PK_COUNT
};
struct PerkInfo {
  const char* key;
  const char* desc;
  const char* notify;
};
extern const PerkInfo kPerks[PK_COUNT];

// The weapons (world.js Weapons), in the order the original lists them: the combat screen's
// attack buttons follow it.
enum WeaponType : uint8_t { WT_UNARMED, WT_MELEE, WT_RANGED };
struct WeaponInfo {
  Item item;            // IT_NONE for the fists
  const char* verb;
  WeaponType type;
  int8_t damage;        // -1: stun
  uint8_t cooldown;     // seconds
  Item ammo;            // IT_NONE: none
  uint8_t ammoN;
};
constexpr int kWeaponCount = 12;
extern const WeaponInfo kWeapons[kWeaponCount];
const WeaponInfo* weaponFor(Item it);  // null when the item is no weapon

}  // namespace adr
