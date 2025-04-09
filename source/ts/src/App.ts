import Native from './Native';
import TranscriptGrid from './TranscriptGrid';
import { AudioSource, PlaybackRegion, RegionSequence } from './ARA';
import { htmlEscape, timestampToString } from './Utils';

declare global {
  interface Window {
    __JUCE__: {
      initialisationData: {
        webState?: string[];
      }
    }
    App: typeof App;
  }
}

export default class App {
  private native: Native;
  public transcriptGrid: TranscriptGrid;
  public state: any;

  constructor() {
    this.native = new Native();

    this.state = {
      modelName: 'small',
      language: '',
      translate: false,
      transcript: null
    };
  }

  init() {
    this.initState();
    this.initProcessButton();
    this.initCreateButton();
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
    document.getElementById('process-button').onclick = () => { this.handleProcess(); };
  }

  initCreateButton() {
    document.getElementById('create-markers').onclick = () => { this.handleCreateMarkers('markers'); };
    document.getElementById('create-regions').onclick = () => { this.handleCreateMarkers('regions'); };
    document.getElementById('create-notes').onclick = () => { this.handleCreateMarkers('notes'); };
  }

  startPolling() {
    setInterval(() => {
      this.update();
    }, 500);
  }

  loadState() {
    if (!window.__JUCE__.initialisationData.webState
      || !window.__JUCE__.initialisationData.webState[0]) {
      console.warn('Missing web state');
      return Promise.resolve();
    }
    try {
      this.state = JSON.parse(window.__JUCE__.initialisationData.webState[0]);
    } catch (e) {
      console.warn('Failed to parse web state:', e);
      this.showAlert('danger', '<b>Error:</b> Failed to read project data!');
    }
    return Promise.resolve();
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

    if (this.state.transcript) {
      const groups = this.state.transcript.groups;
      if (groups && groups.length > 0) {
        this.showTranscript();
        groups.forEach((group) => {
          this.transcriptGrid.addSegments(group.segments, group.audioSource);
        });
      }
    }
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
    this.disableProcessButton();
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
          this.enableProcessButton();
          this.hideSpinner();
          this.setProcessText('Process');
          return this.saveState();
        }

        const audioSource = audioSources.shift();

        return this.native.transcribeAudioSource(audioSource.persistentID, asrOptions).then((result) => {
          if (result.segments && result.segments.length > 0) {
            this.showTranscript();
            this.transcriptGrid.addSegments(result.segments, audioSource);
            this.state.transcript = this.state.transcript || { groups: [] };
            this.state.transcript.groups.push({ segments: result.segments, audioSource: audioSource });
          } else if (result.error) {
            this.showAlert('danger', '<b>Error:</b> ' + htmlEscape(result.error));
            audioSources.length = 0;
          }

          return processNextAudioSource();
        });
      };

      return processNextAudioSource();
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
    this.updateTranscriptionStatus();
    this.updatePlaybackRegions();
  }

  updateTranscriptionStatus() {
    this.native.getTranscriptionStatus().then((status) => {
      if (status.status !== '') {
        this.setProcessText(status.status + '...');
      }
      const progress = document.getElementById('progress');
      const progressBar = progress.querySelector('.progress-bar') as HTMLElement;
      progress.setAttribute('aria-valuenow', status.progress.toString());
      progressBar.style.width = status.progress + '%';
    });
  }

  updatePlaybackRegions() {
    this.native.getRegionSequences().then((regionSequences: RegionSequence[]) => {
      this.updatePlaybackForRegionSequences(regionSequences);
    });
  }

  updatePlaybackForRegionSequences(regionSequences: RegionSequence[]) {
    const playbackRegionsByAudioSource =
      this.collectPlaybackRegionsByAudioSource(regionSequences);
    this.transcriptGrid.setPlaybackRegionMap(playbackRegionsByAudioSource);
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

  enableProcessButton() {
    (document.getElementById('process-button') as HTMLButtonElement).disabled = false;
  }

  disableProcessButton() {
    (document.getElementById('process-button') as HTMLButtonElement).disabled = true;
  }

  setProcessText(text) {
    document.getElementById('process-text').innerText = text;
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

  clearTranscript() {
    this.transcriptGrid.clear();
    this.state.transcript = null;
    return this.saveState();
  }

  playAt(seconds: number) {
    return this.native.stop().then(() => {
      return this.native.setPlaybackPosition(seconds).then(() => {
        return this.native.play();
      });
    });
  }
}

window.App = App;
