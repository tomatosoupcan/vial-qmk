#pragma once

#include "quantum.h"

#ifdef __cplusplus
extern "C" {
#endif

void process_gamepad(uint16_t keycode);
bool handle_socd(bool pressed, int axis, int direction, int button, int mode, int arrow);
void clear_button_axis(void);

extern bool P_STATE;
extern bool D_STATE;
extern bool L_STATE;
extern bool R_STATE;


#ifdef __cplusplus
}
#endif
