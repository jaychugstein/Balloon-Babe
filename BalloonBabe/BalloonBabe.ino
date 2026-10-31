// BALLOON BABE - for Arduboy FX, FX-C and POOM. Jump on the pump to blow up the balloon, then watch it fly up.
// A = jump (hold it for a full jump, tap it for a hop). Each jump scores 5..100 for timing it right as the plate bottoms
// out; the gauge on the hose shows how full the balloon is; the score is jump points plus how high the balloon flies.
#include <Arduboy2.h>
#include <ArduboyTones.h>
#include <avr/eeprom.h>
#include "font.h"
#include "sprites.h"
#include "fxdata.h"                        // the art lives on the FX flash chip (Arduboy FX / FX-C, POOM)
Arduboy2Base ab;
ARDUBOY_NO_USB                             // saves ~3 KB, like most cart games (press reset to upload something else)
bool mute;                                 // the title is silent without switching the speaker off and on (that crackled)
bool sndOn() { return ab.audio.enabled() && !mute; }
ArduboyTones snd(sndOn);

// ---------------------------------------------------------------- small text helpers (5x7 font)
int16_t tx_, ty_; bool tw_;
void at(int16_t x, int16_t y) { tx_ = x; ty_ = y; }
void pc(char c) {
  if (c == '{' || c == '}') {                                  // a button: a round black button with its letter in white
    int16_t x = tx_ + 4, y = ty_ + 3; bool w = tw_;
    ab.fillCircle(x, y, 3, w ? WHITE : BLACK); ab.drawFastHLine(x - 2, y - 3, 5, w ? WHITE : BLACK); ab.drawFastHLine(x - 2, y + 3, 5, w ? WHITE : BLACK);   // exactly as tall as a letter
    static const uint8_t AB[2][3] PROGMEM = { { 30, 5, 30 }, { 31, 21, 10 } };   // little 3x5 A and B
    for (uint8_t i = 0; i < 3; i++) { uint8_t b = pgm_read_byte(&AB[c == '}'][i]); for (uint8_t j = 0; j < 5; j++) if (b >> j & 1) ab.drawPixel(x - 1 + i, y - 2 + j, w ? BLACK : WHITE); }
    tx_ += 9; return;
  }
  if (c == '[' || c == ']') {                                  // a d-pad arrow: a little black key with a white arrow
    bool w = tw_; ab.fillRoundRect(tx_, ty_, 7, 7, 2, w ? WHITE : BLACK);
    for (uint8_t i = 0; i < 3; i++) ab.drawFastVLine(c == '[' ? tx_ + 2 + i : tx_ + 4 - i, ty_ + 3 - i, 1 + 2 * i, w ? BLACK : WHITE);
    tx_ += 8; return;
  }
  uint8_t g = (c >= 32 && c <= 90) ? c - 32 : 0;
  for (uint8_t i = 0; i < 6; i++, tx_++) {
    uint8_t b = i < 5 ? pgm_read_byte(FONT + g * 5 + i) : 0;
    for (uint8_t j = 0; j < 7; j++) if (b >> j & 1) ab.drawPixel(tx_, ty_ + j, tw_ ? WHITE : BLACK);
  }
}
void pp(PGM_P s) { char c; while ((c = pgm_read_byte(s++))) pc(c); }
void pn(uint32_t v) { char b[10]; uint8_t n = 0; do { b[n++] = '0' + v % 10; v /= 10; } while (v); while (n) pc(b[--n]); }
void outlined(int16_t x, int16_t y, PGM_P s, int32_t n) {     // text with a 1 px white edge (n < 0: no number after it)
  for (uint8_t pass = 0; pass < 10; pass++) {
    int8_t dx = pass < 9 ? pass % 3 - 1 : 0, dy = pass < 9 ? pass / 3 - 1 : 0; if (pass == 4) continue;
    tw_ = pass < 9; at(x + dx, y + dy); pp(s); if (n >= 0) pn(n);
  }
  tw_ = false;
}
void boldText(int16_t x, int16_t y, PGM_P s, int32_t n = -1) {   // a hair bolder: printed twice a pixel apart, white edge
  for (uint8_t pass = 0; pass < 2; pass++)
    for (int8_t dy = pass ? 0 : -1; dy <= (pass ? 0 : 1); dy++) for (int8_t dx = pass ? 0 : -1; dx <= (pass ? 1 : 2); dx++) {
      tw_ = !pass; at(x + dx, y + dy); pp(s); if (n >= 0) pn(n); }
  tw_ = false;
}
uint16_t rs = 0xACE1;                                         // a tiny random number generator (xorshift)
uint16_t rnd() { if (!rs) rs = 0xACE1; rs ^= rs << 7; rs ^= rs >> 9; rs ^= rs << 8; return rs; }
void bigc(int16_t x, int16_t y, char c, uint8_t k) {        // one character blown up k times, with a white outline
  uint8_t g = c - 32;
  for (uint8_t pass = 0; pass < 2; pass++)
    for (uint8_t i = 0; i < 5; i++) { uint8_t b = pgm_read_byte(FONT + g * 5 + i);
      for (uint8_t j = 0; j < 7; j++) if (b >> j & 1) {
        if (pass == 0) ab.fillRect(x + i * k - 1, y + j * k - 1, k + 2, k + 2, WHITE);
        else ab.fillRect(x + i * k, y + j * k, k, k, BLACK); } }
}

// ---------------------------------------------------------------- state
enum { S_START, S_PUMP, S_FLY, S_SCORE, S_SOUND, S_TALK };
uint8_t st = S_START;
uint16_t tick;
const uint8_t ROUND_S = 11;                // every round is 11 seconds of pumping
uint16_t timeLeft;                         // frames
// her and the pump. The pump is a long-stroke plunger: she lands on the plate and rides it down under her weight,
// pushing air the whole way. At the bottom she jumps; the plate springs back up while she's in the air.
enum { P_AIR, P_STROKE, P_BOTTOM };
uint8_t phase;
float feet, vy;                            // feet: her height above the ground line
float plate;                               // plate travel: 0 = all the way up, 1 = bottomed out
float pv;                                  // plate speed going down
uint8_t level;                             // bounce level 0..5: on-the-beat jumps from the bottom send her higher
uint16_t frameN, bottomAt;                 // when the plate bottomed out
float bp, bvel, lastFeet, lastFv;                            // her chest on a little spring: -2 (lifted) .. +2 (settled)
float sk, skv;                             // her skirt on a spring: 0 hanging, 1 swinging out, 2 flipped up
// the balloon
float airIn;                               // total air pumped in
float bx, by, bvx, bvy, bAir, bDist;       // flight (bx, by = centre of the balloon)
float bth, bom, spinTo; uint8_t spinT;     // which way the neck points (the jet pushes the other way), spin, and the spin it's swinging toward
float trail[8][2]; uint8_t trailAge[8], trailN;   // air puffs left behind, growing as they fade
float camX, startY, fr0;                       // flight camera: how far up the view is (eased, so the balloon moves about the screen); launch height
uint8_t cdown;                            // 3-2-1-JUMP countdown at the start of a round (frames)
uint16_t best;
// timing to the millisecond: buttons are watched between frames, and the moment the plate bottoms out is worked out
// to a fraction of a frame. Each jump scores 0..100 by how close it was to that moment.
uint32_t frameMs, pressMs, bottomMs; bool pressed; uint8_t lastRaw;
uint16_t timingPts;                        // jump points this round (5..100 a jump)
float psi, nv;                             // the gauge on the hose: how full the balloon is, 0..1 round the dial (on a spring)
constexpr float ZLO = 0.86f;               // a jump this good (timing x height) is PERFECT
uint8_t verdict, emptyT; bool newBest;     // the last cheer (see WORDS), frames since the balloon ran out, new best score
const char W0[] PROGMEM = "PERFECT!",
  W1[] PROGMEM = "FLAWLESS!",
  W2[] PROGMEM = "STUNNING!",
  W3[] PROGMEM = "WOW!!",
  W4[] PROGMEM = "INCREDIBLE!",
  W5[] PROGMEM = "NAILED IT!",
  W6[] PROGMEM = "GREAT!",
  W7[] PROGMEM = "AWESOME!",
  W8[] PROGMEM = "AMAZING!",
  W9[] PROGMEM = "SUPER!",
  W10[] PROGMEM = "FANTASTIC!",
  W11[] PROGMEM = "BRILLIANT!",
  W12[] PROGMEM = "NICE!",
  W13[] PROGMEM = "SWEET!",
  W14[] PROGMEM = "LOVELY!",
  W15[] PROGMEM = "COOL!",
  W16[] PROGMEM = "GORGEOUS!",
  W17[] PROGMEM = "PRETTY!",
  W18[] PROGMEM = "GOOD!",
  W19[] PROGMEM = "YAY!",
  W20[] PROGMEM = "NEAT!",
  W21[] PROGMEM = "WOO!",
  W22[] PROGMEM = "NOT BAD!",
  W23[] PROGMEM = "GOOD ONE!",
  W24[] PROGMEM = "KEEP GOING!",
  W25[] PROGMEM = "YOU GOT IT!",
  W26[] PROGMEM = "GO GIRL!",
  W27[] PROGMEM = "KEEP IT UP!",
  W28[] PROGMEM = "ALMOST!",
  W29[] PROGMEM = "STAY CUTE!";
const char* const WORDS[30] PROGMEM = { W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10, W11, W12, W13, W14, W15, W16, W17, W18, W19, W20, W21, W22, W23, W24, W25, W26, W27, W28, W29 };   // 6 cheers for each of 5 levels
uint8_t talkPage, talkN;                   // how-to-play talk: which page, how many letters shown
uint8_t popV, popT;                        // the +points pop-up for the last jump
bool cut;                                  // let go of A on the way up (only counts once)
float tq, apexF;                           // this jump's timing (0..1) and how high it went
bool autoJ;                                // the title screen's "player": holds the jump for full height
void watchButtons() {
  uint8_t b = ab.buttonsState(), down = b & ~lastRaw & (A_BUTTON | B_BUTTON); lastRaw = b;
  if (st == S_PUMP && down && !pressed) { pressed = true; pressMs = millis(); }
}

const uint8_t G = 62;                      // ground line
const int8_t PUMPX = 24;                   // pump / her centre (over to the left: room for a big balloon)
const uint8_t PTOP = 16, PBOT = 6;            // plate height above the ground: all the way up / bottomed out
constexpr float GRAV = 0.10f;                  // real-feeling: quick up, a short hang at the top, quick down (~0.5 s in the air)
float plateY() { return PTOP - (PTOP - PBOT) * plate; }

void jump(float strength) {                                // leave the plate
  phase = P_AIR; feet = plateY();
  { float room = PTOP + 5 - feet + (autoJ ? 2 : 0);              // a full jump peaks 5 px above the plate's top, then drops onto it
                      // (on the title she may jump higher: her head can go off the top)
    float vmax = room > 0 ? sqrtf(2 * GRAV * room) : 0.3f;   // never so high her head leaves the top:
    vy = min(strength, max(0.3f, vmax)); }                                         // jumping off a high plate is only a hop
  skv -= 0.4f;                                             // the skirt drags down as she shoots up
  apexF = feet; cut = false;
  snd.tone(380 + level * 70, 25);
}
void startRound() {
  st = S_PUMP; tick = 0; timeLeft = ROUND_S * 60; level = 0; airIn = 0; frameN = 0; psi = nv = 0;
  timingPts = 0; pressed = false; popT = 0;
  plate = 1; phase = P_BOTTOM; feet = lastFeet = PBOT; vy = lastFv = 0; bp = bvel = 0; sk = skv = 0;               // she stands on the pump for the countdown
  cdown = 120;                                                                       // 3, 2, 1 (half a second each), then JUMP!
}
void setTq(int32_t err) { if (err < 0) err = -err; tq = err >= 300 ? 0 : 1 - err / 300.0f; }   // 0 ms off = 1, 300 ms off = 0
void pumpUpdate(bool press) {
  frameN++;
  const float FMS = 1000.0f / 60;
  if (phase == P_AIR) {
    // hold A for a full jump: let go early and she stops rising (extra gravity on the way up), like a real hop.
    // A full jump lasts just long enough for the plate to spring all the way back up; a quick tap lands on it halfway.
    if (feet > apexF) apexF = feet;
    if (vy > 0 && !autoJ && !(ab.buttonsState() & (A_BUTTON | B_BUTTON)) && !cut) { vy *= 0.45f; cut = true; }   // let go early: a shorter hop
    vy -= GRAV; feet += vy;
    if (plate > 0) { plate -= 0.048f; if (plate < 0) plate = 0; }       // the plate springs back up while she's in the air
    if (vy < 0 && feet <= plateY()) {                                    // touch down on the plate
      feet = plateY(); phase = P_STROKE; pv = 0.008f + level * 0.0015f;   // higher drops start the stroke faster
      skv += min(1.4f, -vy * 0.6f); snd.tone(140, 30);
      // how good was that jump? Timing (pressing right as the plate bottomed out) times height (a full, held jump)
      float h = (apexF - PBOT) / (PTOP + 4 - PBOT); if (h > 1) h = 1; if (h < 0) h = 0;
      float q = tq * h; uint8_t pts = 5 + (uint8_t)(q * q * 95 + 0.5f);   // every jump scores something (5..100)
      airIn += 0.3f;                                                   // and every landing puffs a little air in, however weak
      timingPts += pts; popV = pts; popT = 40;
      // the cheer, always friendly: five levels of six words (WORDS)
      { uint8_t tier = q >= ZLO ? 0 : q >= 0.65f ? 1 : q >= 0.45f ? 2 : q >= 0.25f ? 3 : 4, k = rnd() % 6;   // all encouragement,
        if (tier * 6 + k == verdict) k = (k + 1) % 6; verdict = tier * 6 + k; }                               // a random one of six (never the same twice running)
      if (verdict < 6) snd.tone(1320, 80);                               // a ding for a perfect one
    }
    return;
  }
  if (phase == P_STROKE) {
    float before = plate;
    pv += 0.0030f; plate += pv;                                          // her weight and gravity push the plate down
    if (plate >= 1) plate = 1;
    airIn += (plate - before) * (1.2f + level * 0.35f);                  // air goes in all the way down the stroke
    nv += (plate - before) * 0.25f;                                      // and the gauge needle kicks forward
    feet = plateY();
    if ((frameN & 3) == 0) snd.tone(200 + (uint16_t)(plate * 300), 15);
    if (press) {                                                         // jumped before the bottom: how long until it would have bottomed out?
      float rem = 1 - plate, v = pv; uint8_t n = 0; while (rem > 0 && n < 120) { v += 0.003f; rem -= v; n++; }
      int32_t err = (int32_t)pressMs - (int32_t)(frameMs + (uint32_t)(n * FMS)); setTq(err);
      if (err > -60) { level = min(level + 1, 5); jump(1.6f); snd.tone(900 + level * 90, 40); }   // a hair early still counts
      else if (plate > 0.6f) { jump(1.45f); }                 // close enough: a good one
      else { if (level) level--; jump(1.25f); }                                    // too early: a cut-short stroke, a weak hop
      return;
    }
    if (plate >= 1) { phase = P_BOTTOM; bottomAt = frameN; bottomMs = frameMs + (uint32_t)((1 - before) / (plate - before + 0.0001f) * FMS); snd.tone(90, 40); }
    return;
  }
  // P_BOTTOM: the sweet spot is right as it bottoms out
  feet = plateY();
  uint16_t late = autoJ ? 0 : frameN - bottomAt;                   // (the title screen's jumps always count as on time)
  if (press) {
    setTq((int32_t)pressMs - (int32_t)bottomMs);
    if (late <= 10) { level = min(level + 1, 5); jump(1.6f); snd.tone(900 + level * 90, 40); }
    else if (late <= 30) { jump(1.45f); }
    else { if (level) level--; jump(1.3f); }        // too late: you lose one bounce level, not all of them
  }
}

// ---------------------------------------------------------------- drawing
void drawPump(int16_t px, int16_t oy) {       // px: the pump's centre; oy: shifts the whole scene down
  int16_t py = G - (int16_t)plateY() + oy;                                          // plate top
  int16_t bt = G - 3 + oy;                                                          // a low pump housing: the plate comes right down onto it
  if (bt - py - 3 > 0) ab.fillRect(px - 1, py + 3, 3, bt - py - 3, BLACK);           // rod (slides down into the housing)
  ab.fillRect(px - 8, bt, 16, 3, BLACK); ab.drawFastHLine(px - 7, bt + 1, 14, WHITE);   // housing
  ab.fillRect(px + 8, bt, 6, 2, BLACK); ab.drawFastVLine(px + 13, bt - 1, 4, BLACK);    // the nozzle the balloon goes on
  ab.fillRect(px - 12, py, 24, 3, BLACK); ab.drawFastHLine(px - 11, py + 1, 22, WHITE);   // foot plate
  ab.drawFastHLine(px - 14, G - 1 + oy, 28, BLACK);                                 // base
}
void drawCloud(int16_t x, int16_t y) {
  ab.fillCircle(x, y, 3, WHITE); ab.fillCircle(x + 4, y - 1, 4, WHITE); ab.fillCircle(x + 9, y, 3, WHITE);
  ab.drawCircle(x, y, 3, BLACK); ab.drawCircle(x + 4, y - 1, 4, BLACK); ab.drawCircle(x + 9, y, 3, BLACK);
  ab.fillRect(x - 1, y - 1, 12, 3, WHITE); ab.drawFastHLine(x - 3, y + 3, 15, BLACK);
}
// the back yard: a picket fence, grass, an optional tree on the right and drifting clouds. g = where the ground line is.
void drawYard(int16_t g, bool tree, bool clouds) {
  if (clouds) for (uint8_t k = 0; k < 2; k++) {
    int16_t x = (int16_t)((k * 85 + 400 - tick / 16) % 170) - 30;    // half a lap apart, drifting slowly
    drawCloud(x, k ? 26 : 12);
  }
  if (g > 80) return;
  ab.drawFastHLine(0, g - 12, 128, BLACK); ab.drawFastHLine(0, g - 5, 128, BLACK);  // rails
  for (int16_t x = 1; x < 128; x += 7) {                                            // pickets with pointed tops
    ab.fillRect(x + 1, g - 16, 2, 16, WHITE);
    ab.drawFastVLine(x, g - 16, 16, BLACK); ab.drawFastVLine(x + 3, g - 16, 16, BLACK);
    ab.drawPixel(x + 1, g - 17, BLACK); ab.drawPixel(x + 2, g - 17, BLACK);
  }
  if (tree) {
    ab.fillRect(126, g - 24, 6, 24, BLACK); ab.drawFastVLine(128, g - 20, 14, WHITE);   // trunk (the tree is half out of the picture)
    static const int16_t C[3][3] = { { 121, -30, 8 }, { 133, -33, 9 }, { 126, -41, 9 } };
    for (uint8_t i = 0; i < 3; i++) ab.fillCircle(C[i][0], g + C[i][1], C[i][2], BLACK);
    for (uint8_t i = 0; i < 3; i++) ab.fillCircle(C[i][0], g + C[i][1], C[i][2] - 1, WHITE);
    for (int16_t y = g - 49; y < g - 22; y += 2) for (int16_t x = 113 + (y & 2); x < 128; x += 4)   // leaves
      for (uint8_t i = 0; i < 3; i++) { int16_t dx = x - C[i][0], dy = y - g - C[i][1], rr = C[i][2] - 2;
        if (dx * dx + dy * dy <= rr * rr) { ab.drawPixel(x, y, BLACK); break; } }
    static const int16_t F[4][2] = { { 118, -29 }, { 124, -35 }, { 122, -44 }, { 126, -27 } };
    for (uint8_t i = 0; i < 4; i++) { ab.fillCircle(F[i][0], g + F[i][1], 2, WHITE); ab.fillCircle(F[i][0], g + F[i][1], 1, BLACK); ab.drawPixel(F[i][0], g + F[i][1] - 2, BLACK); }   // apples
  }
  ab.drawFastHLine(0, g, 128, BLACK);
  for (int16_t x = 3; x < 128; x += 11) { int16_t t = x + (x * 7 & 3); ab.drawPixel(t, g - 1, BLACK); ab.drawPixel(t + 2, g - 1, BLACK); ab.drawPixel(t + 1, g - 2, BLACK); }   // grass tufts
}
// Balloon size. f = how full it is, 0..1. Full is about what non-stop perfect jumps pump in over the whole round
// (worked out from the pump physics), so it only maxes out on a near-perfect round. The radius grows in step with the air.
float capAir() { return 42; }                // a full balloon: about what non-stop perfect jumps pump in over the 11 s (44 at best)
constexpr float FLY_AIR = 30;              // air in a full balloon at take-off (about 7 s of leaking)
uint8_t bRad(float f) { if (f < 0) f = 0; if (f > 1) f = 1; return 3 + (uint8_t)(f * 23 + 0.5f); }   // full: 26 px radius, nearly the whole sky
// the balloon on the nozzle: tied on at (tx, ty), resting on the ground beside the pump and growing to the right
int16_t balCX(int16_t tx, uint8_t r) { return tx + 3 + r; }
void drawBalloonOn(int16_t tx, int16_t ty, uint8_t r, int16_t g) {
  int16_t cx = balCX(tx, r), cy = g - 1 - r;
  float dx = tx - cx, dy = ty - cy, l = sqrtf(dx * dx + dy * dy); dx /= l; dy /= l;
  const float C = 0.9004f * r, S = 0.4350f * r;                      // the neck: a wedge from the balloon's side to the nozzle (cos, sin of 0.45)
  ab.fillTriangle(tx, ty, cx + dx * C - dy * S, cy + dy * C + dx * S, cx + dx * C + dy * S, cy + dy * C - dx * S, BLACK);
  ab.fillCircle(cx, cy, r, BLACK);
  ab.fillRect(tx - 1, ty - 1, 2, 3, BLACK);                          // the knot of rubber on the nozzle
  if (r > 5) {                                                       // shine: a little arc up top-left and a dot
    static const int8_t A[8][2] PROGMEM = { {-94,-35}, {-89,-46}, {-83,-56}, {-75,-66}, {-67,-74}, {-57,-82}, {-47,-88}, {-36,-93} };   // cos, sin x100 of 3.5..4.34
    for (uint8_t k = 0; k < 8; k++) ab.drawPixel(cx + (int8_t)pgm_read_byte(&A[k][0]) * (r - 2) / 100, cy + (int8_t)pgm_read_byte(&A[k][1]) * (r - 2) / 100, WHITE);
    ab.fillCircle(cx - r / 4, cy - r / 2 - 1, 1, WHITE);
  } else if (r > 3) ab.drawPixel(cx - 1, cy - 1, WHITE);
}
// Her chest: a soft weight on a spring, driven by how her body moves. It feels gravity plus her body's push:
// standing, it sags a little; falling, it goes weightless and floats up while her body drops; the instant she lands
// on the pump her body stops dead and it slams down, bounces up and wobbles to a rest. Pushing off does the reverse.
uint8_t babePose() { return phase != P_AIR ? (plate > 0.5f ? 1 : 5) : (vy > 0.6f ? 2 : vy > -0.6f ? 3 : 4); }
// a loose balloon flying along (fx, fy: the way it's going; the neck points back): an egg shape, round up front,
// tapering to the knot, with the shine up-left where the light is
void drawLoose(int16_t x, int16_t y, uint8_t r, float fx, float fy) {
  ab.fillCircle(x, y, r, BLACK);
  int16_t mx = x - fx * (r * 0.55f), my = y - fy * (r * 0.55f);
  ab.fillCircle(mx, my, r * 2 / 3, BLACK);
  int16_t kx = x - fx * (r + 3), ky = y - fy * (r + 3);
  ab.fillTriangle(kx, ky, mx - fy * 2, my + fx * 2, mx + fy * 2, my - fx * 2, BLACK);
  ab.drawPixel(kx - fx, ky - fy, BLACK);
  if (r > 4) { ab.fillCircle(x - r / 3, y - r / 3, r > 7 ? 1 : 0, WHITE); ab.drawPixel(x - r / 2, y, WHITE); }
}
void springs(bool falling) {
  float v = feet - lastFeet, acc = v - lastFv; lastFeet = feet; lastFv = v;
  float g = 1 + acc / GRAV; if (g > 9) g = 9; if (g < -3) g = -3;                 // 1 = standing, 0 = free fall
  bvel += -0.20f * (bp - (g - 1) * 1.1f); bvel *= 0.86f; bp += bvel; if (bp > 2.4f) bp = 2.4f; if (bp < -2.4f) bp = -2.4f;   // standing: the middle (boldest) look
  float want = falling ? 1.8f : 0;
  skv += (want - sk) * 0.18f; skv *= 0.75f; sk += skv; if (sk < 0) { sk = 0; skv = -skv * 0.3f; } if (sk > 2.2f) sk = 2.2f;
}
void drawBabeAt(int16_t x, int16_t y, uint8_t pose, uint8_t chest, uint8_t skirt) {   // body, then skirt, then chest on top
  FX::drawBitmap(x, y, FX_BABE, pose, dbmMasked);
  int16_t cx = x + (int8_t)pgm_read_byte(&CHEST_AT[pose][0]), cy = y + (int8_t)pgm_read_byte(&CHEST_AT[pose][1]);
  FX::drawBitmap(cx - 1, cy + 12, FX_SKIRT, skirt, dbmMasked);
  FX::drawBitmap(cx, cy, FX_CHEST, chest, dbmMasked);
}
void drawBabe(int16_t px, uint8_t pose, int8_t oy) {
  int16_t fy = G - (int16_t)feet + oy, x = px - BABE_W / 2, y = fy - BABE_H;
  int8_t v = (int8_t)(bp + (bp >= 0 ? 0.5f : -0.5f)) + 2; if (v < 0) v = 0; if (v > 4) v = 4;
  drawBabeAt(x, y, pose, v, sk > 1.4f ? 2 : sk > 0.6f ? 1 : 0);
}
const int16_t GX = 58, GY = 47, TIE_X = 70;     // the gauge, and where the balloon is tied on
void drawGauge() {                                // a little dial on a loop of hose between the pump and the balloon
  // the hose: out of the pump, up into the gauge, out and down to the balloon (2 px thick, with a white edge so it
  // stands out from the fence behind it)
  static const int8_t H[4][4] = { { PUMPX + 13, G - 3, GX - 8 - PUMPX - 13, 2 }, { GX - 10, GY, 2, G - 1 - GY },
                                  { GX + 8, GY, 2, G - 1 - GY }, { GX + 8, G - 3, TIE_X - GX - 8, 2 } };
  for (uint8_t pass = 0; pass < 2; pass++) for (uint8_t i = 0; i < 4; i++)
    ab.fillRect(H[i][0] - 1 + pass, H[i][1] - 1 + pass, H[i][2] + 2 - pass * 2, H[i][3] + 2 - pass * 2, pass ? BLACK : WHITE);
  ab.fillCircle(GX, GY, 10, WHITE);
  ab.fillCircle(GX, GY, 9, BLACK); ab.fillCircle(GX, GY, 8, WHITE);
  // the dial: three quarters of a turn, from empty at the lower left, round over the top, to full at the lower right
  for (uint8_t k = 0; k <= 8; k++) {                                   // nine tick marks, the ends and middle longer
    float t = 2.356f + k * 0.589f, c = cosf(t), sn = sinf(t);
    for (uint8_t d = (k % 4 == 0) ? 5 : 6; d <= 7; d++) ab.drawPixel(GX + c * d, GY + sn * d, BLACK);
  }
  float t = 2.356f + max(0.0f, min(psi, 1.03f)) * 4.712f;
  ab.drawLine(GX, GY, GX + cosf(t) * 6.5f, GY + sinf(t) * 6.5f, BLACK);  // the needle
  ab.fillRect(GX - 1, GY - 1, 3, 3, BLACK);       // the hub
}
void hud() {                               // the time left
  uint16_t s10 = (timeLeft + 5) / 6; tw_ = false;
  { int16_t x = s10 >= 100 ? 52 : 55; ab.fillRect(x - 1, 0, 128 - 2 * x + 2, 9, WHITE); at(x, 1); pn(s10 / 10); pc('.'); pn(s10 % 10); }   // top centre, on a clear patch of sky
}

// ---------------------------------------------------------------- screens
uint8_t relT;                                // title: counting through the balloon flying off
void startScreen() {
  // she plays exactly as in the game (same physics, poses, chest and skirt), doing slow full jumps by herself.
  // When the balloon is a fair size it slips off and whooshes up past the logo, and she starts a new one.
  const int16_t TX = 20;                                             // the pump's x on this screen
  if (!relT) {
    autoJ = true; mute = true;                                         // the title is silent
    pumpUpdate(phase == P_BOTTOM && frameN - bottomAt >= 12 && fabsf(bvel) < 0.02f && fabsf(bp) < 0.3f);   // wait at the bottom until her chest has stopped jiggling, then jump
    snd.noTone(); mute = false; autoJ = false;
    if (airIn > 9 && phase == P_BOTTOM) relT = 1;
  } else if (++relT > 80) { relT = 0; airIn = 0; }
  springs(phase == P_AIR && vy < -0.3f);
  ab.fillScreen(WHITE);
  drawYard(G, false, true);
  drawPump(TX, 0);
  uint8_t r = bRad(airIn / 26);
  if (!relT) drawBalloonOn(TX + 14, G - 2, r, G);
  else {                                                             // let go: up it goes, wobbling, past the logo
    float t = relT; int16_t cx = balCX(TX + 14, r) + (int16_t)(t * 0.4f + sinf(t * 0.35f) * 5), cy = G - 1 - r - (int16_t)(t * t * 0.03f + t * 0.6f);
    float a = -1.57f + sinf(t * 0.35f) * 0.7f; drawLoose(cx, cy, r, cosf(a), sinf(a));
  }
  drawBabe(TX, babePose(), 0);
  FX::drawBitmap(124 - LOGO_W, 2, FX_LOGO, 0, dbmMasked);            // a few pixels in from the edges
  tw_ = false;
  if (best) { static const char T2[] PROGMEM = "BEST "; uint8_t n = 5 + 1 + (best > 9) + (best > 99) + (best > 999); boldText(124 - LOGO_W + LOGO_BABE_CX - n * 3, 44, T2, best); }
  { static const char T2[] PROGMEM = "PRESS {"; if (tick & 32) boldText(124 - LOGO_W + LOGO_BABE_CX - 22, 54, T2); }   // centred under BABE; blinks with a thin white edge
  if (ab.justPressed(A_BUTTON) || ab.justPressed(B_BUTTON)) { snd.noTone(); st = S_SOUND; tick = 0; rs ^= (uint16_t)micros() | 1; }   // (seeds the cheers and flight paths)
}
void pumpScreen() {
  bool press = pressed; pressed = false;
  if (cdown > 30) {                                   // 3, 2, 1: she waits on the pump; the clock hasn't started
    cdown--; press = false;
    if (cdown % 30 == 29) snd.tone(520, 80);                            // beep on 3, 2, 1
    if (cdown == 30) { bottomAt = frameN; bottomMs = frameMs; snd.tone(1040, 220); }   // GO: jumping right on JUMP! scores as a perfect
  } else {
    if (cdown) cdown--;
    pumpUpdate(press);
  }
  springs(phase == P_AIR && vy < -0.3f);               // chest: overshoot, wobble, settle. Skirt: falling, the air flips it up
  if (timeLeft && cdown <= 30) timeLeft--;
  const int8_t oy = 0;                                                              // the view stays still: no bobbing with her jumps
  ab.fillScreen(WHITE);
  drawYard(G, true, true);
  drawPump(PUMPX, oy);
  // the gauge on the hose shows how full the balloon is
  nv += (min(1.0f, airIn / capAir()) - psi) * 0.2f; nv *= 0.72f; psi += nv;     // follows the fill on a spring: kicks with each stroke, wobbles, settles
  drawGauge();
  drawBalloonOn(TIE_X, G - 2, bRad(airIn / capAir()), G);   // only ever grows: it shows exactly the air that's in it
  drawBabe(PUMPX, babePose(), oy);
  hud();
  if (popT) {                                         // the review of the last jump, big at the top, and its points by her
    PGM_P v = (PGM_P)pgm_read_word(&WORDS[verdict]); uint8_t w = strlen_P(v) * 6 + 5;   // in a little black badge, white letters
    ab.fillRoundRect(85 - w / 2, 10, w, 11, 4, WHITE); ab.fillRoundRect(86 - w / 2, 11, w - 2, 9, 3, BLACK);
    tw_ = true; at(89 - w / 2, 12); pp(v); tw_ = false; }
  if (popT) { popT--; static const char PL[] PROGMEM = "+"; outlined(PUMPX + 16, (int16_t)(G - feet - BABE_H) + 30 - (40 - popT) / 10, PL, popV); }   // points from the last pump, on top
  if (cdown > 30) { char c = '0' + (cdown - 31) / 30 + 1; bigc(76, 16, c, 4); }       // big 3, 2, 1 over the empty side
  else if (cdown) { static const char J[] PROGMEM = "JUMP!"; for (uint8_t i = 0; i < 5; i++) bigc(52 + i * 13, 22, pgm_read_byte(J + i), 2); }
  if (!timeLeft && phase != P_AIR) {         // time's up: the balloon slips off the nozzle and takes off
    st = S_FLY; tick = 0; { uint8_t r0 = bRad(airIn / capAir()); bx = balCX(TIE_X, r0); by = G - 1 - r0; } bvx = -0.3f; bvy = -1.5f; bAir = min(1.0f, airIn / capAir()) * FLY_AIR; emptyT = 0; bDist = 0; bth = -1.57f; camX = 0; startY = by; fr0 = (2 + bRad(bAir / FLY_AIR) * 9 / 24) / 2.0f;   // (its top at launch counts as 0 ft) bom = 0; spinTo = 0; spinT = 0; trailN = 0; camX = 0;
    for (uint8_t k = 0; k < 8; k++) trailAge[k] = 99;
  }
}
void flyScreen() {
  // a leaking balloon let go: the jet out of the neck shoves it the other way while the neck whips about, so it shoots
  // UP in loops, swoops and spirals that change every half second or so. We measure how high it gets (2 px = 1 ft).
  // The view is zoomed out (the balloon is drawn smaller than on the pump) and follows it up the sky.
  // camX here is the view's height: the world y at the top of the screen (0 = the pump scene, more negative = higher).
  uint8_t r = 2 + bRad(bAir / FLY_AIR) * 9 / 24;                                     // up to ~11 px
  if (bAir > 0) {
    if (!spinT--) {                                                                 // every so often the neck flips into a new spin
      spinT = 10 + rnd() % 25;
      spinTo = ((int8_t)(rnd() % 29) - 14) * 0.011f;
      float d = -1.57f - bth; while (d > 3.1416f) d -= 6.2832f; while (d < -3.1416f) d += 6.2832f;
      spinTo += d * 0.08f;                                                          // ...leaning toward straight up
    }
    bom += (spinTo - bom) * 0.12f; bth += bom;
    if (bth > 3.1416f) bth -= 6.2832f; if (bth < -3.1416f) bth += 6.2832f;
    float push = 0.07f + bAir * 0.0045f;
    bvx += cosf(bth) * push + (64 - bx) * 0.0005f; bvy += sinf(bth) * push - 0.026f;   // jet, a little lift, drifting back toward the middle
    bAir -= 0.07f; if (bAir < 0) { bAir = 0; snd.tone(200, 80); }
    if ((tick & 3) == 0) snd.tone(380 + (uint16_t)(bAir * 6) + (uint16_t)(fabsf(bom) * 3000), 30);   // pfffft, wavering with the spin
    if ((tick % 3) == 0) { uint8_t k = trailN & 7; trail[k][0] = bx - cosf(bth) * (r + 4); trail[k][1] = by - sinf(bth) * (r + 4); trailAge[k] = 0; trailN++; }
    bvx *= 0.95f; bvy *= 0.95f;
  } else {                                                                           // empty: a limp rag, rocking side to side as it drops
    bth = 1.57f + sinf(tick * 0.09f) * 0.8f;
    bvx += sinf(tick * 0.045f) * 0.03f; bvy += 0.022f; bvx *= 0.96f; bvy *= 0.94f;
  }
  for (uint8_t k = 0; k < 8; k++) if (trailAge[k] < 99) trailAge[k]++;
  bx += bvx; by += bvy;
  if (bx < r + 1) { bx = r + 1; if (bvx < 0) bvx = -bvx * 0.5f; }                  // the sides of the screen keep it in view
  if (bx > 112 - r) { bx = 112 - r; if (bvx > 0) bvx = -bvx * 0.5f; }              // (the height scale is on the right)
  if (by > G - r - 2) { by = G - r - 2; if (bvy > 0) bvy = -bvy * 0.6f; }           // skims off the ground if it dives
  bDist = max(bDist, (startY - (by - r)) / 2 - fr0);                              // height: the highest the top of the balloon (full or empty) got, 2 px = 1 ft
  if (bAir > 0) { float want = min(0.0f, by - 30 + bvy * 10); camX += (want - camX) * 0.08f; }   // follow it up; stop when it's empty
  int16_t cam = (int16_t)camX;
  ab.fillScreen(WHITE);
  // the sky in zones on the way up: CLOUDS (20-70 ft), then BIRDS flapping across (70-130 ft), then PLANES (130-190 ft),
  // then the edge of space with twinkling STARS (190 ft and up). One thing to a 24 px band of sky, so nothing bunches.
  for (int16_t i = (-cam - 24) / 24; i * 24 < -cam + 88; i++) {                     // i: band number going up
    int16_t wy = G - i * 24, y = wy - cam, ft = (int16_t)(startY - wy) / 2;
    if (ft < 10) continue;
    if (ft >= 190) {                                                                 // stars: some twinkle into little crosses
      for (uint8_t k = 0; k < 3; k++) { int16_t sx = (i * 37 + k * 43) % 110 + 2, sy = y + k * 8;
        ab.drawPixel(sx, sy, BLACK);
        if (((tick >> 5) + i * 3 + k * 5) % 7 == 0) { ab.drawPixel(sx - 1, sy, BLACK); ab.drawPixel(sx + 1, sy, BLACK); ab.drawPixel(sx, sy - 1, BLACK); ab.drawPixel(sx, sy + 1, BLACK); } }
      continue;
    }
    if ((ft >= 20 && ft < 70) || (i % 3) == 0) drawCloud((i * 53 + 11) % 96 + 4, y);
    bool rt = i & 1; int16_t mx = rt ? (int16_t)((tick * 2 + i * 40) % 170) - 20 : 150 - (int16_t)((tick * 2 + i * 40) % 170);
    if (ft >= 70 && ft < 130) {                                                      // a bird flying by in this band
      int8_t up = ((tick >> 3) + i) & 1 ? 1 : -1; int16_t by2 = y + 12;
      ab.drawLine(mx - 3, by2 - up, mx, by2 + up, BLACK); ab.drawLine(mx, by2 + up, mx + 3, by2 - up, BLACK);
    } else if (ft >= 130 && ft < 190 && (i & 1)) {                                   // a little plane, going the other way to the bird
      int16_t px = 150 - (int16_t)((tick + i * 50) % 190), py = y + 10;            // cruising slowly across
      ab.fillRect(px - 6, py, 13, 2, BLACK); ab.drawPixel(px - 7, py, BLACK);        // body, nose (flying left)
      ab.drawLine(px + 5, py - 3, px + 6, py - 1, BLACK); ab.drawPixel(px + 4, py - 3, BLACK);   // tail fin
      ab.drawLine(px, py + 1, px + 3, py + 4, BLACK); ab.drawLine(px, py, px + 2, py - 2, BLACK);   // wings
      ab.drawPixel(px - 4, py, WHITE); ab.drawPixel(px - 2, py, WHITE);                  // windows
    }
  }
  if (cam > -70) {                                                                   // the ground scene, sliding away below
    int16_t g = G - cam;
    drawYard(g, true, false);
    plate = 1; drawPump(PUMPX, -cam);
    drawBabeAt(PUMPX - BABE_W / 2, g - PBOT - BABE_H, 0, 2, 0);                      // she watches it go
  }
  ab.drawFastVLine(127, 0, 64, BLACK);                                              // the height scale up the right side
  { int16_t f0 = (int16_t)((startY - cam - 70) / 2); if (f0 < 0) f0 = 0; f0 = f0 / 10 * 10;
    for (int16_t f = f0; ; f += 10) {
      int16_t y = (int16_t)(startY - f * 2) - cam; if (y < -8) break; if (y > 70) continue;
      bool big = f % 50 == 0;
      ab.drawFastHLine(big ? 122 : 125, y, big ? 5 : 2, BLACK);
      if (big && f) { tw_ = false; uint8_t n = f >= 100 ? 3 : 2; ab.fillRect(121 - n * 6, y - 4, n * 6 + 1, 9, WHITE); at(122 - n * 6, y - 3); pn(f); }
    } }
  for (uint8_t k = 0; k < 8; k++) {                                                 // puffs of air: little rings that grow and fade
    uint8_t a = trailAge[k]; if (a > 22) continue;
    int16_t tx = (int16_t)trail[k][0], ty = (int16_t)trail[k][1] - cam;
    if (a < 6) ab.drawPixel(tx, ty, BLACK); else if (a < 14) ab.drawCircle(tx, ty, 1, BLACK);
    else { ab.drawPixel(tx - 2, ty, BLACK); ab.drawPixel(tx + 2, ty, BLACK); ab.drawPixel(tx, ty - 2, BLACK); ab.drawPixel(tx, ty + 2, BLACK); }
  }
  { int16_t x = (int16_t)bx, y = (int16_t)by - cam;
    float fx = cosf(bth), fy = sinf(bth);                                            // fx, fy: the way it's flying (the neck points back)
    if (bAir > 0) drawLoose(x, y, r - ((int8_t)(fabsf(bom) * 12) > 1), fx, fy);   // squashes a touch when it spins hard
    else {                                                                         // the empty skin: a crumpled wiggle with the knot
      for (int8_t i = -4; i <= 4; i++) {
        float w = sinf(i * 1.2f + tick * 0.3f) * 1.2f;
        int16_t px = x + fx * i - fy * w, py = y + fy * i + fx * w;
        ab.drawPixel(px, py, BLACK); ab.drawPixel(px + 1, py, BLACK); ab.drawPixel(px, py + 1, BLACK);
      }
      ab.fillCircle(x - fx * 5, y - fy * 5, 1, BLACK);
    } }
  { uint8_t n = 2 + (bDist >= 10) + (bDist >= 100) + (bDist >= 1000);              // the height so far, in a box at the top left
    ab.fillRect(0, 0, n * 6 + 3, 9, WHITE); at(1, 1); pn((uint32_t)bDist); pc('F'); pc('T');
  }
  if (bAir <= 0) {                                                                    // empty: stamp the height on the screen
    if (emptyT < 255) emptyT++;
    if (emptyT == 6) snd.tone(160, 90);
    if (emptyT > 5) { uint8_t n = 3 + (bDist >= 10) + (bDist >= 100);
      int16_t w = n * 12 + 16, x0 = 64 - w / 2;
      ab.fillRoundRect(x0 - 2, 14, w + 4, 37, 6, WHITE); ab.drawRoundRect(x0, 16, w, 33, 5, BLACK); ab.drawRoundRect(x0 + 1, 17, w - 2, 31, 4, BLACK);
      static const char T[] PROGMEM = "HEIGHT"; at(47, 20); pp(T);                     // (room all round the big numbers)
      char b[6]; uint8_t i = 0; uint16_t v = (uint16_t)bDist; char t2[4]; uint8_t m = 0; do { t2[m++] = '0' + v % 10; v /= 10; } while (v);
      while (m) b[i++] = t2[--m]; b[i++] = 'F'; b[i++] = 'T';
      for (uint8_t k = 0; k < i; k++) bigc(x0 + 8 + k * 12, 30, b[k], 2); }
  }
  if (bAir <= 0 && (emptyT > 80 || by - cam > 72 || by >= G - r - 2)) {             // after a short look at the empty skin (1.3 s), or once it's gone
    st = S_SCORE; tick = 0;
    uint16_t sc = (uint16_t)bDist + timingPts;
    newBest = sc > best;
    if (newBest) { best = sc; eeprom_update_word((uint16_t*)420, best ^ 0xBA12); }
    snd.tone(330, 120, 262, 200);
  }
}
const uint8_t HEART[7] PROGMEM = { 0x06, 0x0F, 0x1E, 0x3C, 0x1E, 0x0F, 0x06 };   // a little 7x6 heart (columns, top bit first)
void heart(int16_t x, int16_t y, uint8_t c) { for (uint8_t i = 0; i < 7; i++) { uint8_t b = pgm_read_byte(HEART + i); for (uint8_t j = 0; j < 6; j++) if (b >> j & 1) ab.drawPixel(x + i, y + j, c); } }
void rightNum(int16_t xr, int16_t y, uint32_t v, char pre, bool ft) {   // a number ending at xr, with an optional +, FT
  uint8_t n = 1; for (uint32_t t = v; t >= 10; t /= 10) n++; n += (pre != 0) + (ft ? 2 : 0);
  at(xr - n * 6 + 1, y); if (pre) pc(pre); pn(v); if (ft) { pc('F'); pc('T'); }
}
void scoreScreen() {
  // a pretty results card: hearts floating up in the background, her cheering on the left, the score big in a
  // heart-cornered card, and NEW BEST! with blinking hearts when she beats it
  ab.fillScreen(WHITE);
  uint16_t sc = (uint16_t)bDist + timingPts;
  for (uint8_t c = 0; c < 6; c++) {                                             // hearts drifting up around her: three columns,
    int16_t y = 55 - (int16_t)((tick / 3 + c * 9) % 55);                          // two hearts each, spaced evenly
    int16_t x = 3 + (c % 3) * 14 + (int16_t)(sinf((tick + c * 40) * 0.07f) * 2);
    if (y >= 1) heart(x, y, BLACK);                                               // (nothing touches the screen's edges)
  }
  { uint8_t ph = tick % 96; bool shake = ph < 44;                                // she cheers, arms going up and down, and shimmies
    int8_t dx = shake ? (((tick >> 3) & 1) ? 1 : -1) : 0;
    ab.drawFastHLine(1, 62, 38, BLACK);
    int16_t bx0 = 3, by0 = 62 - BABE_H;
    FX::drawBitmap(bx0, by0, FX_BABE, (tick >> 4) & 1 ? 2 : 0, dbmMasked);   // arms up, arms down, cheering (both poses share the chest spot)
    int16_t cx = bx0 + (int8_t)pgm_read_byte(&CHEST_AT[0][0]), cy = by0 + (int8_t)pgm_read_byte(&CHEST_AT[0][1]);
    FX::drawBitmap(cx - 1 - dx, cy + 12, FX_SKIRT, shake ? 1 : 0, dbmMasked);
    FX::drawBitmap(cx + dx, cy, FX_CHEST, shake ? ((tick >> 3) & 1 ? 1 : 3) : 2, dbmMasked); }
  ab.fillRoundRect(41, 1, 86, 53, 7, WHITE); ab.drawRoundRect(41, 1, 86, 53, 7, BLACK); ab.drawRoundRect(42, 2, 84, 51, 6, BLACK);
  heart(46, 5, BLACK); heart(114, 5, BLACK);                                        // hearts either side of SCORE
  { static const char T[] PROGMEM = "SCORE"; at(70, 5); pp(T); }
  { uint8_t n = 1; for (uint16_t t = sc; t >= 10; t /= 10) n++;                // the score, big
    char b[6]; uint8_t i = n; uint16_t v = sc; b[n] = 0; while (i) { b[--i] = '0' + v % 10; v /= 10; }
    for (i = 0; i < n; i++) bigc(84 - n * 6 + i * 12, 13, b[i], 2); }
  { static const char T[] PROGMEM = "HEIGHT"; at(48, 29); pp(T); } rightNum(120, 29, (uint32_t)bDist, 0, true);
  { static const char T[] PROGMEM = "JUMPS"; at(48, 37); pp(T); } rightNum(120, 37, timingPts, '+', false);
  if (newBest) { if (tick & 16) { static const char T[] PROGMEM = "NEW BEST!"; at(58, 45); pp(T); heart(48, 46, BLACK); heart(113, 46, BLACK); } }
  else { static const char T[] PROGMEM = "BEST"; at(48, 45); pp(T); rightNum(120, 45, best, 0, false); }
  { static const char T[] PROGMEM = "[MENU"; outlined(45, 55, T, -1); }
  { static const char T[] PROGMEM = "AGAIN]"; outlined(86, 55, T, -1); }
  if (tick > 40) {
    if (ab.justPressed(RIGHT_BUTTON) || ab.justPressed(A_BUTTON) || ab.justPressed(B_BUTTON)) startRound();   // right (or A / B): again
    if (ab.justPressed(LEFT_BUTTON)) { st = S_START; tick = 0; airIn = 0; relT = 0; }                          // left: back to the title
  }
}

// ---------------------------------------------------------------- sound on/off, then her how-to-play talk
void soundScreen() {
  ab.fillScreen(WHITE);
  for (uint8_t k = 0; k < 8; k++) heart(6 + k * 15, 3 + (k & 1) * 52, BLACK);        // a row of hearts top and bottom
  static const char T[] PROGMEM = "SOUND"; for (uint8_t i = 0; i < 5; i++) bigc(35 + i * 12, 10, pgm_read_byte(T + i), 2);
  bool on = ab.audio.enabled();
  static const char ON[] PROGMEM = "ON", OFF[] PROGMEM = "OFF";
  boldText(58, 31, ON); boldText(55, 42, OFF);
  heart(44, on ? 32 : 43, BLACK); heart(77, on ? 32 : 43, BLACK);                   // hearts either side of the choice
  if (ab.justPressed(UP_BUTTON) && !on) { ab.audio.on(); snd.tone(880, 60); }
  if (ab.justPressed(DOWN_BUTTON) && on) ab.audio.off();
  if (tick > 10 && (ab.justPressed(A_BUTTON) || ab.justPressed(B_BUTTON))) { ab.audio.saveOnOff(); st = S_TALK; tick = 0; talkPage = 0; talkN = 0; }   // either button goes on
}
// her talk: two pages in a speech bubble beside her portrait, typed out a letter at a time. A shows it all, then turns
// the page; after the second page the round starts.
const char TALK1[] PROGMEM = "HEY CUTIE!\nCAN YOU\nHELP ME\nPUMP UP MY\nBALLOON?\n*TIMING IS\nEVERYTHING!*";   // (*...* = bold)
const char TALK2[] PROGMEM = "HOLD { FOR\nA BIG JUMP,\nTAP { FOR\nA SMALL HOP\n\n*LET'S\nBOUNCE!*";
void talkScreen() {
  ab.fillScreen(WHITE);
  FX::drawBitmap(0, 0, FX_PORTRAIT, 0, dbmMasked);
  // a girly bubble: soft round corners, a double line, a curly tail to her mouth
  ab.fillRoundRect(52, 0, 75, 64, 6, WHITE); ab.drawRoundRect(52, 0, 75, 64, 6, BLACK); ab.drawRoundRect(53, 1, 73, 62, 5, BLACK);
  ab.fillTriangle(54, 27, 54, 35, 45, 33, WHITE); ab.drawLine(52, 27, 47, 31, BLACK); ab.drawLine(47, 31, 44, 33, BLACK);   // the tail, to her mouth
  ab.drawLine(44, 33, 48, 35, BLACK); ab.drawLine(48, 35, 52, 35, BLACK); ab.drawPixel(53, 27, WHITE); ab.drawPixel(53, 35, WHITE);

  PGM_P t = talkPage ? TALK2 : TALK1; uint8_t len = strlen_P(t);
  if (talkN < len && (tick & 1)) { talkN++; if ((talkN & 1) && pgm_read_byte(t + talkN - 1) != ' ') snd.tone(1100 + (talkN & 6) * 40, 8); }
  int16_t x = 57, y = 4; tw_ = false;
  bool bold = false;
  for (uint8_t i = 0; i < talkN; i++) { char c = pgm_read_byte(t + i);
    if (c == '*') bold = !bold; else if (c == '\n') { x = 57; y += 8; } else { if (bold) { at(x + 1, y); pc(c); } at(x, y); pc(c); x = tx_; } }
  if (talkN >= len && (tick & 16)) { static const char AB[] PROGMEM = "{"; at(x + 2, y); pp(AB); }   // the A button blinks after the last word: press it
  if (tick > 6 && (ab.justPressed(A_BUTTON) || ab.justPressed(B_BUTTON))) {
    if (talkN < len) talkN = len;
    else if (!talkPage) { talkPage = 1; talkN = 0; tick = 0; }
    else startRound();
  }
}

void setup() {
  ab.boot(); ab.display(); FX::begin(FX_DATA_PAGE); ab.flashlight(); ab.systemButtons(); ab.audio.begin(); ab.waitNoButtons();   // (no boot logo: the title is the splash)
  ab.setFrameRate(60);
  uint16_t raw = eeprom_read_word((uint16_t*)420); best = raw == 0xFFFF ? 0 : raw ^ 0xBA12; if (best > 50000) best = 0;   // fresh memory reads as 0
}
void loop() {
  if (!ab.nextFrame()) { watchButtons(); return; }
  frameMs = millis(); watchButtons(); ab.pollButtons(); tick++;
  switch (st) {
    case S_START: startScreen(); break;
    case S_PUMP: pumpScreen(); break;
    case S_FLY: flyScreen(); break;
    case S_SCORE: scoreScreen(); break;
    case S_SOUND: soundScreen(); break;
    case S_TALK: talkScreen(); break;
  }
  FX::display();
}
