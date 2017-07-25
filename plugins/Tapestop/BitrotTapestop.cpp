#include "DistrhoPlugin.hpp"
#include "Lerp.hpp"
#include "ToggledValue.hpp"

#include <cmath>

#include "DistrhoPluginMain.cpp"


START_NAMESPACE_DISTRHO

class BitrotTapestop : public Plugin {
    static constexpr uint32_t NUM_PARAMS = 3;
    static constexpr uint32_t OVERSAMPLING = 32;
    static constexpr uint32_t BUFFER_SIZE = 192000;

    struct LerpParams {
        float fade;
    };

    struct {
        float active;
        float speed;
        LerpParams current;
        LerpParams old;
    } params;

    std::vector<float> lbuffer;
    std::vector<float> rbuffer;

    double rate;
    double playSpeed;
    double playSpeedFac;
    double speed;

    double readPos;
    uint32_t writePos;

    void reset() {
        for (uint32_t i = 0; i < NUM_PARAMS; ++i) {
            Parameter p;
            initParameter(i, p);
            setParameterValue(i, p.ranges.def);
        }
        params.old = params.current;
    }

public:
    BitrotTapestop() : Plugin(NUM_PARAMS, 0, 0) {
        reset();
        lbuffer.resize(BUFFER_SIZE, 0.f);
        rbuffer.resize(BUFFER_SIZE, 0.f);
    }

protected:
    const char* getLabel() const override {
        return "tapestop";
    }

    const char* getDescription() const override {
        return "Gradually slow down audio";
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
        return 268;
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
            p.name   = "Speed";
            p.symbol = "speed";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
            p.ranges.def = 0.5f;
            break;
        case 2:
            p.hints  = kParameterIsAutomatable | kParameterIsBoolean;
            p.name   = "Fade";
            p.symbol = "fade";

            p.ranges.min = 0.f;
            p.ranges.max = 1.f;
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
            return params.speed;
        case 2:
            return params.current.fade;
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
                params.speed = value;
                break;
            case 2:
                params.current.fade = value;
                break;
            default:
                break;
        }
    }

    void run(const float** inputs, float** outputs, uint32_t nframes) override {
        Lerp fade {
            (float) toggledValue(params.old.fade),
            (float) toggledValue(params.current.fade),
            (float) nframes
        };

        if (speed != params.speed) {
            speed = params.speed;
            playSpeedFac = 0.9999 + ((1.0 - speed) * 0.00009);
        }

        if (toggledValue(params.active)) {
            for (uint32_t i = 0; i < nframes; ++i) {
                if (writePos < BUFFER_SIZE) {
                    lbuffer[writePos] = inputs[0][i];
                    rbuffer[writePos] = inputs[1][i];
                    writePos++;
                }

                float l(0.f);
                float r(0.f);

                for (uint32_t o = 0; o < OVERSAMPLING; ++o) {
                    Lerp fadeAmount { 1.f, (float) playSpeed, 1.f };
                    l += lbuffer[(uint32_t) readPos] * fadeAmount[fade[i]];
                    r += rbuffer[(uint32_t) readPos] * fadeAmount[fade[i]];
                    readPos += playSpeed / (double) OVERSAMPLING;
                }

                outputs[0][i] = l / (float) OVERSAMPLING;
                outputs[1][i] = r / (float) OVERSAMPLING;

                playSpeed *= playSpeedFac;
            }
        } else {
            playSpeed = 1.f;
            writePos = 0;
            readPos = 0.0;
            std::memcpy(outputs[0], inputs[0], sizeof(float) * nframes);
            std::memcpy(outputs[1], inputs[1], sizeof(float) * nframes);
        }

        params.old = params.current;
    }
};

Plugin* createPlugin() {
    return new BitrotTapestop();
}

END_NAMESPACE_DISTRHO
