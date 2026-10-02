// NIEBLA - Stage 6
#include "pluginprocessor.h"
#include "plugineditor.h"

namespace
{
    const juce::StringArray layerIds   { "a", "b", "c", "d" };
    const juce::StringArray layerNames { "A", "B", "C", "D" };

    // Time Retard note values, and their length in beats (quarter note = 1 beat)
    const juce::StringArray syncNames { "Off", "1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                        "1/4T", "1/4", "1/4D", "1/2", "1/2D", "1 bar", "2 bars", "4 bars" };
    const double syncBeats[] { 0.0, 0.0625, 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75,
                               2.0 / 3.0, 1.0, 1.5, 2.0, 3.0, 4.0, 8.0, 16.0 };

    constexpr double maxHumanizeSeconds = 0.06;   // Humanize 100% = up to 60 ms of random extra delay

    const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const juce::StringArray scaleNames { "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian",
                                         "Harmonic Minor", "Major Pentatonic", "Minor Pentatonic",
                                         "Whole Tone", "Hirajoshi", "Chromatic" };

    // Each scale as a 12-bit mask: bit n set = n semitones above the root is in the scale
    constexpr int bits (std::initializer_list<int> steps) { int m = 0; for (int s : steps) m |= 1 << s; return m; }
    const int scaleMasks[] {
        bits ({ 0, 2, 4, 5, 7, 9, 11 }),   // Major
        bits ({ 0, 2, 3, 5, 7, 8, 10 }),   // Minor
        bits ({ 0, 2, 3, 5, 7, 9, 10 }),   // Dorian
        bits ({ 0, 1, 3, 5, 7, 8, 10 }),   // Phrygian
        bits ({ 0, 2, 4, 6, 7, 9, 11 }),   // Lydian
        bits ({ 0, 2, 4, 5, 7, 9, 10 }),   // Mixolydian
        bits ({ 0, 2, 3, 5, 7, 8, 11 }),   // Harmonic Minor
        bits ({ 0, 2, 4, 7, 9 }),          // Major Pentatonic
        bits ({ 0, 3, 5, 7, 10 }),         // Minor Pentatonic
        bits ({ 0, 2, 4, 6, 8, 10 }),      // Whole Tone
        bits ({ 0, 2, 3, 7, 8 }),          // Hirajoshi
        0xFFF                              // Chromatic (every note)
    };

    const juce::StringArray glideSyncNames { "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/4D", "1/2", "1 bar", "2 bars" };
    const double glideBeats[] { 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0, 8.0 };

    const juce::StringArray delaySyncNames { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/2D", "1 bar" };
    const double delayBeats[] { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 3.0, 4.0 };

    const juce::StringArray voiceNames { "Flute", "Bells", "FM", "Sine", "Saw", "Choir", "Organ", "E. Piano", "Strings", "Air" };

    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // Choir vowel formants (Hz): A, E, I, O, U
    const float vowelFormants[5][3] { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2200, 2950 }, { 450, 800, 2830 }, { 325, 700, 2530 } };

    // Organ drawbar footages as frequency ratios (16', 8', 5 1/3', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1')
    const double organRatios[9]  { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0 };
    const double organSoft[9]    { 0.5, 1.0, 0.0, 0.45, 0.0, 0.15, 0.0, 0.0, 0.0 };
    const double organFull[9]    { 0.8, 1.0, 0.75, 0.8, 0.6, 0.65, 0.45, 0.45, 0.55 };

    // Bell partials: harmonic set vs. struck-bar set
    const double bellHarmonic[6] { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    const double bellStruck[6]   { 1.0, 2.76, 5.40, 8.93, 13.34, 18.64 };

    const double fmRatios[10] { 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 7.0 };
}

//==============================================================================
// SCALE HELPERS
//==============================================================================
bool niebla::isInScale (int midiNote, int root, int scaleIndex)
{
    const int mask = scaleMasks[juce::jlimit (0, scaleNames.size() - 1, scaleIndex)];
    const int degree = ((midiNote - root) % 12 + 12) % 12;
    return (mask >> degree) & 1;
}

int niebla::snapToScale (int midiNote, int root, int scaleIndex, int direction)
{
    for (int distance = 0; distance <= 6; ++distance)
    {
        const bool below = isInScale (midiNote - distance, root, scaleIndex);
        const bool above = isInScale (midiNote + distance, root, scaleIndex);

        if (below && above) return direction > 0 ? midiNote + distance : midiNote - distance;
        if (above)          return midiNote + distance;
        if (below)          return midiNote - distance;
    }
    return midiNote;
}

double niebla::glideSyncBeats (int index)
{
    return glideBeats[juce::jlimit (0, glideSyncNames.size() - 1, index)];
}

double niebla::delaySyncBeats (int index)
{
    return delayBeats[juce::jlimit (0, delaySyncNames.size() - 1, index)];
}

double niebla::retardSyncBeats (int index)
{
    return syncBeats[juce::jlimit (0, syncNames.size() - 1, index)];
}

juce::StringArray niebla::noteNameList() { return noteNames; }

//==============================================================================
// DELAY
//==============================================================================
void NieblaDelay::prepare (double sampleRate)
{
    sr = sampleRate;
    buffer.setSize (2, (int) (sampleRate * 2.1) + 8);
    smoothedDelay.reset (sampleRate, 0.15);
    smoothedDelay.setCurrentAndTargetValue ((float) (0.3 * sampleRate));
    reset();
}

void NieblaDelay::reset()
{
    buffer.clear();
    writePos = 0;
    toneL = toneR = 0.0f;
}

void NieblaDelay::process (float* left, float* right, int numSamples, float delaySeconds, float feedback, float mix)
{
    const int size = buffer.getNumSamples();
    smoothedDelay.setTargetValue (juce::jlimit (1.0f, (float) size - 3.0f, (float) (delaySeconds * sr)));
    auto* bufL = buffer.getWritePointer (0);
    auto* bufR = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float d = smoothedDelay.getNextValue();
        float readPos = (float) writePos - d;
        if (readPos < 0.0f) readPos += (float) size;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % size;
        const float frac = readPos - (float) i0;

        const float yl = bufL[i0] + frac * (bufL[i1] - bufL[i0]);
        const float yr = bufR[i0] + frac * (bufR[i1] - bufR[i0]);

        // Each repeat gets a little darker
        toneL += 0.35f * (yl - toneL);
        toneR += 0.35f * (yr - toneR);

        bufL[writePos] = left[i]  + toneL * feedback;
        bufR[writePos] = right[i] + toneR * feedback;
        if (++writePos >= size) writePos = 0;

        left[i]  = left[i]  * (1.0f - mix) + yl * mix;
        right[i] = right[i] * (1.0f - mix) + yr * mix;
    }
}

//==============================================================================
// REVERB: Hall, Pipe and Wood, with Shimmer and Freeze
//==============================================================================
namespace
{
    const double lineLengthsMs[8]  { 31.3, 37.9, 41.7, 47.3, 53.1, 59.9, 67.3, 73.9 };
    const double diffuserLengthsMs[4] { 4.77, 3.59, 12.73, 9.30 };
    constexpr double maxSizeScale = 2.0;

    // Pipe: line lengths as multiples of one trip down the pipe. Near-equal and whole-number
    // multiples make the tail ring at the pipe's note and its harmonics, like metal tubing.
    const double pipeMultiples[8] { 1.0, 1.0013, 2.0, 1.9974, 3.0, 1.5, 4.0, 0.5 };

    // Wood: body resonances of a wooden box (like a guitar body or cajon)
    const float woodBodyHz[3] { 180.0f, 420.0f, 1100.0f };
    const float woodBodyGain[3] { 0.9f, 0.6f, 0.35f };

    constexpr int shiftWindow = 2048;
}

double NieblaReverb::pipeFrequency (float size, int tuneRoot)
{
    const double free = 400.0 * std::pow (0.1, (double) size);   // Size: short pipe (400 Hz) .. long pipe (40 Hz)
    if (tuneRoot < 0)
        return free;
    // Nearest octave of the scale root
    double f = 440.0 * std::pow (2.0, (tuneRoot - 9) / 12.0);
    while (f > free * std::sqrt (2.0)) f *= 0.5;
    while (f < free / std::sqrt (2.0)) f *= 2.0;
    return juce::jlimit (30.0, 500.0, f);
}

void NieblaReverb::prepare (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (int i = 0; i < numLines; ++i)
    {
        lines[(size_t) i].data.assign ((size_t) (lineLengthsMs[i] * 0.001 * maxSizeScale * sampleRate) + 64, 0.0f);
        lines[(size_t) i].lfoRate = 0.11 + 0.07 * i;
        lines[(size_t) i].lfoPhase = i / (double) numLines;
    }
    for (int i = 0; i < numDiffusers; ++i)
        diffusers[(size_t) i].data.assign ((size_t) (diffuserLengthsMs[i] * 0.001 * sampleRate) + 1, 0.0f);

    shiftBuffer.assign (8192, 0.0f);

    juce::dsp::ProcessSpec stereo { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    juce::dsp::ProcessSpec mono   { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    lowCutFilter.prepare (stereo);   lowCutFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCutFilter.prepare (stereo);  highCutFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    shimmerHighPass.prepare (mono);  shimmerHighPass.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    shimmerHighPass.setCutoffFrequency (250.0f);
    for (int i = 0; i < 3; ++i)
    {
        woodBody[(size_t) i].prepare (stereo);
        woodBody[(size_t) i].setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        woodBody[(size_t) i].setCutoffFrequency (woodBodyHz[i]);
        woodBody[(size_t) i].setResonance (2.5f + 0.5f * i);
    }
    reset();
}

void NieblaReverb::reset()
{
    for (auto& l : lines)     { std::fill (l.data.begin(), l.data.end(), 0.0f); l.writePos = 0; l.lowpass = 0.0f; }
    for (auto& a : diffusers) { std::fill (a.data.begin(), a.data.end(), 0.0f); a.pos = 0; }
    std::fill (shiftBuffer.begin(), shiftBuffer.end(), 0.0f);
    shiftWrite = 0; shiftPhase = 0.0; shimmerFeedback = 0.0f;
    lowCutFilter.reset(); highCutFilter.reset(); shimmerHighPass.reset();
    for (auto& b : woodBody) b.reset();
}

void NieblaReverb::process (float* left, float* right, int numSamples, const ReverbSettings& s)
{
    std::array<float, numLines> delay {}, gain {};
    float diffusion = 0.62f, damp = 0.05f + 0.85f * s.damping, modDepth = 6.0f;

    if (s.type == pipe)
    {
        const double period = sr / pipeFrequency (s.size, s.tuneRoot);
        for (int i = 0; i < numLines; ++i) delay[(size_t) i] = (float) (period * pipeMultiples[i]);
        diffusion = 0.3f;                          // little smearing: echoes stay distinct, like flutter in a tube
        damp = 0.02f + 0.6f * s.damping;           // bright steel .. dull iron
        modDepth = 0.4f;
    }
    else if (s.type == wood)
    {
        const double scale = 0.12 + 0.35 * s.size; // wooden box .. wooden chapel
        for (int i = 0; i < numLines; ++i) delay[(size_t) i] = (float) (lineLengthsMs[i] * 0.001 * scale * sr);
        diffusion = 0.7f;                          // dense early reflections
        damp = 0.35f + 0.6f * s.damping;           // wood swallows the highs quickly
        modDepth = 2.0f;
    }
    else
    {
        const double scale = 0.35 + (maxSizeScale - 0.35) * s.size;
        for (int i = 0; i < numLines; ++i) delay[(size_t) i] = (float) (lineLengthsMs[i] * 0.001 * scale * sr);
    }

    for (int i = 0; i < numLines; ++i)
    {
        delay[(size_t) i] = juce::jlimit (4.0f, (float) lines[(size_t) i].data.size() - modDepth - 4.0f, delay[(size_t) i]);
        gain[(size_t) i] = s.freeze ? 1.0f
                                    : (float) std::pow (10.0, -3.0 * delay[(size_t) i] / (juce::jmax (0.1f, s.decay) * sr));
    }
    if (s.freeze) damp = 0.0f;   // Freeze: hold the tail forever

    lowCutFilter.setCutoffFrequency (juce::jlimit (20.0f, 2000.0f, s.lowCut));
    highCutFilter.setCutoffFrequency (juce::jlimit (500.0f, 20000.0f, s.highCut));
    const bool useLowCut = s.lowCut > 21.0f, useHighCut = s.highCut < 19900.0f;

    const double ratio = s.shimmerFifth ? 1.5 : 2.0;
    // Wood's resonant body feeds the shimmer loop much harder, so it gets a gentler loop
    const float shimmerAmount = s.freeze ? 0.0f : s.shimmer * (s.type == wood ? 0.3f : 1.0f);
    const float inputGain = s.freeze ? 0.0f : 1.0f;
    constexpr float wetGain = 0.35f;
    const int shiftSize = (int) shiftBuffer.size();

    for (int n = 0; n < numSamples; ++n)
    {
        float x = inputGain * 0.5f * (left[n] + right[n]) + shimmerFeedback;
        for (auto& a : diffusers)
        {
            const float v = a.data[(size_t) a.pos];
            const float y = v - diffusion * x;
            a.data[(size_t) a.pos] = x + diffusion * y;
            if (++a.pos >= (int) a.data.size()) a.pos = 0;
            x = y;
        }

        std::array<float, numLines> out {}, fb {};
        float sum = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            auto& l = lines[(size_t) i];
            const int len = (int) l.data.size();
            l.lfoPhase += l.lfoRate / sr;
            if (l.lfoPhase >= 1.0) l.lfoPhase -= 1.0;
            const float d = delay[(size_t) i] + modDepth * (float) std::sin (twoPi * l.lfoPhase);

            float readPos = (float) l.writePos - d;
            while (readPos < 0.0f) readPos += (float) len;
            const int i0 = (int) readPos;
            const int i1 = (i0 + 1) % len;
            const float frac = readPos - (float) i0;
            const float y = l.data[(size_t) i0] + frac * (l.data[(size_t) i1] - l.data[(size_t) i0]);

            out[(size_t) i] = y;
            l.lowpass = y * (1.0f - damp) + l.lowpass * damp;
            fb[(size_t) i] = l.lowpass * gain[(size_t) i];
            sum += fb[(size_t) i];
        }

        const float mixTerm = sum * (2.0f / numLines);
        for (int i = 0; i < numLines; ++i)
        {
            auto& l = lines[(size_t) i];
            l.data[(size_t) l.writePos] = fb[(size_t) i] - mixTerm + x * ((i & 1) ? -0.5f : 0.5f);
            if (++l.writePos >= (int) l.data.size()) l.writePos = 0;
        }

        float wetL = wetGain * (out[0] - out[2] + out[4] - out[6]);
        float wetR = wetGain * (out[1] - out[3] + out[5] - out[7]);

        if (s.type == wood)
        {
            float bl = 0.0f, br = 0.0f;
            for (int b = 0; b < 3; ++b)
            {
                bl += woodBodyGain[b] * woodBody[(size_t) b].processSample (0, wetL);
                br += woodBodyGain[b] * woodBody[(size_t) b].processSample (1, wetR);
            }
            wetL = 0.5f * wetL + 1.0f * bl;
            wetR = 0.5f * wetR + 1.0f * br;
        }

        if (useLowCut)  { wetL = lowCutFilter.processSample (0, wetL);  wetR = lowCutFilter.processSample (1, wetR); }
        if (useHighCut) { wetL = highCutFilter.processSample (0, wetL); wetR = highCutFilter.processSample (1, wetR); }

        // Shimmer: pitch the reverb up and feed it back in, so the tail climbs in octaves (or fifths)
        shiftBuffer[(size_t) shiftWrite] = 0.5f * (wetL + wetR);
        if (shimmerAmount > 0.0001f)
        {
            shiftPhase += (1.0 - ratio) / shiftWindow;
            while (shiftPhase < 0.0) shiftPhase += 1.0;
            while (shiftPhase >= 1.0) shiftPhase -= 1.0;
            auto readAt = [&] (double ph)
            {
                float rp = (float) shiftWrite - (float) (ph * shiftWindow) - 1.0f;
                while (rp < 0.0f) rp += (float) shiftSize;
                const int j0 = (int) rp, j1 = (j0 + 1) % shiftSize;
                const float fr = rp - (float) j0;
                return shiftBuffer[(size_t) j0] + fr * (shiftBuffer[(size_t) j1] - shiftBuffer[(size_t) j0]);
            };
            const double ph2 = std::fmod (shiftPhase + 0.5, 1.0);
            const float w1 = (float) std::pow (std::sin (juce::MathConstants<double>::pi * shiftPhase), 2.0);
            const float shifted = w1 * readAt (shiftPhase) + (1.0f - w1) * readAt (ph2);
            const float filtered = shimmerHighPass.processSample (0, shifted);
            shimmerFeedback = 0.5f * std::tanh (filtered * shimmerAmount * 0.9f * 2.0f);   // bounded, never runs away
        }
        else
        {
            shimmerFeedback = 0.0f;
        }
        if (++shiftWrite >= shiftSize) shiftWrite = 0;

        left[n]  = left[n]  * (1.0f - s.mix) + wetL * s.mix;
        right[n] = right[n] * (1.0f - s.mix) + wetR * s.mix;
    }
}

//==============================================================================
// VOICE
//==============================================================================
NieblaVoice::NieblaVoice (juce::AudioProcessorValueTreeState& state, const juce::String& p, const std::atomic<double>& hostBpm)
    : bpm (hostBpm)
{
    voiceType  = state.getRawParameterValue (p + "voice");
    character1 = state.getRawParameterValue (p + "char1");
    character2 = state.getRawParameterValue (p + "char2");
    octave     = state.getRawParameterValue (p + "octave");
    attack     = state.getRawParameterValue (p + "attack");
    decay      = state.getRawParameterValue (p + "decay");
    sustain    = state.getRawParameterValue (p + "sustain");
    release    = state.getRawParameterValue (p + "release");
    cutoff     = state.getRawParameterValue (p + "cutoff");
    resonance  = state.getRawParameterValue (p + "resonance");
    filterType = state.getRawParameterValue (p + "filtertype");
    filterEnv  = state.getRawParameterValue (p + "filterenv");
    interval   = state.getRawParameterValue (p + "interval");
    fine       = state.getRawParameterValue (p + "fine");
    pitchMode  = state.getRawParameterValue (p + "pitchmode");
    glideMode  = state.getRawParameterValue (p + "glidemode");
    glideSync  = state.getRawParameterValue (p + "glidesync");
    glideTime  = state.getRawParameterValue (p + "glidetime");
    glideCurve = state.getRawParameterValue (p + "glidecurve");
    scaleRoot  = state.getRawParameterValue ("root");
    scaleType  = state.getRawParameterValue ("scale");
    analogDrift = state.getRawParameterValue ("drift");
}

void NieblaVoice::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    adsr.setSampleRate (sr);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };
    filter.prepare (spec);
    noiseFilter.prepare (spec);
    noiseFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    for (auto& f : formants)
    {
        f.prepare (spec);
        f.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    }
}

void NieblaVoice::updateParameters()
{
    juce::ADSR::Parameters p;
    p.attack  = attack->load();
    p.decay   = decay->load();
    p.sustain = sustain->load();
    p.release = release->load();
    adsr.setParameters (p);

    switch ((int) filterType->load())
    {
        case 1:  filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
        case 2:  filter.setType (juce::dsp::StateVariableTPTFilterType::highpass); break;
        default: filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);  break;
    }
    filter.setResonance (juce::jmap (resonance->load(), 0.0f, 1.0f, 0.707f, 10.0f));
    baseCutoff = cutoff->load() * cutoffDrift;
    envAmount  = filterEnv->load();

    c1 = character1->load();
    c2 = character2->load();
    const double nyquistSafe = sampleRate * 0.45;

    if (type == niebla::bells)
    {
        double total = 0.0;
        for (int k = 0; k < 6; ++k)
            total += partialAmp[(size_t) k] = std::pow (k + 1.0, -(2.0 - 1.7 * c1));   // Hardness brightens upper partials
        for (int k = 0; k < 6; ++k)
            partialAmp[(size_t) k] /= total;
    }
    else if (type == niebla::organ)
    {
        double total = 0.0;
        for (int k = 0; k < 9; ++k)
            total += partialAmp[(size_t) k] = organSoft[k] + (organFull[k] - organSoft[k]) * c1;   // Drawbars
        for (int k = 0; k < 9; ++k)
            partialAmp[(size_t) k] /= total;
    }
    else if (type == niebla::choir)
    {
        const float v = c1 * 4.0f;   // A -> E -> I -> O -> U
        const int i0 = juce::jlimit (0, 3, (int) v);
        const float frac = v - (float) i0;
        const float q[3] { 6.0f, 9.0f, 12.0f };
        for (int f = 0; f < 3; ++f)
        {
            formants[(size_t) f].setCutoffFrequency (vowelFormants[i0][f] + frac * (vowelFormants[i0 + 1][f] - vowelFormants[i0][f]));
            formants[(size_t) f].setResonance (q[f]);
        }
    }
    else if (type == niebla::strings)
    {
        const double fc = 9000.0 * (1.0 - c2) + 1200.0 * c2;   // Bow softness
        softCoef = (float) std::exp (-twoPi * fc / sampleRate);
    }
    else if (type == niebla::air)
    {
        const double colour = 0.35 * std::pow (8.0, (double) c1);                     // dark .. bright
        const double freeCentre = 1400.0 * colour;
        const double trackedCentre = juce::jmin (nyquistSafe, targetFrequency * 2.0);
        const double centre = std::exp (std::log (freeCentre) + (std::log (trackedCentre) - std::log (freeCentre)) * c2);
        noiseFilter.setCutoffFrequency ((float) juce::jlimit (40.0, nyquistSafe, centre));
        noiseFilter.setResonance (1.2f + 9.0f * c2);   // Pitch tracking also narrows the band
    }
    else if (type == niebla::flute)
    {
        noiseFilter.setCutoffFrequency ((float) juce::jmin (nyquistSafe, targetFrequency * 2.0));
        noiseFilter.setResonance (1.5f);
    }
}

void NieblaVoice::setupEngine()
{
    type = juce::jlimit (0, 9, (int) voiceType->load());
    noteAge = 0.0;
    tick = 0;
    drift = driftTarget = 0.0;
    softL = softR = soft2L = soft2R = 0.0f;
    envValue = 0.0f;
    filterCounter = 0;

    // Unison-style voices start with random phases so they don't phase-cancel
    const bool randomPhase = type == niebla::saw || type == niebla::strings || type == niebla::choir;
    for (auto& p : phases)
        p = randomPhase ? noise.nextDouble() : 0.0;
    for (auto& p : lfoPhases)
        p = noise.nextDouble();

    for (auto& f : formants) f.reset();
    noiseFilter.reset();
    filter.reset();

    partialEnv.fill (1.0);
    if (type == niebla::bells)
    {
        for (int k = 0; k < 6; ++k)
            partialDecay[(size_t) k] = std::exp (-1.0 / ((5.0 / (1.0 + k * 1.2)) * sampleRate));   // Higher partials ring shorter
    }
    else if (type == niebla::epiano)
    {
        const double seconds = 3.0 * std::sqrt (261.6 / targetFrequency);   // Low notes ring longer
        partialDecay[0] = std::exp (-1.0 / (seconds * sampleRate));
    }
}

void NieblaVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    const int octaveShift = 12 * (int) std::round (octave->load());
    const int shift = (int) std::round (interval->load());

    int targetNote = midiNote;
    if (shift != 0)
        targetNote = niebla::snapToScale (midiNote + shift, (int) scaleRoot->load(), (int) scaleType->load(), shift);

    startPitch  = midiNote + octaveShift;
    targetPitch = targetNote + octaveShift + fine->load() / 100.0;
    targetFrequency = 440.0 * std::pow (2.0, (targetPitch - 69.0) / 12.0);

    glidePosition = 1.0;
    if (pitchMode->load() > 0.5f)
    {
        const double seconds = glideMode->load() < 0.5f
                                 ? niebla::glideSyncBeats ((int) glideSync->load()) * 60.0 / bpm.load()
                                 : (double) glideTime->load();
        if (seconds > 0.0)
        {
            glidePosition  = 0.0;
            glideIncrement = 1.0 / (seconds * getSampleRate());
            glideShape     = glideCurve->load() * 8.0;
        }
    }

    velocityGain = velocity;

    // Analog drift: every note gets its own small pitch and filter offsets, like a vintage polysynth
    const double d = analogDrift->load();
    driftStaticCents = (noise.nextDouble() * 2.0 - 1.0) * 6.0 * d;
    cutoffDrift = (float) std::exp2 ((noise.nextDouble() * 2.0 - 1.0) * 0.3 * d);
    voiceDrift = voiceDriftTarget = 0.0;

    setupEngine();
    updateParameters();
    filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, baseCutoff));
    adsr.reset();
    adsr.noteOn();
}

void NieblaVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
    }
}

double NieblaVoice::polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt;            return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

double NieblaVoice::saw (int index, double increment)
{
    double& p = phases[(size_t) index];
    const double v = (2.0 * p - 1.0) - polyBlep (p, increment);
    p += increment;
    if (p >= 1.0) p -= 1.0;
    return v;
}

double NieblaVoice::sine (int index, double increment)
{
    double& p = phases[(size_t) index];
    const double v = std::sin (twoPi * p);
    p += increment;
    if (p >= 1.0) p -= 1.0;
    return v;
}

// One stereo sample from the selected voice type
void NieblaVoice::renderEngine (double frequency, float& left, float& right)
{
    const double inc = frequency / sampleRate;
    const double nyquistSafe = 0.45;
    auto advanceLfo = [this] (int i, double hz) { lfoPhases[(size_t) i] += hz / sampleRate; if (lfoPhases[(size_t) i] >= 1.0) lfoPhases[(size_t) i] -= 1.0; return std::sin (twoPi * lfoPhases[(size_t) i]); };
    auto cents = [] (double c) { return std::exp2 (c / 1200.0); };
    auto white = [this] { return noise.nextFloat() * 2.0f - 1.0f; };

    double l = 0.0, r = 0.0;

    switch (type)
    {
        case niebla::flute:   // Character 1 = Breath, Character 2 = Vibrato
        {
            const double vib = advanceLfo (0, 5.0) * c2 * 30.0 * juce::jmin (1.0, noteAge / 0.5);
            const double i = inc * cents (vib);
            const double p = phases[0];
            const double tone = (std::sin (twoPi * p) + 0.2 * std::sin (2.0 * twoPi * p) + 0.06 * std::sin (3.0 * twoPi * p)) / 1.26;
            phases[0] += i; if (phases[0] >= 1.0) phases[0] -= 1.0;
            const double chiff = 0.6 + 0.4 * std::exp (-noteAge * 6.0);
            const double breath = noiseFilter.processSample (0, white()) * c1 * 1.4 * chiff;
            l = r = tone + breath;
            break;
        }

        case niebla::bells:   // Character 1 = Hardness, Character 2 = Inharmonic
        {
            for (int k = 0; k < 6; ++k)
            {
                const double ratio = bellHarmonic[k] + (bellStruck[k] - bellHarmonic[k]) * c2;
                const double pi = inc * ratio;
                partialEnv[(size_t) k] *= partialDecay[(size_t) k];
                if (pi >= nyquistSafe) continue;
                const double s = partialAmp[(size_t) k] * partialEnv[(size_t) k] * sine (k, pi);
                l += s * ((k & 1) ? 0.85 : 1.15);
                r += s * ((k & 1) ? 1.15 : 0.85);
            }
            l *= 1.6; r *= 1.6;
            break;
        }

        case niebla::fm:      // Character 1 = Ratio, Character 2 = Index
        {
            const double ratio = fmRatios[juce::jlimit (0, 9, (int) std::round (c1 * 9.0f))];
            const double index = c2 * 8.0 * (0.35 + 0.65 * envValue);   // brighter while the envelope is high
            const double mod = sine (1, inc * ratio);
            const double p = phases[0];
            l = r = std::sin (twoPi * p + index * mod);
            phases[0] += inc; if (phases[0] >= 1.0) phases[0] -= 1.0;
            break;
        }

        case niebla::sine:    // Character 1 = Drift, Character 2 = Harmonics
        {
            if ((tick++ & 2047) == 0)
                driftTarget = (noise.nextDouble() * 2.0 - 1.0) * c1 * 15.0;
            drift += (driftTarget - drift) * 0.0004;
            const double i = inc * cents (drift);
            const double p = phases[0];
            const double s = std::sin (twoPi * p) + c2 * (0.5 * std::sin (2.0 * twoPi * p) + 0.3 * std::sin (3.0 * twoPi * p));
            phases[0] += i; if (phases[0] >= 1.0) phases[0] -= 1.0;
            l = r = s / (1.0 + 0.8 * c2);
            break;
        }

        case niebla::saw:     // Character 1 = Detune, Character 2 = Width
        {
            const double d = c1 * 25.0;
            const double s1 = saw (0, inc * cents (-d)), s2 = saw (1, inc), s3 = saw (2, inc * cents (d));
            const double w = c2;
            l = 0.5 * s2 + (0.25 + 0.25 * w) * s1 + (0.25 - 0.25 * w) * s3;
            r = 0.5 * s2 + (0.25 - 0.25 * w) * s1 + (0.25 + 0.25 * w) * s3;
            break;
        }

        case niebla::choir:   // Character 1 = Vowel (A-E-I-O-U), Character 2 = Ensemble
        {
            const double vib = advanceLfo (0, 5.5) * 12.0 * juce::jmin (1.0, noteAge / 0.6);
            const double ens = c2 * 14.0;
            const double d1 = -ens * (0.7 + 0.3 * advanceLfo (1, 0.31));
            const double d3 =  ens * (0.7 + 0.3 * advanceLfo (2, 0.47));
            const double base = inc * cents (vib);
            const double s1 = saw (0, base * cents (d1)), s2 = saw (1, base), s3 = saw (2, base * cents (d3));
            const float breath = white() * 0.04f;
            const float inL = (float) (s1 + 0.7 * s2) + breath;
            const float inR = (float) (s3 + 0.7 * s2) + breath;
            l = formants[0].processSample (0, inL) + 0.6f * formants[1].processSample (0, inL) + 0.25f * formants[2].processSample (0, inL);
            r = formants[0].processSample (1, inR) + 0.6f * formants[1].processSample (1, inR) + 0.25f * formants[2].processSample (1, inR);
            l *= 1.3; r *= 1.3;
            break;
        }

        case niebla::organ:   // Character 1 = Drawbars, Character 2 = Rotary speed
        {
            double s = 0.0;
            for (int k = 0; k < 9; ++k)
            {
                const double pi = inc * organRatios[k];
                if (pi >= nyquistSafe) continue;
                s += partialAmp[(size_t) k] * sine (k, pi);
            }
            const double depth = juce::jmin (1.0, c2 * 5.0) * 0.35;
            const double rot = advanceLfo (0, 0.7 + 6.0 * c2);
            const double rotCos = std::cos (twoPi * lfoPhases[0]);
            l = s * (1.0 + depth * rot) / (1.0 + depth);
            r = s * (1.0 + depth * rotCos) / (1.0 + depth);
            if (noteAge < 0.004)   // key click
            {
                const double click = white() * 0.25 * (1.0 - noteAge / 0.004);
                l += click; r += click;
            }
            break;
        }

        case niebla::epiano:  // Character 1 = Tine hardness, Character 2 = Tremolo
        {
            const double index = (0.4 + 2.6 * c1) * std::exp (-noteAge / 0.3) + 0.12;
            const double mod = sine (1, inc);
            const double p = phases[0];
            double s = std::sin (twoPi * p + index * mod);
            phases[0] += inc; if (phases[0] >= 1.0) phases[0] -= 1.0;
            if (inc * 14.0 < nyquistSafe)
                s += sine (2, inc * 14.0) * c1 * 0.12 * std::exp (-noteAge / 0.03);
            partialEnv[0] *= partialDecay[0];
            s *= partialEnv[0];
            const double pan = c2 * advanceLfo (0, 4.5);
            l = s * (1.0 - 0.8 * pan) / (1.0 + 0.8 * c2);
            r = s * (1.0 + 0.8 * pan) / (1.0 + 0.8 * c2);
            break;
        }

        case niebla::strings: // Character 1 = Ensemble, Character 2 = Bow softness
        {
            const double det1 = c1 * (10.0 + 4.0 * advanceLfo (0, 0.6));
            const double det2 = c1 * (10.0 + 4.0 * advanceLfo (1, 0.83));
            const double fast = c1 * 3.0 * advanceLfo (2, 5.1);
            const double s1 = saw (0, inc * cents (-det1 + fast));
            const double s2 = saw (1, inc * cents (0.5 * fast));
            const double s3 = saw (2, inc * cents (det2 - fast));
            const float rawL = (float) ((s1 + 0.6 * s2) / 1.6);
            const float rawR = (float) ((s3 + 0.6 * s2) / 1.6);
            softL  = rawL  + (softL  - rawL)  * softCoef;
            softR  = rawR  + (softR  - rawR)  * softCoef;
            soft2L = softL + (soft2L - softL) * softCoef;
            soft2R = softR + (soft2R - softR) * softCoef;
            l = soft2L * (1.0 + 1.5 * c2); r = soft2R * (1.0 + 1.5 * c2);   // make up level lost to softening
            break;
        }

        case niebla::air:     // Character 1 = Colour, Character 2 = Pitch tracking
        default:
        {
            const double gain = 2.0 + 6.0 * c2;
            l = noiseFilter.processSample (0, white()) * gain;
            r = noiseFilter.processSample (1, white()) * gain;
            break;
        }
    }

    left = (float) l;
    right = (float) r;
}

void NieblaVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    updateParameters();
    const bool stereo = output.getNumChannels() > 1;

    for (int i = 0; i < numSamples; ++i)
    {
        double frequency = targetFrequency;
        if (glidePosition < 1.0)
        {
            glidePosition = juce::jmin (1.0, glidePosition + glideIncrement);
            const double shaped = glideShape < 0.01 ? glidePosition
                                                    : (1.0 - std::exp (-glideShape * glidePosition)) / (1.0 - std::exp (-glideShape));
            frequency = 440.0 * std::pow (2.0, (startPitch + (targetPitch - startPitch) * shaped - 69.0) / 12.0);
        }

        if (analogDrift->load() > 0.0001f)
        {
            if ((filterCounter & 31) == 0 && noise.nextFloat() < 0.004f)
                voiceDriftTarget = (noise.nextDouble() * 2.0 - 1.0) * 5.0 * analogDrift->load();
            voiceDrift += (voiceDriftTarget - voiceDrift) * 0.0002;
            frequency *= std::exp2 ((driftStaticCents + voiceDrift) / 1200.0);
        }

        float l, r;
        renderEngine (frequency, l, r);

        envValue = adsr.getNextSample();

        // Filter envelope: Env amount opens (or closes) the cutoff by up to 5 octaves
        if (filterCounter-- <= 0)
        {
            filterCounter = 32;
            filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, baseCutoff * std::exp2 (envAmount * 5.0f * envValue)));
        }

        const float gain = envValue * velocityGain * 0.2f;
        l = filter.processSample (0, l) * gain;
        r = filter.processSample (1, r) * gain;

        if (stereo)
        {
            output.addSample (0, startSample + i, l);
            output.addSample (1, startSample + i, r);
        }
        else
        {
            output.addSample (0, startSample + i, 0.5f * (l + r));
        }

        noteAge += 1.0 / sampleRate;

        if (! adsr.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

//==============================================================================
// PARAMETERS
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout NieblaAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto timeRange = [] (float minSeconds, float maxSeconds, float centre)
    {
        NormalisableRange<float> r (minSeconds, maxSeconds, 0.001f);
        r.setSkewForCentre (centre);
        return r;
    };

    const auto percent = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    const auto signedPercent = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { const int p = roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + String (p) + " %"; });
    const auto panText = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        const int amount = roundToInt (std::abs (v) * 100.0f);
        return amount == 0 ? String ("C") : (v < 0 ? "L " : "R ") + String (amount);
    });
    const auto seconds = AudioParameterFloatAttributes().withLabel ("s");

    NormalisableRange<float> cutoffRange (20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre (1000.0f);
    NormalisableRange<float> lowCutRange (20.0f, 1000.0f, 1.0f);
    lowCutRange.setSkewForCentre (150.0f);
    NormalisableRange<float> highCutRange (1000.0f, 20000.0f, 1.0f);
    highCutRange.setSkewForCentre (5000.0f);

    // Global scale
    auto global = std::make_unique<AudioProcessorParameterGroup> ("global", "Global Scale", " | ");
    global->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Root", noteNames, 0));
    global->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames, 0));
    global->addChild (std::make_unique<AudioParameterBool>   (ParameterID { "forcescale", 1 }, "Force to Scale", true));
    layout.add (std::move (global));

    // Starting sound (the "Dawn field" example): Strings, E. Piano, Choir, Bells blooming into a chord
    const int   defVoice[]     { niebla::strings, niebla::epiano, niebla::choir, niebla::bells };
    const float defChar1[]     { 0.7f, 0.4f, 0.45f, 0.5f };
    const float defChar2[]     { 0.4f, 0.3f, 0.7f, 0.35f };
    const float defLevel[]     { 0.7f, 0.6f, 0.65f, 0.5f };
    const float defAttack[]    { 2.0f, 0.005f, 2.5f, 0.002f };
    const float defDecay[]     { 1.5f, 3.0f, 1.5f, 4.0f };
    const float defSustain[]   { 0.8f, 0.4f, 0.8f, 0.3f };
    const float defRelease[]   { 6.0f, 3.0f, 6.0f, 5.0f };
    const float defCutoff[]    { 2500.0f, 6000.0f, 3000.0f, 8000.0f };
    const int   defSync[]      { 0, 10, 12, 14 };
    const float defPan[]       { 0.0f, -0.2f, 0.15f, 0.35f };
    const int   defInterval[]  { 0, 4, 7, 12 };
    const float defFine[]      { 0.0f, -6.0f, 8.0f, 3.0f };
    const int   defPitchMode[] { 0, 0, 1, 1 };
    const int   defGlide[]     { 7, 7, 7, 6 };
    const float defDelayMix[]  { 0.1f, 0.25f, 0.15f, 0.2f };
    const float defReverbMix[] { 0.35f, 0.35f, 0.4f, 0.5f };

    for (int i = 0; i < numLayers; ++i)
    {
        const auto p = layerIds[i] + "_";
        const auto n = layerNames[i] + " ";

        auto group = std::make_unique<AudioProcessorParameterGroup> (layerIds[i], "Layer " + layerNames[i], " | ");

        group->addChild (std::make_unique<AudioParameterBool>   (ParameterID { p + "on", 1 }, n + "On", true));

        // Voice
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "voice", 1 }, n + "Voice", voiceNames, defVoice[i]));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "char1", 1 }, n + "Character 1", NormalisableRange<float> (0.0f, 1.0f), defChar1[i], percent));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "char2", 1 }, n + "Character 2", NormalisableRange<float> (0.0f, 1.0f), defChar2[i], percent));
        group->addChild (std::make_unique<AudioParameterInt>    (ParameterID { p + "octave", 1 }, n + "Octave", -2, 2, 0));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "level", 1 }, n + "Level", NormalisableRange<float> (0.0f, 1.0f), defLevel[i], percent));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "pan", 1 }, n + "Pan", NormalisableRange<float> (-1.0f, 1.0f), defPan[i], panText));

        // Envelope
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "attack", 1 },  n + "Attack",  timeRange (0.001f, 20.0f, 1.0f), defAttack[i], seconds));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "decay", 1 },   n + "Decay",   timeRange (0.001f, 20.0f, 1.0f), defDecay[i], seconds));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "sustain", 1 }, n + "Sustain", NormalisableRange<float> (0.0f, 1.0f), defSustain[i], percent));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "release", 1 }, n + "Release", timeRange (0.001f, 30.0f, 1.0f), defRelease[i], seconds));

        // Filter
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "filtertype", 1 }, n + "Filter Type", StringArray { "LP", "BP", "HP" }, 0));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "cutoff", 1 }, n + "Cutoff", cutoffRange, defCutoff[i], AudioParameterFloatAttributes().withLabel ("Hz")));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "resonance", 1 }, n + "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.2f, percent));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "filterenv", 1 }, n + "Filter Env Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, signedPercent));

        // Time Retard
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "retardmode", 1 }, n + "Retard Mode", StringArray { "Sync", "Free" }, 0));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "retardsync", 1 }, n + "Retard (Sync)", syncNames, defSync[i]));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "retardtime", 1 }, n + "Retard (Free)", timeRange (0.0f, 10.0f, 1.0f), 0.0f, seconds));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "humanize", 1 }, n + "Humanize", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));

        // Pitch Shift
        group->addChild (std::make_unique<AudioParameterInt>    (ParameterID { p + "interval", 1 }, n + "Interval", -12, 12, defInterval[i], AudioParameterIntAttributes().withLabel ("st")));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "fine", 1 }, n + "Fine", NormalisableRange<float> (-50.0f, 50.0f, 0.1f), defFine[i], AudioParameterFloatAttributes().withLabel ("ct")));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "pitchmode", 1 }, n + "Pitch Mode", StringArray { "Jump", "Glide" }, defPitchMode[i]));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "glidemode", 1 }, n + "Glide Mode", StringArray { "Sync", "Free" }, 0));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "glidesync", 1 }, n + "Glide (Sync)", glideSyncNames, defGlide[i]));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "glidetime", 1 }, n + "Glide (Free)", timeRange (0.0f, 10.0f, 1.0f), 1.0f, seconds));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "glidecurve", 1 }, n + "Glide Curve", NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));

        // Delay
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "delaymode", 1 }, n + "Delay Mode", StringArray { "Sync", "ms" }, 0));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "delaysync", 1 }, n + "Delay (Sync)", delaySyncNames, 9));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "delayms", 1 }, n + "Delay (ms)", timeRange (1.0f, 2000.0f, 300.0f), 375.0f, AudioParameterFloatAttributes().withLabel ("ms")));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "delayfb", 1 }, n + "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.45f, percent));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "delaymix", 1 }, n + "Delay Mix", NormalisableRange<float> (0.0f, 1.0f), defDelayMix[i], percent));

        // Reverb
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "revsize", 1 }, n + "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.75f, percent));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "revdecay", 1 }, n + "Reverb Decay", timeRange (0.3f, 20.0f, 4.0f), 7.0f, seconds));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "revdamp", 1 }, n + "Reverb Damping", NormalisableRange<float> (0.0f, 1.0f), 0.4f, percent));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "revmix", 1 }, n + "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f), defReverbMix[i], percent));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "revtype", 1 }, n + "Reverb Type", StringArray { "Hall", "Pipe", "Wood" }, 0));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "revshimmer", 1 }, n + "Shimmer", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { p + "revshift", 1 }, n + "Shimmer Interval", StringArray { "Octave", "Fifth" }, 0));
        group->addChild (std::make_unique<AudioParameterBool>   (ParameterID { p + "revfreeze", 1 }, n + "Freeze", false));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "revlowcut", 1 }, n + "Reverb Low Cut", lowCutRange, 20.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
        group->addChild (std::make_unique<AudioParameterFloat>  (ParameterID { p + "revhighcut", 1 }, n + "Reverb High Cut", highCutRange, 20000.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
        group->addChild (std::make_unique<AudioParameterBool>   (ParameterID { p + "revtune", 1 }, n + "Tune Pipe to Scale", true));

        layout.add (std::move (group));
    }

    auto warmthGroup = std::make_unique<AudioProcessorParameterGroup> ("warmthgroup", "Warmth", " | ");
    warmthGroup->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "warmth", 1 }, "Warmth", NormalisableRange<float> (0.0f, 1.0f), 0.25f, percent));
    warmthGroup->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "drift", 1 }, "Drift", NormalisableRange<float> (0.0f, 1.0f), 0.2f, percent));
    layout.add (std::move (warmthGroup));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (0.0f, 1.0f), 0.55f, percent));
    return layout;
}

//==============================================================================
// PROCESSOR
//==============================================================================
NieblaAudioProcessor::NieblaAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    masterLevel = apvts.getRawParameterValue ("master");
    warmth      = apvts.getRawParameterValue ("warmth");
    scaleRoot   = apvts.getRawParameterValue ("root");
    scaleType   = apvts.getRawParameterValue ("scale");
    forceScale  = apvts.getRawParameterValue ("forcescale");
    for (int n = 0; n < 128; ++n)
        snappedNote[(size_t) n] = n;

    for (int i = 0; i < numLayers; ++i)
    {
        auto& layer = layers[(size_t) i];
        const auto p = layerIds[i] + "_";

        layer.on            = apvts.getRawParameterValue (p + "on");
        layer.level         = apvts.getRawParameterValue (p + "level");
        layer.pan           = apvts.getRawParameterValue (p + "pan");
        layer.retardMode    = apvts.getRawParameterValue (p + "retardmode");
        layer.retardSync    = apvts.getRawParameterValue (p + "retardsync");
        layer.retardTime    = apvts.getRawParameterValue (p + "retardtime");
        layer.humanize      = apvts.getRawParameterValue (p + "humanize");
        layer.delayMode     = apvts.getRawParameterValue (p + "delaymode");
        layer.delaySync     = apvts.getRawParameterValue (p + "delaysync");
        layer.delayTime     = apvts.getRawParameterValue (p + "delayms");
        layer.delayFeedback = apvts.getRawParameterValue (p + "delayfb");
        layer.delayMix      = apvts.getRawParameterValue (p + "delaymix");
        layer.reverbSize    = apvts.getRawParameterValue (p + "revsize");
        layer.reverbDecay   = apvts.getRawParameterValue (p + "revdecay");
        layer.reverbDamping = apvts.getRawParameterValue (p + "revdamp");
        layer.reverbMix     = apvts.getRawParameterValue (p + "revmix");
        layer.reverbType    = apvts.getRawParameterValue (p + "revtype");
        layer.reverbShimmer = apvts.getRawParameterValue (p + "revshimmer");
        layer.reverbShift   = apvts.getRawParameterValue (p + "revshift");
        layer.reverbFreeze  = apvts.getRawParameterValue (p + "revfreeze");
        layer.reverbLowCut  = apvts.getRawParameterValue (p + "revlowcut");
        layer.reverbHighCut = apvts.getRawParameterValue (p + "revhighcut");
        layer.reverbTune    = apvts.getRawParameterValue (p + "revtune");

        layer.synth.addSound (new NieblaSound());
        for (int v = 0; v < voicesPerLayer; ++v)
            layer.synth.addVoice (new NieblaVoice (apvts, p, currentBpm));

        layer.pending.reserve (1024);
    }
}

void NieblaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    layerBuffer.setSize (2, samplesPerBlock);
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();
    setLatencySamples (juce::roundToInt (oversampling.getLatencyInSamples()));
    tapeToneL = tapeToneR = 0.0f;

    for (auto& layer : layers)
    {
        layer.synth.setCurrentPlaybackSampleRate (sampleRate);
        layer.pending.clear();
        layer.noteDelay.fill (0);
        layer.blockMidi.ensureSize (2048);
        layer.delay.prepare (sampleRate);
        layer.reverb.prepare (sampleRate, samplesPerBlock);

        for (int v = 0; v < layer.synth.getNumVoices(); ++v)
            if (auto* voice = dynamic_cast<NieblaVoice*> (layer.synth.getVoice (v)))
                voice->prepare (sampleRate, samplesPerBlock);
    }
}

bool NieblaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

int NieblaAudioProcessor::computeDelaySamples (NieblaLayer& layer, double bpm)
{
    double seconds = 0.0;

    if (layer.retardMode->load() < 0.5f)
    {
        const int index = juce::jlimit (0, syncNames.size() - 1, (int) layer.retardSync->load());
        seconds = syncBeats[index] * 60.0 / bpm;
    }
    else
    {
        seconds = layer.retardTime->load();
    }

    seconds += random.nextDouble() * layer.humanize->load() * maxHumanizeSeconds;
    return (int) std::round (seconds * currentSampleRate);
}

void NieblaAudioProcessor::queueEvent (NieblaLayer& layer, juce::int64 dueSample, const juce::MidiMessage& m)
{
    if (layer.pending.size() < layer.pending.capacity())
        layer.pending.push_back ({ dueSample, m });
}

void NieblaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto hostBpm = position->getBpm())
                if (*hostBpm > 0.0)
                    bpm = *hostBpm;
    currentBpm = bpm;

    const juce::int64 blockStart = sampleCounter;

    for (auto& layer : layers)
    {
        const bool isOn = layer.on->load() > 0.5f;
        if (layer.wasOn && ! isOn)
            layer.synth.allNotesOff (0, true);
        layer.wasOn = isOn;
    }

    const int root = (int) scaleRoot->load();
    const int scale = (int) scaleType->load();

    // 1. Queue every incoming MIDI event per layer, delayed by that layer's Time Retard
    for (const auto metadata : midi)
    {
        auto m = metadata.getMessage();

        if (m.isNoteOn())
        {
            const int played = m.getNoteNumber();
            const int snapped = forceScale->load() > 0.5f ? juce::jlimit (0, 127, niebla::snapToScale (played, root, scale, 0)) : played;
            snappedNote[(size_t) played] = snapped;
            m = juce::MidiMessage::noteOn (m.getChannel(), snapped, m.getVelocity());
        }
        else if (m.isNoteOff())
        {
            m = juce::MidiMessage::noteOff (m.getChannel(), snappedNote[(size_t) m.getNoteNumber()]);
        }

        const juce::int64 when = blockStart + metadata.samplePosition;

        for (auto& layer : layers)
        {
            if (m.isNoteOn())
            {
                if (layer.on->load() < 0.5f)
                    continue;

                const int delay = computeDelaySamples (layer, bpm);
                layer.noteDelay[(size_t) m.getNoteNumber()] = delay;
                queueEvent (layer, when + delay, m);
            }
            else if (m.isNoteOff())
            {
                queueEvent (layer, when + layer.noteDelay[(size_t) m.getNoteNumber()], m);
            }
            else
            {
                queueEvent (layer, when, m);
            }
        }
    }

    // 2. Render each layer through its delay and reverb, then mix with level and pan
    layerBuffer.setSize (2, numSamples, false, false, true);
    const juce::int64 blockEnd = blockStart + numSamples;

    for (auto& layer : layers)
    {
        layer.blockMidi.clear();

        auto isDue = [&] (const PendingEvent& e) { return e.dueSample < blockEnd; };
        for (const auto& e : layer.pending)
            if (isDue (e))
                layer.blockMidi.addEvent (e.message, (int) juce::jmax ((juce::int64) 0, e.dueSample - blockStart));

        layer.pending.erase (std::remove_if (layer.pending.begin(), layer.pending.end(), isDue), layer.pending.end());

        layerBuffer.clear();
        layer.synth.renderNextBlock (layerBuffer, layer.blockMidi, 0, numSamples);

        auto* left  = layerBuffer.getWritePointer (0);
        auto* right = layerBuffer.getWritePointer (1);

        const float delayMix = layer.delayMix->load();
        if (delayMix > 0.0001f)
        {
            const double delaySeconds = layer.delayMode->load() < 0.5f
                                          ? niebla::delaySyncBeats ((int) layer.delaySync->load()) * 60.0 / bpm
                                          : layer.delayTime->load() * 0.001;
            layer.delay.process (left, right, numSamples, (float) juce::jmin (2.0, delaySeconds), layer.delayFeedback->load(), delayMix);
        }

        const float reverbMix = layer.reverbMix->load();
        if (reverbMix > 0.0001f)
        {
            ReverbSettings rv;
            rv.type = (int) layer.reverbType->load();
            rv.size = layer.reverbSize->load();
            rv.decay = layer.reverbDecay->load();
            rv.damping = layer.reverbDamping->load();
            rv.mix = reverbMix;
            rv.shimmer = layer.reverbShimmer->load();
            rv.shimmerFifth = layer.reverbShift->load() > 0.5f;
            rv.freeze = layer.reverbFreeze->load() > 0.5f;
            rv.lowCut = layer.reverbLowCut->load();
            rv.highCut = layer.reverbHighCut->load();
            rv.tuneRoot = layer.reverbTune->load() > 0.5f ? root : -1;
            layer.reverb.process (left, right, numSamples, rv);
        }

        // Balance-style pan keeps the layer's own stereo image
        const float level = layer.level->load();
        const float pan = layer.pan->load();
        const float gainL = level * juce::jmin (1.0f, 1.0f - pan);
        const float gainR = level * juce::jmin (1.0f, 1.0f + pan);

        if (buffer.getNumChannels() >= 2)
        {
            buffer.addFrom (0, 0, layerBuffer, 0, 0, numSamples, gainL);
            buffer.addFrom (1, 0, layerBuffer, 1, 0, numSamples, gainR);
        }
        else
        {
            buffer.addFrom (0, 0, layerBuffer, 0, 0, numSamples, 0.5f * level);
            buffer.addFrom (0, 0, layerBuffer, 1, 0, numSamples, 0.5f * level);
        }
    }

    buffer.applyGain (masterLevel->load());

    // Warmth: gentle tape saturation (a little uneven, for soft even harmonics) and tape's high-end roll-off.
    // Always runs through the oversampler so the plugin's latency never changes.
    if (buffer.getNumChannels() >= 2)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = oversampling.processSamplesUp (block);
        const float drive = warmth->load();
        if (drive > 0.0005f)
        {
            constexpr float g = 1.8f, bias = 0.2f;
            const float offset = std::tanh (g * bias);
            const float norm = 1.0f / (g * (1.0f - offset * offset));
            const float toneCoef = std::exp (-juce::MathConstants<float>::twoPi * (20000.0f - 11000.0f * drive) / (float) (currentSampleRate * 2.0));
            for (size_t ch = 0; ch < 2; ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                float& tone = ch == 0 ? tapeToneL : tapeToneR;
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    const float x = d[i];
                    const float sat = (std::tanh (g * (x + bias)) - offset) * norm;
                    const float y = x + drive * (sat - x);
                    tone = y + (tone - y) * toneCoef;
                    d[i] = tone;
                }
            }
        }
        oversampling.processSamplesDown (block);
    }

    sampleCounter = blockEnd;
}

juce::AudioProcessorEditor* NieblaAudioProcessor::createEditor()
{
    return new NieblaAudioProcessorEditor (*this);
}

//==============================================================================
// PRESETS
//==============================================================================
namespace
{
    struct FactoryPreset { const char* name; std::vector<std::pair<const char*, float>> values; };

    // Values are plain parameter values; choices are given as their index. Anything not listed uses its default.
    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> presets {
            { "Dawn Field", {} },

            { "G Minor Organs", {
                { "root", 7 }, { "scale", 1 },
                { "a_voice", 6 }, { "a_char1", 0.55f }, { "a_char2", 0.25f }, { "a_interval", 0 }, { "a_retardsync", 0 },  { "a_pan", -0.1f }, { "a_level", 0.65f }, { "a_delaysync", 6 }, { "a_delaymix", 0.18f }, { "a_revmix", 0.22f },
                { "b_voice", 6 }, { "b_char1", 0.3f },  { "b_char2", 0.3f },  { "b_interval", 3 }, { "b_retardsync", 4 },  { "b_pan", 0.4f },  { "b_level", 0.5f },  { "b_delaysync", 6 }, { "b_delaymix", 0.12f }, { "b_revmix", 0.22f },
                { "c_voice", 6 }, { "c_char1", 0.85f }, { "c_char2", 0.6f },  { "c_interval", 7 }, { "c_retardsync", 7 },  { "c_pan", -0.4f }, { "c_level", 0.45f }, { "c_delaysync", 8 }, { "c_delaymix", 0.22f }, { "c_revmix", 0.3f },
                { "d_voice", 6 }, { "d_char1", 0.4f },  { "d_char2", 0.1f },  { "d_interval", 0 }, { "d_retardsync", 0 },  { "d_pan", 0.0f },  { "d_level", 0.5f },  { "d_octave", -1 },   { "d_delaymix", 0.0f },  { "d_revmix", 0.12f },
                { "a_fine", 0 }, { "b_fine", 0 }, { "c_fine", 0 }, { "d_fine", 0 },
                { "a_pitchmode", 0 }, { "b_pitchmode", 0 }, { "c_pitchmode", 0 }, { "d_pitchmode", 0 } } },

            { "Glass Choir", {
                { "root", 4 }, { "scale", 4 },   // E Lydian
                { "a_voice", 5 }, { "a_char1", 0.2f }, { "a_char2", 0.8f }, { "a_attack", 3.0f }, { "a_release", 8.0f }, { "a_revmix", 0.5f },
                { "b_voice", 9 }, { "b_char1", 0.6f }, { "b_char2", 0.8f }, { "b_interval", 7 }, { "b_retardsync", 12 }, { "b_level", 0.45f }, { "b_attack", 2.0f }, { "b_sustain", 0.9f }, { "b_release", 6.0f }, { "b_pitchmode", 1 },
                { "c_voice", 1 }, { "c_char1", 0.7f }, { "c_char2", 0.6f }, { "c_interval", 12 }, { "c_retardsync", 10 }, { "c_revmix", 0.6f }, { "c_pitchmode", 0 },
                { "d_voice", 3 }, { "d_char1", 0.5f }, { "d_char2", 0.3f }, { "d_octave", -1 }, { "d_interval", 0 }, { "d_retardsync", 14 }, { "d_attack", 4.0f }, { "d_release", 10.0f }, { "d_pitchmode", 0 } } },

            { "Iron Pipe", {
                { "root", 2 }, { "scale", 1 }, { "warmth", 0.35f },   // D minor
                { "a_voice", 2 }, { "a_char1", 0.333f }, { "a_char2", 0.35f }, { "a_attack", 0.003f }, { "a_decay", 0.6f }, { "a_sustain", 0.0f }, { "a_release", 0.5f },
                { "a_revtype", 1 }, { "a_revmix", 0.5f }, { "a_revdecay", 6.0f }, { "a_revsize", 0.45f }, { "a_revdamp", 0.25f }, { "a_delaymix", 0.2f },
                { "b_voice", 1 }, { "b_char1", 0.6f }, { "b_char2", 0.3f }, { "b_interval", 7 }, { "b_retardsync", 7 }, { "b_pitchmode", 0 }, { "b_attack", 0.002f }, { "b_sustain", 0.0f },
                { "b_revtype", 1 }, { "b_revmix", 0.45f }, { "b_revdecay", 8.0f }, { "b_revsize", 0.6f },
                { "c_voice", 9 }, { "c_char1", 0.5f }, { "c_char2", 0.7f }, { "c_interval", 12 }, { "c_attack", 1.0f }, { "c_pitchmode", 1 },
                { "c_revtype", 1 }, { "c_revmix", 0.5f }, { "c_revdecay", 10.0f },
                { "d_voice", 8 }, { "d_octave", -1 }, { "d_interval", 0 }, { "d_retardsync", 0 }, { "d_pitchmode", 0 }, { "d_level", 0.5f }, { "d_revmix", 0.35f } } },

            { "Wooden Chapel", {
                { "root", 7 }, { "scale", 0 }, { "warmth", 0.5f }, { "drift", 0.3f },   // G major
                { "a_voice", 7 }, { "a_char1", 0.3f }, { "a_char2", 0.2f }, { "a_revtype", 2 }, { "a_revmix", 0.5f }, { "a_revsize", 0.7f }, { "a_revdecay", 3.0f },
                { "b_voice", 0 }, { "b_char1", 0.5f }, { "b_char2", 0.4f }, { "b_interval", 4 }, { "b_retardsync", 10 }, { "b_pitchmode", 0 }, { "b_attack", 0.4f },
                { "b_revtype", 2 }, { "b_revmix", 0.45f }, { "b_revsize", 0.7f }, { "b_revdecay", 3.0f },
                { "c_voice", 6 }, { "c_char1", 0.1f }, { "c_char2", 0.15f }, { "c_interval", 7 }, { "c_pitchmode", 0 }, { "c_attack", 0.8f },
                { "c_revtype", 2 }, { "c_revmix", 0.4f }, { "c_revdecay", 3.5f },
                { "d_voice", 8 }, { "d_octave", -1 }, { "d_interval", 0 }, { "d_retardsync", 0 }, { "d_pitchmode", 0 }, { "d_level", 0.55f },
                { "d_revtype", 2 }, { "d_revmix", 0.4f }, { "d_revdecay", 4.0f } } },

            { "Shimmer Veil", {
                { "root", 9 }, { "scale", 1 },   // A minor
                { "a_voice", 5 }, { "a_char1", 0.3f }, { "a_char2", 0.8f }, { "a_revshimmer", 0.5f }, { "a_revdecay", 12.0f }, { "a_revmix", 0.55f },
                { "b_voice", 8 }, { "b_interval", 3 }, { "b_retardsync", 12 }, { "b_pitchmode", 0 }, { "b_attack", 2.5f }, { "b_sustain", 0.8f },
                { "b_revshimmer", 0.4f }, { "b_revshift", 1 }, { "b_revdecay", 12.0f }, { "b_revmix", 0.5f },
                { "c_voice", 9 }, { "c_char2", 0.6f }, { "c_interval", 7 }, { "c_retardsync", 14 }, { "c_revmix", 0.5f },
                { "d_voice", 1 }, { "d_interval", 12 }, { "d_retardsync", 10 }, { "d_pitchmode", 0 }, { "d_revshimmer", 0.3f }, { "d_revmix", 0.6f } } },

            { "Slow Tide", {
                { "root", 2 }, { "scale", 2 },   // D Dorian
                { "a_voice", 8 }, { "a_char1", 0.8f }, { "a_char2", 0.7f }, { "a_attack", 4.0f }, { "a_release", 10.0f },
                { "b_voice", 2 }, { "b_char1", 0.12f }, { "b_char2", 0.2f }, { "b_interval", 3 }, { "b_retardsync", 14 }, { "b_pitchmode", 1 }, { "b_glidesync", 8 }, { "b_attack", 3.0f }, { "b_sustain", 0.8f }, { "b_release", 8.0f },
                { "c_voice", 0 }, { "c_char1", 0.4f }, { "c_char2", 0.5f }, { "c_interval", 7 }, { "c_retardsync", 15 }, { "c_attack", 2.0f }, { "c_pitchmode", 0 },
                { "d_voice", 9 }, { "d_char1", 0.3f }, { "d_char2", 0.5f }, { "d_octave", -1 }, { "d_interval", 0 }, { "d_level", 0.4f }, { "d_attack", 5.0f }, { "d_sustain", 1.0f }, { "d_release", 12.0f }, { "d_pitchmode", 0 } } }
        };
        return presets;
    }

    const juce::Identifier presetNameId ("presetName");
}

int NieblaAudioProcessor::getNumFactoryPresets() { return (int) factoryPresets().size(); }

juce::File NieblaAudioProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("ZOONIDO").getChildFile ("NIEBLA").getChildFile ("Presets");
}

juce::Array<juce::File> NieblaAudioProcessor::getUserPresetFiles() const
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.nieblapreset");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });
    return files;
}

juce::StringArray NieblaAudioProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factoryPresets()) names.add (p.name);
    for (auto& f : getUserPresetFiles()) names.add (f.getFileNameWithoutExtension());
    return names;
}

juce::String NieblaAudioProcessor::getCurrentPresetName() const
{
    return apvts.state.getProperty (presetNameId, "Dawn Field").toString();
}

void NieblaAudioProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void NieblaAudioProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

void NieblaAudioProcessor::loadPreset (int index)
{
    const auto names = getPresetNames();
    if (names.isEmpty()) return;
    index = (index % names.size() + names.size()) % names.size();   // wraps around

    if (index < getNumFactoryPresets())
    {
        resetToDefaults();
        for (auto& [id, value] : factoryPresets()[(size_t) index].values)
            setParam (id, value);
    }
    else
    {
        const auto file = getUserPresetFiles()[index - getNumFactoryPresets()];
        if (auto xml = juce::XmlDocument::parse (file))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    currentPreset = index;
    apvts.state.setProperty (presetNameId, names[index], nullptr);
}

bool NieblaAudioProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;

    auto folder = getUserPresetFolder();
    folder.createDirectory();
    apvts.state.setProperty (presetNameId, clean, nullptr);

    auto xml = apvts.copyState().createXml();
    const bool ok = xml != nullptr && xml->writeTo (folder.getChildFile (clean + ".nieblapreset"));
    if (ok)
        currentPreset = juce::jmax (0, getPresetNames().indexOf (clean));
    return ok;
}

void NieblaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NieblaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        currentPreset = juce::jmax (0, getPresetNames().indexOf (getCurrentPresetName()));
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NieblaAudioProcessor();
}
