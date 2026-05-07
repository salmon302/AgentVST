#pragma once

#include <vector>
#include <cmath>

namespace dualism {

class UndertoneBank {
public:
    UndertoneBank(double sampleRate);
    ~UndertoneBank();

    void prepare();
    void setFundamentalFrequency(double f);
    void setGravity(float g);
    // generate undertone content and mix into output buffer (in-place add)
    void generateUndertones(float* output, int numSamples, float bias);

private:
    double sr_;
    double fundamental_ = 0.0;
    float gravity_ = 0.6f;

    struct Osc {
        double phase = 0.0;
        double phaseInc = 0.0;
        double amp = 0.0;
        double lastOut = 0.0;
    };
    std::vector<Osc> partials_;

    static double polyBlep(double phase, double dt);
    static double mapBiasToMix(float bias, float gravity);
};

} // namespace dualism
