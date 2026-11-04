#include "Recorder.h"
#include "Core/OfflineRenderer.h"

Recorder::Recorder()
{
    writerThread.startThread();
}

Recorder::~Recorder()
{
    stop();
    writerThread.stopThread (2000);
}

void Recorder::prepare (double newSampleRate)
{
    if (! isRecording())
        sampleRate = newSampleRate;
}

bool Recorder::start()
{
    stop();

    auto file = ric::newRecordingFile ("RecklessRicochet");
    auto writer = ric::createWavWriter (file, sampleRate);
    if (writer == nullptr)
        return false;

    samplesWritten = 0;
    currentFile = file;
    threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, 65536);

    const juce::SpinLock::ScopedLockType sl (writerLock);
    active = threaded.get();
    return true;
}

juce::File Recorder::stop()
{
    {
        const juce::SpinLock::ScopedLockType sl (writerLock);
        active = nullptr;
    }

    if (threaded == nullptr)
        return {};

    threaded.reset(); // flushes and closes the file
    return currentFile;
}

void Recorder::process (const float* left, const float* right, int numSamples) noexcept
{
    const juce::SpinLock::ScopedTryLockType sl (writerLock);
    if (! sl.isLocked())
        return;

    if (auto* w = active.load())
    {
        const float* channels[] { left, right };
        if (w->write (channels, numSamples))
            samplesWritten += numSamples;
    }
}
