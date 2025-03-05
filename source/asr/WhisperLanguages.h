#pragma once

#include <juce_core/juce_core.h>
#include <whisper.h>

class WhisperLanguages
{
public:
    static const juce::Array<juce::var>& get()
    {
        static juce::Array<juce::var> languages = createLanguageArray();
        return languages;
    }

private:
    static juce::Array<juce::var> createLanguageArray()
    {
        juce::Array<juce::var> result;
        const int maxId = whisper_lang_max_id();

        for (int id = 0; id <= maxId; ++id)
        {
            juce::DynamicObject::Ptr langObj = new juce::DynamicObject();
            langObj->setProperty ("id", id);
            langObj->setProperty ("code", juce::String(whisper_lang_str(id)));
            langObj->setProperty ("name", juce::String(whisper_lang_str_full(id)));
            result.add (langObj.get());
        }

        struct LanguageComparator
        {
            int compareElements (const juce::var& a, const juce::var& b)
            {
                return a["name"].toString().compare (b["name"].toString());
            }
        };

        LanguageComparator comparator;
        result.sort(comparator);

        return result;
    }

    // Prevent instantiation
    WhisperLanguages() = delete;
    ~WhisperLanguages() = delete;
};
