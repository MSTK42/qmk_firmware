// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mato_engine.h"
#include "quantum_keycodes.h"

#define MATRIX_ROWS 4
#define MATRIX_COLS 12
enum { _GEMINI, _QWERTY, _QWERTY_SHIFT, _O24_NUMBER, _O24_NAV, _O24_SYSTEM };
enum { MJR_EDGE_TOP=0x7000, MJR_EDGE_BOTTOM, STN_DR, STN_ZR, STN_TEST };
typedef unsigned long layer_state_t;
typedef struct { struct { struct { uint8_t row, col; } key; bool pressed; } event; } keyrecord_t;
typedef struct { const int *mappings; uint8_t count; } alt_layout_def_t;
static const int o24[] = {1};
#define ALT_LAYOUT(name) {name, sizeof(name)/sizeof(name[0])}
static alt_layout_def_t alt_en_layout, alt_jp_layout;
static uint16_t physical[MATRIX_ROWS], now;
static uint8_t held[256], weak_mods, oneshot_mods, combo_fifo_len;
static uint8_t last_lang;
static unsigned presses, releases, legacy_calls, scan_calls, steno_calls, failures;
static uint16_t default_layer;
static layer_state_t layer_state, default_layer_state;
static int kbd_lang=1, stn_lang=3;
static bool is_mejiro_mode=true;
static bool force_qwerty_active=false;
typedef struct { bool top_pressed, bottom_pressed, other_pressed; uint8_t other_down_count; } edge_toggle_state_t;
static edge_toggle_state_t o24_edge_toggle_state, steno_edge_toggle_state;
static uint16_t matrix_get_row(uint8_t r) {return physical[r];}
static uint16_t timer_read(void) {return now;}
static uint8_t get_mods(void) {uint8_t m=0;for(int i=0;i<8;i++)if(held[KC_LCTL+i])m|=1<<i;return m;}
static uint8_t get_weak_mods(void) {return weak_mods;}
static uint8_t get_oneshot_mods(void) {return oneshot_mods;}
static uint8_t get_highest_layer(layer_state_t s) {uint8_t i=0;while(s>>=1)i++;return i;}
static void register_code16(uint16_t c) {if(c<256){held[c]=1;if(c<KC_LCTL)presses++;}}
static void unregister_code16(uint16_t c) {if(c<256){held[c]=0;if(c<KC_LCTL)releases++;}}
static void update_lang(uint8_t lang) {last_lang=lang;}
static void default_layer_set(layer_state_t state) {default_layer_state=state;}
static void layer_move(uint8_t l) {layer_state=1ul<<l;}
static void mejiro_reset_state(void) {}
#include "legacy_edges.inc"
static void legacy_keyboard_post_init_user(void) {default_layer=_QWERTY;default_layer_set(1ul<<_QWERTY);layer_move(_QWERTY);}
static void legacy_matrix_scan_user(void) {scan_calls++;}
static bool legacy_process_record_user(uint16_t code,keyrecord_t *r) {
    legacy_calls++;
    if(default_layer==_GEMINI) {steno_calls++;return !process_steno_edge_toggle_key(code,r);}
    if(is_o24_edge_toggle_key(code))return process_o24_edge_toggle_key(code,r);
    update_o24_edge_toggle_other(code,r);
    return true;
}
#include "mato_wrapper.inc"

static void check(bool ok,const char *message) {if(!ok){printf("FAIL wrapper: %s\n",message);failures++;}}
static bool event(uint8_t row,uint8_t col,uint16_t code,bool down) {
    now++;
    if(down)physical[row]|=1u<<col;else physical[row]&=~(1u<<col);
    keyrecord_t r={0};r.event.key.row=row;r.event.key.col=col;r.event.pressed=down;
    return process_record_user(code,&r);
}
static void scan(void) {now++;matrix_scan_user();}
static void choose_mato(void) {
    layer_move(_O24_SYSTEM);physical[3]=(1u<<4)|(1u<<7);
    event(0,6,KC_NO,true);event(0,6,KC_NO,false);scan();
    check(!mato_enabled,"entry waits for held SYSTEM thumbs");
    physical[3]=0;layer_move(_QWERTY);scan();
    check(mato_enabled&&kbd_lang==mato_lang,"entry all-up applies mato language");
}
static void edges(bool steno) {
    event(0,11,steno?STN_DR:MJR_EDGE_TOP,true);
    event(1,11,steno?STN_ZR:MJR_EDGE_BOTTOM,true);
    event(0,11,steno?STN_DR:MJR_EDGE_TOP,false);
    event(1,11,steno?STN_ZR:MJR_EDGE_BOTTOM,false);scan();
}
static void no_stuck(void) {
    check(get_mods()==0,"no held modifiers");
    for(int i=2;i<256;i++)check(!held[i],"no held HID keys");
    check(mato_owned==0&&mato_control_owned==0,"release ownership is clear");
    for(int i=0;i<MATO_KEYS;i++)check(!mato.keys[i].down&&!mato.keys[i].pending,"no pending engine keys");
}
int main(void) {
    keyboard_post_init_user();
    check(!mato_enabled&&default_layer==_QWERTY,"boots legacy O24");
    check(alt_en_layout.mappings==o24&&alt_jp_layout.mappings==o24,"both legacy layouts O24");
    check(event(0,3,KC_E,true),"O24 key uses legacy path");event(0,3,KC_E,false);
    for(int r=0;r<2;r++)for(int c=1;c<=10;c++)check(mato_position(r,c)==r*10+c-1,"core geometry");
    for(int c=4;c<=7;c++)check(mato_position(3,c)==20+c-4,"thumb geometry");
    choose_mato();check(last_lang==2&&mato_saved_o24_lang==1,"first mato entry Japanese preserves O24 English");
    unsigned old_calls=legacy_calls;
    event(0,3,KC_E,true);event(0,3,KC_E,false);
    check(legacy_calls==old_calls&&presses==1,"mato input bypasses O24 FIFO");
    edges(false);check(default_layer==_GEMINI&&mato_enabled,"legacy edge enters Mejiro");
    old_calls=steno_calls;event(0,3,STN_TEST,true);event(0,3,STN_TEST,false);
    check(steno_calls==old_calls+2,"Mejiro events passed through unchanged");
    force_qwerty_active=true;layer_move(_QWERTY);old_calls=legacy_calls;
    event(0,3,KC_K,true);event(0,3,KC_K,false);
    check(legacy_calls==old_calls+2,"Mejiro forced QWERTY stays on legacy path");
    force_qwerty_active=false;layer_move(_GEMINI);
    edges(true);check(default_layer==_QWERTY&&mato_enabled&&last_lang==2,"legacy edge returns mato Japanese");
    event(2,4,KC_LNG2,true);event(2,4,KC_LNG2,false);
    check(mato_lang==1&&kbd_lang==1&&mato_saved_o24_lang==1,"mato language tracked separately");
    edges(false);edges(true);check(last_lang==1,"mato English survives Mejiro roundtrip");
    // Core plus edge must not toggle or leak a modifier.
    event(0,1,KC_NO,true);edges(false);check(default_layer==_QWERTY,"other held core blocks exact edge chord");event(0,1,KC_NO,false);
    // Cancel a pending Enter when selecting Mejiro; late releases remain owned.
    unsigned old_presses=presses;
    event(3,6,KC_NO,true);event(1,0,KC_NO,true);event(1,0,KC_NO,false);scan();
    check(default_layer==_QWERTY,"mode exit waits for every key release");
    event(3,6,KC_NO,false);scan();
    check(default_layer==_GEMINI&&presses==old_presses,"mode request cancels pending thumb without text");
    no_stuck();edges(true);
    // A pending request suppresses newly pressed keys until the all-up boundary.
    event(0,0,KC_NO,true);event(0,3,KC_NO,true);event(0,0,KC_NO,false);event(0,3,KC_NO,false);scan();
    check(!mato_enabled&&last_lang==1&&presses==old_presses,"O24 exit cancels transition input and restores language");
    no_stuck();
    // Restoring a Japanese O24 selection is independent of English mato.
    kbd_lang=2;choose_mato();check(last_lang==1,"mato remembers its own English selection");
    event(0,0,KC_NO,true);event(0,0,KC_NO,false);scan();
    check(kbd_lang==2&&last_lang==2,"O24 Japanese selection restored");no_stuck();
    check(scan_calls>0,"legacy scanner remains active");
    if(failures)return EXIT_FAILURE;
    puts("PASS: production wrapper, geometry, mode transitions, language restoration, actual legacy edges");
    return EXIT_SUCCESS;
}
