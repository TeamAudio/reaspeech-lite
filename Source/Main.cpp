#include <JuceHeader.h>

#include "ara/ReaSpeechLiteDocumentController.h"
#include "plugin/ReaSpeechLiteAudioProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReaSpeechLiteAudioProcessor();
}

const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<ReaSpeechLiteDocumentController>();
}
