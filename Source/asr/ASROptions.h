#pragma once

#include <JuceHeader.h>

struct ASROptions
{
    juce::String language;
    bool translate;

    juce::String toJSON() const
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty ("language", language);
        obj->setProperty ("translate", translate);
        return juce::JSON::toString (juce::var (obj));
    }
};
