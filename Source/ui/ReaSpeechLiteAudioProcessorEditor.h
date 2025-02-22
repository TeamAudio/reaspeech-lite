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
        AudioProcessorEditorARAExtension (&p)
    {
        if (auto* editorView = getARAEditorView())
        {
            nativeFunctions = std::make_unique<NativeFunctions> (*editorView, p);

            webComponent = std::make_unique<juce::WebBrowserComponent> (
                juce::WebBrowserComponent::Options{}
                    .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                    .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                        .withUserDataFolder (juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory)))
                    .withResourceProvider ([] (const auto& url) { return Resources::get (url); })
                    .withNativeIntegrationEnabled()
                    .withOptionsFrom (*nativeFunctions)
            );
            addAndMakeVisible (*webComponent);

            // Navigate to index page
            webComponent->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
        }

        // ARA plugins must be resizable for proper view embedding
        setResizable (true, false);

        // Make sure that before the constructor has finished, you've set the
        // editor's size to whatever you need it to be.
        setSize (900, 600);
    }

    ~ReaSpeechLiteAudioProcessorEditor() override {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

        if (! isARAEditorView())
        {
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (15.0f));
            g.drawFittedText ("ARA host isn't detected. This plugin only supports ARA mode.",
                              getLocalBounds(),
                              juce::Justification::centred,
                              1);
        }
    }

    void resized() override
    {
        if (webComponent != nullptr)
            webComponent->setBounds (getLocalBounds());
    }

private:
    std::unique_ptr<NativeFunctions> nativeFunctions;
    std::unique_ptr<juce::WebBrowserComponent> webComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReaSpeechLiteAudioProcessorEditor)
};
