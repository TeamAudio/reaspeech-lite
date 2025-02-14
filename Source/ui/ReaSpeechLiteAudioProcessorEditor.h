#pragma once

#include <JuceHeader.h>

#include "../plugin/ReaSpeechLiteAudioProcessorImpl.h"
#include "NativeFunctions.h"
#include "Resources.h"

class ReaSpeechLiteAudioProcessorEditor final :
    public juce::AudioProcessorEditor,
    public juce::AudioProcessorEditorARAExtension
{
public:
    ReaSpeechLiteAudioProcessorEditor (ReaSpeechLiteAudioProcessorImpl& p) :
        AudioProcessorEditor (&p),
        AudioProcessorEditorARAExtension (&p),
        audioProcessor (p)
    {
        addAndMakeVisible (webComponent);

        // Navigate to index page
        webComponent.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

        // ARA plugins must be resizable for proper view embedding
        setResizable (true, false);

        // Make sure that before the constructor has finished, you've set the
        // editor's size to whatever you need it to be.
        setSize (900, 600);
    }

    ~ReaSpeechLiteAudioProcessorEditor() override {}

    void paint (juce::Graphics& g) override
    {
        // (Our component is opaque, so we must completely fill the background with a solid colour)
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        webComponent.setBounds (getLocalBounds());
    }

private:
    ReaSpeechLiteAudioProcessorImpl& audioProcessor;
    NativeFunctions nativeFunctions { *this, audioProcessor };

    juce::WebBrowserComponent webComponent {
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory)))
            .withResourceProvider ([] (const auto& url) { return Resources::get (url); })
            .withNativeIntegrationEnabled()
            .withOptionsFrom (nativeFunctions)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReaSpeechLiteAudioProcessorEditor)
};
