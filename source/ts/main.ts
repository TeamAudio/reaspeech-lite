// @ts-ignore
import * as Juce from "juce-framework-frontend";

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

class Native {
  canCreateMarkers: Function = Juce.getNativeFunction("canCreateMarkers");
  createMarkers: Function = Juce.getNativeFunction("createMarkers");
  getAudioSources: Function = Juce.getNativeFunction("getAudioSources");
  getModels: Function = Juce.getNativeFunction("getModels");
  getRegionSequences: Function = Juce.getNativeFunction("getRegionSequences");
  getTranscriptionStatus: Function = Juce.getNativeFunction("getTranscriptionStatus");
  getWhisperLanguages: Function = Juce.getNativeFunction("getWhisperLanguages");
  play: Function = Juce.getNativeFunction("play");
  stop: Function = Juce.getNativeFunction("stop");
  setPlaybackPosition: Function = Juce.getNativeFunction("setPlaybackPosition");
  setWebState: Function = Juce.getNativeFunction("setWebState");
  transcribeAudioSource: Function = Juce.getNativeFunction("transcribeAudioSource");
}

export default class App {
  private native: Native;
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
    this.loadState().then(() => {
      this.initModels();
      this.initLanguages();
      this.initTranscript();
    });

    document.getElementById('process-button').onclick = () => { this.handleProcess(); };
    document.getElementById('create-markers').onclick = () => { this.handleCreateMarkers('markers'); };
    document.getElementById('create-regions').onclick = () => { this.handleCreateMarkers('regions'); };
    document.getElementById('create-notes').onclick = () => { this.handleCreateMarkers('notes'); };

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
    this.native.getModels().then((models) => {
      const select = document.getElementById('model-select') as HTMLSelectElement;

      models.forEach((model) => {
        const option = document.createElement('option');
        option.selected = (this.state.modelName === model.name);
        option.value = model.name;
        option.innerText = model.label;
        select.appendChild(option);
      });

      select.onchange = () => {
        this.state.modelName = select.options[select.selectedIndex].value;
        this.saveState();
      };
    });
  }

  initLanguages() {
    this.native.getWhisperLanguages().then((languages) => {
      const select = document.getElementById('language-select') as HTMLSelectElement;

      languages.forEach((language) => {
        const option = document.createElement('option');
        option.selected = (this.state.language === language.code);
        option.value = language.code;
        option.innerText = language.name.charAt(0).toUpperCase() + language.name.slice(1);
        select.appendChild(option);
      });

      select.onchange = () => {
        this.state.language = select.options[select.selectedIndex].value;
        this.saveState();
      };
    });

    const translateCheckbox = document.getElementById('translate-checkbox') as HTMLInputElement;
    translateCheckbox.checked = this.state.translate;
    translateCheckbox.onchange = () => {
      this.state.translate = translateCheckbox.checked;
      this.saveState();
    };
  }

  initTranscript() {
    if (this.state.transcript) {
      const groups = this.state.transcript.groups;
      if (groups && groups.length > 0) {
        this.showTranscript();
        groups.forEach((group) => {
          this.addSegments(group.segments, group.audioSource);
        });
      }
    }
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

    this.native.getAudioSources().then((audioSources) => {
      const processNextAudioSource = () => {
        if (audioSources.length === 0) {
          this.enableProcessButton();
          this.hideSpinner();
          this.setProcessText('Process');
          this.saveState();
          return;
        }

        const audioSource = audioSources.shift();

        this.native.transcribeAudioSource(audioSource.persistentID, asrOptions).then((result) => {
          if (result.segments && result.segments.length > 0) {
            this.showTranscript();
            this.addSegments(result.segments, audioSource);
            this.state.transcript = this.state.transcript || { groups: [] };
            this.state.transcript.groups.push({ segments: result.segments, audioSource: audioSource });
          } else if (result.error) {
            this.showAlert('danger', '<b>Error:</b> ' + this.htmlEscape(result.error));
            audioSources.length = 0;
          }

          processNextAudioSource();
        });
      };

      processNextAudioSource();
    });
  }

  handleCreateMarkers(markerType) {
    const segments = document.querySelectorAll('.segment');
    let markers = [];

    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];
      const segmentStart = segment.querySelector('.segment-start') as HTMLElement;
      const segmentEnd = segment.querySelector('.segment-end') as HTMLElement;

      if (segmentStart.dataset.playbackStart) {
        const playbackStart = segmentStart.dataset.playbackStart;
        const playbackEnd = segmentEnd.dataset.playbackEnd;
        const text = (segment.querySelector('.segment-text') as HTMLElement).innerText;

        markers.push({
          start: playbackStart,
          end: playbackEnd,
          name: text
        });
      }
    }

    if (markers.length > 0) {
      this.native.createMarkers(markers, markerType).then((result) => {
        if (result && result.error) {
          this.showAlert('danger', '<b>Error:</b> ' + this.htmlEscape(result.error));
        }
      });
    }
  }

  update() {
    this.updateTranscriptionStatus();
    this.updatePlaybackRegions();
  }

  updateTranscriptionStatus() {
    this.native.getTranscriptionStatus().then((status) => {
      if (status !== '') {
        this.setProcessText(status + '...');
      }
    });
  }

  updatePlaybackRegions() {
    this.native.getRegionSequences().then((regionSequences) => {
      this.updatePlaybackForRegionSequences(regionSequences);
    });
  }

  updatePlaybackForRegionSequences(regionSequences) {
    const playbackRegionsByAudioSource =
      this.collectPlaybackRegionsByAudioSource(regionSequences);

    for (const segment of document.querySelectorAll('.segment')) {
      const source = segment.querySelector('.segment-source') as HTMLElement;
      const sourceID = source.dataset.persistentId;
      const playbackRegions = playbackRegionsByAudioSource[sourceID] || [];

      this.updatePlaybackForSegment(segment, playbackRegions);
    }
  }

  updatePlaybackForSegment(segment, playbackRegions) {
    const startElement = segment.querySelector('.segment-start');
    const endElement = segment.querySelector('.segment-end');
    const textElement = segment.querySelector('.segment-text');

    const segmentStart = parseFloat(startElement.dataset.segmentTime);
    const segmentEnd = parseFloat(endElement.dataset.segmentTime);

    const playbackRange =
      this.findPlayableRegion(playbackRegions, segmentStart, segmentEnd);

    if (playbackRange) {
      startElement.dataset.playbackStart = playbackRange.start;
      endElement.dataset.playbackEnd = playbackRange.end;
      textElement.dataset.playbackStart = playbackRange.start;
      startElement.innerText = this.timestampToString(playbackRange.start);
      endElement.innerText = this.timestampToString(playbackRange.end);
    } else {
      startElement.dataset.playbackStart = '';
      endElement.dataset.playbackEnd = '';
      textElement.dataset.playbackStart = '';
      startElement.innerText = '';
      endElement.innerText = '';
    }
  }

  collectPlaybackRegionsByAudioSource(regionSequences) {
    const result = {};
    for (const rs of regionSequences) {
      for (const pr of rs.playbackRegions) {
        const sourceID = pr.audioSourcePersistentID;
        if (!result[sourceID]) {
          result[sourceID] = [];
        }
        result[sourceID].push(pr);
      }
    }
    return result;
  }

  findPlayableRegion(playbackRegions, segmentStart, segmentEnd) {
    for (const pr of playbackRegions) {
      const playbackStart = pr.playbackStart;
      const playbackEnd = pr.playbackEnd;
      const modificationStart = pr.modificationStart;

      const start = playbackStart + segmentStart - modificationStart;
      const end = playbackStart + segmentEnd - modificationStart;

      if (start >= playbackStart && start <= playbackEnd) {
        return { start, end };
      }
    }

    return null;
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
    const table = document.querySelector('#transcript table');
    const tbody = table.querySelector('tbody');
    tbody.innerHTML = '';

    this.state.transcript = null;
    this.saveState();
  }

  addSegments(segments, audioSource) {
    this.addRows(segments.map((segment) => [
      this.formatStartTime(segment.start, 'segment-start small'),
      this.formatEndTime(segment.end, 'segment-end small text-muted'),
      this.formatText(segment.text, 'segment-text'),
      this.formatScore(segment, 'segment-score'),
      this.formatSource(audioSource.name, 'segment-source small', audioSource.persistentID),
    ]));
  }

  addRows(rows) {
    const table = document.querySelector('#transcript table');
    const tbody = table.querySelector('tbody');
    const fragment = document.createDocumentFragment();

    rows.forEach(row => {
      const tr = document.createElement('tr');
      tr.className = 'segment align-middle';

      const cells = row.map((cell, index) => {
        const td = document.createElement('td');
        td.innerHTML = cell;

        if (index === 4) {
          // Truncate audio source name with tooltip
          td.className = 'text-muted text-truncate';
          td.style = 'max-width: 200px;';
          td.title = td.textContent;
        }

        return td;
      });

      tr.append(...cells);
      fragment.appendChild(tr);
    });

    tbody.appendChild(fragment);
  }

  formatStartTime(segmentTime, className) {
    const linkClasses = 'link-offset-2 link-underline link-underline-opacity-0 link-underline-opacity-50-hover';
    return `<a href="javascript:" onclick="app.playSegment(this)" class="${className} ${linkClasses}" data-segment-time="${segmentTime}">${this.timestampToString(segmentTime)}</a>`;
  }

  formatEndTime(segmentTime, className) {
    return `<span class="${className}" data-segment-time="${segmentTime}">${this.timestampToString(segmentTime)}</span>`;
  }

  formatText(text, className) {
    const linkClasses = 'link-light link-offset-2 link-underline link-underline-opacity-0 link-underline-opacity-50-hover';
    return `<a href="javascript:" onclick="app.playSegment(this)" class="${className} ${linkClasses}">${this.htmlEscape(text)}</a>`;
  }

  formatScore(segment, className) {
    const score = segment.score;
    const color = this.scoreColor(score);
    const percentage = score * 100;
    return [
      '<div class="progress" style="height: 2px">',
      `  <div class="progress-bar ${className}" style="width: ${percentage}%; background-color: ${color}"></div>`,
      '</div>'
    ].join('');
  }

  formatSource(text, className, audioSourcePersistentID) {
    return `<span class="${className}" data-persistent-id="${this.htmlEscape(audioSourcePersistentID)}">${this.htmlEscape(text)}</span>`;
  }

  htmlEscape(str) {
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  playSegment(segment) {
    if (segment.dataset.playbackStart) {
      const playbackStart = parseFloat(segment.dataset.playbackStart);
      this.playAt(playbackStart);
    }
  }

  playAt(seconds) {
    this.native.stop();
    this.native.setPlaybackPosition(seconds).then(() => {
      this.native.play();
    });
  }

  scoreColor(value) {
    if (value > 0.9) {
      return "#a3ff00";
    } else if (value > 0.8) {
      return "#2cba00";
    } else if (value > 0.7) {
      return "#ffa700";
    } else if (value > 0.0) {
      return "#ff2c2f";
    } else {
      return "transparent";
    }
  }

  // Return a string of the form M:SS.mmm
  timestampToString(seconds) {
    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = seconds - minutes * 60;
    const milliseconds = Math.round((remainingSeconds - Math.floor(remainingSeconds)) * 1000);
    return `${minutes}:${String(Math.floor(remainingSeconds)).padStart(2, '0')}.${String(milliseconds).padStart(3, '0')}`;
  }
}

window.App = App;
