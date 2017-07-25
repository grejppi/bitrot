#pragma once

#if defined(DISTRHO_PLUGIN_TARGET_VST)
    #define TOGGLED_VALUE_THRESHOLD 0.5f
#else
    #define TOGGLED_VALUE_THRESHOLD 1.f
#endif

static inline bool toggledValue(float v) {
    return v >= TOGGLED_VALUE_THRESHOLD;
}

#undef TOGGLED_VALUE_THRESHOLD
