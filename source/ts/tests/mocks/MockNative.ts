// Provides mock implementations for native functions
// @ts-nocheck

export class MockNative {
  private mockFunctions: Map<string, jest.Mock> = new Map();

  // Common native functions exposed as properties
  public getModels: jest.Mock;
  public setWebState: jest.Mock;
  // Add other common functions here

  constructor() {
    // Setup the global Juce.getNativeFunction mock
    this.setupJuceNativeFunctionMock();

    // Initialize common mocks (without setting defaults yet)
    this.getModels = this.createMock('getModels');
    this.setWebState = this.createMock('setWebState');

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
    this.getModels.mockReturnValue(Promise.resolve([]));
    this.setWebState.mockReturnValue(Promise.resolve());

    // Add defaults for any other mocks here
  }
}

// Create and export a singleton instance
const mockNative = new MockNative();
export default mockNative;
