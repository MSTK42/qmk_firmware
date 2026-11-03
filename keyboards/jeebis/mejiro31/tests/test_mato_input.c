// SPDX-License-Identifier: GPL-2.0-or-later
#include "../mejiro_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { MO1 = 16, MO2, MO3, MO4, SPACE_SHIFT, EXTRA, SHIFT, SHORTCUT, PHYSICAL_KEYS };
enum { SFT = 2, CTL = 1, ALT = 4, GUI = 8, SPACE = 44, ENTER = 40, ZKHK = 53, EXTRA_KEY = 240 };
#define PLAIN(code) ((mji_binding_t){.tap = (code), .hold_layer = MJI_NO_LAYER})
#define MOD(code, mods) ((mji_binding_t){.tap = (code), .hold_mods = (mods), .hold_layer = MJI_NO_LAYER})
#define LAYER(code, layer) ((mji_binding_t){.tap = (code), .hold_layer = (layer)})

static const mji_pair_t pairs[] = {
    {0, 1, 48, 0}, {2, 3, 49, 0}, {4, 5, 50, 0}, {6, 7, 51, 0},
    {8, 9, 52, 0}, {10, 11, 53, 0}, {12, 13, 54, 0}, {14, 15, 55, 0},
};
static const uint8_t combo_mods[] = {SFT, GUI, ALT, CTL, CTL, ALT, GUI, SFT};
typedef struct { uint16_t code; bool down; uint8_t mods; } report_t;
typedef struct {
    mji_engine_t engine;
    mji_binding_t maps[6][64];
    bool defined[6][64], held[256];
    report_t reports[4096];
    unsigned count;
    uint8_t mods;
} fixture_t;
static fixture_t f;

static mji_binding_t lookup(void *user, uint16_t logical, const mji_context_t *context) {
    fixture_t *s = user;
    assert(logical < 64);
    for (uint8_t i = 0; i < context->layer_count; ++i) {
        uint8_t layer = context->layers[i];
        assert(layer < 6);
        if (s->defined[layer][logical]) {
            mji_binding_t result = s->maps[layer][logical];
            result.layer = layer;
            return result;
        }
    }
    return PLAIN(0);
}
static void key(void *user, uint8_t source, uint16_t code, bool down, uint8_t mods) {
    (void)source;
    fixture_t *s = user;
    assert(code < 256);
    assert(s->count < sizeof(s->reports) / sizeof(*s->reports));
    s->reports[s->count++] = (report_t){code, down, mods};
    s->held[code] = down;
}
static void mods(void *user, uint8_t value) { ((fixture_t *)user)->mods = value; }
static void clear(void *user) {
    fixture_t *s = user;
    memset(s->held, 0, sizeof(s->held));
    s->mods = 0;
}
static bool decode(void *user, uint16_t code, mji_key_t *result) {
    (void)user;
    if (code <= 255) {
        *result = (mji_key_t){code, 0};
        return true;
    }
    if (code <= 0x1fff) {
        *result = (mji_key_t){code & 255, (code >> 8) & 15};
        return true;
    }
    return false;
}
static void bind(uint8_t layer, uint8_t logical, mji_binding_t value) {
    f.defined[layer][logical] = true;
    f.maps[layer][logical] = value;
}
static uint16_t letter(uint8_t layer, uint8_t physical) { return 100 + layer * 20 + physical; }
static void setup(bool mato) {
    memset(&f, 0, sizeof(f));
    mji_callbacks_t callbacks = {.lookup = lookup, .key = key, .mods = mods, .clear = clear, .decode = decode};
    assert(mji_init(&f.engine, pairs, sizeof(pairs) / sizeof(*pairs), (mji_timing_t){50, 15, 175}, 0, callbacks, &f));
    assert(!f.engine.mato_mode);
    f.engine.mato_mode = mato;
    for (uint8_t layer = 0; layer < 6; ++layer) {
        for (uint8_t physical = 0; physical < 16; ++physical)
            bind(layer, physical, PLAIN(letter(layer, physical)));
        for (uint8_t pair = 0; pair < 8; ++pair)
            bind(layer, 48 + pair, MOD(0, layer <= 1 ? combo_mods[pair] : 0));
    }
    bind(0, MO1, LAYER(0, 1));
    bind(0, MO2, LAYER(ZKHK, 2));
    bind(0, MO3, LAYER(SPACE, 3));
    bind(0, MO4, LAYER(ENTER, 4));
    bind(0, SPACE_SHIFT, MOD(SPACE, SFT));
    bind(0, EXTRA, PLAIN(EXTRA_KEY));
    bind(0, SHIFT, MOD(0, SFT));
    bind(0, SHORTCUT, PLAIN(0x0106)); // Ctrl+C, including a held Ctrl combo.
}
static void at(uint32_t now, uint8_t physical, bool down) { mji_event(&f.engine, physical, down, now); }
static void tick(uint32_t now) { mji_tick(&f.engine, now); }
static void expect(unsigned index, uint16_t code, bool down, uint8_t mods_value) {
    if (index >= f.count || f.reports[index].code != code || f.reports[index].down != down || f.reports[index].mods != mods_value) {
        fprintf(stderr, "report %u expected %u/%d/%u, count=%u", index, code, down, mods_value, f.count);
        if (index < f.count) fprintf(stderr, " actual=%u/%d/%u", f.reports[index].code, f.reports[index].down, f.reports[index].mods);
        fputc('\n', stderr);
        assert(false);
    }
}
static void no_stuck(void) {
    assert(!f.engine.physical && !f.engine.ignored && !f.engine.event_count && !f.mods);
    for (unsigned i = 0; i < 256; ++i) assert(!f.held[i]);
    for (unsigned i = 0; i < MJI_PHYSICAL_COUNT; ++i) assert(!f.engine.owner[i]);
    for (unsigned i = 0; i < MJI_TOKEN_COUNT; ++i)
        assert(f.engine.tokens[i].state != MJI_ACTIVE && f.engine.tokens[i].state != MJI_PREPARED);
}
static void test_combo_boundaries(void) {
    for (uint32_t delta = 49; delta <= 51; ++delta) {
        setup(true);
        at(0, 0, true);
        at(delta, 1, true);
        if (delta == 49) {
            assert(f.mods == SFT && !f.count);
            at(delta + 1, 0, false);
            assert(!f.mods);
            tick(1000);
            assert(!f.count); // The still-held partner is consumed.
            at(1001, 1, false);
        } else {
            assert(!f.mods);
            tick(delta + 50);
            expect(0, letter(0, 0), true, 0);
            expect(1, letter(0, 1), true, 0);
            at(delta + 51, 0, false);
            at(delta + 52, 1, false);
            assert(f.count == 4);
        }
        no_stuck();
    }
}
static void test_all_eight_modifiers(void) {
    for (uint8_t layer = 0; layer <= 1; ++layer) {
        for (uint8_t pair = 0; pair < 8; ++pair) {
            setup(true);
            if (layer) at(0, MO1, true);
            at(1, pair * 2, true);
            at(2, pair * 2 + 1, true);
            assert(f.mods == combo_mods[pair]);
            at(3, EXTRA, true);
            expect(0, EXTRA_KEY, true, combo_mods[pair]);
            at(4, EXTRA, false);
            at(5, pair * 2 + 1, false);
            assert(!f.mods);
            tick(500);
            at(501, pair * 2, false);
            if (layer) at(502, MO1, false);
            assert(f.count == 2);
            no_stuck();
        }
    }
}
static void test_modifier_release_flushes_pending_letter(void) {
    setup(true);
    at(0, 0, true);
    at(1, 1, true);
    at(2, 2, true);
    assert(!f.count);
    at(3, 0, false);
    expect(0, letter(0, 2), true, SFT);
    assert(!f.mods);
    at(4, 2, false);
    tick(1000);
    at(1001, 1, false);
    assert(f.count == 2);
    no_stuck();
}
static void test_consumed_partner_and_repress(void) {
    setup(true);
    at(0, 0, true);
    at(1, 1, true);
    at(2, 0, false);
    at(3, 0, true);
    tick(53);
    expect(0, letter(0, 0), true, 0);
    assert(!f.mods);
    at(54, 0, false);
    at(55, 1, false);
    assert(f.count == 2);
    no_stuck();
}
static void test_successor_commits_in_order(void) {
    setup(true);
    at(0, 0, true);
    at(5, EXTRA, true);
    expect(0, letter(0, 0), true, 0);
    expect(1, EXTRA_KEY, true, 0);
    at(10, 1, true); // An intervening key prevents the old pending combo.
    tick(60);
    expect(2, letter(0, 1), true, 0);
    assert(!f.mods);
    at(61, 0, false);
    at(62, 1, false);
    at(63, EXTRA, false);
    no_stuck();
}
static void test_late_shift_does_not_change_earlier_letter(void) {
    setup(true);
    at(0, 0, true);
    at(1, SHIFT, true);
    expect(0, letter(0, 0), true, 0);
    at(2, EXTRA, true);
    expect(1, EXTRA_KEY, true, SFT);
    at(3, EXTRA, false);
    at(4, SHIFT, false);
    at(5, 0, false);
    no_stuck();
}
static void test_pressed_layer_is_preserved(void) {
    setup(true);
    at(0, MO1, true);
    at(1, 0, true);
    at(2, MO1, false);
    at(3, 1, true);
    expect(0, letter(1, 0), true, 0);
    tick(53);
    expect(1, letter(0, 1), true, 0);
    assert(!f.mods);
    at(54, 0, false);
    at(55, 1, false);
    no_stuck();

    setup(true);
    at(0, 0, true);
    at(1, MO1, true);
    expect(0, letter(0, 0), true, 0);
    at(2, 1, true);
    at(3, MO1, false);
    tick(52);
    expect(1, letter(1, 1), true, 0);
    at(53, 0, false);
    at(54, 1, false);
    no_stuck();
}
static void test_numbered_layer_precedence(void) {
    for (unsigned reverse = 0; reverse < 2; ++reverse) {
        setup(true);
        at(0, reverse ? MO2 : MO4, true);
        at(1, reverse ? MO4 : MO2, true);
        at(2, 0, true);
        expect(0, letter(4, 0), true, 0);
        at(3, 0, false);
        at(4, MO4, false);
        at(5, 1, true);
        expect(2, letter(2, 1), true, 0);
        at(6, 1, false);
        at(7, MO2, false);
        assert(f.count == 4); // Both thumbs were used as Hold, never as Tap.
        no_stuck();
    }
}
static void test_dual_role_timing(void) {
    setup(true);
    at(0, SPACE_SHIFT, true);
    tick(174);
    assert(!f.mods && !f.count);
    at(174, SPACE_SHIFT, false);
    expect(0, SPACE, true, 0);
    expect(1, SPACE, false, 0);
    no_stuck();

    setup(true);
    at(0, SPACE_SHIFT, true);
    tick(175);
    assert(f.mods == SFT);
    at(176, SPACE_SHIFT, false);
    assert(!f.count);
    no_stuck();

    setup(true);
    at(0, SPACE_SHIFT, true);
    at(10, 2, true);
    assert(f.mods == SFT && !f.count);
    at(11, SPACE_SHIFT, false);
    expect(0, letter(0, 2), true, SFT);
    at(12, 2, false);
    assert(f.count == 2);
    no_stuck();

    setup(true);
    at(0, EXTRA, true);
    at(10, SPACE_SHIFT, true);
    tick(170);
    assert(!f.mods); // An older held key does not promote the new thumb.
    at(180, SPACE_SHIFT, false);
    expect(1, SPACE, true, 0);
    at(181, EXTRA, false);
    no_stuck();
}
static void test_duplicate_modifiers_and_shortcut(void) {
    setup(true);
    at(0, 0, true);
    at(1, 1, true);
    at(2, 14, true);
    at(3, 15, true);
    at(4, 0, false);
    assert(f.mods == SFT);
    at(5, 14, false);
    assert(!f.mods);
    at(6, 1, false);
    at(7, 15, false);
    no_stuck();

    setup(true);
    at(0, 6, true);
    at(1, 7, true);
    at(2, SHORTCUT, true);
    expect(0, 6, true, CTL);
    at(3, SHORTCUT, false);
    assert(f.mods == CTL);
    at(4, 6, false);
    at(5, 7, false);
    no_stuck();
}
static void test_reset_and_false_policy(void) {
    setup(true);
    at(0, 0, true);
    at(1, 1, true);
    at(2, 2, true);
    mji_reset(&f.engine, 5);
    assert(f.engine.mato_mode && !f.mods && !f.count);
    at(3, 0, false);
    at(4, 1, false);
    at(5, 2, false);
    no_stuck();
    mji_reset(&f.engine, 0);
    at(6, 0, true);
    at(7, 1, true);
    assert(f.mods == SFT);
    at(8, 0, false);
    at(9, 1, false);
    no_stuck();

    setup(false);
    at(0, 0, true);
    tick(50);
    at(100, 1, true); // Existing compression still accepts a late partner.
    expect(0, letter(0, 0), true, 0);
    expect(1, letter(0, 0), false, 0);
    assert(f.mods == SFT);
    at(101, 0, false);
    tick(116); // Existing continuation still emits the remaining key.
    expect(2, letter(0, 1), true, 0);
    at(117, 1, false);
    no_stuck();

    setup(false);
    at(0, MO4, true);
    at(1, MO2, true);
    at(2, 0, true);
    expect(0, letter(2, 0), true, 0); // Existing newest-Hold precedence.
    at(3, 0, false);
    at(4, MO2, false);
    at(5, MO4, false);
    no_stuck();
}
static uint32_t random_state = 0x7140a23u;
static uint32_t next_random(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
static void test_random_release_and_reset(void) {
    setup(true);
    uint32_t now = 0;
    for (unsigned batch = 0; batch < 500; ++batch) {
        f.count = 0;
        for (unsigned step = 0; step < 200; ++step) {
            uint8_t p = next_random() % PHYSICAL_KEYS;
            now += next_random() % 55;
            at(now, p, !(f.engine.physical & (UINT64_C(1) << p)));
            if (next_random() % 97 == 0) mji_reset(&f.engine, 0);
        }
        for (uint8_t p = 0; p < PHYSICAL_KEYS; ++p) at(++now, p, false);
        tick(now += 200);
        no_stuck();
        assert(f.engine.mato_mode);
    }
}
int main(void) {
    test_combo_boundaries();
    test_all_eight_modifiers();
    test_modifier_release_flushes_pending_letter();
    test_consumed_partner_and_repress();
    test_successor_commits_in_order();
    test_late_shift_does_not_change_earlier_letter();
    test_pressed_layer_is_preserved();
    test_numbered_layer_precedence();
    test_dual_role_timing();
    test_duplicate_modifiers_and_shortcut();
    test_reset_and_false_policy();
    test_random_release_and_reset();
    puts("mato input tests passed (100000 random transitions)");
    return 0;
}
