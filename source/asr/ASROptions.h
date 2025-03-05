#pragma once

#include <juce_core/juce_core.h>

struct ASROptions
{
    juce::String modelName;
    juce::String language;
    bool translate;

    juce::String toJSON() const
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty ("modelName", modelName);
        obj->setProperty ("language", language);
        obj->setProperty ("translate", translate);
        return juce::JSON::toString (juce::var (obj));
    }
};
