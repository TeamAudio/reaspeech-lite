#include "../source/asr/ASREngine.h"
#include "../source/asr/WhisperLanguages.h"
#include <deque>
#include <iostream>
#include <stdexcept>

namespace {
std::deque<std::string> events;
std::string returnedEvent;
const float* expectedSamples = nullptr;
juce::String expectedLanguage = "auto";
bool cancelled = false;
bool startFails = false;
bool automaticLanguage = false;
void require (bool value, const char* message) { if (! value) throw std::runtime_error (message); }
}
extern "C" bool reaspeech_start_from_memory (const float* samples, size_t sampleCount,
                                               uint32_t sampleRate, uint32_t channels,
                                               const char* optionsJson, char* id, int size)
{
    require (samples == expectedSamples && sampleCount == 1600 && sampleRate == 16000 && channels == 1,
             "Audio vector must reach the memory API directly as 16 kHz mono PCM");
    require (samples[0] == 0.25f && samples[1599] == -0.375f, "PCM samples must remain unchanged");
    const auto options = juce::JSON::parse (juce::String::fromUTF8 (optionsJson));
    require (options["model"].toString() == "small" && !static_cast<bool> (options["translate"])
             && static_cast<bool> (options["vad"]) && static_cast<bool> (options["words"]),
             "Options must reach the C API");
    automaticLanguage = !options.hasProperty ("language");
    require (expectedLanguage == "auto" ? automaticLanguage : options["language"].toString() == expectedLanguage,
             "Language must reach the C API");
    juce::String (startFails ? "Backend start failed" : "test-job").copyToUTF8 (id, size);
    return ! startFails;
}
extern "C" const char* reaspeech_poll (const char*)
{
    returnedEvent = events.empty() ? "" : events.front();
    if (!events.empty()) events.pop_front();
    return returnedEvent.c_str();
}
extern "C" int reaspeech_cancel (const char*)
{
    cancelled = true;
    events.push_back (R"({"type":"cancelled"})");
    return 1;
}
int main()
{
    try
    {
        ASREngine engine (juce::File::getSpecialLocation (juce::File::tempDirectory).getFullPathName().toStdString());
        ASROptions options { "small", "auto", false, true };
        std::vector<float> audio (1600, 0.25f);
        audio.back() = -0.375f;
        expectedSamples = audio.data();
        std::vector<ASRSegment> segments;
        events = { R"({"type":"started"})", R"({"type":"progress","completed":1,"total":2})",
                   R"({"type":"segment","segment":{"startMs":1250,"endMs":2500,"text":" Hello world ","words":[{"word":"Hello","start":1.25,"end":1.75,"probability":0.8},{"word":"world","start":1.75,"end":2.5,"probability":0.9}]}})",
                   R"({"type":"completed"})" };
        require (engine.transcribe (audio, options, segments, [] { return false; }), "Successful job must return true");
        require (automaticLanguage, "Auto language must be omitted");
        require (segments.size() == 1 && segments[0].text == "Hello world" && segments[0].start == 1.25f
                 && segments[0].end == 2.5f && segments[0].words.size() == 2 && segments[0].words[0].probability == 0.8f
                 && engine.getProgress() == 100, "Segment and word conversion");
        segments.clear();
        options.language = "fr";
        expectedLanguage = "fr";
        events = { R"({"type":"completed"})" };
        require (engine.transcribe (audio, options, segments, [] { return false; }), "Explicit language job must succeed");
        events = { R"({"type":"error","error":"Model unavailable"})" };
        require (!engine.transcribe (audio, options, segments, [] { return false; })
                 && engine.getError() == "Model unavailable" && segments.empty(), "Backend errors must propagate");
        int abortChecks = 0;
        events.clear();
        require (!engine.transcribe (audio, options, segments, [&] { return ++abortChecks >= 3; })
                 && cancelled && events.empty(), "Cancellation must drain terminal event");
        startFails = true;
        require (!engine.transcribe (audio, options, segments, [] { return false; })
                 && engine.getError() == "Backend start failed", "Start failures must propagate");
        require (WhisperLanguages::get().size() >= 99, "Language catalogue must remain available");
        std::cout << "ASR adapter checks passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
