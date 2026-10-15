#include "drivers/ps2_keymap.h"
#include <stdio.h>

static unsigned passed, failed;
#define CHECK(name, expr) do { \
    if (expr) { ++passed; printf("[PASS] %s\n", name); } \
    else { ++failed; printf("[FAIL] %s\n", name); } \
} while (0)

static int key(fluxmax_ps2_keymap_state_t *state, unsigned scan, char expected) {
    char out = 0;
    int got = fluxmax_ps2_keymap_feed(state, (uint8_t)scan, &out);
    return expected ? (got == 1 && out == expected) : (got == 0);
}

int main(void) {
    fluxmax_ps2_keymap_state_t s;
    fluxmax_ps2_keymap_reset(&s);
    CHECK("normal letter maps to lowercase", key(&s, 0x1E, 'a'));
    CHECK("key release does not emit a character", key(&s, 0x9E, 0));
    CHECK("Enter maps to newline", key(&s, 0x1C, '\n'));
    CHECK("Backspace is preserved", key(&s, 0x0E, '\b'));

    fluxmax_ps2_keymap_reset(&s);
    key(&s, 0x2A, 0);
    CHECK("left Shift uppercases letters", key(&s, 0x1E, 'A'));
    key(&s, 0xAA, 0);
    CHECK("left Shift release restores lowercase", key(&s, 0x1E, 'a'));

    fluxmax_ps2_keymap_reset(&s);
    key(&s, 0x2A, 0); key(&s, 0x36, 0); key(&s, 0xAA, 0);
    CHECK("right Shift remains active when left Shift is released", key(&s, 0x1E, 'A'));
    key(&s, 0xB6, 0);
    CHECK("both Shift releases restore lowercase", key(&s, 0x1E, 'a'));

    fluxmax_ps2_keymap_reset(&s);
    key(&s, 0x3A, 0); key(&s, 0x3A, 0);
    CHECK("held Caps Lock toggles only once", s.caps_lock == 1);
    CHECK("Caps Lock capitalizes letters", key(&s, 0x1E, 'A'));
    CHECK("Caps Lock does not alter number row", key(&s, 0x02, '1'));
    key(&s, 0xBA, 0); key(&s, 0x2A, 0);
    CHECK("Caps Lock plus Shift produces lowercase", key(&s, 0x1E, 'a'));
    key(&s, 0xAA, 0); key(&s, 0xBA, 0);

    fluxmax_ps2_keymap_reset(&s);
    key(&s, 0xE0, 0);
    CHECK("extended navigation scan code is ignored", key(&s, 0x48, 0));
    CHECK("extended prefix does not corrupt next normal key", key(&s, 0x1E, 'a'));
    key(&s, 0xE1, 0);
    CHECK("Pause/Break sequence bytes are ignored", key(&s, 0x1D, 0) && key(&s, 0x45, 0) &&
          key(&s, 0xE1, 0) && key(&s, 0x9D, 0) && key(&s, 0xC5, 0));

    fluxmax_ps2_keymap_reset(&s);
    key(&s, 0x2A, 0);
    CHECK("Shifted number row maps punctuation", key(&s, 0x02, '!'));
    CHECK("null state is rejected", fluxmax_ps2_keymap_feed(0, 0x1E, &(char){0}) == 0);
    CHECK("null output is rejected", fluxmax_ps2_keymap_feed(&s, 0x1E, 0) == 0);
    CHECK("null reset is safe", (fluxmax_ps2_keymap_reset(0), 1));

    printf("PS/2 keyboard keymap tests: %u passed, %u failed.\n", passed, failed);
    return failed ? 1 : 0;
}
