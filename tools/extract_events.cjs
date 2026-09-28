// extract_events.cjs — the game's events, straight from game/script/events/*.js (the original, a
// pinned submodule), as C++ tables.
//
// The event files are evaluated in a sandbox (the text, costs, rewards, loot, enemies and
// branches come out exactly as the browser sees them), then written as
// src/generated/events_data.cpp and src/generated/event_hooks.h. Each function in the events
// (an onLoad, an isAvailable, a button's available or onChoose, a combat special) is recognised
// by its source and named here; src/core/game_events.cpp ports it by hand. A function this table
// does not know stops the build, so a change to the game's events cannot slip by unported.
//
//   node tools/extract_events.cjs
'use strict';
const fs = require('fs'), vm = require('vm'), path = require('path');

const ROOT = path.resolve(__dirname, '../game');
const OUT = path.resolve(__dirname, '../src/generated');

// ---- the sandbox --------------------------------------------------------------------------

const ctx = {
  _: (s, ...a) => s.replace(/\{(\d)\}/g, (m, i) => a[i]),
  AudioLibrary: new Proxy({}, { get: () => null }),
  Events: { _LEAVE_COOLDOWN: 1 },
  World: { applyMap: function applyMap() {} },
  Enemies: undefined,
  console,
};
ctx.window = ctx;
vm.createContext(ctx);
for (const f of ['global', 'room', 'outside', 'marketing', 'encounters', 'setpieces', 'executioner'])
  vm.runInContext(fs.readFileSync(path.join(ROOT, 'script/events', f + '.js'), 'utf8'), ctx, { filename: f + '.js' });
const E = ctx.Events;

// ---- the functions, by their source -------------------------------------------------------

const norm = fn => fn.toString().replace(/\s+/g, ' ').trim();
// source (whitespace folded) -> hook name
const HOOKS = {
  // availability of the village's events
  "function() { return (Engine.activeModule == Room || Engine.activeModule == Outside) && $SM.get('game.thieves') == 1; }": 'AV_THIEF',
  "function() { return Engine.activeModule == Room && $SM.get('stores.fur', true) > 0; }": 'AV_ROOM_FUR_GT0',
  "function() { return Engine.activeModule == Room && $SM.get('stores.wood'); }": 'AV_ROOM_WOOD',
  "function() { return Engine.activeModule == Room && $SM.get('stores.fur'); }": 'AV_ROOM_FUR',
  "function() { return Engine.activeModule == Room && $SM.get('game.buildings[\"hut\"]', true) >= 5 && $SM.get('game.buildings[\"hut\"]', true) < 20; }": 'AV_SHADY_BUILDER',
  "function() { return Engine.activeModule == Room && $SM.get('features.location.world'); }": 'AV_ROOM_WORLD',
  "function() { return Engine.activeModule == Room && $SM.get('stores.medicine', true) > 0; }": 'AV_ROOM_MEDICINE',
  "function() { return Engine.activeModule == Outside && $SM.get('game.buildings[\"trap\"]', true) > 0; }": 'AV_OUT_TRAPS',
  "function() { return Engine.activeModule == Outside && $SM.get('game.buildings[\"hut\"]', true) > 0 && $SM.get('game.population', true) > 50; }": 'AV_OUT_FIRE',
  "function() { return Engine.activeModule == Outside && $SM.get('game.population', true) > 10 && $SM.get('game.population', true) < 50 && $SM.get('stores.medicine', true) > 0; }": 'AV_OUT_SICKNESS',
  "function() { return Engine.activeModule == Outside && $SM.get('game.population', true) > 50 && $SM.get('stores.medicine', true) > 0; }": 'AV_OUT_PLAGUE',
  "function() { return Engine.activeModule == Outside && $SM.get('game.population', true) > 0; }": 'AV_OUT_BEASTS',
  "function() { return Engine.activeModule == Outside && $SM.get('game.population', true) > 0 && $SM.get('game.cityCleared'); }": 'AV_OUT_RAID',
  "() => !$SM.get('marketing.penrose')": 'AV_NEVER',
  // the encounters: distance from the village and the ground underfoot
  "function() { return World.getDistance() <= 10 && World.getTerrain() == World.TILE.FOREST; }": 'AV_T1_FOREST',
  "function() { return World.getDistance() <= 10 && World.getTerrain() == World.TILE.BARRENS; }": 'AV_T1_BARRENS',
  "function() { return World.getDistance() <= 10 && World.getTerrain() == World.TILE.FIELD; }": 'AV_T1_FIELD',
  "function() { return World.getDistance() > 10 && World.getDistance() <= 20 && World.getTerrain() == World.TILE.BARRENS; }": 'AV_T2_BARRENS',
  "function() { return World.getDistance() > 10 && World.getDistance() <= 20 && World.getTerrain() == World.TILE.FOREST; }": 'AV_T2_FOREST',
  "function() { return World.getDistance() > 10 && World.getDistance() <= 20 && World.getTerrain() == World.TILE.FIELD; }": 'AV_T2_FIELD',
  "function() { return World.getDistance() > 20 && World.getTerrain() == World.TILE.FOREST; }": 'AV_T3_FOREST',
  "function() { return World.getDistance() > 20 && World.getTerrain() == World.TILE.BARRENS; }": 'AV_T3_BARRENS',
  "function() { return World.getDistance() > 20 && World.getTerrain() == World.TILE.FIELD; }": 'AV_T3_FIELD',
  // a button's availability
  "function() { return $SM.get('stores.compass', true) < 1; }": 'BA_NO_COMPASS',
  "function() { return !World.seenAll; }": 'BA_MAP_UNSEEN',
  "function() { return !$SM.hasPerk('scout'); }": 'BA_NO_SCOUT',
  "function() { return !$SM.hasPerk('evasive'); }": 'BA_NO_EVASIVE',
  "function() { return !$SM.hasPerk('precise'); }": 'BA_NO_PRECISE',
  "function() { return !$SM.hasPerk('barbarian'); }": 'BA_NO_BARBARIAN',
  "function() { return !World.state.engineering; }": 'BA_NOT_ENGINEERING',
  "function() { return !World.state.medical; }": 'BA_NOT_MEDICAL',
  "function() { return !World.state.martial; }": 'BA_NOT_MARTIAL',
  "function() { return World.state.engineering && World.state.medical && World.state.martial; }": 'BA_ALL_WINGS',
  // a button's onChoose (or onClick)
  'function applyMap() {}': 'CH_APPLY_MAP',
  "function() { $SM.addPerk('scout'); }": 'CH_PERK_SCOUT',
  "function() { $SM.addPerk('evasive'); }": 'CH_PERK_EVASIVE',
  "function() { $SM.addPerk('precise'); }": 'CH_PERK_PRECISE',
  "function() { $SM.addPerk('barbarian'); }": 'CH_PERK_BARBARIAN',
  "() => { $SM.set('marketing.penrose', true); }": 'CH_NONE',
  'function() { World.setHp(World.getMaxHealth()); }': 'CH_FULL_HEAL',
  '() => { for (let i = 0; i < 3; i++) { World.applyMap(); } }': 'CH_APPLY_MAP_3',
  // a scene's onLoad
  "function() { $SM.set('game.thieves', 2); $SM.remove('income.thieves'); $SM.addM('stores', $SM.get('game.stolen')); }": 'LD_THIEF_HANGED',
  "function() { $SM.set('game.thieves', 2); $SM.remove('income.thieves'); $SM.addPerk('stealthy'); }": 'LD_THIEF_SPARED',
  "function() { var numWood = $SM.get('stores.wood', true); numWood = Math.floor(numWood * 0.1); if(numWood === 0) numWood = 1; var numScales = Math.floor(numWood / 5); if(numScales === 0) numScales = 1; $SM.addM('stores', {'wood': -numWood, 'scales': numScales}); }": 'LD_WOOD_FOR_SCALES',
  "function() { var numWood = $SM.get('stores.wood', true); numWood = Math.floor(numWood * 0.1); if(numWood === 0) numWood = 1; var numTeeth = Math.floor(numWood / 5); if(numTeeth === 0) numTeeth = 1; $SM.addM('stores', {'wood': -numWood, 'teeth': numTeeth}); }": 'LD_WOOD_FOR_TEETH',
  "function() { var numWood = $SM.get('stores.wood', true); numWood = Math.floor(numWood * 0.1); if(numWood === 0) numWood = 1; var numCloth = Math.floor(numWood / 5); if(numCloth === 0) numCloth = 1; $SM.addM('stores', {'wood': -numWood, 'cloth': numCloth}); }": 'LD_WOOD_FOR_CLOTH',
  "function() { var n = $SM.get('game.buildings[\"hut\"]', true); if(n < 20){ $SM.set('game.buildings[\"hut\"]',n+1); } }": 'LD_SHADY_HUT',
  "function() { $SM.add('stores[\"alien alloy\"]', 1); }": 'LD_GIFT_ALLOY',
  "function() { $SM.add('stores[\"energy cell\"]', 3); }": 'LD_GIFT_CELLS',
  "function() { $SM.add('stores.scales', 5); }": 'LD_GIFT_SCALES',
  "function() { var numWrecked = Math.floor(Math.random() * $SM.get('game.buildings[\"trap\"]', true)) + 1; $SM.add('game.buildings[\"trap\"]', -numWrecked); Outside.updateVillage(); Outside.updateTrapButton(); }": 'LD_WRECK_TRAPS',
  'function() { Outside.destroyHuts(1); }': 'LD_HUT_FIRE',
  "function() { var numKilled = Math.floor(Math.random() * Math.floor($SM.get('game.population', true)/2)) + 1; Outside.killVillagers(numKilled); }": 'LD_SICKNESS_DEATHS',
  'function() { var numKilled = Math.floor(Math.random() * 5) + 2; Outside.killVillagers(numKilled); }': 'LD_PLAGUE_HEALED',
  'function() { var numKilled = Math.floor(Math.random() * 80) + 10; Outside.killVillagers(numKilled); }': 'LD_PLAGUE_DEATHS',
  'function() { var numKilled = Math.floor(Math.random() * 10) + 1; Outside.killVillagers(numKilled); }': 'LD_BEAST_DEATHS',
  'function() { var numKilled = Math.floor(Math.random() * 40) + 1; Outside.killVillagers(numKilled); }': 'LD_RAID_DEATHS',
  'function() { World.useOutpost(); }': 'LD_USE_OUTPOST',
  "function() { $SM.addPerk('gastronome'); World.markVisited(World.curPos[0], World.curPos[1]); }": 'LD_SWAMP_TALK',
  'function() { World.clearDungeon(); }': 'LD_CLEAR_DUNGEON',
  '() => { World.clearDungeon(); }': 'LD_CLEAR_DUNGEON',
  "function() { World.clearDungeon(); $SM.set('game.cityCleared', true); }": 'LD_CLEAR_CITY',
  "function() { World.markVisited(World.curPos[0], World.curPos[1]); World.setWater(World.getMaxWater()); Notifications.notify(null, _('water replenished')); }": 'LD_HOUSE_WATER',
  'function() { World.markVisited(World.curPos[0], World.curPos[1]); }': 'LD_MARK_VISITED',
  'function() { World.markVisited(World.curPos[0], World.curPos[1]); World.drawRoad(); World.state.ship = true; }': 'LD_FOUND_SHIP',
  'function() { World.drawRoad(); World.state.sulphurmine = true; World.markVisited(World.curPos[0], World.curPos[1]); }': 'LD_SULPHUR_MINE',
  'function() { World.drawRoad(); World.state.coalmine = true; World.markVisited(World.curPos[0], World.curPos[1]); }': 'LD_COAL_MINE',
  'function() { World.drawRoad(); World.state.ironmine = true; World.markVisited(World.curPos[0], World.curPos[1]); }': 'LD_IRON_MINE',
  'function() { World.markVisited(World.curPos[0], World.curPos[1]); Prestige.collectStores(); }': 'LD_CACHE',
  '() => { World.drawRoad(); World.state.executioner = true; }': 'LD_FOUND_EXECUTIONER',
  '() => { World.state.engineering = true; }': 'LD_ENGINEERING_DONE',
  '() => { World.state.martial = true; }': 'LD_MARTIAL_DONE',
  '() => { World.state.medical = true; }': 'LD_MEDICAL_DONE',
  "() => { Events._lastSpecial = 'none'; }": 'LD_RESET_SPECIAL',
  // combat: at a health threshold, or on a timer
  "fighter => { Events.setStatus(fighter, 'venomous'); return 'venomous'; }": 'SP_VENOMOUS',
  "(fighter) => { Events.setStatus(fighter, 'shield'); return 'shield'; }": 'SP_SHIELD',
  "(fighter) => { Events.setStatus(fighter, 'energised'); return 'energised'; }": 'SP_ENERGISED',
  "(fighter) => { Events.setStatus(fighter, 'enraged'); return 'enraged'; }": 'SP_ENRAGED',
  "(fighter) => { const last = Events._lastSpecial ?? 'none'; const possible = [ 'shield', 'enraged', 'meditation' ].filter(p => p !== last); const status = possible[Math.floor(Math.random() * possible.length)]; Events.setStatus(fighter, status); Events._lastSpecial = status; return status; }": 'SP_COMMANDER',
};
// The mysterious wanderers: onLoad calls this.action, which differs by scene.
const WANDERER = {
  wood100: 'LD_WANDERER_WOOD100', wood500: 'LD_WANDERER_WOOD500',
  fur100: 'LD_WANDERER_FUR100', fur500: 'LD_WANDERER_FUR500',
};

const hooks = new Set(['H_NONE']);
function hook(fn, where, sceneKey) {
  if (fn === undefined || fn === null) return 'H_NONE';
  if (typeof fn !== 'function') throw new Error(`${where}: expected a function`);
  const src = norm(fn);
  let name = src.includes('this.action(') ? WANDERER[sceneKey] : HOOKS[src];
  if (!name) throw new Error(`${where}: a function this table does not know:\n  ${src}\nadd it to HOOKS and port it in src/core/game_hooks.cpp`);
  name = 'H_' + name;
  hooks.add(name);
  return name;
}

// ---- values -------------------------------------------------------------------------------

const itemId = k => 'IT_' + k.toUpperCase().replace(/[^A-Z0-9]+/g, '_');
const cstr = s => (s === undefined || s === null ? 'nullptr' : JSON.stringify(s).replace(/[\u0080-￿]/g, c => {
  // UTF-8, as octal escapes (a hex escape would swallow the letters after it)
  return [...Buffer.from(c, 'utf8')].map(b => '\\' + b.toString(8).padStart(3, '0')).join('');
}));
const num = v => {
  if (typeof v !== 'number' || !isFinite(v)) throw new Error('not a number: ' + v);
  return Number.isInteger(v) ? String(v) : v + 'f';
};

// ---- the tables ---------------------------------------------------------------------------

const events = [];
const pushGroup = (group, list, keyOf) => {
  for (const [key, ev] of list) events.push({ group, key: keyOf(key), ev });
};
pushGroup('EG_GLOBAL', E.Global.map((e, i) => [i, e]), i => 'global.' + i);
pushGroup('EG_ROOM', E.Room.map((e, i) => [i, e]), i => 'room.' + i);
pushGroup('EG_OUTSIDE', E.Outside.map((e, i) => [i, e]), i => 'outside.' + i);
pushGroup('EG_ENCOUNTER', E.Encounters.map((e, i) => [i, e]), i => 'encounter.' + i);
pushGroup('EG_SETPIECE', Object.entries(E.Setpieces), k => k);
pushGroup('EG_EXECUTIONER', Object.entries(E.Executioner), k => k);
// The marketing event (a link to another game) has no place on the device.
const eventIndex = new Map(events.map((e, i) => [e.key, i]));

let out = [];
const w = s => out.push(s);
let uid = 0;
const arr = (type, items) => {
  if (!items.length) return 'nullptr';
  const name = `k${type.replace(/\W/g, '')}_${uid++}`;
  w(`static const ${type} ${name}[] = {\n  ${items.join(',\n  ')}\n};`);
  return name;
};
const amounts = (obj, where) => arr('Amount', Object.entries(obj || {}).map(([k, v]) => `{${itemId(k)}, ${num(v)}}`));
const amountCount = obj => Object.keys(obj || {}).length;

function emitScene(ev, sceneKeys, sk, sc, where) {
  const text = arr('char* const', (sc.text || []).map(cstr));
  const loot = arr('LootEntry', Object.entries(sc.loot || {}).map(([k, l]) =>
    `{${itemId(k)}, ${l.min}, ${l.max}, ${num(l.chance)}}`));
  const buttons = [];
  for (const [bk, b] of Object.entries(sc.buttons || {})) {
    const bw = `${where}.buttons.${bk}`;
    let end = false, next = [], nextEvent = -1;
    if (b.nextScene === 'end') end = true;
    else if (b.nextScene) {
      for (const [p, s] of Object.entries(b.nextScene)) {
        if (!sceneKeys.includes(s)) throw new Error(`${bw}: no scene ${s}`);
        next.push([Number(p), sceneKeys.indexOf(s)]);
      }
      next.sort((a, b2) => a[0] - b2[0]);
    }
    if (b.nextEvent) {
      if (!eventIndex.has(b.nextEvent)) throw new Error(`${bw}: no event ${b.nextEvent}`);
      nextEvent = eventIndex.get(b.nextEvent);
    }
    if (b.link) end = true;  // the marketing link: never shown
    const nextArr = arr('Branch', next.map(([p, s]) => `{${num(p)}, ${s}}`));
    const choose = b.onChoose ? hook(b.onChoose, bw + '.onChoose', sk) : hook(b.onClick, bw + '.onClick', sk);
    buttons.push(`{${cstr(bk)}, ${cstr(b.text)}, ${amounts(b.cost)}, ${amountCount(b.cost)}, ${amounts(b.reward)}, ` +
      `${amountCount(b.reward)}, ${cstr(b.notification)}, ${b.cooldown | 0}, ${hook(b.available, bw + '.available', sk)}, ` +
      `${choose}, ${nextEvent}, ${end}, ${nextArr}, ${next.length}}`);
  }
  const btnArr = arr('EventButton', buttons);
  const specials = arr('Special', (sc.specials || []).map((s, i) => `{${num(s.delay)}, ${hook(s.action, `${where}.specials.${i}`, sk)}}`));
  const atHealth = arr('AtHealth', Object.entries(sc.atHealth || {}).map(([hp, f]) => `{${Number(hp)}, ${hook(f, `${where}.atHealth.${hp}`, sk)}}`));
  let leaveTo = -1;
  if (typeof sc.nextScene === 'string' && sc.nextScene !== 'end') leaveTo = sceneKeys.indexOf(sc.nextScene);
  const known = new Set(['text', 'notification', 'onLoad', 'reward', 'loot', 'buttons', 'blink', 'combat', 'enemy',
    'enemyName', 'deathMessage', 'chara', 'damage', 'hit', 'attackDelay', 'health', 'ranged', 'explosion', 'specials',
    'atHealth', 'nextScene', 'action', 'plural']);
  for (const k of Object.keys(sc)) if (!known.has(k)) throw new Error(`${where}: unknown scene field ${k}`);
  return `{${cstr(sk)}, ${text}, ${(sc.text || []).length}, ${cstr(sc.notification)}, ${hook(sc.onLoad, where + '.onLoad', sk)}, ` +
    `${amounts(sc.reward)}, ${amountCount(sc.reward)}, ${loot}, ${Object.keys(sc.loot || {}).length}, ${btnArr}, ${buttons.length}, ` +
    `${!!sc.blink}, ${!!sc.combat}, ${cstr(sc.chara || null)}, ${cstr(sc.enemyName)}, ${cstr(sc.deathMessage)}, ` +
    `${sc.damage | 0}, ${num(sc.hit || 0)}, ${num(sc.attackDelay || 0)}, ${sc.health | 0}, ${!!sc.ranged}, ${sc.explosion | 0}, ` +
    `${specials}, ${(sc.specials || []).length}, ${atHealth}, ${Object.keys(sc.atHealth || {}).length}, ${leaveTo}}`;
}

const eventRows = [];
for (const { group, key, ev } of events) {
  const sceneKeys = Object.keys(ev.scenes);
  if (sceneKeys[0] !== 'start') {  // 'start' first: scene 0
    sceneKeys.splice(sceneKeys.indexOf('start'), 1);
    sceneKeys.unshift('start');
  }
  const scenes = sceneKeys.map(sk => emitScene(ev, sceneKeys, sk, ev.scenes[sk], `${key}.scenes.${sk}`));
  const sceneArr = arr('Scene', scenes);
  eventRows.push(`{${cstr(key)}, ${cstr(ev.title)}, ${group}, ${hook(ev.isAvailable, key + '.isAvailable')}, ${sceneArr}, ${scenes.length}}`);
}
const body = out.join('\n');
out = [];

const hookList = [...hooks].sort((a, b) => (a === 'H_NONE' ? -1 : b === 'H_NONE' ? 1 : a.localeCompare(b)));
fs.mkdirSync(OUT, { recursive: true });
fs.writeFileSync(path.join(OUT, 'event_hooks.h'),
  `// Generated by tools/extract_events.cjs from game/script/events/*.js. Do not edit.\n#pragma once\n#include <cstdint>\n\n` +
  `namespace adr {\n\nenum Hook : uint8_t {\n${hookList.map(h => '  ' + h + ',').join('\n')}\n  H_COUNT\n};\n\n}  // namespace adr\n`);

const src = `// Generated by tools/extract_events.cjs from game/script/events/*.js. Do not edit.
#include "../core/events.h"

namespace adr {

${body}

const EventDef kEvents[] = {
  ${eventRows.join(',\n  ')}
};
const int kEventCount = ${eventRows.length};

}  // namespace adr
`;
fs.writeFileSync(path.join(OUT, 'events_data.cpp'), src);
console.log(`${events.length} events, ${hookList.length - 1} hooks -> ${path.relative(process.cwd(), OUT)}`);
