#pragma once

#include <algorithm>

struct Lerp {
    float a;
    float b;
    float nframes;

    float operator[](float f) {
        if (nframes == 0.f) {
            nframes = 1.f;
        }
        float fac = (f / nframes);
        return a + ((b - a) * fac);
    }
};
