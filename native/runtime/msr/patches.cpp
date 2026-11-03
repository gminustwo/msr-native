// The amiibo extras on msr-native: src/msr/cheats.h's instruction patches, written at boot for the extras the player
// has on; then cheats.cpp keeps them following the settings each VBlank, as the emulator-based build does
// (video_core/gpu.cpp). With a randomizer the Fusion suit stays off (it has its own option).
#include <cstdio>
#include "msr.h"
#include "msr/cheats.h"

#include "cpu.h"

extern "C" void msr_apply_patches(void) {
    namespace C = MSR::Cheats;
    if (!C::randomizer.load() && (rd32(C::RandomizerHook) & 0xFF000000u) == 0xEB000000u) {   // its bl
        C::randomizer = true;
        std::fprintf(stderr, "msr: randomizer code found: the Fusion suit setting stays off\n");
    }
    const u32 enabled = C::enabled.load() & (C::randomizer.load() ? ~static_cast<u32>(C::FusionSuit) : ~0u);
    u32 bad = 0;
    for (const auto& p : C::Patches) {
        if ((enabled & p.flag) && rd32(p.address) != p.original) {
            std::fprintf(stderr, "msr: patch at %08x does not fit this program (%08x)\n", p.address, rd32(p.address));
            bad |= p.flag;
        }
    }
    u32 written = 0;
    for (const auto& p : C::Patches) {
        if ((enabled & p.flag) && !(bad & p.flag)) {
            wr32(p.address, p.replacement);
            written++;
        }
    }
    std::fprintf(stderr, "msr: extras %s%s%s: %u instructions rewritten\n",
                 (enabled & ~bad & C::Galleries) ? "[galleries]" : "",
                 (enabled & ~bad & C::FusionMode) ? "[Fusion Mode]" : "",
                 (enabled & ~bad & C::FusionSuit) ? "[Fusion suit]" : "", written);
}
