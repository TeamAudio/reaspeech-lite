import { afterEach, beforeEach, expect, jest, test } from '@jest/globals';
import App from '../src/App';
import mockNative from './mocks/MockNative';

// Spy on console.warn
let warnSpy: any;

beforeEach(() => {
  mockNative.reset();
  warnSpy = jest.spyOn(console, 'warn').mockImplementation(() => {});

  window.__JUCE__ = {
    initialisationData: {
      webState: ['']
    }
  };
});

afterEach(() => {
  warnSpy.mockRestore();
});

test('constructs App', () => {
  const app = new App();
  expect(app.state.modelName).toBe('small');
  expect(app.state.language).toBe('');
  expect(app.state.translate).toBe(false);
  expect(app.state.transcript).toBeNull();
});

test('loads state', () => {
  window.__JUCE__.initialisationData.webState = [JSON.stringify({
    modelName: 'medium',
    language: 'fr',
    translate: true,
    transcript: null
  })];
  const app = new App();
  app.loadState().then(() => {
    expect(app.state.modelName).toBe('medium');
    expect(app.state.language).toBe('fr');
    expect(app.state.translate).toBe(true);
    expect(app.state.transcript).toBeNull();
  });
});

test('handles initial webState when loading state', async () => {
  const app = new App();
  await app.loadState();
  expect(app.state.modelName).toBe('small');
  expect(app.state.language).toBe('');
  expect(app.state.translate).toBe(false);
  expect(app.state.transcript).toBeNull();
});

test('handles invalid JSON when loading state', async () => {
  window.__JUCE__.initialisationData.webState = ['invalid'];
  const app = new App();
  await app.loadState();
  expect(app.state.modelName).toBe('small');
  expect(app.state.language).toBe('');
  expect(app.state.translate).toBe(false);
  expect(app.state.transcript).toBeNull();
  expect(warnSpy).toHaveBeenCalled();
});

test('saves state', async () => {
  const app = new App();
  app.state.modelName = 'large';
  app.state.language = 'de';

  let newStateJSON = '';
  mockNative.setWebState.mockImplementation((state: string) => {
    newStateJSON = state;
    return Promise.resolve();
  });

  await app.saveState();
  const newState = JSON.parse(newStateJSON);
  expect(newState.modelName).toBe('large');
  expect(newState.language).toBe('de');
});
