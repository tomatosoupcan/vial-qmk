#include "fightstick.h"

bool P_STATE = false;
bool D_STATE = false;
bool L_STATE = false;
bool R_STATE = false;

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

void process_gamepad(uint16_t keycode);
bool handle_socd(bool pressed, int axis, int direction, int button, int mode, int arrow);
