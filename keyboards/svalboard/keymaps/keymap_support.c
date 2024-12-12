/*
Copyright 2023 Morgan Venable @_claussen

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

#include QMK_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>
#include "svalboard.h"
#include "features/achordion.h"
#include "keymap_support.h"

// in keymap.c:
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
void pointing_device_init_user(void) {
    set_auto_mouse_layer(MH_AUTO_BUTTONS_LAYER); // only required if AUTO_MOUSE_DEFAULT_LAYER is not set to index of <mouse_layer>
    set_auto_mouse_enable(true);         // always required before the auto mouse feature will work
}
#endif

#if (defined MH_AUTO_BUTTONS && defined PS2_MOUSE_ENABLE && defined MOUSEKEY_ENABLE) || defined(POINTING_DEVICE_AUTO_MOUSE_MH_ENABLE)

static uint16_t mh_auto_buttons_timer;
extern int tp_buttons; // mousekey button state set in action.c and used in ps2_mouse.c

void mouse_mode(bool);

#endif

#if defined(POINTING_DEVICE_AUTO_MOUSE_MH_ENABLE)

#define SCROLL_DIVISOR 20

bool mouse_mode_enabled = false;

static int _ds_l_x = 0;
static int _ds_l_y = 0;
static int _ds_r_x = 0;
static int _ds_r_y = 0;

static bool scroll_hold = false, scroll_toggle = false;

report_mouse_t pointing_device_task_combined_user(report_mouse_t reportMouse1, report_mouse_t reportMouse2) {
    report_mouse_t ret_mouse;
    if (reportMouse1.x == 0 && reportMouse1.y == 0 && reportMouse2.x == 0 && reportMouse2.y == 0)
        return pointing_device_combine_reports(reportMouse1, reportMouse2);

    if ((global_saved_values.left_scroll != scroll_hold) != scroll_toggle) {
        int div_x;
        int div_y;

        reportMouse1.y = -reportMouse1.y;

        _ds_l_x += reportMouse1.x;
        _ds_l_y += reportMouse1.y;

        div_x = _ds_l_x / SCROLL_DIVISOR;
        div_y = _ds_l_y / SCROLL_DIVISOR;
        if (div_x != 0) {
            reportMouse1.h += div_x;
            _ds_l_x -= div_x * SCROLL_DIVISOR;
        }

        if (div_y != 0) {
            reportMouse1.v += div_y;
            _ds_l_y -= div_y * SCROLL_DIVISOR;
        }
        reportMouse1.x = 0;
        reportMouse1.y = 0;
    }

    if ((global_saved_values.right_scroll != scroll_hold) != scroll_toggle) {
        int div_x;
        int div_y;

        reportMouse2.y = -reportMouse2.y;

        _ds_r_x += reportMouse2.x;
        _ds_r_y += reportMouse2.y;

        div_x = _ds_r_x / SCROLL_DIVISOR;
        div_y = _ds_r_y / SCROLL_DIVISOR;
        if (div_x != 0) {
            reportMouse2.h += div_x;
            _ds_r_x -= div_x * SCROLL_DIVISOR;
        }

        if (div_y != 0) {
            reportMouse2.v += div_y;
            _ds_r_y -= div_y * SCROLL_DIVISOR;
        }
        reportMouse2.x = 0;
        reportMouse2.y = 0;
    }

    mouse_mode(true);
    ret_mouse = pointing_device_combine_reports(reportMouse1, reportMouse2);

    return pointing_device_task_user(ret_mouse);
}

static int snipe_x = 0;
static int snipe_y = 0;

static int snipe_div = 1;
report_mouse_t pointing_device_task_user(report_mouse_t reportMouse) {
    if (reportMouse.x == 0 && reportMouse.y == 0)
        return reportMouse;

    mouse_mode(true);

    if (snipe_div != 1) {
        int div_x;
        int div_y;
        snipe_x += reportMouse.x;
        snipe_y += reportMouse.y;

        reportMouse.x = 0;
        reportMouse.y = 0;

        div_x = snipe_x / snipe_div;
        div_y = snipe_y / snipe_div;
        if (div_x != 0) {
            reportMouse.x = div_x;
            snipe_x -= div_x * snipe_div;
        }

        if (div_y != 0) {
            reportMouse.y = div_y;
            snipe_y -= div_y * snipe_div;
        }
    }
    return reportMouse;
}
#endif

void mh_change_timeouts(void) {
    if (sizeof(mh_timer_choices) / sizeof(int16_t) - 1 <= global_saved_values.mh_timer_index) {
        global_saved_values.mh_timer_index = 0;
    } else {
        global_saved_values.mh_timer_index++;
    }
    uprintf("mh_timer:%d\n", mh_timer_choices[global_saved_values.mh_timer_index]);
    write_eeprom_kb();
}

void toggle_achordion(void) {
    global_saved_values.disable_achordion = !global_saved_values.disable_achordion;
    write_eeprom_kb();
}

#define LAYER_2345_MASK (0x3C)
void check_layer_67(void) {
    if ((layer_state & LAYER_2345_MASK) == LAYER_2345_MASK) {
        layer_on(6);
        layer_on(7);
    } else {
        layer_off(6);
        layer_off(7);
    }
}

bool in_mod_tap = false;
int8_t in_mod_tap_layer = -1;
int8_t mouse_keys_pressed = 0;

//keep track of the current dpad mode: 0 for lstick, 1 for dpad, 2 for rstick
int8_t dpad_mode = 0;
//track socd mode: 10 Up Priority Last Input SOCD, 20 Last Input SOCD, 30 Up Priority Neutral SOCD, and 40 Neutral SOCD
int8_t socd_mode = 10;
//track state of direction inputs
bool UNP_STATE = false;
bool UND_STATE = false;
bool UNL_STATE = false;
bool UNR_STATE = false;

void clear_button_axis(void) {
    joystick_set_axis(0, 0);
    joystick_set_axis(1, 0);
    joystick_set_axis(2, 0);
    joystick_set_axis(3, 0);
    unregister_joystick_button(15);
    unregister_joystick_button(16);
    unregister_joystick_button(17);
    unregister_joystick_button(18);
    UNP_STATE = false;
    UND_STATE = false;
    UNL_STATE = false;
    UNR_STATE = false;
}

bool handle_socd(bool pressed, int axis, int direction, int button, int mode, int arrow) {
    int altb = 0;
    if (pressed) {
        switch (mode) {
                    case 10:
                        if (!(UNP_STATE && arrow == 2)) {
                            joystick_set_axis(axis, direction * 127);
                        }
                        return false;
                    case 11:
                        if (!(UNP_STATE && arrow == 2)) {
                            register_joystick_button(button);
                        }
                        return false;
                    case 12:
                        if (!(UNP_STATE && arrow == 2)) {
                            joystick_set_axis(axis + 3, direction * 127);
                        }
                        return false;
                }
    }
    else {
        switch (mode) {
                    case 10:
                        if ((UND_STATE && arrow == 0) || (UNL_STATE && arrow == 3) || (UNR_STATE && arrow == 1)) {
                            joystick_set_axis(axis, direction * 127 * -1);
                        }
                        else if (UNP_STATE && arrow == 2) {
                            return false;
                        }
                        else {
                            joystick_set_axis(axis, 0);
                        }
                        return false;
                    case 11:
                        if (button >= 16) {
                            altb = button - 2;
                        }
                        else {
                            altb = button + 2;
                        }

                        unregister_joystick_button(button);

                        if ((UND_STATE && arrow == 0) || (UNL_STATE && arrow == 3) || (UNR_STATE && arrow == 1)) {
                            register_joystick_button(altb);
                        }
                        return false;
                    case 12:
                        if ((UND_STATE && arrow == 0) || (UNL_STATE && arrow == 3) || (UNR_STATE && arrow == 1)) {
                            joystick_set_axis(axis + 3, direction * 127 * -1);
                        }
                        else if (UNP_STATE && arrow == 2) {
                            return false;
                        }
                        else {
                            joystick_set_axis(axis + 3, 0);
                        }
                        return false;
                }
    }
    return false;
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {

    //handle joystick input
    switch (keycode) {
		case GC_SQU:
			if (record->event.pressed) {
				register_joystick_button(0);
			} else {
				unregister_joystick_button(0);
			}
			return false;
		case GC_CRO:
			if (record->event.pressed) {
				register_joystick_button(1);
			} else {
				unregister_joystick_button(1);
			}
			return false;
		case GC_CIR:
			if (record->event.pressed) {
				register_joystick_button(2);
			} else {
				unregister_joystick_button(2);
			}
			return false;
		case GC_TRI:
			if (record->event.pressed) {
				register_joystick_button(3);
			} else {
				unregister_joystick_button(3);
			}
			return false;
		case GC_L1:
			if (record->event.pressed) {
				register_joystick_button(4);
			} else {
				unregister_joystick_button(4);
			}
			return false;
		case GC_R1:
			if (record->event.pressed) {
				register_joystick_button(5);
			} else {
				unregister_joystick_button(5);
			}
			return false;
		case GC_L2:
			if (record->event.pressed) {
				register_joystick_button(6);
			} else {
				unregister_joystick_button(6);
			}
			return false;
		case GC_R2:
			if (record->event.pressed) {
				register_joystick_button(7);
			} else {
				unregister_joystick_button(7);
			}
			return false;
		case GC_SEL:
			if (record->event.pressed) {
				register_joystick_button(8);
			} else {
				unregister_joystick_button(8);
			}
			return false;
		case GC_STA:
			if (record->event.pressed) {
				register_joystick_button(9);
			} else {
				unregister_joystick_button(9);
			}
			return false;
		case GC_L3:
			if (record->event.pressed) {
				register_joystick_button(10);
			} else {
				unregister_joystick_button(10);
			}
			return false;
		case GC_R3:
			if (record->event.pressed) {
				register_joystick_button(11);
			} else {
				unregister_joystick_button(11);
			}
			return false;
		case GC_HOM:
			if (record->event.pressed) {
				register_joystick_button(12);
			} else {
				unregister_joystick_button(12);
			}
			return false;
        case GC_TOG:
            if (record->event.pressed) {
				if (dpad_mode == 2) {
                    dpad_mode = 0;
                }
                else {
                    dpad_mode = dpad_mode + 1;
                }
			}
            clear_button_axis();
            return false;
        case GC_SCD:
            if (record->event.pressed) {
				if (socd_mode == 40) {
                    socd_mode = 10;
                }
                else {
                    socd_mode = socd_mode + 10;
                }
			}
            clear_button_axis();
            return false;
        case GC_UNP:
            if (record->event.pressed) {
                UNP_STATE = true;
                handle_socd(true, 1, -1, 18, socd_mode + dpad_mode, 0);
            }
            else {
                UNP_STATE = false;
                handle_socd(false, 1, -1, 18, socd_mode + dpad_mode, 0);
            }
            return false;
        case GC_UND:
            if (record->event.pressed) {
                UND_STATE = true;
                handle_socd(true, 1, 1, 16, socd_mode + dpad_mode, 2);
            }
            else {
                UND_STATE = false;
                handle_socd(false, 1, 1, 16, socd_mode + dpad_mode, 2);
            }
            return false;
        case GC_UNL:
            if (record->event.pressed) {
                UNL_STATE = true;
                handle_socd(true, 0, -1, 15, socd_mode + dpad_mode, 1);
            }
            else {
                UNL_STATE = false;
                handle_socd(false, 0, -1, 15, socd_mode + dpad_mode, 1);
            }
            return false;
        case GC_UNR:
            if (record->event.pressed) {
                UNR_STATE = true;
                handle_socd(true, 0, 1, 17, socd_mode + dpad_mode, 3);
            }
            else {
                UNR_STATE = false;
                handle_socd(false, 0, 1, 17, socd_mode + dpad_mode, 3);
            }
            return false;

    }

    // Abort additional processing if userspace code did
    if (!process_record_user(keycode, record)) { return false;}
    if (!in_mod_tap && !global_saved_values.disable_achordion && !process_achordion(keycode, record)) { return false; }


    // We are in a mod tap, with a KC_TRANSPARENT, lets make it transparent...
    if (IS_QK_MOD_TAP(keycode) && ((keycode & 0xFF) == KC_TRANSPARENT) &&
        record->tap.count > 0 && !in_mod_tap &&
        in_mod_tap_layer == -1 && record->event.pressed) {

        in_mod_tap_layer = get_highest_layer(layer_state);
        layer_state = layer_state & ~(1 << in_mod_tap_layer);

        in_mod_tap = true;

        return true;
    }

    // Fix things up on the release for the mod_tap case.
    if (!record->event.pressed && in_mod_tap) {
        in_mod_tap = false;
        layer_state = layer_state | (1 << in_mod_tap_layer);
        in_mod_tap_layer = -1;
        return true;
    }

    // If console is enabled, it will print the matrix position and status of each key pressed
#ifdef CONSOLE_ENABLE
    uprintf("KL: kc: 0x%04X, col: %2u, row: %2u, pressed: %u, time: %5u, int: %u, count: %u\n", keycode, record->event.key.col, record->event.key.row, record->event.pressed, record->event.time, record->tap.interrupted, record->tap.count);
#endif

    if (mouse_mode_enabled && layer_state & (1 << MH_AUTO_BUTTONS_LAYER)) {
        // The keycodes below are all that are forced to drop you out of mouse mode.
        // The intent is for this list to eventually become just KC_NO, and KC_TRNS
        // as more functionality is exported to keybard, and those keys are removed
        // from the firmmware. - ilc - 2024-10-05
#define BAD_KEYCODE_CONDITONAL  (keycode == KC_NO ||  \
	                    keycode == KC_TRNS || \
		            keycode == SV_LEFT_DPI_INC || \
	                    keycode == SV_LEFT_DPI_DEC || \
	                    keycode == SV_RIGHT_DPI_INC || \
	                    keycode == SV_RIGHT_DPI_DEC || \
	                    keycode == SV_LEFT_SCROLL_TOGGLE || \
		            keycode == SV_RIGHT_SCROLL_TOGGLE || \
		            keycode == SV_TOGGLE_ACHORDION || \
	                    keycode == SV_MH_CHANGE_TIMEOUTS)

        uint16_t layer_keycode = keymap_key_to_keycode(MH_AUTO_BUTTONS_LAYER, record->event.key);
        if (BAD_KEYCODE_CONDITONAL ||
	    layer_keycode != keycode) {
#ifdef CONSOLE_ENABLE
            uprintf("process_record - mh_auto_buttons: off\n");
#endif
            if (BAD_KEYCODE_CONDITONAL) {
                mouse_mode(false);
	        return false;
            } else {
                mouse_mode(false);
            }
        } else {
            if (record->event.pressed) {
                mouse_keys_pressed++;
                mouse_mode(true);
            } else {
                // keys that are held before the mouse layer is active can mess this up.
                if (mouse_keys_pressed > 0) {
                    mouse_keys_pressed--;
                }
                mouse_mode(true);
            }
        }
    }

    if (record->event.pressed) { // key pressed
        switch (keycode) {
            case SV_LEFT_DPI_INC:
                increase_left_dpi();
                return false;
            case SV_LEFT_DPI_DEC:
                decrease_left_dpi();
                return false;
            case SV_RIGHT_DPI_INC:
                increase_right_dpi();
                return false;
            case SV_RIGHT_DPI_DEC:
                decrease_right_dpi();
                return false;
            case SV_LEFT_SCROLL_TOGGLE:
                global_saved_values.left_scroll = !global_saved_values.left_scroll;
                write_eeprom_kb();
                return false;
            case SV_RIGHT_SCROLL_TOGGLE:
                global_saved_values.right_scroll = !global_saved_values.right_scroll;
                write_eeprom_kb();
                return false;
            case SV_RECALIBRATE_POINTER:
                recalibrate_pointer();
                return false;
            case SV_MH_CHANGE_TIMEOUTS:
                mh_change_timeouts();
                return false;
            case SV_CAPS_WORD:
                caps_word_toggle();
                return false;
            case SV_TOGGLE_ACHORDION:
                toggle_achordion();
                return false;
            case SV_TOGGLE_23_67:
                layer_on(2);
                layer_on(3);
                check_layer_67();
                return false;
            case SV_TOGGLE_45_67:
                layer_on(4);
                layer_on(5);
                check_layer_67();
                return false;
            case SV_SNIPER_2:
                snipe_x *= 2;
                snipe_y *= 2;
                snipe_div *= 2;
                return false;
            case SV_SNIPER_3:
                snipe_div *= 3;
                snipe_x *= 3;
                snipe_y *= 3;
                return false;
            case SV_SNIPER_5:
                snipe_div *= 5;
                snipe_x *= 5;
                snipe_y *= 5;
                return false;
            case SV_SCROLL_HOLD:
                scroll_hold = true;
                return false;
            case SV_SCROLL_TOGGLE:
                return false;
            case SV_OUTPUT_STATUS:
                output_keyboard_info();
                return false;
        }
    } else { // key released
        switch (keycode) {
            // These keys are all holds and require un-setting upon release.
            case SV_TOGGLE_23_67:
                layer_off(2);
                layer_off(3);
                check_layer_67();
                return false;
            case SV_TOGGLE_45_67:
                layer_off(4);
                layer_off(5);
                check_layer_67();
                return false;
            case SV_SNIPER_2:
                snipe_div /= 2;
                snipe_x /= 2;
                snipe_y /= 2;
                return false;
            case SV_SNIPER_3:
                snipe_div /= 3;
                snipe_x /= 3;
                snipe_y /= 3;
                return false;
            case SV_SNIPER_5:
                snipe_div /= 5;
                snipe_x /= 5;
                snipe_y /= 5;
                return false;
            case SV_SCROLL_HOLD:
                scroll_hold = false;
                return false;
            case SV_SCROLL_TOGGLE:
                scroll_toggle ^= true;
                return false;
        }
    }

    // Neither the user nor the keyboard handled the event, so continue with normal handling
    return true;
};

#if defined MH_AUTO_BUTTONS && defined PS2_MOUSE_ENABLE && defined MOUSEKEY_ENABLE
void ps2_mouse_moved_user(report_mouse_t *mouse_report) {
    mouse_mode(true);
}
#endif

void matrix_scan_kb(void) {
    if (!global_saved_values.disable_achordion) {
        achordion_task();
    }

    if ((mh_timer_choices[global_saved_values.mh_timer_index] >= 0) && mouse_mode_enabled && (timer_elapsed(mh_auto_buttons_timer) > mh_timer_choices[global_saved_values.mh_timer_index]) && mouse_keys_pressed == 0) {
        if (!tp_buttons) {
            mouse_mode(false);
#if defined CONSOLE_ENABLE
            print("matrix - mh_auto_buttons: off\n");
#endif
        }
    }

    matrix_scan_user();
}

void mouse_mode(bool on) {
    if (on) {
        layer_on(MH_AUTO_BUTTONS_LAYER);
        mh_auto_buttons_timer = timer_read();
        mouse_mode_enabled = true;
    } else {
        layer_off(MH_AUTO_BUTTONS_LAYER);
        mh_auto_buttons_timer = 0;
        mouse_mode_enabled = false;
        mouse_keys_pressed = 0;
    }
}

// Joystick Config
joystick_config_t joystick_axes[JOYSTICK_AXIS_COUNT] = {
	JOYSTICK_AXIS_VIRTUAL,
	JOYSTICK_AXIS_VIRTUAL,
	JOYSTICK_AXIS_VIRTUAL,
	JOYSTICK_AXIS_VIRTUAL,
	JOYSTICK_AXIS_VIRTUAL,
	JOYSTICK_AXIS_VIRTUAL,
};
