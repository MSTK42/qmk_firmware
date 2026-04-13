/*
 * Mejiro (メジロ式) chord capture and conversion stub for QMK
 */
#pragma once

#include QMK_KEYBOARD_H

// Public API
// true: 何かキーが離された時点で確定（First Up Chord Send）
// false: すべてのキーが離された時点で確定
extern bool mejiro_first_up_chord_send;

bool is_stn_key(uint16_t kc);
void mejiro_on_press(uint16_t kc);
void mejiro_on_release(uint16_t kc);
bool mejiro_should_send_passthrough(void);
void mejiro_send_passthrough_keys(void);
void mejiro_reset_state(void);
