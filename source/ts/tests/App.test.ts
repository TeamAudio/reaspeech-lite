import { afterEach, beforeEach, describe, expect, it, jest } from '@jest/globals';
import App from '../src/App';
import fs from 'fs';
import mockNative from './mocks/MockNative';
import path from 'path';

describe('App', () => {
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

    const htmlPath = path.resolve(__dirname, '../../../assets/index.html');
    const html = fs.readFileSync(htmlPath, 'utf8');
    document.documentElement.innerHTML = html;
  });

  afterEach(() => {
    warnSpy.mockRestore();
  });

  it('constructs App', () => {
    const app = new App();

    expect(app.state.modelName).toBe('small');
    expect(app.state.language).toBe('');
    expect(app.state.translate).toBe(false);
    expect(app.state.transcript).toBeNull();
  });

  it('loads state', () => {
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

  it('loads state with transcript', () => {
    const transcript = {
      groups: [{
        segments: [{ text: 'test', start: 0, end: 1 }],
        audioSource: { persistentID: 'audio1', name: 'Audio 1' }
      }]
    };

    window.__JUCE__.initialisationData.webState = [JSON.stringify({
      modelName: 'medium',
      language: 'fr',
      translate: true,
      transcript: transcript
    })];

    const app = new App();

    app.loadState().then(() => {
      expect(app.state.modelName).toBe('medium');
      expect(app.state.language).toBe('fr');
      expect(app.state.translate).toBe(true);
      expect(app.state.transcript).toEqual(transcript);
    });
  });

  it('handles missing state', async () => {
    window.__JUCE__.initialisationData.webState = undefined;

    const app = new App();
    await app.loadState();

    expect(app.state.language).toBe('');
    expect(app.state.translate).toBe(false);
    expect(app.state.transcript).toBeNull();

    expect(warnSpy).toHaveBeenCalled();
  });

  it('handles invalid JSON when loading state', async () => {
    window.__JUCE__.initialisationData.webState = ['invalid'];

    const app = new App();
    await app.loadState();

    expect(app.state.language).toBe('');
    expect(app.state.translate).toBe(false);
    expect(app.state.transcript).toBeNull();

    expect(warnSpy).toHaveBeenCalled();
  });

  it('saves state', async () => {
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

  it('does not crash if app.state is null when saving state', async () => {
    const app = new App();
    app.state = null;
    await app.saveState();
    expect(mockNative.setWebState).not.toHaveBeenCalled();
  });

  it('initializes models correctly', async () => {
    const mockModels = [
      { name: 'small', label: 'Small' },
      { name: 'medium', label: 'Medium' }
    ];
    mockNative.getModels.mockResolvedValue(mockModels);

    const app = new App();
    await app.initModels();

    expect(mockNative.getModels).toHaveBeenCalled();

    const select = document.getElementById('model-select') as HTMLSelectElement;
    expect(select.options[0].value).toBe('small');
    expect(select.options[0].textContent).toBe('Small');
    expect(select.options[1].value).toBe('medium');
    expect(select.options[1].textContent).toBe('Medium');
  });

  it('initializes languages correctly', async () => {
    const mockLanguages = [
      { code: 'en', name: 'english' },
      { code: 'fr', name: 'french' }
    ];
    mockNative.getWhisperLanguages.mockResolvedValue(mockLanguages);

    const app = new App();
    await app.initLanguages();

    expect(mockNative.getWhisperLanguages).toHaveBeenCalled();

    const select = document.getElementById('language-select') as HTMLSelectElement;
    expect(select.options[0].value).toBe('');
    expect(select.options[0].textContent).toBe('Detect');
    expect(select.options[1].value).toBe('en');
    expect(select.options[1].textContent).toBe('English');
    expect(select.options[2].value).toBe('fr');
    expect(select.options[2].textContent).toBe('French');
  });

  it('initializes transcript grid correctly', () => {
    const app = new App();

    app.state.transcript = {
      groups: [{
        segments: [{ text: 'test', start: 0, end: 1 }],
        audioSource: { persistentID: 'audio1', name: 'Audio 1' }
      }]
    };

    app.initTranscriptGrid();

    const rows = app.transcriptGrid.getRows();
    expect(rows.length).toBe(1);
    expect(rows[0].playbackStart).toBe(0);
    expect(rows[0].playbackEnd).toBe(1);
    expect(rows[0].text).toBe('test');
    expect(rows[0].source).toBe('Audio 1');
    expect(rows[0].sourceID).toBe('audio1');
  });

  it('handles model selection change', async () => {
    const mockModels = [
      { name: 'small', label: 'Small' },
      { name: 'medium', label: 'Medium' }
    ];
    mockNative.getModels.mockResolvedValue(mockModels);

    const app = new App();
    const mockSaveState = jest.spyOn(app, 'saveState').mockImplementation(() => Promise.resolve());
    await app.initModels();

    const select = document.getElementById('model-select') as HTMLSelectElement;
    select.selectedIndex = 1;

    await app.handleModelChange();

    expect(app.state.modelName).toBe('medium');
    expect(mockSaveState).toHaveBeenCalled();

    mockSaveState.mockRestore();
  });

  it('handles language selection change', async () => {
    const mockLanguages = [
      { code: 'en', name: 'english' },
      { code: 'fr', name: 'french' }
    ];
    mockNative.getWhisperLanguages.mockResolvedValue(mockLanguages);

    const app = new App();
    const mockSaveState = jest.spyOn(app, 'saveState').mockImplementation(() => Promise.resolve());
    await app.initLanguages();

    const select = document.getElementById('language-select') as HTMLSelectElement;
    select.selectedIndex = 2;

    await app.handleLanguageChange();

    expect(app.state.language).toBe('fr');
    expect(mockSaveState).toHaveBeenCalled();

    mockSaveState.mockRestore();
  });

  it('handles translate checkbox change', async () => {
    const app = new App();
    const mockSaveState = jest.spyOn(app, 'saveState').mockImplementation(() => Promise.resolve());

    const checkbox = document.getElementById('translate-checkbox') as HTMLInputElement;
    checkbox.checked = true;

    await app.handleTranslateChange();

    expect(app.state.translate).toBe(true);
    expect(mockSaveState).toHaveBeenCalled();

    mockSaveState.mockRestore();
  });

  it('handles process button click', async () => {
    const app = new App();

    (app as any).transcriptGrid = {
      addSegments: jest.fn(),
      clear: jest.fn()
    };

    const audioSource1 = { persistentID: 'audio1', name: 'Audio 1' };
    const audioSource2 = { persistentID: 'audio2', name: 'Audio 2' };

    mockNative.getAudioSources.mockResolvedValue([
      audioSource1,
      audioSource2
    ]);

    const segments = [{ text: 'test', start: 0, end: 1 }];

    mockNative.transcribeAudioSource.mockResolvedValue({ segments });

    await app.handleProcess();

    expect(app.transcriptGrid.clear).toHaveBeenCalled();
    expect(app.transcriptGrid.addSegments).toHaveBeenCalledTimes(2);
    expect(app.transcriptGrid.addSegments).toHaveBeenCalledWith(segments, audioSource1);
    expect(app.transcriptGrid.addSegments).toHaveBeenCalledWith(segments, audioSource2);
    expect(app.state.transcript).toEqual({
      groups: [
        { segments, audioSource: audioSource1 },
        { segments, audioSource: audioSource2 }
      ]
    });
  });

  it('handles process errors', async () => {
    const app = new App();

    (app as any).transcriptGrid = {
      addSegments: jest.fn(),
      clear: jest.fn()
    };

    const audioSource = { persistentID: 'audio1', name: 'Audio 1' };
    mockNative.getAudioSources.mockResolvedValue([audioSource]);

    const error = 'Test error';
    mockNative.transcribeAudioSource.mockResolvedValue({ error });

    await app.handleProcess();

    expect(app.transcriptGrid.clear).toHaveBeenCalled();
    expect(app.transcriptGrid.addSegments).not.toHaveBeenCalled();
    expect(app.state.transcript).toBeNull();

    const alerts = document.getElementById('alerts') as HTMLElement;
    expect(alerts.innerHTML).toContain(error);
  });

  it('clears transcript correctly', async () => {
    const app = new App();

    (app as any).transcriptGrid = {
      clear: jest.fn()
    };

    app.state.transcript = { groups: [] };

    await app.clearTranscript();

    expect(app.transcriptGrid.clear).toHaveBeenCalled();
    expect(app.state.transcript).toBeNull();
  });

  it('creates markers correctly', async () => {
    const app = new App();

    (app as any).transcriptGrid = {
      getRows: jest.fn().mockReturnValue([
        { playbackStart: 0, playbackEnd: 1, text: 'Test 1' },
        { playbackStart: 2, playbackEnd: 3, text: 'Test 2' }
      ])
    };

    await app.handleCreateMarkers('markers');

    expect(mockNative.createMarkers).toHaveBeenCalledWith([
      { start: 0, end: 1, name: 'Test 1' },
      { start: 2, end: 3, name: 'Test 2' }
    ], 'markers');
  });

  it('does not call createMarkers if there are no rows', async () => {
    const app = new App();

    (app as any).transcriptGrid = {
      getRows: jest.fn().mockReturnValue([])
    };

    await app.handleCreateMarkers('markers');

    expect(mockNative.createMarkers).not.toHaveBeenCalled();
  });

  it('handles errors creating markers', async () => {
    const app = new App();

    (app as any).transcriptGrid = {
      getRows: jest.fn().mockReturnValue([
        { playbackStart: 0, playbackEnd: 1, text: 'Test 1' }
      ])
    };

    const error = 'Test error';
    mockNative.createMarkers.mockResolvedValue({ error });

    await app.handleCreateMarkers('markers');

    const alerts = document.getElementById('alerts') as HTMLElement;
    expect(alerts.innerHTML).toContain(error);
  });

  it('plays at a given time', async () => {
    const app = new App();
    const seconds = 10;

    await app.playAt(seconds);

    expect(mockNative.stop).toHaveBeenCalled();
    expect(mockNative.setPlaybackPosition).toHaveBeenCalledWith(seconds);
    expect(mockNative.play).toHaveBeenCalled();
  });
});
