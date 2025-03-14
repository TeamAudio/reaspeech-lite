import * as Juce from "juce-framework-frontend";

export default class Native {
  canCreateMarkers = Juce.getNativeFunction("canCreateMarkers");
  createMarkers = Juce.getNativeFunction("createMarkers");
  getAudioSources = Juce.getNativeFunction("getAudioSources");
  getModels = Juce.getNativeFunction("getModels");
  getRegionSequences = Juce.getNativeFunction("getRegionSequences");
  getTranscriptionStatus = Juce.getNativeFunction("getTranscriptionStatus");
  getWhisperLanguages = Juce.getNativeFunction("getWhisperLanguages");
  play = Juce.getNativeFunction("play");
  stop = Juce.getNativeFunction("stop");
  setPlaybackPosition = Juce.getNativeFunction("setPlaybackPosition");
  setWebState = Juce.getNativeFunction("setWebState");
  transcribeAudioSource = Juce.getNativeFunction("transcribeAudioSource");
}
