// adr_host — the app on the host, driven by a script, saving what the glass would show as PNG.
//
//   adr_host [--device x4pro|x4c|x4|x3] [--rot 0|1] [--seed N] [--out DIR] [--state DIR] COMMAND...
//
// Commands (run in order):
//   key NAME [long]       up, down, left, right, confirm, back, home
//   tap X Y | hold X Y    page coordinates
//   swipe X0 Y0 X1 Y1
//   wait SECONDS          time passing (frames drawn as the firmware would draw them)
//   snap NAME             the glass, as NAME.png
//   sleep NAME            the sleep screen (the keys pointed out), as NAME.png
//   give ITEM N           add to the stores (an item's key, spaces as underscores)
//   state                 print where the game is
//   save | reload         round-trip the save through the platform's store
//   play MINUTES          a simple player: stoke, gather, build, as it can
//
// Every frame that would refresh the glass is counted and its refresh kind printed with -v.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../../src/core/app.h"
#include "../../src/core/save.h"

using namespace adr;

namespace {

struct HostPlatform : Platform {
  const DeviceProfile* prof = &kX4Pro;
  uint32_t ms = 0;
  uint64_t seed = 1;
  std::string stateDir;
  std::map<std::string, std::vector<uint8_t>> store;
  const DeviceProfile& profile() override { return *prof; }
  uint32_t millis() override { return ms; }
  uint32_t random() override {
    seed = seed * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<uint32_t>(seed >> 33);
  }
  size_t load(const char* key, uint8_t* buf, size_t cap) override {
    if (!stateDir.empty()) {
      FILE* f = fopen((stateDir + "/" + key).c_str(), "rb");
      if (!f) return 0;
      const size_t n = fread(buf, 1, cap, f);
      fclose(f);
      return n;
    }
    auto it = store.find(key);
    if (it == store.end() || it->second.size() > cap) return 0;
    memcpy(buf, it->second.data(), it->second.size());
    return it->second.size();
  }
  bool save(const char* key, const uint8_t* buf, size_t len) override {
    if (!stateDir.empty()) {
      FILE* f = fopen((stateDir + "/" + key).c_str(), "wb");
      if (!f) return false;
      fwrite(buf, 1, len, f);
      fclose(f);
      return true;
    }
    store[key] = std::vector<uint8_t>(buf, buf + len);
    return true;
  }
  int batteryPercent() override { return 80; }
  bool charging() override { return false; }
};

// ---- PNG, stored (no compression): enough for looking ------------------------------------------

uint32_t crcTable[256];
void crcInit() {
  for (uint32_t n = 0; n < 256; ++n) {
    uint32_t c = n;
    for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crcTable[n] = c;
  }
}
uint32_t crc(const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFFu) {
  for (size_t i = 0; i < n; ++i) c = crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8);
  return c;
}
void put32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(x >> 24);
  v.push_back(x >> 16);
  v.push_back(x >> 8);
  v.push_back(x);
}
void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
  put32(out, static_cast<uint32_t>(data.size()));
  std::vector<uint8_t> td(type, type + 4);
  td.insert(td.end(), data.begin(), data.end());
  out.insert(out.end(), td.begin(), td.end());
  put32(out, ~crc(td.data(), td.size()));
}
bool writePng(const char* path, Canvas& c) {
  const int w = c.width(), h = c.height();
  std::vector<uint8_t> raw;
  for (int y = 0; y < h; ++y) {
    raw.push_back(0);
    for (int x = 0; x < w; ++x) raw.push_back(c.get(x, y) ? 0 : 255);
  }
  std::vector<uint8_t> z = {0x78, 0x01};
  size_t at = 0;
  uint32_t a = 1, b = 0;
  for (uint8_t v : raw) {
    a = (a + v) % 65521;
    b = (b + a) % 65521;
  }
  while (at < raw.size()) {
    const size_t n = raw.size() - at > 65535 ? 65535 : raw.size() - at;
    z.push_back(at + n == raw.size() ? 1 : 0);
    z.push_back(n & 0xFF);
    z.push_back(n >> 8);
    z.push_back(~n & 0xFF);
    z.push_back((~n >> 8) & 0xFF);
    z.insert(z.end(), raw.begin() + at, raw.begin() + at + n);
    at += n;
  }
  put32(z, (b << 16) | a);
  std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  std::vector<uint8_t> ihdr;
  put32(ihdr, w);
  put32(ihdr, h);
  ihdr.insert(ihdr.end(), {8, 0, 0, 0, 0});
  chunk(out, "IHDR", ihdr);
  chunk(out, "IDAT", z);
  chunk(out, "IEND", {});
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  fwrite(out.data(), 1, out.size(), f);
  fclose(f);
  return true;
}

Game game;
App app;
HostPlatform platform;
uint8_t fb[kMaxFrameBytes];
bool verbose = false;
int frames = 0, refreshes[4] = {};

void frame() {
  const Refresh r = app.render();
  frames++;
  refreshes[static_cast<int>(r)]++;
  if (verbose && r != Refresh::None) {
    char d[160];
    app.describe(d, sizeof(d));
    printf("  frame %s: %s\n", r == Refresh::Fast ? "fast" : r == Refresh::Half ? "half" : "full", d);
  }
}

void pass(uint32_t ms) {
  // as the firmware's loop: 20 ms at a time, a frame when one is due
  for (uint32_t t = 0; t < ms; t += 20) {
    platform.ms += 20;
    app.tick(20);
    if (app.wantsFrame()) frame();
  }
}

void input(const Input& e) {
  app.handle(e);
  frame();
  pass(100);
}

Input::KeyCode keyCode(const char* s) {
  if (!strcmp(s, "up")) return Input::Up;
  if (!strcmp(s, "down")) return Input::Down;
  if (!strcmp(s, "left")) return Input::Left;
  if (!strcmp(s, "right")) return Input::Right;
  if (!strcmp(s, "back")) return Input::Back;
  if (!strcmp(s, "home")) return Input::Home;
  return Input::Confirm;
}

Item itemByName(const char* s) {
  std::string n(s);
  for (char& c : n)
    if (c == '_') c = ' ';
  for (int i = 0; i < kStoreCount; ++i)
    if (n == kItems[i].key) return static_cast<Item>(i);
  fprintf(stderr, "no item %s\n", s);
  exit(2);
}

void printState() {
  char d[200];
  app.describe(d, sizeof(d));
  const GameState& s = game.st();
  printf("[adr] %s | t=%llds fire %d temp %d builder %d wood %.0f huts %d pop %d log %d\n", d,
         static_cast<long long>(s.now / 1000), s.fire, s.temperature, s.builderLevel, game.store(IT_WOOD),
         s.buildings[BD_HUT], s.population, s.logN);
}

// A plain player, for getting the game along: it stokes, gathers, checks traps, builds what it
// can, and says the last choice to any event.
void play(int minutes) {
  GameState& s = game.st();
  for (int t = 0; t < minutes * 60; ++t) {
    pass(1000);
    if (s.ev.event >= 0) {
      const Scene* sc = game.scene();
      if (s.ev.combat && !s.ev.lootShown) continue;
      if (s.ev.combat) game.combatButton(CB_LEAVE);
      else if (sc->buttonN) game.chooseButton(sc->buttonN - 1);
      continue;
    }
    if (s.active == M_ROOM && !game.onCooldown(CD_FIRE) && s.fire < 4) game.stokeFire();
    if (s.hasOutside) {
      if (t % 20 == 0) game.travelTo(s.active == M_ROOM ? M_OUTSIDE : M_ROOM);
      if (!game.onCooldown(CD_GATHER) && s.active == M_OUTSIDE) game.gatherWood();
      if (!game.onCooldown(CD_TRAPS) && s.active == M_OUTSIDE) game.checkTraps();
      for (int i = 0; i < kCraftCount; ++i)
        if (game.craftVisible(i) && !game.craftAtMax(i)) {
          Amount c[3];
          int n;
          game.craftCost(i, c, n);
          bool ok = true;
          for (int k = 0; k < n; ++k)
            if (game.store(c[k].item) < c[k].n) ok = false;
          if (ok) game.build(i);
        }
      for (int w = 1; w < WK_COUNT; ++w)
        if (game.numGatherers() > 4) game.adjustWorker(static_cast<Worker>(w), 1);
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  crcInit();
  std::string out = ".";
  int rot = -1;
  int i = 1;
  for (; i < argc; ++i) {
    if (!strcmp(argv[i], "--device") && i + 1 < argc) {
      const char* d = argv[++i];
      platform.prof = !strcmp(d, "x3") ? &kX3 : !strcmp(d, "x4c") ? &kX4Classic : !strcmp(d, "x4") ? &kX4 : &kX4Pro;
    } else if (!strcmp(argv[i], "--rot") && i + 1 < argc) {
      rot = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
      platform.seed = strtoull(argv[++i], nullptr, 10);
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    } else if (!strcmp(argv[i], "--state") && i + 1 < argc) {
      platform.stateDir = argv[++i];
    } else if (!strcmp(argv[i], "-v")) {
      verbose = true;
    } else {
      break;
    }
  }
  if (rot >= 0) {
    DeviceSettings s;
    s.rotation = static_cast<uint8_t>(rot);
    platform.save("cfg", reinterpret_cast<const uint8_t*>(&s), sizeof(s));
  }
  app.begin(game, platform, fb);
  app.loadOrStart();
  frame();
  for (; i < argc; ++i) {
    const std::string c = argv[i];
    auto arg = [&](int k) { return i + k < argc ? argv[i + k] : ""; };
    if (c == "key") {
      const bool held = !strcmp(arg(2), "long");
      input(Input::keyPress(keyCode(arg(1)), held));
      i += held ? 2 : 1;
    } else if (c == "tap" || c == "hold") {
      const int x = atoi(arg(1)), y = atoi(arg(2));
      input(c == "tap" ? Input::tap(x, y) : Input::hold(x, y));
      i += 2;
    } else if (c == "swipe") {
      input(Input::swipe(atoi(arg(1)), atoi(arg(2)), atoi(arg(3)), atoi(arg(4))));
      i += 4;
    } else if (c == "wait") {
      pass(static_cast<uint32_t>(atof(arg(1)) * 1000));
      i += 1;
    } else if (c == "snap") {
      frame();
      const std::string path = out + "/" + arg(1) + ".png";
      if (!writePng(path.c_str(), app.canvas())) fprintf(stderr, "cannot write %s\n", path.c_str());
      i += 1;
    } else if (c == "sleep") {
      app.renderSleep(false);
      const std::string path = out + "/" + arg(1) + ".png";
      if (!writePng(path.c_str(), app.canvas())) fprintf(stderr, "cannot write %s\n", path.c_str());
      i += 1;
    } else if (c == "give") {
      game.debugGive(itemByName(arg(1)), atof(arg(2)));
      pass(100);
      i += 2;
    } else if (c == "event") {
      const int e = findEvent(arg(1));
      if (e < 0) fprintf(stderr, "no event %s\n", arg(1));
      else game.debugEvent(e);
      pass(100);
      i += 1;
    } else if (c == "late") {  // a late game: everything built, bought and known
      GameState& s = game.st();
      s.builderLevel = 4;
      s.hasOutside = s.hasPath = s.hasShip = s.hasFabricator = true;
      if (!s.hasWorld) {
        game.debugGive(IT_COMPASS, 1);
        pass(100);
      }
      s.fire = 4;
      s.temperature = 4;
      for (int k = 0; k < kStoreCount; ++k)
        if (itemType(static_cast<Item>(k)) != ItemType::Blueprint && k != IT_FLEET_BEACON)
          game.debugGive(static_cast<Item>(k), 40 + k * 37 % 900);
      for (int b = 0; b < BD_COUNT; ++b) {
        s.buildings[b] = b == BD_HUT ? 18 : b == BD_TRAP ? 7 : 1;
        s.buildingSeen |= static_cast<uint16_t>(1u << b);
      }
      s.population = 70;
      s.perks = 0x2F5;
      s.blueprints = 0x2B;
      s.hull = 6;
      s.thrusters = 4;
      pass(2000);
    } else if (c == "embark") {  // packed and out of the village
      GameState& s = game.st();
      game.travelTo(M_PATH);
      s.outfit[IT_CURED_MEAT] = 20;
      s.outfit[IT_BULLETS] = 12;
      s.outfit[IT_RIFLE] = 1;
      s.outfit[IT_IRON_SWORD] = 1;
      s.outfit[IT_MEDICINE] = 2;
      s.outfit[IT_TORCH] = 2;
      s.cd[CD_EMBARK].end = 0;
      game.embark();
      pass(200);
    } else if (c == "win") {  // the fight on the glass, won
      GameState& s = game.st();
      for (int t = 0; t < 600 && s.ev.event >= 0 && !s.ev.lootShown; ++t) {
        s.trip.health = static_cast<int16_t>(game.maxHealth());
        s.ev.wanderer.hp = s.trip.health;
        for (int b = kWeaponCount - 1; b >= 0; --b)
          if (game.combatButtonEnabled(b)) game.combatButton(b);
        pass(100);
      }
    } else if (c == "liftoff") {
      GameState& s = game.st();
      if (s.hull < 1) s.hull = 5;
      s.hasShip = true;
      s.active = M_SHIP;
      s.cd[CD_LIFTOFF].end = 0;
      game.liftOff();
      pass(100);
    } else if (c == "walk") {  // N steps in one direction: n s e w
      const char d = arg(1)[0];
      const int n = atoi(arg(2));
      for (int k = 0; k < n && game.st().ev.event < 0; ++k)
        input(Input::keyPress(d == 'n' ? Input::Up : d == 's' ? Input::Down : d == 'w' ? Input::Left : Input::Right));
      i += 2;
    } else if (c == "ending") {
      GameState& s = game.st();
      s.space.active = false;
      s.ending = s.gameOver = true;
      s.lastScore = 1234;
      s.prevScore = 5678;
      pass(100);
    } else if (c == "state") {
      printState();
    } else if (c == "save") {
      app.save();
    } else if (c == "reload") {
      app.save();
      uint8_t buf[kMaxSave];
      const size_t n = platform.load("game", buf, sizeof(buf));
      printf("[adr] save: %zu bytes\n", n);
      app.begin(game, platform, fb);
      if (!app.loadOrStart()) printf("[adr] the save did not load\n");
      frame();
    } else if (c == "play") {
      play(atoi(arg(1)));
      i += 1;
    } else {
      fprintf(stderr, "unknown command %s\n", c.c_str());
      return 2;
    }
  }
  printState();
  printf("[adr] %d frames: %d fast, %d half, %d full, %d unchanged\n", frames, refreshes[1], refreshes[2], refreshes[3],
         refreshes[0]);
  return 0;
}
