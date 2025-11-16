import { afterEach, beforeEach, describe, expect, it, jest } from '@jest/globals';
import App from '../src/App';
import TranscriptGrid from '../src/TranscriptGrid';
import fs from 'fs';
import mockNative from './mocks/MockNative';
import path from 'path';

describe('App', () => {
  // Spy on console.warn
  let warnSpy: any;

  beforeEach(() => {
    jest.useFakeTimers();
    mockNative.reset();
    warnSpy = jest.spyOn(console, 'warn').mockImplementation(() => {});

    window.__JUCE__ = {
      backend: {
        addEventListener: jest.fn(),
      },
      initialisationData: {
        webState: ['']
      }
    };

    const htmlPath = path.resolve(__dirname, '../../../assets/index.html');
    const html = fs.readFileSync(htmlPath, 'utf8');
    document.documentElement.innerHTML = html;
  });

  afterEach(() => {
    jest.useRealTimers();
    warnSpy.mockRestore();
  });

  describe('initialization', () => {
    it('constructs App', () => {
      const app = new App();

      expect(app.state.modelName).toBe('parakeet-tdt-0.6b-v3');
      expect(app.state.language).toBe('');
      expect(app.state.translate).toBe(false);
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

    it('initializes transcript grid correctly', async () => {
      const app = new App();

      mockNative.getAudioSourceTranscript.mockResolvedValue({
        segments: [{ text: 'test', start: 0, end: 1 }]
      });

      mockNative.getAudioSources.mockResolvedValue([
        { persistentID: 'audio1', name: 'Audio 1' }
      ]);

      await app.initTranscript();

      const rows = app.transcriptGrid.getRows();
      expect(rows.length).toBe(1);
      expect(rows[0].playbackStart).toBe(0);
      expect(rows[0].playbackEnd).toBe(1);
      expect(rows[0].text).toBe('test');
      expect(rows[0].source).toBe('Audio 1');
      expect(rows[0].sourceID).toBe('audio1');
    });
  });

  describe('state management', () => {
    it('loads state', () => {
      window.__JUCE__.initialisationData.webState = [JSON.stringify({
        modelName: 'medium',
        language: 'fr',
        translate: true,
      })];

      const app = new App();

      app.loadState().then(() => {
        expect(app.state.modelName).toBe('medium');
        expect(app.state.language).toBe('fr');
        expect(app.state.translate).toBe(true);
      });
    });

    it('handles missing state', async () => {
      window.__JUCE__.initialisationData.webState = undefined;

      const app = new App();
      await app.loadState();

      expect(app.state.language).toBe('');
      expect(app.state.translate).toBe(false);
    });

    it('handles invalid JSON when loading state', async () => {
      window.__JUCE__.initialisationData.webState = ['invalid'];

      const app = new App();
      await app.loadState();

      expect(app.state.language).toBe('');
      expect(app.state.translate).toBe(false);

      expect(warnSpy).toHaveBeenCalled();

      const alerts = document.getElementById('alerts') as HTMLElement;
      expect(alerts.innerHTML).toContain('Failed to read project data');
    });

    it('migrates transcripts from state to audio sources', async () => {
      window.__JUCE__.initialisationData.webState = [JSON.stringify({
        transcript: {
        groups: [{
          segments: [{ text: 'test', start: 0, end: 1 }],
          audioSource: { persistentID: 'audio1', name: 'Audio 1' }
        }]
      }})];

      const app = new App();
      await app.loadState();

      expect(mockNative.setAudioSourceTranscript).toHaveBeenCalledWith('audio1', {
        segments: [{ text: 'test', start: 0, end: 1 }]
      });
      expect(app.state.transcript).toBeUndefined();
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
  });

  describe('event handling', () => {
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

    it('handles audio source addition', async () => {
      const app = new App();

      (app as any).audioSourceGrid = {
        addRows: jest.fn(),
        clear: jest.fn(),
        setRowSelected: jest.fn(),
        getSelectedRowIds: jest.fn().mockReturnValue(['audio1']),
        setSelectedRowIds: jest.fn(),
      };

      mockNative.getAudioSources.mockResolvedValue([
        { persistentID: 'audio1', name: 'Audio 1' },
        { persistentID: 'audio2', name: 'Audio 2' }
      ]);

      await app.handleAudioSourceAdded({persistentID: 'audio2'});

      expect(app.audioSourceGrid.clear).toHaveBeenCalled();
      expect(app.audioSourceGrid.addRows).toHaveBeenCalledWith([
        { persistentID: 'audio1', name: 'Audio 1' },
        { persistentID: 'audio2', name: 'Audio 2' }
      ]);
      expect(app.audioSourceGrid.setSelectedRowIds).toHaveBeenCalledWith(['audio1']);
      expect(app.audioSourceGrid.setRowSelected).toHaveBeenCalledWith('audio2', true);
    });

    it('handles audio source removal', async () => {
      const app = new App();

      (app as any).audioSourceGrid = {
        addRows: jest.fn(),
        clear: jest.fn(),
        setRowSelected: jest.fn(),
        getSelectedRowIds: jest.fn().mockReturnValue(['audio1', 'audio2']),
        setSelectedRowIds: jest.fn(),
      };

      mockNative.getAudioSources.mockResolvedValue([
        { persistentID: 'audio1', name: 'Audio 1' }
      ]);

      await app.handleAudioSourceRemoved({persistentID: 'audio2'});

      expect(app.audioSourceGrid.clear).toHaveBeenCalled();
      expect(app.audioSourceGrid.addRows).toHaveBeenCalledWith([
        { persistentID: 'audio1', name: 'Audio 1' }
      ]);
      expect(app.audioSourceGrid.setSelectedRowIds).toHaveBeenCalledWith(['audio1']);
      expect(app.audioSourceGrid.setRowSelected).not.toHaveBeenCalled();
    });

    it('handles audio source update', async () => {
      const app = new App();

      (app as any).audioSourceGrid = {
        addRows: jest.fn(),
        clear: jest.fn(),
        setRowSelected: jest.fn(),
        getSelectedRowIds: jest.fn().mockReturnValue(['audio1']),
        setSelectedRowIds: jest.fn(),
      };

      mockNative.getAudioSourceTranscript
        .mockResolvedValueOnce({})
        .mockResolvedValueOnce({
          segments: [{ text: 'test', start: 0, end: 1 }]
        });

      mockNative.getAudioSources.mockResolvedValue([
        { persistentID: 'audio1', name: 'Audio 1' }
      ]);

      await app.initTranscript();

      const initRows = app.transcriptGrid.getRows();
      expect(initRows.length).toBe(0);

      await app.handleAudioSourceUpdated({ persistentID: 'audio1' });

      const rows = app.transcriptGrid.getRows();
      expect(rows.length).toBe(1);
      expect(rows[0].playbackStart).toBe(0);
      expect(rows[0].playbackEnd).toBe(1);
      expect(rows[0].text).toBe('test');
      expect(rows[0].source).toBe('Audio 1');
      expect(rows[0].sourceID).toBe('audio1');

      expect(app.audioSourceGrid.clear).toHaveBeenCalled();
      expect(app.audioSourceGrid.addRows).toHaveBeenCalledWith([
        { persistentID: 'audio1', name: 'Audio 1' }
      ]);
      // First, audio1 is expected to be selected, as we restore selection state
      expect(app.audioSourceGrid.setSelectedRowIds).toHaveBeenCalledWith(['audio1']);
      // Then, audio1 is expected to be deselected, since its transcription is done
      expect(app.audioSourceGrid.setRowSelected).toHaveBeenCalledWith('audio1', false);
    });
  });

  describe('live updates', () => {
    it('updates transcription status', async () => {
      const app = new App();
      const mockSetProcessText = jest.spyOn(app, 'setProcessText').mockImplementation(() => {});

      mockNative.getTranscriptionStatus.mockResolvedValue({
        status: 'Processing',
        progress: 75
      });

      app.processing = true;
      await app.updateTranscriptionStatus();

      expect(mockNative.getTranscriptionStatus).toHaveBeenCalled();
      expect(mockSetProcessText).toHaveBeenCalledWith('Processing...');

      const progress = document.getElementById('progress');
      if (progress) {
        const progressBar = progress.querySelector('.progress-bar') as HTMLElement;
        expect(progress.getAttribute('aria-valuenow')).toBe('75');
        expect(progressBar.style.width).toBe('75%');
      } else {
        throw new Error('Progress element not found');
      }

      mockSetProcessText.mockRestore();
    });

    it('does not update process text when status is empty', async () => {
      const app = new App();
      const mockSetProcessText = jest.spyOn(app, 'setProcessText').mockImplementation(() => {});

      mockNative.getTranscriptionStatus.mockResolvedValue({
        status: '',
        progress: 0
      });

      app.processing = true;
      await app.updateTranscriptionStatus();

      expect(mockNative.getTranscriptionStatus).toHaveBeenCalled();
      expect(mockSetProcessText).not.toHaveBeenCalled();

      mockSetProcessText.mockRestore();
    });

    it('resets progress bar when not processing', async () => {
      const app = new App();
      const progress = document.getElementById('progress');
      const progressBar = progress?.querySelector('.progress-bar') as HTMLElement;

      if (!progress || !progressBar) {
        throw new Error('Progress element or progress bar not found');
      }

      progress.setAttribute('aria-valuenow', '50');
      progressBar.style.width = '50%';

      app.processing = false;

      await app.updateTranscriptionStatus();

      expect(progress.getAttribute('aria-valuenow')).toBe('0');
      expect(progressBar.style.width).toBe('0%');
    });

    it('updates playback regions', async () => {
      const app = new App();

      app.transcriptGrid = {
        setPlaybackRegionMap: jest.fn(),
        setPlaybackPosition: jest.fn()
      } as unknown as TranscriptGrid;

      const mockRegionSequences = [
        {
          name: 'Track 1',
          orderIndex: 0,
          playbackRegions: [
            {
              playbackStart: 0,
              playbackEnd: 1,
              modificationStart: 0,
              modificationEnd: 1,
              audioSourcePersistentID: 'audio1'
            }
          ]
        }
      ];

      mockNative.getRegionSequences.mockResolvedValue(mockRegionSequences);
      mockNative.getPlayHeadState.mockResolvedValue({ timeInSeconds: 0.5, isPlaying: true });

      await app.updatePlaybackRegions();

      expect(mockNative.getRegionSequences).toHaveBeenCalled();
      expect(mockNative.getPlayHeadState).toHaveBeenCalled();

      expect(app.transcriptGrid.setPlaybackRegionMap).toHaveBeenCalled();
      expect(app.transcriptGrid.setPlaybackPosition).toHaveBeenCalledWith(0.5, true);
    });

    it('integrates update method correctly', async () => {
      const app = new App();

      const mockUpdateTranscriptionStatus = jest.spyOn(app, 'updateTranscriptionStatus')
        .mockImplementation(() => Promise.resolve());
      const mockUpdatePlaybackRegions = jest.spyOn(app, 'updatePlaybackRegions')
        .mockImplementation(() => Promise.resolve());

      await app.update();

      expect(mockUpdateTranscriptionStatus).toHaveBeenCalled();
      expect(mockUpdatePlaybackRegions).toHaveBeenCalled();

      mockUpdateTranscriptionStatus.mockRestore();
      mockUpdatePlaybackRegions.mockRestore();
    });
  });

  describe('transcription', () => {
    it('handles process button click', async () => {
      const app = new App();

      (app as any).audioSourceGrid = {
        getSelectedRowIds: jest.fn().mockReturnValue(['audio1', 'audio2']),
      };

      (app as any).transcriptGrid = {
        addSegments: jest.fn(),
        clear: jest.fn(),
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

      expect(mockNative.setAudioSourceTranscript).toHaveBeenCalledWith('audio1', { segments });
      expect(mockNative.setAudioSourceTranscript).toHaveBeenCalledWith('audio2', { segments });
    });

    it('handles process errors', async () => {
      const app = new App();

      (app as any).audioSourceGrid = {
        getSelectedRowIds: jest.fn().mockReturnValue(['audio1']),
      };

      (app as any).transcriptGrid = {
        addSegments: jest.fn(),
      };

      const audioSource = { persistentID: 'audio1', name: 'Audio 1' };
      mockNative.getAudioSources.mockResolvedValue([audioSource]);

      const error = 'Test error';
      mockNative.transcribeAudioSource.mockResolvedValue({ error });

      await app.handleProcess();

      expect(app.transcriptGrid.addSegments).not.toHaveBeenCalled();

      const alerts = document.getElementById('alerts') as HTMLElement;
      expect(alerts.innerHTML).toContain(error);
    });

    it('clears transcript correctly', async () => {
      const app = new App();

      (app as any).transcriptGrid = {
        clear: jest.fn()
      };

      mockNative.getAudioSources.mockResolvedValue([
        { persistentID: 'audio1', name: 'Audio 1' }
      ]);

      await app.clearTranscript();

      expect(app.transcriptGrid.clear).toHaveBeenCalled();
      expect(mockNative.setAudioSourceTranscript).toHaveBeenCalledWith('audio1', {});
    });
  });

  describe('cancellation', () => {
    it('handles cancel button click', async () => {
      const app = new App();
      app.processing = true;

      (app as any).transcriptGrid = {
        clear: jest.fn()
      };

      await app.handleCancel();

      expect(app.processing).toBe(false);
      expect(mockNative.abortTranscription).toHaveBeenCalled();
      expect(app.transcriptGrid.clear).toHaveBeenCalled();
    });

    it('retries if transcription abort fails', async () => {
      const app = new App();
      app.processing = true;

      (app as any).transcriptGrid = {
        clear: jest.fn()
      };

      // First call returns false (failure), second call returns true (success)
      mockNative.abortTranscription
        .mockResolvedValueOnce(false)
        .mockResolvedValueOnce(true);

      // Start the cancel operation
      const cancelPromise = app.handleCancel();

      // Advance timers after each async operation
      await jest.runAllTimersAsync();

      // Wait for the cancel operation to complete
      await cancelPromise;

      expect(app.processing).toBe(false);
      expect(mockNative.abortTranscription).toHaveBeenCalledTimes(2);
      expect(app.transcriptGrid.clear).toHaveBeenCalled();
    });
  });

  describe('interaction with results', () => {
    it('collects playback regions by audio source', () => {
      const app = new App();

      const region1 = { playbackStart: 0, playbackEnd: 1, modificationStart: 0, modificationEnd: 1, audioSourcePersistentID: 'audio1' };
      const region2 = { playbackStart: 2, playbackEnd: 3, modificationStart: 2, modificationEnd: 3, audioSourcePersistentID: 'audio1' };
      const region3 = { playbackStart: 4, playbackEnd: 5, modificationStart: 4, modificationEnd: 5, audioSourcePersistentID: 'audio2' };
      const region4 = { playbackStart: 6, playbackEnd: 7, modificationStart: 6, modificationEnd: 7, audioSourcePersistentID: 'audio2' };

      const regionSequences = [
        {
          name: 'Track 1',
          orderIndex: 0,
          playbackRegions: [region1, region2, region3]
        },
        {
          name: 'Track 2',
          orderIndex: 1,
          playbackRegions: [region4]
        }
      ];

      const regions = app.collectPlaybackRegionsByAudioSource(regionSequences);

      expect(regions.size).toBe(2);
      expect(regions.get('audio1')).toEqual([region1, region2]);
      expect(regions.get('audio2')).toEqual([region3, region4]);
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

  describe('search', () => {
    it('handles search input', () => {
      const app = new App();

      (app as any).transcriptGrid = {
        filter: jest.fn()
      };

      const searchInput = document.getElementById('search-input') as HTMLInputElement;
      searchInput.value = 'test';

      const event = { target: searchInput } as unknown as Event;
      app.handleSearch(event);

      expect(app.transcriptGrid.filter).toHaveBeenCalledWith('test');
    });

    it('clears search input', () => {
      const app = new App();

      (app as any).transcriptGrid = {
        filter: jest.fn()
      };

      const searchInput = document.getElementById('search-input') as HTMLInputElement;
      searchInput.value = 'test';

      app.clearSearch();

      expect(searchInput.value).toBe('');
      expect(app.transcriptGrid.filter).toHaveBeenCalledWith('');
    });

    it('focuses search input', () => {
      const app = new App();

      const searchInput = document.getElementById('search-input') as HTMLInputElement;
      const focusSpy = jest.spyOn(searchInput, 'focus');
      const selectSpy = jest.spyOn(searchInput, 'select');

      app.focusSearch();

      expect(focusSpy).toHaveBeenCalled();
      expect(selectSpy).toHaveBeenCalled();

      focusSpy.mockRestore();
      selectSpy.mockRestore();
    });
  });
});
