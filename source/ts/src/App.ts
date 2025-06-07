import Native from './Native';
import TranscriptGrid from './TranscriptGrid';
import { AudioSource, PlaybackRegion, RegionSequence } from './ARA';
import { delay, htmlEscape } from './Utils';

declare global {
  interface Window {
    __JUCE__: {
      backend: {
        addEventListener: (event: string, callback: (event: any) => void) => void;
      }
      initialisationData: {
        webState?: string[];
      }
    }
    App: typeof App;
  }
}

export default class App {
  private native: Native;
  public processing: boolean = false;
  public state: any;
  public transcriptGrid: TranscriptGrid;

  constructor() {
    this.native = new Native();

    this.state = {
      modelName: 'small',
      language: '',
      translate: false,
    };
  }

  init() {
    this.initState();
    this.initProcessButton();
    this.initCreateButton();
    this.initExportButton();
    this.initNativeEvents();
    this.startPolling();
  }

  initState() {
    this.loadState().then(() => {
      this.initModels();
      this.initLanguages();
      this.initTranscriptGrid();
    });
  }

  initProcessButton() {
    document.getElementById('process-button').onclick = () => {
      if (this.processing) {
        this.handleCancel();
      } else {
        this.handleProcess();
      }
    };

    document.getElementById('process-button').onmouseover = () => {
      if (!this.processing) return;
      this.showCancel();
    };

    document.getElementById('process-button').onmouseout = () => {
      this.hideCancel();
    };
  }

  initCreateButton() {
    document.getElementById('create-markers').onclick = () => { this.handleCreateMarkers('markers'); };
    document.getElementById('create-regions').onclick = () => { this.handleCreateMarkers('regions'); };
    document.getElementById('create-notes').onclick = () => { this.handleCreateMarkers('notes'); };
  }

  initExportButton() {
    document.getElementById('export-csv').onclick = () => { this.transcriptGrid.exportCSV(this.saveAs.bind(this)); }
    document.getElementById('export-srt').onclick = () => { this.transcriptGrid.exportSRT(this.saveAs.bind(this)); };
  }

  initNativeEvents() {
    window.__JUCE__.backend.addEventListener('audioSourceContentUpdated', this.handleAudioSourceUpdate.bind(this));
  }

  startPolling() {
    setInterval(() => {
      this.update();
    }, 500);
  }

  loadState() {
    if (!window.__JUCE__.initialisationData.webState
        || !window.__JUCE__.initialisationData.webState[0]) {
      return Promise.resolve();
    }
    try {
      this.state = JSON.parse(window.__JUCE__.initialisationData.webState[0]);
    } catch (e) {
      console.warn('Failed to parse web state:', e);
      this.showAlert('danger', '<b>Error:</b> Failed to read project data!');
    }
    if (this.state.transcript) {
      return this.migrateTranscript();
    } else {
      return Promise.resolve();
    }
  }

  migrateTranscript() {
    const groups = this.state.transcript.groups || [];
    const promises = groups.map((group) => {
      return this.native.setAudioSourceTranscript(
        group.audioSource.persistentID,
        { segments: group.segments }
      );
    });
    return Promise.all(promises).then(() => {
      delete this.state.transcript;
      return this.saveState();
    });
  }

  saveState() {
    if (this.state) {
      return this.native.setWebState(JSON.stringify(this.state));
    }
    return Promise.resolve();
  }

  initModels() {
    return this.native.getModels().then((models) => {
      const select = document.getElementById('model-select') as HTMLSelectElement;

      models.forEach((model) => {
        const option = document.createElement('option');
        option.selected = (this.state.modelName === model.name);
        option.value = model.name;
        option.textContent = model.label;
        select.appendChild(option);
      });

      select.onchange = this.handleModelChange.bind(this);
    });
  }

  initLanguages() {
    return this.native.getWhisperLanguages().then((languages) => {
      const select = document.getElementById('language-select') as HTMLSelectElement;

      languages.forEach((language) => {
        const option = document.createElement('option');
        option.selected = (this.state.language === language.code);
        option.value = language.code;
        option.textContent = language.name.charAt(0).toUpperCase() + language.name.slice(1);
        select.appendChild(option);
      });

      select.onchange = this.handleLanguageChange.bind(this);

      const translateCheckbox = document.getElementById('translate-checkbox') as HTMLInputElement;
      translateCheckbox.checked = this.state.translate;
      translateCheckbox.onchange = this.handleTranslateChange.bind(this);
    });
  }

  initTranscriptGrid() {
    this.transcriptGrid = new TranscriptGrid('#transcript-grid', (seconds) => this.playAt(seconds));
    return this.native.getAudioSources().then((audioSources: AudioSource[]) => {
      const promises = audioSources.map((audioSource) => {
        return this.mergeTranscript(audioSource);
      });
      return Promise.all(promises).then(() => {});
    });
  }

  handleAudioSourceUpdate(event: { persistentID: string }) {
    return this.native.getAudioSources().then((audioSources: AudioSource[]) => {
      for (const audioSource of audioSources) {
        if (audioSource.persistentID === event.persistentID) {
          return this.mergeTranscript(audioSource);
        }
      }
    });
  }

  handleModelChange() {
    const select = document.getElementById('model-select') as HTMLSelectElement;
    this.state.modelName = select.options[select.selectedIndex].value;
    return this.saveState();
  }

  handleLanguageChange() {
    const select = document.getElementById('language-select') as HTMLSelectElement;
    this.state.language = select.options[select.selectedIndex].value;
    return this.saveState();
  }

  handleTranslateChange() {
    this.state.translate = (document.getElementById('translate-checkbox') as HTMLInputElement).checked;
    return this.saveState();
  }

  handleProcess() {
    this.processing = true;
    this.showSpinner();
    this.setProcessText('Processing...');
    this.clearTranscript();
    this.hideTranscript();

    const languageSelect = document.getElementById('language-select') as HTMLSelectElement;
    const languageCode = languageSelect.options[languageSelect.selectedIndex].value;
    const translate = (document.getElementById('translate-checkbox') as HTMLInputElement).checked;
    const asrOptions = {
      modelName: this.state.modelName,
      language: languageCode,
      translate: translate
    };

    return this.native.getAudioSources().then((audioSources: AudioSource[]) => {
      const processNextAudioSource = () => {
        if (audioSources.length === 0) {
          this.processing = false;
          this.setProcessText('Process');
          this.hideCancel();
          this.hideSpinner();
          return Promise.resolve();
        }

        const audioSource = audioSources.shift();

        return this.native.transcribeAudioSource(audioSource.persistentID, asrOptions).then((result) => {
          if (!this.processing) {
            return Promise.resolve();
          }

          if (result.error) {
            this.showAlert('danger', '<b>Error:</b> ' + htmlEscape(result.error));
            audioSources.length = 0;
            return processNextAudioSource();
          }

          return this.native.setAudioSourceTranscript(audioSource.persistentID, result).then(() => {
            return processNextAudioSource();
          });
        });
      };

      return processNextAudioSource();
    });
  }

  handleCancel(retriesLeft = 100) {
    this.processing = false;
    this.hideCancel();
    this.hideSpinner();
    this.hideTranscript();

    this.disableProcessButton();
    this.setProcessText('Canceling...');

    return this.native.abortTranscription().then((success: boolean) => {
      if (!success) {
        if (retriesLeft > 0) {
          console.warn('Timed out trying to abort transcription job! Retrying...');
          return delay(1000).then(() => this.handleCancel(retriesLeft - 1));
        } else {
          this.showAlert('warning', '<b>Warning:</b> Unable to cancel transcription!');
        }
      }

      this.enableProcessButton();
      this.setProcessText('Process');

      return this.clearTranscript();
    });
  }

  handleCreateMarkers(markerType: string) {
    const rows = this.transcriptGrid.getRows();
    let markers = [];

    for (const row of rows) {
      if (row.playbackStart !== null && row.playbackEnd !== null) {
        markers.push({
          start: row.playbackStart,
          end: row.playbackEnd,
          name: row.text
        });
      }
    }

    if (markers.length > 0) {
      return this.native.createMarkers(markers, markerType).then((result) => {
        if (result && result.error) {
          this.showAlert('danger', '<b>Error:</b> ' + htmlEscape(result.error));
        }
      });
    } else {
      return Promise.resolve();
    }
  }

  update() {
    return Promise.all([
      this.updateTranscriptionStatus(),
      this.updatePlaybackRegions(),
    ]);
  }

  updateTranscriptionStatus() {
    if (!this.processing) {
      this.setProgress(0);
      return Promise.resolve();
    }
    return this.native.getTranscriptionStatus().then((status) => {
      if (status.status !== '') {
        this.setProcessText(status.status + '...');
      }
      this.setProgress(status.progress);
    });
  }

  updatePlaybackRegions() {
    return this.native.getRegionSequences().then((regionSequences: RegionSequence[]) => {
      const playbackRegionsByAudioSource =
        this.collectPlaybackRegionsByAudioSource(regionSequences);
      this.transcriptGrid.setPlaybackRegionMap(playbackRegionsByAudioSource);

      return this.native.getPlayHeadState().then((playHeadState) => {
        this.transcriptGrid.setPlaybackPosition(playHeadState.timeInSeconds, playHeadState.isPlaying);
      });
    });
  }

  collectPlaybackRegionsByAudioSource(regionSequences: RegionSequence[]): Map<string, PlaybackRegion[]> {
    const result = new Map<string, PlaybackRegion[]>();
    for (const rs of regionSequences) {
      for (const pr of rs.playbackRegions) {
        const sourceID = pr.audioSourcePersistentID;
        if (!result.has(sourceID)) {
          result.set(sourceID, []);
        }
        result.get(sourceID).push(pr);
      }
    }
    return result;
  }

  setProcessText(text) {
    document.getElementById('process-text').innerText = text;
  }

  setProgress(progress: number) {
    const progressElement = document.getElementById('progress');
    if (!progressElement) {
      console.warn('Progress element not found in the DOM.');
      return;
    }
    const progressBar = progressElement.querySelector('.progress-bar') as HTMLElement;
    if (!progressBar) {
      console.warn('Progress bar element not found in the DOM.');
      return;
    }
    progressElement.setAttribute('aria-valuenow', progress.toString());
    progressBar.style.width = progress + '%';
  }

  enableProcessButton() {
    (document.getElementById('process-button') as HTMLButtonElement).disabled = false;
  }

  disableProcessButton() {
    (document.getElementById('process-button') as HTMLButtonElement).disabled = true;
  }

  showCancel() {
    document.getElementById('process-button').classList.add('btn-danger');
    document.getElementById('process-button').classList.remove('btn-primary');
    document.getElementById('process-cancel').style.display = 'inline-block';
    document.getElementById('process-text').style.display = 'none';
  }

  hideCancel() {
    document.getElementById('process-button').classList.add('btn-primary');
    document.getElementById('process-button').classList.remove('btn-danger');
    document.getElementById('process-cancel').style.display = 'none';
    document.getElementById('process-text').style.display = 'inline-block';
  }

  showSpinner() {
    document.getElementById('spinner').style.display = 'inline-block';
  }

  hideSpinner() {
    document.getElementById('spinner').style.display = 'none';
  }

  showAlert(type, message) {
    const alerts = document.getElementById('alerts');
    const wrapper = document.createElement('div');
    wrapper.innerHTML = [
      `<div class="alert alert-${type} alert-dismissible mb-2" role="alert">`,
      `   <div>${message}</div>`,
      '   <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>',
      '</div>'
    ].join('');
    alerts.append(wrapper);
  }

  showCreateMenu() {
    document.getElementById('create-menu').style.display = 'block';
  }

  showTranscript() {
    document.getElementById('transcript').style.display = 'block';

    this.native.canCreateMarkers().then((canCreateMarkers) => {
      if (canCreateMarkers) {
        this.showCreateMenu();
      }
    });
  }

  hideTranscript() {
    document.getElementById('transcript').style.display = 'none';
  }

  mergeTranscript(audioSource: AudioSource) {
    return this.native.getAudioSourceTranscript(audioSource.persistentID).then((transcript) => {
      if (transcript.error) {
        console.warn('Error loading transcript for audio source:', audioSource.persistentID, transcript.error);
        return;
      }

      this.transcriptGrid.removeRowsBySourceID(audioSource.persistentID);

      if (transcript.segments) {
        this.showTranscript();
        this.transcriptGrid.addSegments(transcript.segments, audioSource);
      }
    });
  }

  clearTranscript() {
    this.transcriptGrid.clear();
    return this.native.getAudioSources().then((audioSources: AudioSource[]) => {
      const promises = audioSources.map((audioSource) => {
        return this.native.setAudioSourceTranscript(audioSource.persistentID, {});
      });
      return Promise.all(promises).then(() => {});
    });
  }

  playAt(seconds: number) {
    return this.native.stop().then(() => {
      return this.native.setPlaybackPosition(seconds).then(() => {
        return this.native.play();
      });
    });
  }

  saveAs(content: string, _mimeType: string, filename: string) {
    const title = "Save As";
    const extension = filename.split('.').pop();
    const patterns = "*." + extension;
    return this.native.saveFile(title, filename, patterns, content).then((result) => {
      if (result.error) {
        this.showAlert('danger', '<b>Error:</b> ' + htmlEscape(result.error));
      } else if (result.filePath) {
        this.showAlert('success', '<b>Success:</b> File saved to ' + htmlEscape(result.filePath));
      }
    });
  }
}

window.App = App;
