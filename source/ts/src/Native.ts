import * as Juce from "juce-framework-frontend";

export default class Native {
  abortTranscription = Juce.getNativeFunction("abortTranscription");
  canCreateMarkers = Juce.getNativeFunction("canCreateMarkers");
  createMarkers = Juce.getNativeFunction("createMarkers");
  getAudioSources = Juce.getNativeFunction("getAudioSources");
  getAudioSourceTranscript = Juce.getNativeFunction("getAudioSourceTranscript");
  getModels = Juce.getNativeFunction("getModels");
  getPlayHeadState = Juce.getNativeFunction("getPlayHeadState");
  getProcessingTime = Juce.getNativeFunction("getProcessingTime");
  getRegionSequences = Juce.getNativeFunction("getRegionSequences");
  getTranscriptionStatus = Juce.getNativeFunction("getTranscriptionStatus");
  getWhisperLanguages = Juce.getNativeFunction("getWhisperLanguages");
  play = Juce.getNativeFunction("play");
  stop = Juce.getNativeFunction("stop");
  saveFile = Juce.getNativeFunction("saveFile");
  setAudioSourceTranscript = Juce.getNativeFunction("setAudioSourceTranscript");
  setDebugMode = Juce.getNativeFunction("setDebugMode");
  setPlaybackPosition = Juce.getNativeFunction("setPlaybackPosition");
  setWebState = Juce.getNativeFunction("setWebState");
  transcribeAudioSource = Juce.getNativeFunction("transcribeAudioSource");
}
