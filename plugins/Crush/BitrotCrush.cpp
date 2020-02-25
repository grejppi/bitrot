/*
 * BitrotCrush.cpp
 *
 * Copyright 2013-2018 Henna Haahti
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DistrhoPlugin.hpp"
#include "Label.hpp"
#include "Lerp.hpp"
#include "Version.hpp"

#include <random>

#include "DistrhoPluginMain.cpp"


START_NAMESPACE_DISTRHO

class BitrotCrush : public Plugin {
    static constexpr uint32_t NUM_PARAMS = 6;

    struct LerpParams {
        float noisebias;
        float prenoise;
        float postnoise;
        float distort;
        float postclip;
    };

    struct {
        float downsample;
        LerpParams current;
        LerpParams old;
    } params;

    std::linear_congruential_engine<uint32_t, 24691, 1103515245, 0> rng;

    float lcache;
    float rcache;
    uint32_t sampleCounter;


    // Math helpers
    // ------------

    static float softClip(float x, float amount, float boost) {
        float y(x);
        y *= 1.f - amount;
        y += amount * boost * rationalTanh(x);
        return y;
    }

    float applyNoise(float x, float amount, float bias) {
        float noise((rng() / (float) 0xffffffff) - bias);
        float y(x);
        y *= 1.f - amount;
        y += amount * (x + (x * x * noise));
        return y;
    }


    // rational tanh approximation
    // by cschueler
    //
    // http://www.musicdsp.org/showone.php?id=238
    static float rationalTanh(float x) {
        if (x < -3) {
            return -1;
        } else if (x > 3) {
            return 1;
        } else {
            return x * (27 + x * x) / (27 + 9 * x * x);
        }
    }

    void reset() {
        for (uint32_t i = 0; i < NUM_PARAMS; ++i) {
            Parameter p;
            initParameter(i, p);
            setParameterValue(i, p.ranges.def);
        }
        params.old = params.current;
    }

public:
    BitrotCrush() : Plugin(NUM_PARAMS, 0, 0) {
        reset();
        activate();
    }

protected:
    const char* getLabel() const override {
        return LABEL("crush");
    }

    const char* getDescription() const override {
        return "Sample rate reduction";
    }

    const char* getMaker() const override {
        return "grejppi";
    }

    const char* getLicense() const override {
        return "Apache 2.0";
    }

    uint32_t getVersion() const override {
        return BITROT_VERSION();
    }

    int64_t getUniqueId() const override {
        return 269;
    }

    void initParameter(uint32_t index, Parameter& p) override {
        switch (index) {
        case 0:
            p.hints  = kParameterIsAutomatable | kParameterIsInteger;
            p.name   = "Downsample";
            p.symbol = "downsample";

            p.ranges.min =  1.f;
            p.ranges.max = 16.f;
            p.ranges.def =  1.f;
            break;
        case 1:
            p.hints  = kParameterIsAutomatable;
            p.name   = "Noise Bias";
            p.symbol = "noisebias";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 0.5f;
            break;
        case 2:
            p.hints  = kParameterIsAutomatable;
            p.name   = "Input Noise";
            p.symbol = "prenoise";

            p.ranges.min = 0.f;
            p.ranges.max = 1.0f;
            p.ranges.def = 0.f;
            break;
        case 3:
            p.hints  = kParameterIsAutomatable;
            p.name   = "Output Noise";
            p.symbol = "postnoise";

            p.ranges.min = 0.f;
            p.ranges.max = 1.0f;
            p.ranges.def = 0.f;
            break;
        case 4:
            p.hints  = kParameterIsAutomatable;
            p.name   = "Distort";
            p.symbol = "distort";

            p.ranges.min = 0.f;
            p.ranges.max = 1.0f;
            p.ranges.def = 0.f;
            break;
        case 5:
            p.hints  = kParameterIsAutomatable;
            p.name   = "Post Clip";
            p.symbol = "postclip";

            p.ranges.min = 0.f;
            p.ranges.max = 1.0f;
            p.ranges.def = 0.f;
            break;
        default:
            break;
        }
    }

    float getParameterValue(uint32_t index) const override {
        switch (index) {
        case 0:
            return params.downsample;
        case 1:
            return params.current.noisebias;
        case 2:
            return params.current.prenoise;
        case 3:
            return params.current.postnoise;
        case 4:
            return params.current.distort;
        case 5:
            return params.current.postclip;
        default:
            return 0.f;
        }
    }

    void setParameterValue(uint32_t index, float value) override {
        switch (index) {
        case 0:
            params.downsample = value;
            break;
        case 1:
            params.current.noisebias = value;
            break;
        case 2:
            params.current.prenoise = value;
            break;
        case 3:
            params.current.postnoise = value;
            break;
        case 4:
            params.current.distort = value;
            break;
        case 5:
            params.current.postclip = value;
            break;
        default:
            break;
        }
    }

    void activate() override {
        lcache = 0.f;
        rcache = 0.f;
        sampleCounter = 0;
        params.old = params.current;
    }

    void run(const float** inputs, float** outputs, uint32_t nframes) override {
        Lerp distort { params.old.distort, params.current.distort, (float) nframes };
        Lerp prenoise { params.old.prenoise, params.current.prenoise, (float) nframes };
        Lerp postclip { params.old.postclip, params.current.postclip, (float) nframes };
        Lerp postnoise { params.old.postnoise, params.current.postnoise, (float) nframes };
        Lerp noisebias { params.old.noisebias, params.current.noisebias, (float) nframes };

        for (uint32_t i = 0; i < nframes; ++i) {
            float lsample(inputs[0][i]);
            float rsample(inputs[1][i]);

            if (sampleCounter++ % (int) params.downsample == 0) {
                lsample = softClip(lsample, distort[i], 2.f);
                rsample = softClip(rsample, distort[i], 2.f);

                lsample = applyNoise(lsample, prenoise[i], noisebias[i]);
                rsample = applyNoise(rsample, prenoise[i], noisebias[i]);

                lcache = lsample;
                rcache = rsample;
            }

            lsample = lcache;
            rsample = rcache;

            lsample = softClip(lsample, postclip[i], 1.f);
            rsample = softClip(rsample, postclip[i], 1.f);

            lsample = applyNoise(lsample, postnoise[i], noisebias[i]);
            rsample = applyNoise(rsample, postnoise[i], noisebias[i]);

            outputs[0][i] = lsample;
            outputs[1][i] = rsample;
        }

        params.old = params.current;
    }
};

Plugin* createPlugin() {
    return new BitrotCrush();
}

END_NAMESPACE_DISTRHO
