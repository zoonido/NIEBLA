// NIEBLA - Stage 6
// Four layers (A-D). Each layer: 10 voice types with 2 character knobs, octave,
// ADSR, LP/BP/HP filter with envelope amount, level, pan, Time Retard,
// Pitch Shift (scale-snapped interval, fine, Jump/Glide), Delay and Reverb.
// Global: scale (root + type) and Force to Scale.
#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

//==============================================================================
namespace niebla
{
    bool isInScale (int midiNote, int root, int scaleIndex);
    int snapToScale (int midiNote, int root, int scaleIndex, int direction);
    double glideSyncBeats (int index);
    double delaySyncBeats (int index);
    double retardSyncBeats (int index);
    juce::StringArray noteNameList();

    enum VoiceType { flute, bells, fm, sine, saw, choir, organ, epiano, strings, air };
}

//==============================================================================
// Stereo delay with smoothed time changes and a warm (low-passed) feedback path
class NieblaDelay
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* left, float* right, int numSamples, float delaySeconds, float feedback, float mix);

private:
    juce::AudioBuffer<float> buffer;
    int writePos = 0;
    double sr = 44100.0;
    juce::SmoothedValue<float> smoothedDelay;
    float toneL = 0.0f, toneR = 0.0f;
};

//==============================================================================
// Reverb: an 8-line feedback delay network that can be a Hall, a metal Pipe or a Wood room,
// with Shimmer (pitch-shifted feedback), Freeze and low/high cut on the reverb sound
struct ReverbSettings
{
    int type = 0;            // 0 Hall, 1 Pipe, 2 Wood
    float size = 0.75f, decay = 7.0f, damping = 0.4f, mix = 0.35f;
    float shimmer = 0.0f;
    bool shimmerFifth = false, freeze = false;
    float lowCut = 20.0f, highCut = 20000.0f;
    int tuneRoot = -1;       // Pipe: pitch class to tune the pipe to, or -1 for off
};

class NieblaReverb
{
public:
    enum Type { hall, pipe, wood };

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void process (float* left, float* right, int numSamples, const ReverbSettings& s);

    static double pipeFrequency (float size, int tuneRoot);   // the note a Pipe resonates at

private:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;

    struct Line { std::vector<float> data; int writePos = 0; float lowpass = 0.0f; double lfoPhase = 0.0, lfoRate = 0.2; };
    struct Allpass { std::vector<float> data; int pos = 0; };

    std::array<Line, numLines> lines;
    std::array<Allpass, numDiffusers> diffusers;
    double sr = 44100.0;

    // Shimmer: two-head pitch shifter in the feedback path
    std::vector<float> shiftBuffer;
    int shiftWrite = 0;
    double shiftPhase = 0.0;
    float shimmerFeedback = 0.0f;
    juce::dsp::StateVariableTPTFilter<float> shimmerHighPass;

    juce::dsp::StateVariableTPTFilter<float> lowCutFilter, highCutFilter;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 3> woodBody;
};

//==============================================================================
struct NieblaSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
class NieblaVoice : public juce::SynthesiserVoice
{
public:
    NieblaVoice (juce::AudioProcessorValueTreeState& state, const juce::String& layerPrefix, const std::atomic<double>& hostBpm);

    void prepare (double sampleRate, int samplesPerBlock);

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<NieblaSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;

private:
    void updateParameters();
    void setupEngine();
    void renderEngine (double frequency, float& left, float& right);
    static double polyBlep (double t, double dt);
    double saw (int index, double increment);
    double sine (int index, double increment);

    std::atomic<float>* voiceType  = nullptr;
    std::atomic<float>* character1 = nullptr;
    std::atomic<float>* character2 = nullptr;
    std::atomic<float>* octave     = nullptr;
    std::atomic<float>* attack     = nullptr;
    std::atomic<float>* decay      = nullptr;
    std::atomic<float>* sustain    = nullptr;
    std::atomic<float>* release    = nullptr;
    std::atomic<float>* cutoff     = nullptr;
    std::atomic<float>* resonance  = nullptr;
    std::atomic<float>* filterType = nullptr;
    std::atomic<float>* filterEnv  = nullptr;
    std::atomic<float>* interval   = nullptr;
    std::atomic<float>* fine       = nullptr;
    std::atomic<float>* pitchMode  = nullptr;
    std::atomic<float>* glideMode  = nullptr;
    std::atomic<float>* glideSync  = nullptr;
    std::atomic<float>* glideTime  = nullptr;
    std::atomic<float>* glideCurve = nullptr;
    std::atomic<float>* scaleRoot  = nullptr;
    std::atomic<float>* scaleType  = nullptr;
    const std::atomic<double>& bpm;

    juce::ADSR adsr;
    juce::dsp::StateVariableTPTFilter<float> filter;            // the layer's main filter (stereo)
    std::array<juce::dsp::StateVariableTPTFilter<float>, 3> formants;   // Choir vowels
    juce::dsp::StateVariableTPTFilter<float> noiseFilter;       // Air, Flute breath
    juce::Random noise;

    // Engine state
    int type = niebla::saw;
    float c1 = 0.5f, c2 = 0.5f;
    std::array<double, 12> phases {};
    std::array<double, 3> lfoPhases {};
    std::array<double, 12> partialAmp {}, partialEnv {}, partialDecay {};
    std::atomic<float>* analogDrift = nullptr;
    double driftStaticCents = 0.0, voiceDrift = 0.0, voiceDriftTarget = 0.0;
    float cutoffDrift = 1.0f;
    float softCoef = 0.0f;
    int tick = 0;
    double sampleRate = 44100.0;
    double noteAge = 0.0;                 // seconds since this voice started
    double drift = 0.0, driftTarget = 0.0;
    float softL = 0.0f, softR = 0.0f, soft2L = 0.0f, soft2R = 0.0f;   // Strings bow softness (2-pole)
    float envValue = 0.0f;
    float baseCutoff = 1000.0f, envAmount = 0.0f;
    int filterCounter = 0;

    double phase = 0.0;
    float velocityGain = 1.0f;

    double startPitch = 60.0, targetPitch = 60.0;
    double targetFrequency = 261.6;
    double glidePosition = 1.0, glideIncrement = 0.0, glideShape = 0.0;
};

//==============================================================================
struct PendingEvent
{
    juce::int64 dueSample;
    juce::MidiMessage message;
};

struct NieblaLayer
{
    juce::Synthesiser synth;
    std::vector<PendingEvent> pending;
    std::array<int, 128> noteDelay {};
    juce::MidiBuffer blockMidi;
    bool wasOn = true;

    NieblaDelay delay;
    NieblaReverb reverb;

    std::atomic<float>* on         = nullptr;
    std::atomic<float>* level      = nullptr;
    std::atomic<float>* pan        = nullptr;
    std::atomic<float>* retardMode = nullptr;
    std::atomic<float>* retardSync = nullptr;
    std::atomic<float>* retardTime = nullptr;
    std::atomic<float>* humanize   = nullptr;
    std::atomic<float>* delayMode  = nullptr;
    std::atomic<float>* delaySync  = nullptr;
    std::atomic<float>* delayTime  = nullptr;
    std::atomic<float>* delayFeedback = nullptr;
    std::atomic<float>* delayMix   = nullptr;
    std::atomic<float>* reverbSize = nullptr;
    std::atomic<float>* reverbDecay = nullptr;
    std::atomic<float>* reverbDamping = nullptr;
    std::atomic<float>* reverbMix  = nullptr;
    std::atomic<float>* reverbType = nullptr;
    std::atomic<float>* reverbShimmer = nullptr;
    std::atomic<float>* reverbShift = nullptr;
    std::atomic<float>* reverbFreeze = nullptr;
    std::atomic<float>* reverbLowCut = nullptr;
    std::atomic<float>* reverbHighCut = nullptr;
    std::atomic<float>* reverbTune = nullptr;
};

//==============================================================================
class NieblaAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numLayers = 4;
    static constexpr int voicesPerLayer = 8;

    NieblaAudioProcessor();
    ~NieblaAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Tempo reported by the host (for the Bloom view)
    double getHostBpm() const { return currentBpm.load(); }

    // Presets: factory presets first, then the user's saved presets
    juce::StringArray getPresetNames() const;
    int getCurrentPresetIndex() const { return currentPreset; }
    juce::String getCurrentPresetName() const;
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    static juce::File getUserPresetFolder();
    static int getNumFactoryPresets();

private:
    void resetToDefaults();
    void setParam (const juce::String& id, float plainValue);
    juce::Array<juce::File> getUserPresetFiles() const;
    int currentPreset = 0;

    int computeDelaySamples (NieblaLayer& layer, double bpm);
    void queueEvent (NieblaLayer& layer, juce::int64 dueSample, const juce::MidiMessage& m);

    std::array<NieblaLayer, numLayers> layers;
    std::array<int, 128> snappedNote {};
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<float>* scaleRoot  = nullptr;
    std::atomic<float>* scaleType  = nullptr;
    std::atomic<float>* forceScale = nullptr;
    juce::AudioBuffer<float> layerBuffer;
    std::atomic<float>* masterLevel = nullptr;
    std::atomic<float>* warmth = nullptr;

    // Warmth: tape-style saturation, run at 2x sample rate to keep it clean
    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    float tapeToneL = 0.0f, tapeToneR = 0.0f;

    double currentSampleRate = 44100.0;
    juce::int64 sampleCounter = 0;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NieblaAudioProcessor)
};
