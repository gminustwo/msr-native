// The controller's buttons where the game and the port name them (msr-native): the player's device, from the last input
// (video.c), picks the game's face-button icons (src/msr/button_glyphs.h, drawn into the game's two icon textures as
// pica_vk.c uploads them, a texture pack's too) and the control reminders' names (overlay.cpp). An Xbox pad (also
// Steam's virtual pads and other pads SDL cannot name) shows the game's letter icons as Xbox labels its buttons, a
// PlayStation pad its symbols, a Nintendo pad the game's own icons; the keyboard the letter or digit of the key bound
// to each (the space bar's sign for Space, the game's icon where the key's name is longer) and in the L and R icons the
// letters of their keys (Q and E), and the reminders name its keys (the circle pad's and touch navigation's from the
// bindings too). MSR_BUTTON_ICONS=xbox|playstation|nintendo|keyboard forces one (tests).
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <SDL.h>
#include "msr.h"
#include "common/cityhash.h"
#include "msr/button_glyphs.h"
extern "C" {
#include "input.h"
}

namespace {

namespace G = MSR::ButtonGlyphs;

std::atomic<unsigned> version{1};
u32 device_type = SDL_CONTROLLER_TYPE_UNKNOWN;   // the pad's, when the device is a pad

u32 DeviceOf(int sdl_type) {
    switch (sdl_type) {
    case SDL_CONTROLLER_TYPE_PS3: case SDL_CONTROLLER_TYPE_PS4: case SDL_CONTROLLER_TYPE_PS5: return G::PlayStation;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return G::Nintendo;
    default: return G::Xbox;   // Xbox, Steam's virtual pads (SDL names the pad behind them when it can), others
    }
}

// a pad button's label on the device in use (SDL maps buttons by position: A south, B east, X west, Y north)
const char* Label(int button) {
    const u32 dev = G::device.load(std::memory_order_relaxed);
    static const char* const xbox[] = {"A", "B", "X", "Y", "View", "Guide", "Menu", "LS", "RS", "LB", "RB", "D-pad up", "D-pad down", "D-pad left", "D-pad right"};
    static const char* const ps[] = {"Cross", "Circle", "Square", "Triangle", "Create", "PS", "Options", "L3", "R3", "L1", "R1", "D-pad up", "D-pad down", "D-pad left", "D-pad right"};
    static const char* const nin[] = {"B", "A", "Y", "X", "-", "Home", "+", "LS", "RS", "L", "R", "D-pad up", "D-pad down", "D-pad left", "D-pad right"};
    if (button < 0 || button > SDL_CONTROLLER_BUTTON_DPAD_RIGHT) return "";
    return dev == G::PlayStation ? ps[button] : dev == G::Nintendo ? nin[button] : xbox[button];
}

const char* PadName(u32 control) {
    static const char* const triggers[3][2] = {{"LT", "RT"}, {"L2", "R2"}, {"ZL", "ZR"}};
    const u32 dev = G::device.load(std::memory_order_relaxed);
    if (control == G::CZL || control == G::CZR) {
        const int b = input_pad_binds()[control];
        if (b >= 0) return Label(b);
        return triggers[dev == G::PlayStation ? 1 : dev == G::Nintendo ? 2 : 0][control == G::CZR];
    }
    const int b = control < C_COUNT ? input_pad_binds()[control] : -1;
    return b >= 0 ? Label(b) : "";
}

const char* RoleName(u32 role) {
    switch (role) {
    case G::PadSouth: return Label(SDL_CONTROLLER_BUTTON_A);
    case G::PadEast: return Label(SDL_CONTROLLER_BUTTON_B);
    case G::PadWest: return Label(SDL_CONTROLLER_BUTTON_X);
    case G::PadNorth: return Label(SDL_CONTROLLER_BUTTON_Y);
    case G::PadL1: return Label(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    case G::PadR1: return Label(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    case G::PadL3: return Label(SDL_CONTROLLER_BUTTON_LEFTSTICK);
    case G::PadBack: return Label(SDL_CONTROLLER_BUTTON_BACK);
    case G::PadStick: return "Left stick";
    case G::PadDpad: return "D-pad";
    default: return "";
    }
}

std::string BoundKey(int control) {
    const int sym = control >= 0 && control < C_COUNT ? input_key_binds()[control] : -1;
    if (sym < 0) return "";
    std::string name = SDL_GetKeyName(static_cast<SDL_Keycode>(sym));
    if (name == "Return") name = "Enter";
    else if (name == "Escape") name = "Esc";
    return name;
}

// a control's key, or one of the keyboard's own roles (G::KeyStick ...: the circle pad's keys, touch navigation's)
const char* KeyName(u32 control) {
    static thread_local std::string name;
    const auto either = [](const std::string& a, const char* b) { return a.empty() ? std::string(b) : a + " / " + b; };
    switch (control) {
    case G::KeyStick: case G::KeyMove: {
        // the circle pad's four keys: "WASD" when each is a letter, else their names
        const std::string up = BoundKey(C_CUP), left = BoundKey(C_CLEFT), down = BoundKey(C_CDOWN), right = BoundKey(C_CRIGHT);
        if (up.size() == 1 && left.size() == 1 && down.size() == 1 && right.size() == 1) name = up + left + down + right;
        else if (!up.empty() || !left.empty() || !down.empty() || !right.empty()) name = up + " " + left + " " + down + " " + right;
        else name = "";
        name = control == G::KeyMove || name.empty() ? either(name, "arrows") : name;
        break;
    }
    case G::KeyTouch: name = "T"; break;
    case G::KeyTap: name = either(BoundKey(C_A), "Enter"); break;
    case G::KeyLeave: name = either(BoundKey(C_B), "Esc"); break;
    default: name = BoundKey(control < C_COUNT ? static_cast<int>(control) : -1); break;
    }
    return name.c_str();
}

// the icons and symbols for the device and the bindings: each face shows the button it is bound to
void Apply(u32 dev) {
    u32 shown = G::Identity, symbols = 0, letters = 0, shoulders = 0;
    if (dev == G::Xbox || dev == G::Nintendo || dev == G::PlayStation) {
        static const u32 nintendo_letter[4] = {G::B, G::A, G::Y, G::X};   // south, east, west, north
        static const u32 ps_symbol[4] = {G::Cross, G::Circle, G::Square, G::Triangle};
        for (u32 face = 0; face < G::FaceCount; ++face) {
            const int b = input_pad_binds()[face];
            if (b < SDL_CONTROLLER_BUTTON_A || b > SDL_CONTROLLER_BUTTON_Y) continue;   // not a face button: its own icon
            const u32 show = dev == G::Xbox ? static_cast<u32>(b) : dev == G::Nintendo ? nintendo_letter[b] : face;
            shown = (shown & ~(3u << (face * 2))) | show << (face * 2);
            if (dev == G::PlayStation) symbols |= ps_symbol[b] << (face * 3);
        }
    }
    if (dev == G::Keyboard) {
        for (u32 face = 0; face < G::FaceCount; ++face) {
            const int sym = input_key_binds()[face];
            const char* name = sym >= 0 ? SDL_GetKeyName(static_cast<SDL_Keycode>(sym)) : "";
            if (name && name[0] && !name[1] && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= '0' && name[0] <= '9')))
                letters |= static_cast<u32>(name[0]) << (face * 8);
            else if (sym == SDLK_SPACE)
                letters |= static_cast<u32>(' ') << (face * 8);   // the space bar's sign
        }
        for (u32 i = 0; i < G::ShoulderCount; ++i) {   // the L and R icons: the keys L and R are bound to (Q and E)
            const int sym = input_key_binds()[C_L + static_cast<int>(i)];
            const char* name = sym >= 0 ? SDL_GetKeyName(static_cast<SDL_Keycode>(sym)) : "";
            if (name && name[0] && !name[1] && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= '0' && name[0] <= '9')))
                shoulders |= static_cast<u32>(name[0]) << (i * 8);
        }
    }
    const bool changed = G::device.exchange(dev) != dev | G::shown.exchange(shown) != shown | G::symbols.exchange(symbols) != symbols |
                         G::letters.exchange(letters) != letters | G::shoulder_letters.exchange(shoulders) != shoulders;
    if (changed) {
        version.fetch_add(1);
        static const char* const names[] = {"unknown", "keyboard", "Xbox", "PlayStation", "Nintendo"};
        std::fprintf(stderr, "msr: buttons: %s\n", names[dev]);
    }
}

u32 Forced() {
    static int forced = -1;
    if (forced < 0) {
        const char* e = std::getenv("MSR_BUTTON_ICONS");
        forced = !e ? 0 : !std::strcmp(e, "xbox") ? G::Xbox : !std::strcmp(e, "playstation") || !std::strcmp(e, "ps") ? G::PlayStation
                 : !std::strcmp(e, "nintendo") ? G::Nintendo : !std::strcmp(e, "keyboard") ? G::Keyboard : 0;
    }
    return static_cast<u32>(forced);
}

} // namespace

// video.c: the input just used. pad = 0: the keyboard or mouse, else a pad of SDL type sdl_type.
extern "C" void msr_input_device(int pad, int sdl_type) {
    static bool named;
    if (!named) {
        named = true;
        G::key_name.store(KeyName); G::pad_name.store(PadName); G::role_name.store(RoleName);
    }
    if (const u32 f = Forced()) { Apply(f); return; }
    if (pad) device_type = static_cast<u32>(sdl_type);
    Apply(pad ? DeviceOf(sdl_type) : G::Keyboard);
}
// video.c: a pad was used last (the mouse pointer hides in play)
extern "C" int msr_input_pad_active(void) {
    const u32 dev = G::device.load(std::memory_order_relaxed);
    return dev == G::Xbox || dev == G::PlayStation || dev == G::Nintendo;
}
// the bindings changed (the settings' Controls): the icons follow
extern "C" void msr_input_bindings_changed(void) {
    const u32 dev = G::device.load();
    if (dev != G::Unknown) { G::shown.store(~0u); Apply(dev); }
}

// pica_vk.c: whether a texture is one of the game's two icon sheets (1 or 2), 0 if not; by its size, format and the
// CityHash64 of its bytes (only computed for the two sizes)
extern "C" int msr_glyph_sheet(const uint8_t* level0, uint32_t w, uint32_t h, int fmt) {
    static const int bits[16] = {32, 24, 16, 16, 16, 16, 16, 8, 8, 8, 4, 4, 4, 8, 0, 0};
    for (std::size_t i = 0; i < G::Sheets.size(); ++i) {
        const auto& sheet = G::Sheets[i];
        if (w != sheet.width || h != sheet.height || static_cast<u32>(fmt) != sheet.format) continue;
        if (Common::CityHash64(reinterpret_cast<const char*>(level0), std::size_t{w} * h * bits[fmt & 15] / 8) == sheet.hash) return static_cast<int>(i) + 1;
    }
    return 0;
}
// the face cells of an upload of sheet (1 or 2), RGBA rows bottom first, the game's size or a whole multiple (a pack)
extern "C" void msr_glyph_patch(int sheet, uint8_t* rgba, uint32_t w, uint32_t h) {
    if (sheet < 1 || sheet > static_cast<int>(G::Sheets.size())) return;
    const auto& s = G::Sheets[static_cast<std::size_t>(sheet - 1)];
    const G::Image image{rgba, w, h, 4};
    const u32 shown = G::shown.load(), symbols = G::symbols.load(), letters = G::letters.load(), shoulders = G::shoulder_letters.load();
    if (!G::Fits(s, image) || (shown == G::Identity && !symbols && !letters && !shoulders)) return;
    const G::Icons icons = G::Read(s, image);
    G::Write(s, image, icons, shown);
    G::WriteSymbols(s, image, icons, symbols, letters);
    G::WriteShoulders(s, image, shoulders);
}
extern "C" unsigned msr_glyph_version(void) { return version.load(std::memory_order_relaxed); }
