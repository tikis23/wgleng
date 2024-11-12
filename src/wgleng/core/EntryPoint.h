#pragma once

#include <emscripten.h>
#include <emscripten/bind.h>

#include "../core/Context.h"
#include "../util/Timer.h"

void onInit(Context* ctx);
void onDeinit(Context* ctx);
void onTick(Context* ctx, TimeDuration dt);

// export functions to JS
bool I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_start();
void I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_stop();
void I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_setFocused(bool focused);
void I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_setHidden(bool hidden);
#define WGLENG_INIT_ENGINE EMSCRIPTEN_BINDINGS (my_module) { \
        emscripten::function("start", &I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_start); \
        emscripten::function("stop", &I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_stop); \
        emscripten::function("setFocused", &I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_setFocused); \
        emscripten::function("setHidden", &I_N_T_E_R_N_A_L_wgleng_internal_entrypoint_setHidden); \
    }