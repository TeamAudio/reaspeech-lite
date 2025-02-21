class App {
  constructor() {
    this.canCreateMarkers = getNativeFunction("canCreateMarkers");
    this.createMarkers = getNativeFunction("createMarkers");
    this.getAudioSources = getNativeFunction("getAudioSources");
    this.getRegionSequences = getNativeFunction("getRegionSequences");
    this.getTranscriptionStatus = getNativeFunction("getTranscriptionStatus");
    this.getWebState = getNativeFunction("getWebState");
    this.getWhisperLanguages = getNativeFunction("getWhisperLanguages");
    this.play = getNativeFunction("play");
    this.stop = getNativeFunction("stop");
    this.setPlaybackPosition = getNativeFunction("setPlaybackPosition");
    this.setWebState = getNativeFunction("setWebState");
    this.transcribeAudioSource = getNativeFunction("transcribeAudioSource");

    this.state = {
      language: '',
      translate: false,
      transcript: null
    };
  }

  init() {
    this.loadState().then(() => {
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
    return this.getWebState().then((state) => {
      if (state) {
        this.state = JSON.parse(state);
      }
      return this.state;
    });
  }

  saveState() {
    if (this.state) {
      return this.setWebState(JSON.stringify(this.state));
    }
    return Promise.resolve();
  }

  initLanguages() {
    this.getWhisperLanguages().then((languages) => {
      const select = document.getElementById('language-select');

      languages.forEach((language) => {
        const option = document.createElement('option');
        if (this.state.language === language.code) {
          option.selected = true;
        }
        option.value = language.code;
        option.innerText = language.name.charAt(0).toUpperCase() + language.name.slice(1);
        select.appendChild(option);
      });

      select.onchange = () => {
        this.state.language = select.options[select.selectedIndex].value;
        this.saveState();
      }
    });

    const translateCheckbox = document.getElementById('translate-checkbox');
    translateCheckbox.checked = this.state.translate;
    translateCheckbox.onchange = () => {
      this.state.translate = translateCheckbox.checked;
      this.saveState();
    }
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
    this.getTranscriptionStatus().then((status) => {
      if (status !== '') {
        this.setProcessText(status + '...');
      }
    });

    this.getRegionSequences().then((regionSequences) => {
      for (let i = 0; i < regionSequences.length; i++) {
        const rs = regionSequences[i];

        for (let j = 0; j < rs.playbackRegions.length; j++) {
          const pr = rs.playbackRegions[j];

          const playbackStart = pr.playbackStart;
          const playbackEnd = pr.playbackEnd;
          const modificationStart = pr.modificationStart;
          const audioSourcePersistentID = pr.audioSourcePersistentID;

          const segments = document.querySelectorAll('.segment');
          segments.forEach((segment) => {
            const source = segment.querySelector('.segment-source');

            if (source.dataset.audioSourcePersistentId === audioSourcePersistentID) {
              const startElement = segment.querySelector('.segment-start');
              const endElement = segment.querySelector('.segment-end');
              const segmentStart = parseFloat(startElement.dataset.segmentTime);
              const segmentEnd = parseFloat(endElement.dataset.segmentTime);

              let start = playbackStart + segmentStart - modificationStart;
              let end = playbackStart + segmentEnd - modificationStart;

              if (start < playbackStart || start > playbackEnd) {
                startElement.dataset.playbackStart = '';
                startElement.dataset.playbackEnd = '';
                startElement.innerText = '';
                endElement.innerText = '';
              } else {
                startElement.dataset.playbackStart = start;
                endElement.dataset.playbackEnd = end;
                startElement.innerText = this.timestampToString(start);
                endElement.innerText = this.timestampToString(end);
              }
            }
          });
        }
      }
    });
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
    ].join('')
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

  htmlEscape(str) {
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  small(html, className) {
    return `<small class="${className}">${html}</small>`;
  }

  addSegments(segments, audioSource) {
    this.addRows(segments.map((segment) => [
      this.small(this.formatTimestamp(segment.start, 'segment-start'), 'text-muted'),
      this.small(this.formatTimestamp(segment.end, 'segment-end'), 'text-muted'),
      this.formatText(segment.text, 'segment-text'),
      this.formatScore(segment, 'segment-score'),
      this.small(this.formatSource(audioSource.name, 'segment-source', audioSource.persistentID), ''),
    ]));
  }

  addRows(rows) {
    const table = document.querySelector('#transcript table');
    const tbody = table.querySelector('tbody');

    rows.forEach(row => {
      const tr = document.createElement('tr');
      tr.className = 'segment align-middle';

      row.forEach((cell, index) => {
        const td = document.createElement('td');
        td.innerHTML = cell;

        if (index === 4) {
          // Truncate audio source name with tooltip
          td.className = 'text-muted text-truncate';
          td.style = 'max-width: 200px;';
          td.title = td.innerText;
        }

        tr.appendChild(td);
      });

      tbody.appendChild(tr);
    });
  }

  formatText(text, className) {
    return `<div class="${className}">${this.htmlEscape(text)}</div>`;
  }

  formatSource(text, className, audioSourcePersistentID) {
    return `<span class="${className}" data-audio-source-persistent-id="${this.htmlEscape(audioSourcePersistentID)}">${this.htmlEscape(text)}</span>`;
  }

  formatTimestamp(segmentTime, className) {
    if (className === 'segment-start') {
      const linkClasses = 'link-offset-2 link-underline link-underline-opacity-0 link-underline-opacity-50-hover';
      return `<a href="javascript:" onclick="app.playSegment(this)" class="${className} ${linkClasses}" data-segment-time="${segmentTime}">${this.timestampToString(segmentTime)}</a>`;
    } else {
      return `<span class="${className}" data-segment-time="${segmentTime}">${this.timestampToString(segmentTime)}</span>`;
    }
  }

  playSegment(segment) {
    const playbackStart = parseFloat(segment.dataset.playbackStart);
    this.playAt(playbackStart);
  }

  playAt(seconds) {
    this.stop();
    this.setPlaybackPosition(seconds).then(() => {
      this.play();
    });
  }

  // Return a string of the form M:SS.mmm
  timestampToString(seconds) {
    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = seconds - minutes * 60;
    const milliseconds = Math.round((remainingSeconds - Math.floor(remainingSeconds)) * 1000);
    return `${minutes}:${String(Math.floor(remainingSeconds)).padStart(2, '0')}.${String(milliseconds).padStart(3, '0')}`;
  }

  formatScore(segment, className) {
    const score = this.calcScore(segment);
    const color = this.scoreColor(score);
    const percentage = score * 100;
    return `\
<div class="progress" style="height: 2px">
  <div class="progress-bar ${className}" style="width: ${percentage}%; background-color: ${color}"></div>
</div>`;
  }

  calcScore(segment) {
    if (!segment.words || segment.words.length === 0) {
      return 0;
    }
    let score = 0;
    segment.words.forEach((word) => (score += word.probability));
    return score / segment.words.length;
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
