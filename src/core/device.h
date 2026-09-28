// device.h — what the app needs to know of the device it runs on: the glass, how it is touched
// or keyed, and its light. The firmware picks a profile at boot; the host tests run each.
#pragma once

#include <cstdint>

namespace adr {

// A key on the device's body, for the sleep screen to point at: the edge of the glass it sits
// beside, and where along that edge, both as the device is held tall (the page at R90: x along the
// top and bottom, y down the sides, in pixels). Measured from Xteink's user guides (the device
// overviews) and the side-key and front-key hints CrossPoint lines up with them on the hardware.
enum KeyEdge : uint8_t { KE_TOP, KE_RIGHT, KE_BOTTOM, KE_LEFT };
enum KeyRole : uint8_t { KR_UP, KR_DOWN, KR_BACK, KR_CONFIRM, KR_LEFT, KR_RIGHT, KR_POWER, KR_HOME };
struct KeyMark {
  KeyRole role;
  KeyEdge edge;
  int16_t at;
};

// The X4 Pro and the X4 Classic, one body: a side key on each edge a quarter of the way down (Up,
// "previous page", on the left), Power above the right one. The Pro's Home is a pad under the
// glass; the Classic has four keys there instead: Back, Confirm, Left, Right.
constexpr KeyMark kX4ProKeys[] = {
    {KR_UP, KE_LEFT, 215}, {KR_DOWN, KE_RIGHT, 215}, {KR_POWER, KE_RIGHT, 76}, {KR_HOME, KE_BOTTOM, 240}};
constexpr KeyMark kX4ClassicKeys[] = {
    {KR_UP, KE_LEFT, 215},   {KR_DOWN, KE_RIGHT, 215},   {KR_POWER, KE_RIGHT, 76}, {KR_BACK, KE_BOTTOM, 78},
    {KR_CONFIRM, KE_BOTTOM, 183}, {KR_LEFT, KE_BOTTOM, 298}, {KR_RIGHT, KE_BOTTOM, 403}};
// The first X4: Power, then a rocker (Up over Down), all down the right side; the four keys below.
constexpr KeyMark kX4Keys[] = {
    {KR_POWER, KE_RIGHT, 159},    {KR_UP, KE_RIGHT, 375},    {KR_DOWN, KE_RIGHT, 480}, {KR_BACK, KE_BOTTOM, 78},
    {KR_CONFIRM, KE_BOTTOM, 183}, {KR_LEFT, KE_BOTTOM, 298}, {KR_RIGHT, KE_BOTTOM, 403}};
// The X3: a side key on each edge, Power on the top toward the right, the four keys below.
constexpr KeyMark kX3Keys[] = {
    {KR_UP, KE_LEFT, 192},        {KR_DOWN, KE_RIGHT, 192},  {KR_POWER, KE_TOP, 461}, {KR_BACK, KE_BOTTOM, 91},
    {KR_CONFIRM, KE_BOTTOM, 207}, {KR_LEFT, KE_BOTTOM, 321}, {KR_RIGHT, KE_BOTTOM, 437}};

struct DeviceProfile {
  const char* name;        // "x4pro", "x4c", "x3"
  int16_t panelW, panelH;  // the panel's own pixels, as the framebuffer lays them out
  bool touch;              // a touch layer and a Home key; two side keys
  bool light;              // a frontlight
  bool gyro;               // a gyro that knows which way it is held
  uint8_t cleanEvery;      // fast refreshes before a half refresh clears what they leave
  const KeyMark* keys;     // where its keys are
  uint8_t keyN;
};

// The Xteink X4 Pro: 800 x 480 at 220 ppi, GT911 touch, a capacitive Home key, two side keys,
// a warm and cool frontlight.
constexpr DeviceProfile kX4Pro = {"x4pro", 800, 480, true, true, false, 24, kX4ProKeys, 4};
// The Xteink X4 Classic: the X4 Pro's board and glass without touch or light; seven keys (two
// side keys, Left, Right, Confirm and Back below the glass, and Power).
constexpr DeviceProfile kX4Classic = {"x4c", 800, 480, false, false, false, 24, kX4ClassicKeys, 7};
// The Xteink X4 (the first, an ESP32-C3): 800 x 480, seven keys; the X3's binary drives it.
constexpr DeviceProfile kX4 = {"x4", 800, 480, false, false, false, 24, kX4Keys, 7};
// The Xteink X3: 792 x 528 at 259 ppi, seven keys, no touch, no light.
constexpr DeviceProfile kX3 = {"x3", 792, 528, false, false, true, 24, kX3Keys, 7};

constexpr int frameBytes(const DeviceProfile& d) { return (d.panelW + 7) / 8 * d.panelH; }
constexpr int kMaxFrameBytes = frameBytes(kX3) > frameBytes(kX4Pro) ? frameBytes(kX3) : frameBytes(kX4Pro);

}  // namespace adr
