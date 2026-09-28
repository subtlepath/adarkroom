// game_events.cpp — the events (events.js): the village's visitors, the encounters in the wild,
// the landmarks' stories, fights, loot, and the hooks the event tables name (events/*.js).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "game.h"

namespace adr {

int findEvent(const char* key) {
  for (int i = 0; i < kEventCount; ++i)
    if (strcmp(kEvents[i].key, key) == 0) return i;
  return -1;
}

// ---- scheduling and starting ----------------------------------------------------------------------

void Game::scheduleNextEvent(double scale) {
  double minutes = std::floor(r.real() * (6 - 3)) + 3;
  if (scale > 0) minutes *= scale;
  cancel(T_EVENT);
  schedule(T_EVENT, static_cast<int64_t>(minutes * 60 * 1000));
}

void Game::triggerEvent() {
  if (s.ev.event < 0) {
    int possible[kEventCountMax];
    int n = 0;
    for (int i = 0; i < kEventCount; ++i) {
      const EventDef& e = kEvents[i];
      if ((e.group == EG_GLOBAL || e.group == EG_ROOM || e.group == EG_OUTSIDE) && isAvailable(e.isAvailable))
        possible[n++] = i;
    }
    if (n == 0) {
      scheduleNextEvent(0.5);
      return;
    }
    startEvent(possible[static_cast<int>(std::floor(r.real() * n))]);
  }
  scheduleNextEvent();
}

void Game::triggerFight() {
  int possible[kEventCountMax];
  int n = 0;
  for (int i = 0; i < kEventCount; ++i)
    if (kEvents[i].group == EG_ENCOUNTER && isAvailable(kEvents[i].isAvailable)) possible[n++] = i;
  if (n == 0) return;
  startEvent(possible[static_cast<int>(std::floor(r.real() * n))]);
}

void Game::startEvent(int index) {
  if (index < 0 || index >= kEventCount) return;
  ActiveEvent& ev = s.ev;
  ev = ActiveEvent();
  ev.event = static_cast<int16_t>(index);
  loadScene(0);
  ev.blink = kEvents[index].scenes[0].blink;
  changed = true;
}

void Game::switchEvent(int index) {
  cancelCombat();
  s.ev.event = -1;
  startEvent(index);
}

void Game::endEvent() {
  cancelCombat();
  cancel(T_WIN_FADE);
  cancel(T_WIN_SHOW);
  s.ev.event = -1;
  s.ev.combat = false;
  changed = true;
}

void Game::loadScene(int index) {
  ActiveEvent& ev = s.ev;
  ev.scene = static_cast<uint8_t>(index);
  const Scene& sc = kEvents[ev.event].scenes[index];
  cancelCombat();
  ev.combat = false;
  ev.fought = ev.won = ev.lootShown = ev.exploding = false;
  ev.lootN = 0;
  memset(ev.buttonCd, 0, sizeof(ev.buttonCd));
  memset(ev.combatCd, 0, sizeof(ev.combatCd));
  runHook(sc.onLoad);
  if (s.ev.event < 0) return;  // the hook ended it
  if (sc.notification) notify(M_NONE, sc.notification);
  for (int i = 0; i < sc.rewardN; ++i) addStore(sc.reward[i].item, sc.reward[i].n);
  if (sc.combat) {
    startCombat();
  } else {
    if (sc.loot) {
      drawLoot(sc.loot, sc.lootN);
      canLeave();
    }
    for (int i = 0; i < sc.buttonN && i < kMaxButtons; ++i)
      if (sc.buttons[i].cooldown) {
        ev.buttonCd[i] = s.now + hyperMs(sc.buttons[i].cooldown * 1000LL);
        ev.buttonCdTotal[i] = static_cast<int32_t>(hyperMs(sc.buttons[i].cooldown * 1000LL));
      }
  }
  changed = true;
}

// ---- story buttons ------------------------------------------------------------------------------

int Game::quantity(Item i) const {
  if (i == IT_WATER) return s.trip.water;
  if (i == IT_HP) return s.trip.health;
  const double n = s.active == M_WORLD ? s.outfit[i] : store(i);
  return n < 0 ? 0 : static_cast<int>(n);
}

bool Game::buttonAvailable(int i) const {
  const Scene* sc = scene();
  if (!sc || i < 0 || i >= sc->buttonN) return false;
  const EventButton& b = sc->buttons[i];
  if (b.available != H_NONE && !isAvailable(b.available)) return false;
  const bool glow = s.outfit[IT_GLOWSTONE] > 0;
  for (int k = 0; k < b.costN; ++k) {
    if (glow && b.cost[k].item == IT_TORCH) continue;
    if (quantity(b.cost[k].item) < b.cost[k].n) return false;
  }
  return true;
}

float Game::buttonCooldownLeft(int i) const {
  if (i < 0 || i >= kMaxButtons) return 0;
  const int64_t end = s.ev.buttonCd[i];
  if (end <= s.now || s.ev.buttonCdTotal[i] <= 0) return 0;
  return static_cast<float>(end - s.now) / s.ev.buttonCdTotal[i];
}

int Game::soleButton() const {
  const Scene* sc = scene();
  return sc && sc->buttonN == 1 ? 0 : -1;
}

void Game::chooseButton(int i) {
  const Scene* sc = scene();
  if (!sc || (s.ev.combat && !s.ev.lootShown)) return;
  if (i < 0 || i >= sc->buttonN || !buttonAvailable(i) || buttonCooldownLeft(i) > 0) return;
  const EventButton& b = sc->buttons[i];
  if (b.cooldown) {
    s.ev.buttonCd[i] = s.now + hyperMs(b.cooldown * 1000LL);
    s.ev.buttonCdTotal[i] = static_cast<int32_t>(hyperMs(b.cooldown * 1000LL));
  }
  // the cost
  const bool glow = s.outfit[IT_GLOWSTONE] > 0;
  for (int k = 0; k < b.costN; ++k) {
    const Amount& c = b.cost[k];
    if (glow && c.item == IT_TORCH) continue;
    if (c.item == IT_WATER) setWater(s.trip.water - c.n);
    // (the original meant to take hp here, but read an undefined World.hp; rushing through the
    // flames costs what the button says, never the last of it)
    else if (c.item == IT_HP) setHp(s.trip.health - c.n > 0 ? s.trip.health - c.n : 1);
    else if (s.active == M_WORLD) s.outfit[c.item] -= c.n;
    else addStore(c.item, -c.n);
  }
  runHook(b.onChoose);
  for (int k = 0; k < b.rewardN; ++k) addStore(b.reward[k].item, b.reward[k].n);
  if (b.notification) notify(M_NONE, b.notification);
  if (s.ev.event < 0) return;
  if (b.nextEvent >= 0) {
    switchEvent(b.nextEvent);
    return;
  }
  if (b.end) {
    endEvent();
  } else if (b.nextN) {
    const double roll = r.real();
    for (int k = 0; k < b.nextN; ++k) {
      if (roll < b.next[k].under) {
        loadScene(b.next[k].scene);
        refresh();
        return;
      }
    }
    endEvent();
  }
  refresh();
  changed = true;
}

// ---- fights -----------------------------------------------------------------------------------

// The combat screen runs at the device's tempo (App settings): the original's timings, stretched
// so an e-paper panel and seven keys can keep up. Both sides stretch alike.
static double gCombatTempo = 1.5;
void setCombatTempo(double t) { gCombatTempo = t; }
double combatTempo() { return gCombatTempo; }

int64_t Game::combatMs(double seconds) const { return static_cast<int64_t>(seconds * 1000 * gCombatTempo); }

void Game::scheduleCombat(TimerKind k, int64_t ms, int16_t arg, bool interval) {
  schedule(k, ms, arg, false, interval ? ms : 0);
}

void Game::cancelCombat() {
  for (TimerKind k : {T_ENEMY_ATTACK, T_SPECIAL, T_DOT, T_STUN_END, T_STATUS_END, T_ENRAGE_END, T_EXPLODE})
    cancel(k);
}

void Game::floatText(bool onWanderer, const char* text) {
  Fighter& f = onWanderer ? s.ev.wanderer : s.ev.enemy;
  snprintf(f.floatText, sizeof(f.floatText), "%s", text);
  f.floatAt = s.now;
  changed = true;
}

void Game::startCombat() {
  ActiveEvent& ev = s.ev;
  const Scene& sc = *scene();
  ev.combat = true;
  ev.fought = ev.won = false;
  ev.wanderer = Fighter();
  ev.enemy = Fighter();
  ev.wanderer.hp = s.trip.health;
  ev.wanderer.maxHp = static_cast<int16_t>(maxHealth());
  ev.enemy.hp = ev.enemy.maxHp = sc.health;
  ev.weaponDisabled = 0;
  ev.meditateDmg = 0;
  ev.lastSpecial = ST_NONE;
  // Weapons: every one carried; the fists when none of them can deal damage.
  int numWeapons = 0;
  for (int w = 1; w < kWeaponCount; ++w) {
    const WeaponInfo& wi = kWeapons[w];
    if (s.outfit[wi.item] <= 0) continue;
    if (wi.damage <= 0) numWeapons--;
    else if (wi.ammo != IT_NONE && s.outfit[wi.ammo] < wi.ammoN) numWeapons--;
    numWeapons++;
    if (wi.ammo != IT_NONE && s.outfit[wi.ammo] < wi.ammoN) ev.weaponDisabled |= static_cast<uint16_t>(1u << w);
  }
  ev.fistsAdded = numWeapons == 0;
  ev.enemyAttackMs = static_cast<int16_t>(combatMs(sc.attackDelay));
  scheduleCombat(T_ENEMY_ATTACK, combatMs(sc.attackDelay), 0, true);
  for (int i = 0; i < sc.specialN; ++i) scheduleCombat(T_SPECIAL, combatMs(sc.specials[i].delay), static_cast<int16_t>(i), true);
}

bool Game::combatButtonShown(int b) const {
  const ActiveEvent& ev = s.ev;
  if (!ev.combat) return false;
  if (ev.lootShown) {
    const Scene* sc = scene();
    if (sc && sc->buttonN) return false;  // the scene's own buttons instead
    if (b == CB_LEAVE || b == CB_EAT) return true;
    if (b == CB_MEDS) return s.outfit[IT_MEDICINE] != 0;
    if (b == CB_HYPO) return s.outfit[IT_HYPO] > 0;
    return false;
  }
  if (ev.fought) return false;
  if (b < kWeaponCount) {
    if (b == 0) return ev.fistsAdded;
    return s.outfit[kWeapons[b].item] > 0;
  }
  switch (b) {
    case CB_EAT: return true;
    case CB_MEDS: return s.outfit[IT_MEDICINE] != 0;
    case CB_HYPO: return s.outfit[IT_HYPO] != 0;
    case CB_STIM: return s.outfit[IT_STIM] > 0;
    case CB_SHIELD: return store(IT_KINETIC_ARMOUR) > 0;
    default: return false;
  }
}

bool Game::combatButtonEnabled(int b) const {
  if (!combatButtonShown(b) || combatCooldownLeft(b) > 0) return false;
  const bool canHeal = s.trip.health < maxHealth();
  if (b < kWeaponCount) return !(s.ev.weaponDisabled >> b & 1) && !s.ev.won;
  switch (b) {
    case CB_EAT: return canHeal && s.outfit[IT_CURED_MEAT] > 0;
    case CB_MEDS: return canHeal && s.outfit[IT_MEDICINE] > 0;
    case CB_HYPO: return canHeal && s.outfit[IT_HYPO] > 0;
    case CB_STIM: return canHeal && !s.ev.won;
    case CB_SHIELD: return !s.ev.won;
    case CB_LEAVE: return true;
    default: return false;
  }
}

float Game::combatCooldownLeft(int b) const {
  if (b < 0 || b >= CB_COUNT) return 0;
  const int64_t end = s.ev.combatCd[b];
  if (end <= s.now || s.ev.combatCdTotal[b] <= 0) return 0;
  return static_cast<float>(end - s.now) / s.ev.combatCdTotal[b];
}

const char* Game::combatButtonText(int b) const {
  if (b < kWeaponCount) return kWeapons[b].verb;
  switch (b) {
    case CB_EAT: return "eat meat";
    case CB_MEDS: return "use meds";
    case CB_HYPO: return "use hypo";
    case CB_STIM: return "boost";
    case CB_SHIELD: return "shield";
    case CB_LEAVE: return "leave";
    default: return "";
  }
}

void Game::combatButton(int b) {
  if (!combatButtonEnabled(b)) return;
  ActiveEvent& ev = s.ev;
  // Button.cooldown: its own cooldown first (halved when boosted, or for an unarmed master's fists)
  double cd = 0;
  if (b < kWeaponCount) {
    cd = kWeapons[b].cooldown;
    if (kWeapons[b].type == WT_UNARMED && hasPerk(PK_UNARMED_MASTER)) cd /= 2;
  } else if (b == CB_EAT) cd = ev.lootShown ? 0 : 5;
  else if (b == CB_MEDS || b == CB_HYPO) cd = ev.lootShown ? 0 : 7;
  else if (b == CB_STIM || b == CB_SHIELD) cd = 10;
  else if (b == CB_LEAVE) cd = 1;
  if (ev.wanderer.status == ST_BOOST && b < kWeaponCount) cd /= 2;  // only the weapons are `boosted`
  if (cd > 0) {
    const int64_t ms = hyperMs(b == CB_LEAVE ? static_cast<int64_t>(cd * 1000) : combatMs(cd));
    ev.combatCd[b] = s.now + ms;
    ev.combatCdTotal[b] = static_cast<int32_t>(ms);
  }
  switch (b) {
    case CB_EAT: doHeal(IT_CURED_MEAT, meatHeal(), b); break;
    case CB_MEDS: doHeal(IT_MEDICINE, 20, b); break;
    case CB_HYPO: doHeal(IT_HYPO, 30, b); break;
    case CB_SHIELD:
      ev.wanderer.status = ST_SHIELD;
      break;
    case CB_STIM:
      ev.wanderer.status = ST_BOOST;  // (useStim sets it directly: it lasts the fight)
      dotDamage(true, 10);
      break;
    case CB_LEAVE: {
      const Scene* sc = scene();
      if (sc && sc->leaveTo >= 0) loadScene(sc->leaveTo);
      else endEvent();
      break;
    }
    default: useWeapon(b); break;
  }
  refresh();
  changed = true;
}

void Game::doHeal(Item what, int amount, int button) {
  (void)button;
  if (s.outfit[what] <= 0) return;
  s.outfit[what]--;
  int hp = s.trip.health + amount;
  if (hp > maxHealth()) hp = maxHealth();
  setHp(hp);
  if (s.ev.event >= 0) {
    s.ev.wanderer.hp = static_cast<int16_t>(hp);
    char t[12];
    snprintf(t, sizeof(t), "+%d", amount);
    floatText(true, t);
    if (s.ev.lootN) canLeave();
  }
}

void Game::useWeapon(int w) {
  ActiveEvent& ev = s.ev;
  const WeaponInfo& wi = kWeapons[w];
  if (wi.type == WT_UNARMED) {
    s.punches++;
    if (s.punches == 50 && !hasPerk(PK_BOXER)) addPerk(PK_BOXER);
    else if (s.punches == 150 && !hasPerk(PK_MARTIAL_ARTIST)) addPerk(PK_MARTIAL_ARTIST);
    else if (s.punches == 300 && !hasPerk(PK_UNARMED_MASTER)) addPerk(PK_UNARMED_MASTER);
  }
  if (wi.ammo != IT_NONE) {
    if (s.outfit[wi.ammo] < wi.ammoN) return;
    const bool out = s.outfit[wi.ammo] - wi.ammoN < wi.ammoN;
    s.outfit[wi.ammo] -= wi.ammoN;
    if (out) {
      ev.weaponDisabled |= static_cast<uint16_t>(1u << w);
      bool valid = false;
      for (int k = 1; k < kWeaponCount; ++k)
        if (combatButtonShown(k) && kWeapons[k].damage > 0 && !(ev.weaponDisabled >> k & 1)) valid = true;
      if (!valid) {
        ev.fistsAdded = true;
        ev.weaponDisabled &= static_cast<uint16_t>(~1u);
      }
    }
    // another weapon sharing the ammunition may be out too
    for (int k = 1; k < kWeaponCount; ++k)
      if (kWeapons[k].ammo == wi.ammo && s.outfit[wi.ammo] < kWeapons[k].ammoN)
        ev.weaponDisabled |= static_cast<uint16_t>(1u << k);
  }
  const bool stun = wi.damage < 0;
  const bool hit = r.real() <= 0.8 + (hasPerk(PK_PRECISE) ? 0.1 : 0);
  int dmg = -1;
  if (hit && !stun) {
    dmg = wi.damage;
    if (wi.type == WT_UNARMED && hasPerk(PK_BOXER)) dmg *= 2;
    if (wi.type == WT_UNARMED && hasPerk(PK_MARTIAL_ARTIST)) dmg *= 3;
    if (wi.type == WT_UNARMED && hasPerk(PK_UNARMED_MASTER)) dmg *= 2;
    if (wi.type == WT_MELEE && hasPerk(PK_BARBARIAN)) dmg = static_cast<int>(std::floor(dmg * 1.5));
  }
  const bool landedStun = hit && stun;
  damage(true, landedStun ? -1000 : dmg, wi.type);
  // the thresholds the enemy's health crossed (atHealth), then the win
  const Scene* sc = scene();
  if (!sc) return;
  const int enemyHp = ev.enemy.hp;
  if (!landedStun) {
    for (int k = 0; k < sc->atHealthN; ++k) {
      const int thr = sc->atHealth[k].hp;
      if (enemyHp <= thr && enemyHp + dmg > thr) runHook(sc->atHealth[k].action);
    }
  }
  if (enemyHp <= 0 && !ev.won) {
    ev.won = true;
    if (sc->explosion) {
      cancelCombat();
      ev.exploding = true;
      schedule(T_EXPLODE, 3000, 0, true);
    } else {
      winFight();
    }
  }
}

// One blow. byWanderer: the wanderer hits the enemy; otherwise the enemy the wanderer. dmg -1 is
// a miss, -1000 a stun.
void Game::damage(bool byWanderer, int dmg, WeaponType type) {
  (void)type;
  ActiveEvent& ev = s.ev;
  Fighter& fighter = byWanderer ? ev.wanderer : ev.enemy;
  Fighter& target = byWanderer ? ev.enemy : ev.wanderer;
  const bool shielded = target.status == ST_SHIELD;
  const bool energised = fighter.status == ST_ENERGISED;
  const bool venomous = fighter.status == ST_VENOMOUS;
  const bool meditating = target.status == ST_MEDITATION;
  char msg[12] = "";
  if (dmg == -1000) {
    snprintf(msg, sizeof(msg), "stunned");
    target.stunned = true;
    cancel(T_STUN_END);
    schedule(T_STUN_END, combatMs(4), 0, true);  // (a raw setTimeout: hyper does not halve it)
  } else if (dmg < 0) {
    snprintf(msg, sizeof(msg), "miss");
  } else {
    if (energised) dmg *= 4;
    if (meditating) {
      ev.meditateDmg += dmg;
      snprintf(msg, sizeof(msg), "%d", dmg);
    } else {
      snprintf(msg, sizeof(msg), "%c%d", shielded ? '+' : '-', dmg);
      int hp = target.hp + (shielded ? dmg : -dmg);
      if (hp > target.maxHp) hp = target.maxHp;
      if (hp < 0) hp = 0;
      target.hp = static_cast<int16_t>(hp);
      if (!byWanderer) setHp(hp);
    }
    if (venomous && !shielded) {
      // the poison, on whoever was hit, every second (a raw setInterval: hyper does not halve it)
      cancel(T_DOT);
      ev.dotDmg = static_cast<int16_t>(dmg / 2);
      schedule(T_DOT, combatMs(1), byWanderer ? 0 : 1, true, combatMs(1));
    }
    if (shielded) target.status = ST_NONE;
  }
  if (energised || venomous) fighter.status = ST_NONE;
  floatText(!byWanderer, msg);
}

void Game::enemyAttack() {
  const Scene* sc = scene();
  ActiveEvent& ev = s.ev;
  if (!sc || !ev.combat || ev.won) return;
  if (!ev.enemy.stunned && ev.enemy.status != ST_MEDITATION) {
    double toHit = sc->hit * (hasPerk(PK_EVASIVE) ? 0.8 : 1);
    int dmg = -1;
    if (ev.meditateDmg > 0) {
      dmg = ev.meditateDmg;
      ev.meditateDmg = 0;
    } else if (r.real() <= toHit) {
      dmg = sc->damage;
    }
    damage(false, dmg, sc->ranged ? WT_RANGED : WT_MELEE);
    checkPlayerDeath();
  }
}

void Game::dotDamage(bool onWanderer, int dmg) {
  ActiveEvent& ev = s.ev;
  Fighter& t = onWanderer ? ev.wanderer : ev.enemy;
  int hp = t.hp - dmg;
  if (hp < 0) hp = 0;
  t.hp = static_cast<int16_t>(hp);
  char msg[12];
  snprintf(msg, sizeof(msg), "-%d", dmg);
  floatText(onWanderer, msg);
  if (onWanderer) {
    setHp(hp);
    checkPlayerDeath();
  } else if (hp <= 0 && !ev.won) {
    ev.won = true;
    winFight();
  }
}

bool Game::checkPlayerDeath() {
  if (s.ev.event >= 0 && s.ev.combat && s.ev.wanderer.hp <= 0) {
    endEvent();
    die();
    return true;
  }
  return false;
}

void Game::setStatus(bool wanderer, uint8_t status) {
  ActiveEvent& ev = s.ev;
  Fighter& f = wanderer ? ev.wanderer : ev.enemy;
  f.status = status;
  if (status == ST_ENRAGED && !wanderer) {
    cancel(T_ENEMY_ATTACK);
    ev.enemyAttackMs = static_cast<int16_t>(combatMs(0.5));
    scheduleCombat(T_ENEMY_ATTACK, combatMs(0.5), 0, true);
    cancel(T_ENRAGE_END);
    schedule(T_ENRAGE_END, combatMs(4), 0, true);
  }
  if (status == ST_MEDITATION) {
    ev.meditateDmg = 0;
    schedule(T_STATUS_END, combatMs(5), wanderer ? 0 : 1, true);
  }
  if (status == ST_BOOST) schedule(T_STATUS_END, combatMs(3), wanderer ? 0 : 1, true);
}

void Game::winFight() {
  // FIGHT_SPEED later the fight ends; the enemy fades; a second later, the spoils
  schedule(T_WIN_FADE, 100);
}

void Game::showLoot() {
  ActiveEvent& ev = s.ev;
  const Scene* sc = scene();
  if (!sc) return;
  ev.lootShown = true;
  drawLoot(sc->loot, sc->lootN);
  canLeave();
  memset(ev.combatCd, 0, sizeof(ev.combatCd));
  if (sc->buttonN) {
    for (int i = 0; i < sc->buttonN && i < kMaxButtons; ++i)
      if (sc->buttons[i].cooldown) {
        ev.buttonCd[i] = s.now + hyperMs(sc->buttons[i].cooldown * 1000LL);
        ev.buttonCdTotal[i] = static_cast<int32_t>(hyperMs(sc->buttons[i].cooldown * 1000LL));
      }
  } else {
    const int64_t ms = hyperMs(1000);
    ev.combatCd[CB_LEAVE] = s.now + ms;
    ev.combatCdTotal[CB_LEAVE] = static_cast<int32_t>(ms);
  }
  changed = true;
}

// ---- loot ---------------------------------------------------------------------------------------

void Game::drawLoot(const LootEntry* loot, int n) {
  ActiveEvent& ev = s.ev;
  ev.lootN = 0;
  for (int i = 0; i < n && ev.lootN < kMaxLoot; ++i) {
    if (r.real() < loot[i].chance) {
      const int num = static_cast<int>(std::floor(r.real() * (loot[i].max - loot[i].min))) + loot[i].min;
      // the original's object can hold a key once: a second roll for the same item replaces it
      bool merged = false;
      for (int k = 0; k < ev.lootN; ++k)
        if (ev.loot[k].item == loot[i].item) {
          ev.loot[k].numLeft = static_cast<int16_t>(num);
          merged = true;
        }
      if (!merged) ev.loot[ev.lootN++] = {loot[i].item, static_cast<int16_t>(num)};
    }
  }
}

bool Game::canTakeAnything() const {
  const double free = freeSpace();
  for (int k = 0; k < s.ev.lootN; ++k)
    if (s.ev.loot[k].numLeft > 0 && weight(s.ev.loot[k].item) <= free + 1e-9) return true;
  return false;
}

bool Game::canTakeEverything() const {
  double need = 0;
  for (int k = 0; k < s.ev.lootN; ++k) need += s.ev.loot[k].numLeft * weight(s.ev.loot[k].item);
  return freeSpace() - need >= -1e-9;
}

void Game::takeLoot(int row, bool all) {
  ActiveEvent& ev = s.ev;
  if (row < 0 || row >= ev.lootN) return;
  int times = 1;
  if (all) {
    const double w = weight(ev.loot[row].item);
    times = w > 0 ? static_cast<int>(std::floor(freeSpace() / w + 1e-9)) : ev.loot[row].numLeft;
    if (times > ev.loot[row].numLeft) times = ev.loot[row].numLeft;
  }
  for (int k = 0; k < times; ++k) {
    LootRow& lr = ev.loot[row];
    if (lr.numLeft <= 0 || weight(lr.item) > freeSpace() + 1e-9) break;
    lr.numLeft--;
    s.outfit[lr.item]++;
  }
  // an emptied row goes
  if (ev.loot[row].numLeft <= 0) {
    for (int k = row; k + 1 < ev.lootN; ++k) ev.loot[k] = ev.loot[k + 1];
    ev.lootN--;
  }
  changed = true;
}

int Game::dropOptions(int row, DropOption out[kStoreCount]) const {
  const ActiveEvent& ev = s.ev;
  if (row < 0 || row >= ev.lootN) return 0;
  const Item name = ev.loot[row].item;
  const double w = weight(name), free = freeSpace();
  int n = 0;
  if (w <= free + 1e-9) return 0;
  for (int i = 0; i < kStoreCount; ++i) {
    if (i == name || s.outfit[i] <= 0) continue;
    const double iw = weight(static_cast<Item>(i));
    if (iw <= 0) continue;
    int num = static_cast<int>(std::ceil((w - free) / iw - 1e-9));
    if (num > s.outfit[i]) num = s.outfit[i];
    if (num > 0) out[n++] = {static_cast<Item>(i), num};
  }
  return n;
}

void Game::dropAndTake(int row, Item drop, int n) {
  ActiveEvent& ev = s.ev;
  if (row < 0 || row >= ev.lootN || drop >= kStoreCount || n <= 0 || s.outfit[drop] < n) return;
  const Item target = ev.loot[row].item;
  // what is dropped joins the loot
  bool found = false;
  for (int k = 0; k < ev.lootN; ++k)
    if (ev.loot[k].item == drop) {
      ev.loot[k].numLeft = static_cast<int16_t>(ev.loot[k].numLeft + n);
      found = true;
    }
  if (!found && ev.lootN < kMaxLoot) ev.loot[ev.lootN++] = {drop, static_cast<int16_t>(n)};
  s.outfit[drop] -= n;
  for (int k = 0; k < ev.lootN; ++k)
    if (ev.loot[k].item == target) {
      takeLoot(k, false);
      break;
    }
}

void Game::canLeave() {
  ActiveEvent& ev = s.ev;
  ev.lootFits = canTakeEverything();
  const Scene* sc = scene();
  const bool soleLeave = sc && ((ev.combat && ev.lootShown && sc->buttonN == 0) || (!ev.combat && sc->buttonN == 1) ||
                                (ev.combat && ev.lootShown && sc->buttonN == 1));
  ev.takeLeave = ev.lootN > 0 && soleLeave && ev.lootFits;
}

void Game::takeEverything() {
  ActiveEvent& ev = s.ev;
  const bool leave = ev.takeLeave;
  for (int k = 0; k < ev.lootN;) {
    const int before = ev.lootN;
    takeLoot(k, true);
    if (ev.lootN == before) ++k;
  }
  // "take everything and leave": the one button, pressed
  if (leave) {
    if (ev.combat && ev.lootShown && (!scene() || scene()->buttonN == 0)) combatButton(CB_LEAVE);
    else if (soleButton() == 0 && buttonCooldownLeft(0) <= 0) chooseButton(0);
  }
  changed = true;
}

// ---- the hooks ----------------------------------------------------------------------------------

bool Game::isAvailable(Hook h) const {
  const Module m = activeModule();
  const auto pop = s.population;
  switch (h) {
    case H_NONE: return true;
    case H_AV_THIEF: return (m == M_ROOM || m == M_OUTSIDE) && s.thieves == 1;
    case H_AV_ROOM_FUR_GT0: return m == M_ROOM && store(IT_FUR) > 0;
    case H_AV_ROOM_WOOD: return m == M_ROOM && store(IT_WOOD) != 0;
    case H_AV_ROOM_FUR: return m == M_ROOM && store(IT_FUR) != 0;
    case H_AV_SHADY_BUILDER: return m == M_ROOM && s.buildings[BD_HUT] >= 5 && s.buildings[BD_HUT] < 20;
    case H_AV_ROOM_WORLD: return m == M_ROOM && s.hasWorld;
    case H_AV_ROOM_MEDICINE: return m == M_ROOM && store(IT_MEDICINE) > 0;
    case H_AV_OUT_TRAPS: return m == M_OUTSIDE && s.buildings[BD_TRAP] > 0;
    case H_AV_OUT_FIRE: return m == M_OUTSIDE && s.buildings[BD_HUT] > 0 && pop > 50;
    case H_AV_OUT_SICKNESS: return m == M_OUTSIDE && pop > 10 && pop < 50 && store(IT_MEDICINE) > 0;
    case H_AV_OUT_PLAGUE: return m == M_OUTSIDE && pop > 50 && store(IT_MEDICINE) > 0;
    case H_AV_OUT_BEASTS: return m == M_OUTSIDE && pop > 0;
    case H_AV_OUT_RAID: return m == M_OUTSIDE && pop > 0 && s.cityCleared;
    case H_AV_T1_FOREST: return distance() <= 10 && terrain() == TL_FOREST;
    case H_AV_T1_BARRENS: return distance() <= 10 && terrain() == TL_BARRENS;
    case H_AV_T1_FIELD: return distance() <= 10 && terrain() == TL_FIELD;
    case H_AV_T2_BARRENS: return distance() > 10 && distance() <= 20 && terrain() == TL_BARRENS;
    case H_AV_T2_FOREST: return distance() > 10 && distance() <= 20 && terrain() == TL_FOREST;
    case H_AV_T2_FIELD: return distance() > 10 && distance() <= 20 && terrain() == TL_FIELD;
    case H_AV_T3_FOREST: return distance() > 20 && terrain() == TL_FOREST;
    case H_AV_T3_BARRENS: return distance() > 20 && terrain() == TL_BARRENS;
    case H_AV_T3_FIELD: return distance() > 20 && terrain() == TL_FIELD;
    case H_BA_NO_COMPASS: return store(IT_COMPASS) < 1;
    case H_BA_MAP_UNSEEN: return !s.seenAll;
    case H_BA_NO_SCOUT: return !hasPerk(PK_SCOUT);
    case H_BA_NO_EVASIVE: return !hasPerk(PK_EVASIVE);
    case H_BA_NO_PRECISE: return !hasPerk(PK_PRECISE);
    case H_BA_NO_BARBARIAN: return !hasPerk(PK_BARBARIAN);
    case H_BA_NOT_ENGINEERING: return !(s.trip.world.flags & WF_ENGINEERING);
    case H_BA_NOT_MEDICAL: return !(s.trip.world.flags & WF_MEDICAL);
    case H_BA_NOT_MARTIAL: return !(s.trip.world.flags & WF_MARTIAL);
    case H_BA_ALL_WINGS: {
      const uint8_t all = WF_ENGINEERING | WF_MEDICAL | WF_MARTIAL;
      return (s.trip.world.flags & all) == all;
    }
    default: return true;
  }
}

void Game::runHook(Hook h) {
  Trip& t = s.trip;
  switch (h) {
    case H_NONE: break;
    // choices
    case H_CH_APPLY_MAP: applyMap(); break;
    case H_CH_APPLY_MAP_3: for (int i = 0; i < 3; ++i) applyMap(); break;
    case H_CH_PERK_SCOUT: addPerk(PK_SCOUT); break;
    case H_CH_PERK_EVASIVE: addPerk(PK_EVASIVE); break;
    case H_CH_PERK_PRECISE: addPerk(PK_PRECISE); break;
    case H_CH_PERK_BARBARIAN: addPerk(PK_BARBARIAN); break;
    case H_CH_FULL_HEAL: setHp(maxHealth()); break;
    // the village's events
    case H_LD_THIEF_HANGED:
      s.thieves = 2;
      removeIncome(SRC_THIEVES);
      for (int i = 0; i < kStoreCount; ++i)
        if (s.stolen[i] != 0) addStore(static_cast<Item>(i), s.stolen[i]);
      break;
    case H_LD_THIEF_SPARED:
      s.thieves = 2;
      removeIncome(SRC_THIEVES);
      addPerk(PK_STEALTHY);
      break;
    case H_LD_WOOD_FOR_SCALES: case H_LD_WOOD_FOR_TEETH: case H_LD_WOOD_FOR_CLOTH: {
      int numWood = static_cast<int>(std::floor(store(IT_WOOD) * 0.1));
      if (numWood == 0) numWood = 1;
      int num = numWood / 5;
      if (num == 0) num = 1;
      addStore(IT_WOOD, -numWood);
      addStore(h == H_LD_WOOD_FOR_SCALES ? IT_SCALES : h == H_LD_WOOD_FOR_TEETH ? IT_TEETH : IT_CLOTH, num);
      break;
    }
    case H_LD_SHADY_HUT:
      if (s.buildings[BD_HUT] < 20) setBuilding(BD_HUT, s.buildings[BD_HUT] + 1);
      break;
    case H_LD_GIFT_ALLOY: addStore(IT_ALIEN_ALLOY, 1); break;
    case H_LD_GIFT_CELLS: addStore(IT_ENERGY_CELL, 3); break;
    case H_LD_GIFT_SCALES: addStore(IT_SCALES, 5); break;
    case H_LD_WANDERER_WOOD100: if (r.real() < 0.5) schedule(T_WANDERER, 60 * 1000, 0); break;
    case H_LD_WANDERER_WOOD500: if (r.real() < 0.3) schedule(T_WANDERER, 60 * 1000, 1); break;
    case H_LD_WANDERER_FUR100: if (r.real() < 0.5) schedule(T_WANDERER, 60 * 1000, 2); break;
    case H_LD_WANDERER_FUR500: if (r.real() < 0.3) schedule(T_WANDERER, 60 * 1000, 3); break;
    case H_LD_WRECK_TRAPS: {
      const int wrecked = static_cast<int>(std::floor(r.real() * s.buildings[BD_TRAP])) + 1;
      setBuilding(BD_TRAP, s.buildings[BD_TRAP] - wrecked);
      break;
    }
    case H_LD_HUT_FIRE: destroyHuts(1); break;
    case H_LD_SICKNESS_DEATHS: killVillagers(static_cast<int>(std::floor(r.real() * (s.population / 2))) + 1); break;
    case H_LD_PLAGUE_HEALED: killVillagers(static_cast<int>(std::floor(r.real() * 5)) + 2); break;
    case H_LD_PLAGUE_DEATHS: killVillagers(static_cast<int>(std::floor(r.real() * 80)) + 10); break;
    case H_LD_BEAST_DEATHS: killVillagers(static_cast<int>(std::floor(r.real() * 10)) + 1); break;
    case H_LD_RAID_DEATHS: killVillagers(static_cast<int>(std::floor(r.real() * 40)) + 1); break;
    // the landmarks
    case H_LD_USE_OUTPOST: useOutpost(); break;
    case H_LD_SWAMP_TALK:
      addPerk(PK_GASTRONOME);
      markVisited();
      break;
    case H_LD_CLEAR_DUNGEON: clearDungeon(); break;
    case H_LD_CLEAR_CITY:
      clearDungeon();
      s.cityCleared = true;
      break;
    case H_LD_HOUSE_WATER:
      markVisited();
      setWater(maxWater());
      notify(M_NONE, "water replenished");
      break;
    case H_LD_MARK_VISITED: markVisited(); break;
    case H_LD_FOUND_SHIP:
      markVisited();
      drawRoad();
      t.world.flags |= WF_SHIP;
      break;
    case H_LD_SULPHUR_MINE:
      drawRoad();
      t.world.flags |= WF_SULPHURMINE;
      markVisited();
      break;
    case H_LD_COAL_MINE:
      drawRoad();
      t.world.flags |= WF_COALMINE;
      markVisited();
      break;
    case H_LD_IRON_MINE:
      drawRoad();
      t.world.flags |= WF_IRONMINE;
      markVisited();
      break;
    case H_LD_CACHE:
      markVisited();
      collectPrestige();
      break;
    case H_LD_FOUND_EXECUTIONER:
      drawRoad();
      t.world.flags |= WF_EXECUTIONER;
      break;
    case H_LD_ENGINEERING_DONE: t.world.flags |= WF_ENGINEERING; break;
    case H_LD_MARTIAL_DONE: t.world.flags |= WF_MARTIAL; break;
    case H_LD_MEDICAL_DONE: t.world.flags |= WF_MEDICAL; break;
    case H_LD_RESET_SPECIAL: s.ev.lastSpecial = ST_NONE; break;
    // fights
    case H_SP_VENOMOUS: setStatus(false, ST_VENOMOUS); floatText(false, "venomous"); break;
    case H_SP_SHIELD: setStatus(false, ST_SHIELD); floatText(false, "shield"); break;
    case H_SP_ENERGISED: setStatus(false, ST_ENERGISED); floatText(false, "energised"); break;
    case H_SP_ENRAGED: setStatus(false, ST_ENRAGED); floatText(false, "enraged"); break;
    case H_SP_COMMANDER: {
      uint8_t possible[3];
      int n = 0;
      for (uint8_t st : {ST_SHIELD, ST_ENRAGED, ST_MEDITATION})
        if (st != s.ev.lastSpecial) possible[n++] = st;
      const uint8_t st = possible[static_cast<int>(std::floor(r.real() * n))];
      setStatus(false, st);
      s.ev.lastSpecial = st;
      floatText(false, st == ST_SHIELD ? "shield" : st == ST_ENRAGED ? "enraged" : "meditation");
      break;
    }
    default: break;
  }
  changed = true;
}

}  // namespace adr
