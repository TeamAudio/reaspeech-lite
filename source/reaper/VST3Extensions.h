#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "IReaperHostApplication.h"
#include "ReaperProxy.h"

class VST3Extensions final : public juce::VST3ClientExtensions
{
public:
    explicit VST3Extensions (ReaperProxy& p) : rpr (p) {}

    void setIHostApplication (Steinberg::FUnknown* ptr) override
    {
        if (ptr == nullptr)
            return;

        void* objPtr = nullptr;

        if (ptr->queryInterface (reaper::IReaperHostApplication::iid, &objPtr) == Steinberg::kResultOk)
        {
            if (reaper::IReaperHostApplication* reaperHost = static_cast<reaper::IReaperHostApplication*> (objPtr))
            {
                rpr.load (reaperHost);
            }
        }
    }

private:
    ReaperProxy& rpr;
};
