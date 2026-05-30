/*
Copyright 2025 JEEBIS <jeebis.iox@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "combo_fifo.h"

// グローバル変数の実装
combo_event_t combo_fifo[COMBO_FIFO_LEN];
uint8_t combo_fifo_len = 0;
hold_state_t hold_state = {0, 0, false, 0, 0, false, false, false, false, false};

__attribute__((weak)) bool combo_fifo_custom_action(uint16_t keycode, bool shifted, bool needs_unshift, bool is_hold) {
	(void)keycode;
	(void)shifted;
	(void)needs_unshift;
	(void)is_hold;
	return false;
}

__attribute__((weak)) bool combo_fifo_custom_combo_wait_for_hold(combo_pair_t pair) {
	(void)pair;
	return false;
}

__attribute__((weak)) bool combo_fifo_custom_combo_action(combo_pair_t pair, uint16_t keycode, bool shifted, bool needs_unshift, bool is_hold) {
	(void)pair;
	(void)keycode;
	(void)shifted;
	(void)needs_unshift;
	(void)is_hold;
	return false;
}
