// events.h — the shape of the game's events (script/events.js), as the generated tables
// (src/generated/events_data.cpp, from tools/extract_events.cjs) lay them out.
#pragma once

#include <cstdint>

#include "../generated/event_hooks.h"
#include "items.h"

namespace adr {

struct Amount {
  Item item;
  int32_t n;
};

struct LootEntry {
  Item item;
  uint16_t min, max;
  float chance;
};

// One branch of a button's nextScene: a random draw under `under` goes to scene `scene` (the
// lowest such bound wins, as Events.buttonClick picks it).
struct Branch {
  float under;
  uint8_t scene;
};

enum EventGroup : uint8_t { EG_GLOBAL, EG_ROOM, EG_OUTSIDE, EG_ENCOUNTER, EG_SETPIECE, EG_EXECUTIONER };

struct EventButton {
  const char* key;
  const char* text;
  const Amount* cost;
  uint8_t costN;
  const Amount* reward;
  uint8_t rewardN;
  const char* notification;
  uint8_t cooldown;  // seconds; 0 for none
  Hook available;
  Hook onChoose;
  int16_t nextEvent;  // an index into kEvents, or -1
  bool end;           // nextScene: 'end'
  const Branch* next;
  uint8_t nextN;
};

struct Special {
  float delay;  // seconds between uses
  Hook action;
};

struct AtHealth {
  int16_t hp;
  Hook action;
};

struct Scene {
  const char* key;
  const char* const* text;
  uint8_t textN;
  const char* notification;
  Hook onLoad;
  const Amount* reward;
  uint8_t rewardN;
  const LootEntry* loot;
  uint8_t lootN;
  const EventButton* buttons;
  uint8_t buttonN;
  bool blink;
  // A fight.
  bool combat;
  const char* chara;  // the enemy's letter(s) on the glass
  const char* enemyName;
  const char* deathMessage;
  int16_t damage;
  float hit;
  float attackDelay;  // seconds
  int16_t health;
  bool ranged;
  int16_t explosion;
  const Special* specials;
  uint8_t specialN;
  const AtHealth* atHealth;
  uint8_t atHealthN;
  int8_t leaveTo;  // a fight without buttons: the scene its leave button goes to, or -1 (the end)
};

struct EventDef {
  const char* key;
  const char* title;
  EventGroup group;
  Hook isAvailable;
  const Scene* scenes;  // scene 0 is 'start'
  uint8_t sceneN;
};

constexpr int kEventCountMax = 64;
extern const EventDef kEvents[];
extern const int kEventCount;

int findEvent(const char* key);  // -1 when there is none

}  // namespace adr
