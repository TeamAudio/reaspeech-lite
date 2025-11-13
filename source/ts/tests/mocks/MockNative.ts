// Provides mock implementations for native functions
// @ts-nocheck

export class MockNative {
  private mockFunctions: Map<string, jest.Mock> = new Map();

  // Common native functions exposed as properties
  public abortTranscription: jest.Mock;
  public canCreateMarkers: jest.Mock;
  public createMarkers: jest.Mock;
  public getAudioSources: jest.Mock;
  public getAudioSourceTranscript: jest.Mock;
  public getModels: jest.Mock;
  public getPlayHeadState: jest.Mock;
  public getProcessingTime: jest.Mock;
  public getRegionSequences: jest.Mock;
  public getTranscriptionStatus: jest.Mock;
  public getWhisperLanguages: jest.Mock;
  public play: jest.Mock;
  public setAudioSourceTranscript: jest.Mock;
  public setDebugMode: jest.Mock;
  public setPlaybackPosition: jest.Mock;
  public setWebState: jest.Mock;
  public stop: jest.Mock;
  public transcribeAudioSource: jest.Mock;

  constructor() {
    // Setup the global Juce.getNativeFunction mock
    this.setupJuceNativeFunctionMock();

    // Initialize common mocks (without setting defaults yet)
    this.abortTranscription = this.createMock('abortTranscription');
    this.canCreateMarkers = this.createMock('canCreateMarkers');
    this.createMarkers = this.createMock('createMarkers');
    this.getAudioSources = this.createMock('getAudioSources');
    this.getAudioSourceTranscript = this.createMock('getAudioSourceTranscript');
    this.getModels = this.createMock('getModels');
    this.getPlayHeadState = this.createMock('getPlayHeadState');
    this.getProcessingTime = this.createMock('getProcessingTime');
    this.getRegionSequences = this.createMock('getRegionSequences');
    this.getTranscriptionStatus = this.createMock('getTranscriptionStatus');
    this.getWhisperLanguages = this.createMock('getWhisperLanguages');
    this.play = this.createMock('play');
    this.setAudioSourceTranscript = this.createMock('setAudioSourceTranscript');
    this.setDebugMode = this.createMock('setDebugMode');
    this.setPlaybackPosition = this.createMock('setPlaybackPosition');
    this.setWebState = this.createMock('setWebState');
    this.stop = this.createMock('stop');
    this.transcribeAudioSource = this.createMock('transcribeAudioSource');

    // Initialize all mocks with their default values
    this.reset();
  }

  /**
   * Set up the global Juce.getNativeFunction mock to return our mocks
   */
  private setupJuceNativeFunctionMock(): void {
    if (global.Juce && typeof jest.fn === 'function') {
      global.Juce.getNativeFunction = jest.fn((name: string) => {
        // Return an existing mock if we have one
        if (this.mockFunctions.has(name)) {
          return this.mockFunctions.get(name);
        }

        // Create a new mock on demand if requested
        return this.createMock(name);
      });
    }
  }

  /**
   * Create a mock function for a native function
   * @param functionName - Name of the native function to mock
   * @returns The mock function
   */
  createMock(functionName: string): jest.Mock {
    const mockFn = jest.fn();
    this.mockFunctions.set(functionName, mockFn);
    return mockFn;
  }

  /**
   * Get a mock function by name (for functions not exposed as properties)
   * @param functionName - Name of the native function
   * @returns The mock function or undefined if not mocked
   */
  getMock(functionName: string): jest.Mock | undefined {
    return this.mockFunctions.get(functionName);
  }

  /**
   * Reset all mocks to their initial state
   */
  reset(): void {
    // Clear call history for all mocks
    this.mockFunctions.forEach((mockFn) => {
      mockFn.mockClear();
    });

    // Set default implementations for common mocks
    this.abortTranscription.mockReturnValue(Promise.resolve(true));
    this.canCreateMarkers.mockReturnValue(Promise.resolve(true));
    this.createMarkers.mockReturnValue(Promise.resolve());
    this.getAudioSources.mockReturnValue(Promise.resolve([]));
    this.getAudioSourceTranscript.mockReturnValue(Promise.resolve({}));
    this.getModels.mockReturnValue(Promise.resolve([]));
    this.getPlayHeadState.mockReturnValue(Promise.resolve({"timeInSeconds": 0, "isPlaying": false}));
    this.getProcessingTime.mockReturnValue(Promise.resolve(0));
    this.getRegionSequences.mockReturnValue(Promise.resolve([]));
    this.getTranscriptionStatus.mockReturnValue(Promise.resolve({"status": "", "progress": 0}));
    this.getWhisperLanguages.mockReturnValue(Promise.resolve([]));
    this.play.mockReturnValue(Promise.resolve());
    this.setAudioSourceTranscript.mockReturnValue(Promise.resolve());
    this.setDebugMode.mockReturnValue(Promise.resolve());
    this.setPlaybackPosition.mockReturnValue(Promise.resolve());
    this.setWebState.mockReturnValue(Promise.resolve());
    this.stop.mockReturnValue(Promise.resolve());
    this.transcribeAudioSource.mockReturnValue(Promise.resolve({"segments": []}));
  }
}

// Create and export a singleton instance
const mockNative = new MockNative();
export default mockNative;
