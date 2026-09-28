#include "items.h"

namespace adr {

const ItemInfo kItems[IT_COUNT] = {
#define ADR_ITEM_INFO(id, key, type, w) {key, ItemType::type, w},
    ADR_ITEMS(ADR_ITEM_INFO)
#undef ADR_ITEM_INFO
};

const char* itemLabel(Item i) { return i == IT_GLOWSTONE ? "glow stone" : itemName(i); }

const char* const kBuildingNames[BD_COUNT] = {
#define ADR_B_NAME(id, key) key,
    ADR_BUILDINGS(ADR_B_NAME)
#undef ADR_B_NAME
};

const WorkerInfo kWorkers[WK_COUNT] = {
    {"gatherer", BD_COUNT, 10, {{IT_WOOD, 2}, {IT_NONE, 0}, {IT_NONE, 0}}},
    {"hunter", BD_LODGE, 10, {{IT_FUR, 1}, {IT_MEAT, 1}, {IT_NONE, 0}}},
    {"trapper", BD_LODGE, 10, {{IT_MEAT, -2}, {IT_BAIT, 2}, {IT_NONE, 0}}},
    {"tanner", BD_TANNERY, 10, {{IT_FUR, -10}, {IT_LEATHER, 2}, {IT_NONE, 0}}},
    {"charcutier", BD_SMOKEHOUSE, 10, {{IT_MEAT, -10}, {IT_WOOD, -10}, {IT_CURED_MEAT, 2}}},
    {"iron miner", BD_IRON_MINE, 10, {{IT_CURED_MEAT, -2}, {IT_IRON, 2}, {IT_NONE, 0}}},
    {"coal miner", BD_COAL_MINE, 10, {{IT_CURED_MEAT, -2}, {IT_COAL, 2}, {IT_NONE, 0}}},
    {"sulphur miner", BD_SULPHUR_MINE, 10, {{IT_CURED_MEAT, -2}, {IT_SULPHUR, 2}, {IT_NONE, 0}}},
    {"steelworker", BD_STEELWORKS, 10, {{IT_IRON, -2}, {IT_COAL, -2}, {IT_STEEL, 2}}},
    {"armourer", BD_ARMOURY, 10, {{IT_STEEL, -2}, {IT_SULPHUR, -2}, {IT_BULLETS, 2}}},
};

const char* incomeName(uint8_t src) {
  if (src < WK_COUNT) return kWorkers[src].key;
  return src == SRC_BUILDER ? "builder" : "thieves";
}

const PerkInfo kPerks[PK_COUNT] = {
#define ADR_P_INFO(id, key, desc, notify) {key, desc, notify},
    ADR_PERKS(ADR_P_INFO)
#undef ADR_P_INFO
};

const WeaponInfo kWeapons[kWeaponCount] = {
    {IT_NONE, "punch", WT_UNARMED, 1, 2, IT_NONE, 0},
    {IT_BONE_SPEAR, "stab", WT_MELEE, 2, 2, IT_NONE, 0},
    {IT_IRON_SWORD, "swing", WT_MELEE, 4, 2, IT_NONE, 0},
    {IT_STEEL_SWORD, "slash", WT_MELEE, 6, 2, IT_NONE, 0},
    {IT_BAYONET, "thrust", WT_MELEE, 8, 2, IT_NONE, 0},
    {IT_RIFLE, "shoot", WT_RANGED, 5, 1, IT_BULLETS, 1},
    {IT_LASER_RIFLE, "blast", WT_RANGED, 8, 1, IT_ENERGY_CELL, 1},
    {IT_GRENADE, "lob", WT_RANGED, 15, 5, IT_GRENADE, 1},
    {IT_BOLAS, "tangle", WT_RANGED, -1, 15, IT_BOLAS, 1},
    {IT_PLASMA_RIFLE, "disintegrate", WT_RANGED, 12, 1, IT_ENERGY_CELL, 1},
    {IT_ENERGY_BLADE, "slice", WT_MELEE, 10, 2, IT_NONE, 0},
    {IT_DISRUPTOR, "stun", WT_RANGED, -1, 15, IT_NONE, 0},
};

const WeaponInfo* weaponFor(Item it) {
  if (it == IT_NONE) return &kWeapons[0];
  for (const WeaponInfo& w : kWeapons)
    if (w.item == it) return &w;
  return nullptr;
}

}  // namespace adr
