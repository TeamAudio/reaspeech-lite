// Mock implementation of JUCE framework
const Juce = {
  getNativeFunction: jest.fn((funcName) => {
    // Return a mock function that can be tracked
    return jest.fn();
  })
};

module.exports = Juce;
