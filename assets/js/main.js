class App {
  constructor() {
    this.canCreateMarkers = getNativeFunction("canCreateMarkers");
    this.createMarkers = getNativeFunction("createMarkers");
    this.getAudioSources = getNativeFunction("getAudioSources");
    this.getModels = getNativeFunction("getModels");
    this.getRegionSequences = getNativeFunction("getRegionSequences");
    this.getTranscriptionStatus = getNativeFunction("getTranscriptionStatus");
    this.getWhisperLanguages = getNativeFunction("getWhisperLanguages");
    this.play = getNativeFunction("play");
    this.stop = getNativeFunction("stop");
    this.setPlaybackPosition = getNativeFunction("setPlaybackPosition");
    this.setWebState = getNativeFunction("setWebState");
    this.transcribeAudioSource = getNativeFunction("transcribeAudioSource");

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

    setInterval(() => {
      this.update();
    }, 500);
  }

  loadState() {
    try {
      this.state = JSON.parse(window.__JUCE__.initialisationData.webState);
    } catch (e) {
      console.warn('Failed to parse web state:', e);
    }
    return Promise.resolve();
  }

  saveState() {
    if (this.state) {
      return this.setWebState(JSON.stringify(this.state));
    }
    return Promise.resolve();
  }

  initModels() {
    this.getModels().then((models) => {
      const select = document.getElementById('model-select');

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
    this.getWhisperLanguages().then((languages) => {
      const select = document.getElementById('language-select');

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

    const translateCheckbox = document.getElementById('translate-checkbox');
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

    const languageSelect = document.getElementById('language-select');
    const languageCode = languageSelect.options[languageSelect.selectedIndex].value;
    const translate = document.getElementById('translate-checkbox').checked;
    const asrOptions = {
      modelName: this.state.modelName,
      language: languageCode,
      translate: translate
    };

    this.getAudioSources().then((audioSources) => {
      const processNextAudioSource = () => {
        if (audioSources.length === 0) {
          this.enableProcessButton();
          this.hideSpinner();
          this.setProcessText('Process');
          this.saveState();
          return;
        }

        const audioSource = audioSources.shift();

        this.transcribeAudioSource(audioSource.persistentID, asrOptions).then((result) => {
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
      const segmentStart = segment.querySelector('.segment-start');
      const segmentEnd = segment.querySelector('.segment-end');

      if (segmentStart.dataset.playbackStart) {
        const playbackStart = segmentStart.dataset.playbackStart;
        const playbackEnd = segmentEnd.dataset.playbackEnd;
        const text = segment.querySelector('.segment-text').innerText;

        markers.push({
          start: playbackStart,
          end: playbackEnd,
          name: text
        });
      }
    }

    if (markers.length > 0) {
      this.createMarkers(markers, markerType).then((result) => {
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
    this.getTranscriptionStatus().then((status) => {
      if (status !== '') {
        this.setProcessText(status + '...');
      }
    });
  }

  updatePlaybackRegions() {
    this.getRegionSequences().then((regionSequences) => {
      this.updatePlaybackForRegionSequences(regionSequences);
    });
  }

  updatePlaybackForRegionSequences(regionSequences) {
    const playbackRegionsByAudioSource =
      this.collectPlaybackRegionsByAudioSource(regionSequences);

    for (const segment of document.querySelectorAll('.segment')) {
      const source = segment.querySelector('.segment-source');
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
    document.getElementById('process-button').disabled = false;
  }

  disableProcessButton() {
    document.getElementById('process-button').disabled = true;
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

    this.canCreateMarkers().then((canCreateMarkers) => {
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
    this.stop();
    this.setPlaybackPosition(seconds).then(() => {
      this.play();
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

if (typeof window.__JUCE__ === "undefined") {
  function getNativeFunction() {
    return function() {
      return new Promise((resolve, reject) => {});
    };
  }
} else {
  // The following code is copied from modules/juce_gui_extra/native/javascript/index.js
  // That file is intended to be used with a JS build system, but since we do not
  // currently use one, this is the easiest way to get native functions to work.

  class PromiseHandler {
    constructor() {
      this.lastPromiseId = 0;
      this.promises = new Map();

      window.__JUCE__.backend.addEventListener(
        "__juce__complete",
        ({ promiseId, result }) => {
          if (this.promises.has(promiseId)) {
            this.promises.get(promiseId).resolve(result);
            this.promises.delete(promiseId);
          }
        }
      );
    }

    createPromise() {
      const promiseId = this.lastPromiseId++;
      const result = new Promise((resolve, reject) => {
        this.promises.set(promiseId, { resolve: resolve, reject: reject });
      });
      return [promiseId, result];
    }
  }

  const promiseHandler = new PromiseHandler();

  function getNativeFunction(name) {
    if (!window.__JUCE__.initialisationData.__juce__functions.includes(name))
      console.warn(
        `Creating native function binding for '${name}', which is unknown to the backend`
      );

    const f = function () {
      const [promiseId, result] = promiseHandler.createPromise();

      window.__JUCE__.backend.emitEvent("__juce__invoke", {
        name: name,
        params: Array.prototype.slice.call(arguments),
        resultId: promiseId,
      });

      return result;
    };

    return f;
  }
}
