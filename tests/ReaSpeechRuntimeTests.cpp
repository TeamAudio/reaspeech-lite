#include <reaspeech.h>
#include <filesystem>
#include <iostream>
#include <string>

int main()
{
    const auto missing = (std::filesystem::temp_directory_path() / "reaspeech-api-test-missing" / "audio.wav").string();
    if (std::filesystem::exists (missing)) return 1;
    char error[512] {};
    if (reaspeech_start (missing.c_str(), "small", nullptr, false, false, true, nullptr, error, sizeof (error))
        || std::string (error).find ("audio_path") == std::string::npos)
    {
        std::cerr << "C API did not reject missing audio: " << error << '\n';
        return 1;
    }
    const float samples[] { 0.25f, -0.375f };
    if (reaspeech_start_from_memory (samples, 2, 0, 1, R"({"model":"small"})", error, sizeof (error))
        || std::string (error).find ("sample_rate") == std::string::npos)
    {
        std::cerr << "Memory C API did not reject invalid sample rate: " << error << '\n';
        return 1;
    }
    std::cout << "ReaSpeech static C API linked and validated input\n";
    return 0;
}
