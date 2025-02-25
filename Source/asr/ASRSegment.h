#pragma once

#include <JuceHeader.h>

struct ASRWord
{
    juce::String text;
    float start;
    float end;
    float probability;

    juce::DynamicObject::Ptr toDynamicObject() const
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();

        obj->setProperty ("text", this->text);
        obj->setProperty ("start", this->start);
        obj->setProperty ("end", this->end);
        obj->setProperty ("probability", this->probability);

        return obj;
    }
};

struct ASRSegment
{
    juce::String text;
    float start;
    float end;
    juce::Array<ASRWord> words;

    juce::DynamicObject::Ptr toDynamicObject(bool withWords) const
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();

        obj->setProperty ("text", this->text);
        obj->setProperty ("start", this->start);
        obj->setProperty ("end", this->end);
        obj->setProperty ("score", this->score());

        if (withWords)
        {
            juce::Array<juce::var> wordsArray;
            for (const auto& word : this->words)
                wordsArray.add (word.toDynamicObject().get());
            obj->setProperty ("words", wordsArray);
        }

        return obj;
    }

    float score() const
    {
        if (this->words.isEmpty())
            return 0.0f;
        float score = 0.0f;
        for (const auto& word : this->words)
            score += word.probability;
        return score / this->words.size();
    }
};
