/*
 * BitrotRepeat.cpp
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
#include "ToggledValue.hpp"
#include "Version.hpp"

#include <cmath>
#include <iostream>
#include <vector>


START_NAMESPACE_DISTRHO

class BitrotRepeat : public Plugin {
    static constexpr uint32_t NUM_PARAMS = 10;
    static constexpr int OVERSAMPLING = 32;

    struct LerpParams {
        float speed;
    };

    struct {
        float active;
        float bpm;
        float beats;
        float division;
        float retrigger;
        float attack;
        float hold;
        float release;
        float varispeed;
        float sync;
        LerpParams current;
        LerpParams old;
    } params;

    std::vector<float> lbuffer;
    std::vector<float> rbuffer;

    double rate;
    double fpb;
    uint32_t writePos;
    double readPos;
    double loopLength;
    bool looped;

    uint32_t bpm;
    uint32_t beats;
    uint32_t division;

    float attackDelta;
    float releaseDelta;

    float gain;

    int retriggered;

    void reset() {
        for (uint32_t i = 0; i < NUM_PARAMS; ++i) {
            Parameter p;
            initParameter(i, p);
            setParameterValue(i, p.ranges.def);
        }
        params.old = params.current;
    }

    void updateLoop() {
        fpb = (60.0 / (double) bpm) * rate;
        loopLength = (fpb * beats) / (double) division;
    }

public:
    BitrotRepeat() : Plugin(10, 0, 0) {
        reset();
        sampleRateChanged(getSampleRate());
        activate();
    }

protected:
    const char* getLabel() const override {
        return LABEL("repeat");
    }

    const char* getDescription() const override {
        return "Beat repeat";
    }

    const char* getMaker() const override {
        return "grejppi";
    }

    const char* getLicense() const override {
        return "Apache-2.0";
    }

    uint32_t getVersion() const override {
        return BITROT_VERSION();
    }

    int64_t getUniqueId() const override {
        return 270;
    }

    void initParameter(uint32_t index, Parameter& p) override {
        switch (index) {
        case 0:
            p.hints  = kParameterIsAutomable | kParameterIsBoolean;
            p.name   = "Active";
            p.symbol = "active";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 0.f;
            break;
        case 1:
            p.hints  = kParameterIsAutomable | kParameterIsInteger;
            p.name   = "BPM";
            p.symbol = "bpm";

            p.ranges.min =  10.f;
            p.ranges.max = 480.f;
            p.ranges.def = 100.f;
            break;
        case 2:
            p.hints  = kParameterIsAutomable | kParameterIsInteger;
            p.name   = "Beats";
            p.symbol = "beats";

            p.ranges.min = 1.f;
            p.ranges.max = 4.f;
            p.ranges.def = 2.f;
            break;
        case 3:
            p.hints  = kParameterIsAutomable | kParameterIsInteger;
            p.name   = "Division";
            p.symbol = "division";

            p.ranges.min =  1.f;
            p.ranges.max = 16.f;
            p.ranges.def =  4.f;
            break;
        case 4:
            p.hints  = kParameterIsAutomable | kParameterIsBoolean | kParameterIsTrigger;
            p.name   = "Retrigger";
            p.symbol = "retrigger";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 0.f;
            break;
        case 5:
            p.hints  = kParameterIsAutomable;
            p.name   = "Attack";
            p.symbol = "attack";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 0.f;
            break;
        case 6:
            p.hints  = kParameterIsAutomable;
            p.name   = "Hold";
            p.symbol = "hold";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 1.f;
            break;
        case 7:
            p.hints  = kParameterIsAutomable;
            p.name   = "Release";
            p.symbol = "release";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 1.f;
            break;
        case 8:
            p.hints  = kParameterIsAutomable | kParameterIsBoolean;
            p.name   = "Varispeed";
            p.symbol = "varispeed";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 0.f;
            break;
        case 9:
            p.hints  = kParameterIsAutomable | kParameterIsLogarithmic;
            p.name   = "Speed";
            p.symbol = "speed";

            p.ranges.min = 0.25f;
            p.ranges.max = 4.f;
            p.ranges.def = 1.f;
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
            return params.bpm;
        case 2:
            return params.beats;
        case 3:
            return params.division;
        case 4:
            return params.retrigger;
        case 5:
            return params.attack;
        case 6:
            return params.hold;
        case 7:
            return params.release;
        case 8:
            return params.varispeed;
        case 9:
            return params.current.speed;
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
                params.bpm = value;
                bpm = (uint32_t) value;
                updateLoop();
                break;
            case 2:
                params.beats = value;
                beats = (uint32_t) value;
                updateLoop();
                break;
            case 3:
                params.division = value;
                division = (uint32_t) value;
                updateLoop();
                break;
            case 4:
                params.retrigger = value;
                if (toggledValue(value) && !retriggered) {
                    std::memset(&lbuffer[0], 0, sizeof(float) * lbuffer.size());
                    std::memset(&rbuffer[0], 0, sizeof(float) * rbuffer.size());
                    writePos = 0;
                    readPos = 0.0;
                    retriggered = true;
                    looped = false;
                } else {
                    retriggered = false;
                }
                break;
            case 5:
                params.attack = value;
                if (value != 0.f) {
                    attackDelta = 1.f / (rate * (value / 10.f));
                } else {
                    attackDelta = 1.f;
                }
                break;
            case 6:
                params.hold = value;
                break;
            case 7:
                params.release = value;
                if (value!= 0.f) {
                    releaseDelta = 1.f / (rate * (value/ 10.f));
                } else {
                    releaseDelta = 1.f;
                }
                break;
            case 8:
                params.varispeed = value;
                break;
            case 9:
                params.current.speed = value;
                break;
            default:
                break;
        }
    }

    void activate() override {
        writePos = 0;
        readPos = 0.0;
    }

    void sampleRateChanged(double rate) override {
        int newSize = std::ceil(rate) * 24;
        lbuffer.resize(newSize, 0.f);
        rbuffer.resize(newSize, 0.f);
        this->rate = rate;
        updateLoop();
    }

    void run(const float** inputs, float** outputs, uint32_t nframes) override {
        Lerp speed { params.old.speed, params.current.speed, (float) nframes };

        uint32_t bufSize = lbuffer.size();

        if (toggledValue(params.active)) {
            if (writePos < bufSize) {
                std::memcpy(&lbuffer[writePos], inputs[0],
                    sizeof(float) * std::min(nframes, bufSize - writePos));
                std::memcpy(&rbuffer[writePos], inputs[1],
                    sizeof(float) * std::min(nframes, bufSize - writePos));
                writePos += nframes;
            }

            for (uint32_t i = 0; i < nframes; ++i) {
                if (toggledValue(params.varispeed) && (looped || speed[i] < 1.f)) {
                    float l(0.f);
                    float r(0.f);

                    for (int o = 0; o < OVERSAMPLING; ++o) {
                        l += lbuffer[(uint32_t) readPos] * gain;
                        r += rbuffer[(uint32_t) readPos] * gain;
                        readPos += speed[i] / (float) OVERSAMPLING;
                    }

                    outputs[0][i] = l / (float) OVERSAMPLING;
                    outputs[1][i] = r / (float) OVERSAMPLING;
                } else {
                    outputs[0][i] = lbuffer[(uint32_t) readPos] * gain;
                    outputs[1][i] = rbuffer[(uint32_t) readPos] * gain;
                    readPos += 1.f;
                }

                while (readPos >= loopLength) {
                    readPos -= loopLength;
                    gain = 0.f;
                    looped = true;
                }

                if (readPos <= std::max(params.hold, 0.1f) * loopLength) {
                    gain += attackDelta * speed[i];
                    gain = std::min(gain, 1.f);
                } else {
                    gain -= releaseDelta * speed[i];
                    gain = std::max(0.f, gain);
                }
            }
        } else {
            writePos = 0;
            readPos = 0.0;
            retriggered = 0;
            looped = false;
            gain = 1.f;
            std::memset(&lbuffer[0], 0, sizeof(float) * bufSize);
            std::memset(&rbuffer[0], 0, sizeof(float) * bufSize);
            std::memcpy(outputs[0], inputs[0], sizeof(float) * nframes);
            std::memcpy(outputs[1], inputs[1], sizeof(float) * nframes);
        }

        params.old = params.current;
    }
};

Plugin* createPlugin() {
    return new BitrotRepeat();
}

END_NAMESPACE_DISTRHO
