#include "OfflineRenderer.h"
#include "RenderEngine.h"

namespace ric
{

std::unique_ptr<juce::AudioFormatWriter> createWavWriter (const juce::File& file, double sampleRate,
                                                          int numChannels, int bitsPerSample)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    auto fileStream = std::make_unique<juce::FileOutputStream> (file);
    if (! fileStream->openedOk())
        return {};

    std::unique_ptr<juce::OutputStream> stream (fileStream.release());
    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (sampleRate)
                             .withNumChannels (numChannels)
                             .withBitsPerSample (bitsPerSample);
    return wav.createWriterFor (stream, options);
}

juce::AudioBuffer<float> renderScene (const Scene& scene, const SoundSettings& sound,
                                      const BounceSettings& settings,
                                      std::atomic<float>* progress, int* numHits)
{
    constexpr int block = 512;
    const double sr = settings.sampleRate;
    const double bodySeconds = settings.bars * 4.0 * 60.0 / juce::jmax (20.0, settings.bpm);
    const auto bodySamples = (int) std::ceil (bodySeconds * sr);
    const auto total = bodySamples + (int) std::ceil (settings.tailSeconds * sr);

    auto engine = std::make_unique<RenderEngine>();
    engine->prepare (sr, block);
    engine->setScene (scene);
    engine->reset();
    engine->setSound (sound);
    engine->setRunning (true);

    juce::AudioBuffer<float> out (2, total);
    out.clear();

    int hits = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        if (pos >= bodySamples)
            engine->setRunning (false); // let tails ring out
        engine->process (out.getWritePointer (0, pos), out.getWritePointer (1, pos), n,
                         settings.bpm, -1.0, nullptr, nullptr);
        hits += engine->getNumBlockHits();
        if (progress != nullptr)
            progress->store ((float) (pos + n) / (float) total);
    }

    if (numHits != nullptr) *numHits = hits;
    return out;
}

bool bounceSceneToWav (const Scene& scene, const SoundSettings& sound, const BounceSettings& settings,
                       const juce::File& file, std::atomic<float>* progress, juce::String* error)
{
    auto buffer = renderScene (scene, sound, settings, progress);
    auto writer = createWavWriter (file, settings.sampleRate);
    if (writer == nullptr)
    {
        if (error != nullptr) *error = "Could not open " + file.getFullPathName();
        return false;
    }
    if (! writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()))
    {
        if (error != nullptr) *error = "Could not write audio data";
        return false;
    }
    return true;
}

juce::File recordingsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("Ricochet").getChildFile ("Recordings");
}

juce::File newRecordingFile (const juce::String& prefix)
{
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S");
    return recordingsFolder().getNonexistentChildFile (prefix + "_" + stamp, ".wav", false);
}

} // namespace ric
