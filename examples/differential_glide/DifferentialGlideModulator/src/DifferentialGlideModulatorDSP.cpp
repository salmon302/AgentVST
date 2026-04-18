#include <AgentDSP.h>
#include <cmath>
#include <vector>
#include <algorithm>

constexpr double PI = 3.14159265358979323846;

struct LinkwitzRiley {
    float a0, a1, a2, b1, b2;
    float z1[2], z2[2];
    void reset() { z1[0]=z1[1]=z2[0]=z2[1]=0.0f; }
    void setLowpass(double sr, double freq) {
        double w0 = 2.0 * PI * freq / sr;
        double cosW = std::cos(w0);
        double sinW = std::sin(w0);
        double alpha = sinW / (2.0 * 0.707); // Butterworth Q
        double a0_p = 1.0 + alpha;
        a0 = (float)((1.0 - cosW) / 2.0 / a0_p);
        a1 = (float)((1.0 - cosW) / a0_p);
        a2 = (float)((1.0 - cosW) / 2.0 / a0_p);
        b1 = (float)(-2.0 * cosW / a0_p);
        b2 = (float)((1.0 - alpha) / a0_p);
    }
    void setHighpass(double sr, double freq) {
        double w0 = 2.0 * PI * freq / sr;
        double cosW = std::cos(w0);
        double sinW = std::sin(w0);
        double alpha = sinW / (2.0 * 0.707);
        double a0_p = 1.0 + alpha;
        a0 = (float)((1.0 + cosW) / 2.0 / a0_p);
        a1 = (float)(-(1.0 + cosW) / a0_p);
        a2 = (float)((1.0 + cosW) / 2.0 / a0_p);
        b1 = (float)(-2.0 * cosW / a0_p);
        b2 = (float)((1.0 - alpha) / a0_p);
    }
    float process(int ch, float in) {
        // Add a tiny DC offset to prevent floating-point denormals (subnormals)
        // causing massive CPU spikes leading to watchdog bypasses.
        in += 1e-9f;
        float out = in * a0 + z1[ch];
        z1[ch] = in * a1 - out * b1 + z2[ch];
        z2[ch] = in * a2 - out * b2;
        
        // Snap near-zero states to exact zero
        if (std::abs(z1[ch]) < 1e-9f) z1[ch] = 0.0f;
        if (std::abs(z2[ch]) < 1e-9f) z2[ch] = 0.0f;
        
        return out - 1e-9f * a0;
    }
};

class FractionalDelay {
    std::vector<float> buffer;
    int writePos = 0;
public:
    void prepare(int maxDelay) {
        buffer.assign(maxDelay, 0.0f);
        writePos = 0;
    }
    void write(float in) {
        buffer[writePos] = in;
        writePos = (writePos + 1) % buffer.size();
    }
    float read(float offset) {
        float rp = writePos - offset;
        while (rp < 0.0f) rp += buffer.size();
        while (rp >= buffer.size()) rp -= buffer.size();
        
        int i1 = (int)rp;
        int i2 = (i1 + 1) % buffer.size();
        float frac = rp - i1;
        
        return buffer[i1] * (1.0f - frac) + buffer[i2] * frac;
    }
};

class DifferentialGlideModulatorProcessor : public AgentVST::IAgentDSP {
    double mSampleRate = 44100.0;
    
    LinkwitzRiley lp1, hp1, lp2, hp2;
    FractionalDelay delayLow[2];
    FractionalDelay delayMid[2];
    FractionalDelay delayHigh[2];
    
    float phase = 0.0f;
    
public:
    void prepare(double sampleRate, int /*maxBlockSize*/) override {
        mSampleRate = sampleRate;
        for(int i=0; i<2; ++i) {
            delayLow[i].prepare(sampleRate);
            delayMid[i].prepare(sampleRate);
            delayHigh[i].prepare(sampleRate);
        }
        
        // Crossover split ~300Hz and ~2000Hz using sequential high/low passes
        lp1.setLowpass(sampleRate, 300.0);
        hp1.setHighpass(sampleRate, 300.0);
        lp2.setLowpass(sampleRate, 2000.0);
        hp2.setHighpass(sampleRate, 2000.0);
        
        reset();
    }

    float processSample(int channel, float input,
                        const AgentVST::DSPContext& ctx) override {
        if (ctx.getParameter("bypass") >= 0.5f || channel >= 2)
            return input;

        float barDepth = ctx.getParameter("bar_depth") / 100.0f; 
        float stringTension = ctx.getParameter("string_tension") / 100.0f; 
        float wobbleRate = ctx.getParameter("wobble_rate"); 
        
        // Multi-band split approximation
        float low = lp1.process(channel, input);
        float highPassTemp = hp1.process(channel, input);
        float mid = lp2.process(channel, highPassTemp);
        float high = hp2.process(channel, highPassTemp);
        
        // Advance LFO phase (only on channel 0 to keep phase locked)
        if (channel == 0) {
            phase += static_cast<float>(wobbleRate / mSampleRate);
            if (phase >= 1.0f) phase -= 1.0f;
        }
        
        // Base delay modulation
        float lfo = std::sin(phase * 2.0f * (float)PI);
        
        float baseDelayMs = 15.0f; 
        float modulationMs = barDepth * 10.0f; 
        
        // Apply differential tension parameters (different string offsets)
        float offsetLow = mSampleRate * ((baseDelayMs + lfo * modulationMs * (1.0f - stringTension * 0.5f)) / 1000.0f);
        float offsetMid = mSampleRate * ((baseDelayMs + lfo * modulationMs) / 1000.0f);
        float offsetHigh= mSampleRate * ((baseDelayMs + lfo * modulationMs * (1.0f + stringTension * 0.5f)) / 1000.0f);
        
        delayLow[channel].write(low);
        float modLow = delayLow[channel].read(offsetLow);
        
        delayMid[channel].write(mid);
        float modMid = delayMid[channel].read(offsetMid);
        
        delayHigh[channel].write(high);
        float modHigh = delayHigh[channel].read(offsetHigh);
        
        return modLow + modMid + modHigh;
    }

    void reset() override {
        lp1.reset(); hp1.reset();
        lp2.reset(); hp2.reset();
        phase = 0.0f;
    }
};

AGENTVST_REGISTER_DSP(DifferentialGlideModulatorProcessor)
