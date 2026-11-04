#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>

/** Streams the plug-in output to a WAV file on a background thread. */
class Recorder
{
public:
    Recorder();
    ~Recorder();

    void prepare (double sampleRate);

    /** Message thread. Returns false if the file could not be created. */
    bool start();
    /** Message thread. Returns the finished file (or an empty File). */
    juce::File stop();

    bool isRecording() const noexcept { return active.load() != nullptr; }
    double getRecordedSeconds() const noexcept { return (double) samplesWritten.load() / sampleRate; }
    juce::File getCurrentFile() const { return currentFile; }

    /** Audio thread. */
    void process (const float* left, const float* right, int numSamples) noexcept;

private:
    juce::TimeSliceThread writerThread { "Ricochet recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threaded;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> active { nullptr };
    juce::SpinLock writerLock;
    std::atomic<juce::int64> samplesWritten { 0 };
    double sampleRate = 48000.0;
    juce::File currentFile;
};
