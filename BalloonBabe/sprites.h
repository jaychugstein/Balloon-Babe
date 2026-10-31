// Balloon Babe: stand, crouch, rise, top, fall, land (drawPlusMask); her chest in 5 bounce positions (-2..+2)
constexpr uint8_t BABE_W = 35, BABE_H = 50;
const int8_t CHEST_AT[6][2] PROGMEM = { {8, 21}, {8, 27}, {8, 21}, {8, 21}, {8, 21}, {8, 27} };   // where the chest sprite sits on each pose
