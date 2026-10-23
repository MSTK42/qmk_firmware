// SPDX-License-Identifier: GPL-2.0-or-later
// Independent behavioral review of the production mato engine.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mato_engine.h"
#include "quantum_keycodes.h"

static mato_engine_t state;
static int failures;
static unsigned reports, presses;
static unsigned char held[256], press_codes[4096], press_mods[4096];
static unsigned mods(void) {
    unsigned m = 0;
    for (int i = 0; i < 8; i++) if (held[KC_LCTL + i]) m |= 1u << i;
    return m;
}
static void basic(unsigned code, int down) {
    if (code > 255 || code == KC_NO || code == KC_TRNS) return;
    held[code] = down;
    if (down && code < KC_LCTL && presses < 4096) {
        press_codes[presses] = code; press_mods[presses++] = (unsigned char)mods();
    }
}
static void output(uint16_t code, bool down) {
    reports++;
    // Simulate QMK register_code16/unregister_code16 semantics for encoded mods.
    unsigned m = (code >> 8) & 31u;
    if (down) {
        for (int i=0;i<4;i++) if (m & (1u << i)) basic(KC_LCTL + i + ((m&16)?4:0),1);
        basic(code & 255,1);
    } else {
        basic(code & 255,0);
        for (int i=0;i<4;i++) if (m & (1u << i)) basic(KC_LCTL + i + ((m&16)?4:0),0);
    }
}
static void fresh(void) {
    mato_init(&state, output); memset(held,0,sizeof held);
    reports=presses=0; memset(press_codes,0,sizeof press_codes); memset(press_mods,0,sizeof press_mods);
}
static void check(int yes,const char *label) { if (!yes) { failures++; printf("FAIL: %s\n",label); } }
static void event(int p,int down,int now) { mato_event(&state,(uint8_t)p,down,(uint16_t)now); }
static void all_up(int now) { for(int p=0;p<MATO_KEYS;p++) event(p,0,now+p); }
static void no_stuck(const char *label) { for(int k=2;k<256;k++) if(held[k]) {check(0,label); break;} }

int main(void) {
    fresh();
    event(23,1,0); event(2,1,10); event(23,0,20); event(12,1,30);
    event(12,0,40); event(2,0,45);
    check(presses==2 && press_codes[0]==KC_G && press_codes[1]==KC_T,
          "release MO1 during G then T rollover must preserve G,T order");

    fresh();
    event(0,1,0); event(10,1,10); event(2,1,20); event(0,0,30); event(2,0,40); event(10,0,45);
    check(presses==1 && press_codes[0]==KC_K && (press_mods[0]&2),
          "combo Shift released before pending K must still produce shifted K");
    no_stuck("combo Shift early release leaves no key or modifier held");

    fresh();
    event(23,1,0); event(20,1,10); event(2,1,20); event(20,0,30); event(2,0,40); event(23,0,45);
    check(presses==1 && press_codes[0]==KC_G && (press_mods[0]&2),
          "thumb Shift released before pending G must still produce shifted G");

    fresh();
    event(3,1,0); event(13,1,10); event(21,1,20); event(10,1,30); event(10,0,40);
    check(held[KC_LCTL],"shortcut Ctrl+Z release must preserve held Ctrl combo");
    all_up(50); no_stuck("shortcut with Ctrl combo leaves no held keys");

    fresh();
    event(21,1,0); event(10,1,10); event(21,0,20); event(2,1,30); event(10,0,40); event(2,0,45);
    check(presses==2 && press_codes[1]==KC_K && (press_mods[1]&1),
          "encoded Ctrl shortcut released before pending K must preserve Ctrl on K");
    no_stuck("encoded shortcut early release leaves no held reports");

    fresh();
    event(0,1,0); event(10,1,10); event(9,1,20); event(19,1,30); event(0,0,40);
    check(held[KC_LSFT],"one of two Shift combos released must preserve the other");
    all_up(50); no_stuck("both Shift combos released leave no modifiers");

    for(int delta=49;delta<=51;delta++) {
        fresh(); event(0,1,0); event(10,1,delta);
        check(!!held[KC_LSFT] == (delta<50),"50ms combo boundary must follow documented strict bound");
        all_up(delta+10); no_stuck("combo boundary leaves no keys held");
    }

    fresh(); event(23,1,0);
    int pos[]={12,16,18,14,7,6,10,6};
    int letters[]={KC_D,KC_A,KC_I,KC_J,KC_O,KC_U,KC_B,KC_U};
    for(int i=0;i<8;i++) {event(pos[i],1,10+i*20);event(pos[i],0,20+i*20);}
    event(23,0,180); check(presses==8,"held MO1 daijoubu has eight letters");
    for(int i=0;i<8;i++) check(press_codes[i]==letters[i],"held MO1 preserves vowel/consonant sequence");

    unsigned random=0x6142u;
    for(int round=0;round<200;round++) {
        fresh(); unsigned down=0; uint16_t time=(uint16_t)(65000+round*17);
        for(int n=0;n<500;n++) {
            random=random*1664525u+1013904223u;
            int p=(random>>16)%24;
            random=random*1664525u+1013904223u;
            time+=(random>>16)%71;
            down ^= 1u<<p; event(p,!!(down&(1u<<p)),time);
        }
        all_up(time+100); no_stuck("randomized key/layer/chord release leaves no held reports");
        mato_reset(&state); no_stuck("reset after randomized sequence leaves no held reports");
    }
    printf("Independent mato review: %d failure(s); 100000 randomized transitions checked.\n",failures);
    return failures?1:0;
}
