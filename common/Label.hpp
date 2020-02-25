#pragma once

#if defined(DISTRHO_PLUGIN_TARGET_VST)
    #define LABEL(X) DISTRHO_PLUGIN_NAME
#else
    #define LABEL(X) X
#endif
