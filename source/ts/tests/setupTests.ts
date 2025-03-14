// This file runs before tests
// @ts-nocheck

// Mock modules
jest.mock('juce-framework-frontend');

// Access the mock without using .default
global.Juce = jest.requireMock('juce-framework-frontend');

// Essential DOM mocking
global.document = {
  createElement: jest.fn(() => ({
    style: {},
    addEventListener: jest.fn(),
    removeEventListener: jest.fn(),
    appendChild: jest.fn()
  })),
  body: {
    appendChild: jest.fn(),
    removeChild: jest.fn()
  },
  querySelector: jest.fn(),
  querySelectorAll: jest.fn(),
  addEventListener: jest.fn(),
  removeEventListener: jest.fn()
} as unknown as Document;

// Minimal window object
global.window = {
  addEventListener: jest.fn(),
  removeEventListener: jest.fn(),
  document: global.document,
  innerWidth: 1024,
  innerHeight: 768,
  getComputedStyle: jest.fn()
} as unknown as Window & typeof globalThis;
