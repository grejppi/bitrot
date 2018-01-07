/*
 * BitrotReverser.cpp
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
#include "ToggledValue.hpp"

#include <cmath>
#include <vector>

#include "DistrhoPluginMain.cpp"


START_NAMESPACE_DISTRHO

class BitrotReverser : public Plugin {
    static constexpr uint32_t NUM_PARAMS = 2;

    struct {
        float active;
        float switchDir;
    } params;

    std::vector<float> lwork;
    std::vector<float> rwork;

    std::vector<float> lbuffer;
    std::vector<float> rbuffer;

    int32_t writePos;
    int32_t readPos;
    int32_t copied;

    void reset() {
        for (uint32_t i = 0; i < NUM_PARAMS; ++i) {
            Parameter p;
            initParameter(i, p);
            setParameterValue(i, p.ranges.def);
        }
    }

public:
    BitrotReverser() : Plugin(NUM_PARAMS, 0, 0) {
        reset();
        sampleRateChanged(getSampleRate());
        activate();
    }

protected:
    const char* getLabel() const override {
        return "reverser";
    }

    const char* getDescription() const override {
        return "Play sound backwards";
    }

    const char* getMaker() const override {
        return "grejppi";
    }

    const char* getLicense() const override {
        return "Apache 2.0";
    }

    uint32_t getVersion() const override {
        return d_version(0, 7, 0);
    }

    int64_t getUniqueId() const override {
        return 267;
    }

    void initParameter(uint32_t index, Parameter& p) override {
        switch (index) {
        case 0:
            p.hints  = kParameterIsAutomatable | kParameterIsBoolean;
            p.name   = "Active";
            p.symbol = "active";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 0.f;
            break;
        case 1:
            p.hints  = kParameterIsAutomatable;
            p.name   = "Switch Direction";
            p.symbol = "switch";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 0.f;
            break;
        default:
            break;
        }
    }

    float getParameterValue(uint32_t index) const override {
        switch (index) {
        case 0:
            return params.active;
        case 1:
            return params.switchDir;
        default:
            return 0.f;
        }
    }

    void setParameterValue(uint32_t index, float value) override {
        switch (index) {
        case 0:
            params.active = value;
            break;
        case 1:
            params.switchDir = value;
            break;
        default:
            break;
        }
    }

    void activate() override {
        writePos = 0;
        readPos = 0;
        copied = -1;
    }

    void sampleRateChanged(double rate) override {
        int newSize = std::ceil(rate) * 4;
        lwork.resize(newSize, 0.f);
        rwork.resize(newSize, 0.f);
        lbuffer.resize(newSize, 0.f);
        rbuffer.resize(newSize, 0.f);
    }

    void run(const float** inputs, float** outputs, uint32_t nframes) override {
        bool playing(toggledValue(params.active));
        int bufSize = lwork.size();

        for (uint32_t i = 0; i < nframes; ++i) {
            int w(writePos % bufSize);

            lwork[w] = inputs[0][i];
            rwork[w] = inputs[1][i];

            if (playing) {
                if (copied == -1) {
                    for (uint32_t j = 0; j < bufSize; ++j) {
                        lbuffer[j] = lwork[j];
                        rbuffer[j] = rwork[j];
                        copied = 0;
                    }
                } else if (copied < (bufSize >> 1)) {
                    lbuffer[w] = lwork[w];
                    rbuffer[w] = rwork[w];
                    copied++;
                }

                int advance = toggledValue(params.switchDir) ? 1 : -1;
                readPos = (readPos + advance) % lbuffer.size();
                outputs[0][i] = lbuffer[readPos];
                outputs[1][i] = rbuffer[readPos];
            } else {
                readPos = writePos;
                copied = -1;
                outputs[0][i] = lwork[readPos];
                outputs[1][i] = rwork[readPos];
            }

            writePos = (writePos + 1) % bufSize;
        }
    }
};

Plugin* createPlugin() {
    return new BitrotReverser();
}

END_NAMESPACE_DISTRHO
