#include "drivers/ps2_keymap.h"

static const char normal_map[128] = {
    [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',
    [0x0C]='-',[0x0D]='=',[0x0E]='\b',[0x0F]='\t',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',
    [0x1A]='[',[0x1B]=']',[0x1C]='\n',[0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',
    [0x27]=';',[0x28]='\'',[0x29]='`',[0x2B]='\\',[0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',
    [0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' '
};
static const char shifted_map[128] = {
    [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',
    [0x0C]='_',[0x0D]='+',[0x0E]='\b',[0x0F]='\t',[0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',[0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',
    [0x1A]='{',[0x1B]='}',[0x1C]='\n',[0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',[0x24]='J',[0x25]='K',[0x26]='L',
    [0x27]=':',[0x28]='"',[0x29]='~',[0x2B]='|',[0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',[0x31]='N',[0x32]='M',
    [0x33]='<',[0x34]='>',[0x35]='?',[0x39]=' '
};

void fluxmax_ps2_keymap_reset(fluxmax_ps2_keymap_state_t *state) {
    if (!state) return;
    state->left_shift = state->right_shift = state->caps_lock = state->caps_down = 0;
    state->extended_prefix = state->pause_bytes = 0;
}

int fluxmax_ps2_keymap_feed(fluxmax_ps2_keymap_state_t *state, uint8_t sc, char *out) {
    if (!state || !out) return 0;
    if (state->pause_bytes) { --state->pause_bytes; return 0; }
    if (sc == 0xE1u) { state->extended_prefix = 0; state->pause_bytes = 5u; return 0; }
    if (sc == 0xE0u) { state->extended_prefix = 1u; return 0; }
    if (state->extended_prefix) { state->extended_prefix = 0; return 0; }

    if (sc == 0x2Au) { state->left_shift = 1; return 0; }
    if (sc == 0xAAu) { state->left_shift = 0; return 0; }
    if (sc == 0x36u) { state->right_shift = 1; return 0; }
    if (sc == 0xB6u) { state->right_shift = 0; return 0; }
    if (sc == 0x3Au) {
        if (!state->caps_down) state->caps_lock ^= 1u;
        state->caps_down = 1u;
        return 0;
    }
    if (sc == 0xBAu) { state->caps_down = 0; return 0; }
    if (sc & 0x80u || sc >= 128u) return 0;

    int shift = state->left_shift || state->right_shift;
    char c = normal_map[sc];
    if (c >= 'a' && c <= 'z') {
        if ((shift != 0) ^ (state->caps_lock != 0)) c = (char)(c - 'a' + 'A');
    } else if (shift && shifted_map[sc]) {
        c = shifted_map[sc];
    }
    if (!c) return 0;
    *out = c;
    return 1;
}
