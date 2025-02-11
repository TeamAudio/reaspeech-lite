class App {
  constructor() {
    this.canCreateMarkers = getNativeFunction("canCreateMarkers");
    this.createMarkers = getNativeFunction("createMarkers");
    this.getAudioSources = getNativeFunction("getAudioSources");
    this.getRegionSequences = getNativeFunction("getRegionSequences");
    this.getTranscriptionStatus = getNativeFunction("getTranscriptionStatus");
    this.getWhisperLanguages = getNativeFunction("getWhisperLanguages");
    this.play = getNativeFunction("play");
    this.stop = getNativeFunction("stop");
    this.setPlaybackPosition = getNativeFunction("setPlaybackPosition");
    this.transcribeAudioSource = getNativeFunction("transcribeAudioSource");
  }

  init() {
    this.fillLanguageSelect();

    document.getElementById('process-button').onclick = () => {
      this.handleProcessClick();
    };

    document.getElementById('create-markers-button').onclick = () => {
      this.handleCreateMarkersClick();
    };

    setInterval(() => {
      this.update();
    }, 500);
  }

  fillLanguageSelect() {
    this.getWhisperLanguages().then((languages) => {
      const select = document.getElementById('language-select');
      languages.forEach((language) => {
        if (language.code === 'en') return;
        const option = document.createElement('option');
        option.value = language.code;
        option.innerText = language.name.charAt(0).toUpperCase() + language.name.slice(1);
        select.appendChild(option);
      });
    });
  }

  handleProcessClick() {
    this.disableProcessButton();
    this.showSpinner();
    this.setProcessText('Processing...');
    this.clearTable();
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
          return;
        }

        const audioSource = audioSources.shift();

        this.transcribeAudioSource(audioSource.persistentID, asrOptions).then((result) => {
          if (result.segments && result.segments.length > 0) {
            this.showTranscript();
            this.addSegments(result.segments, audioSource);
          } else if (result.error) {
            console.error(`Error: ${result.error}`);
          }

          processNextAudioSource();
        });
      };

      processNextAudioSource();
    });
  }

  handleCreateMarkersClick() {
    const segments = document.querySelectorAll('.segment');
    let markers = [];
    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];
      const segmentStart = segment.querySelector('.segment-start');
      if (segmentStart.dataset.playbackTime) {
        const playbackTime = segmentStart.dataset.playbackTime;
        const text = segment.querySelector('.segment-text').innerText;
        markers.push({
          position: playbackTime,
          name: text
        });
      }
    }
    if (markers.length > 0) {
      this.createMarkers(markers);
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
                startElement.dataset.playbackTime = '';
                startElement.innerText = '';
                endElement.innerText = '';
              } else {
                startElement.dataset.playbackTime = start;
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

  showTranscript() {
    document.getElementById('transcript').style.display = 'block';

    this.canCreateMarkers().then((canCreateMarkers) => {
      if (canCreateMarkers) {
        document.getElementById('create-markers-button').style.display = 'inline-block';
      }
    });
  }

  hideTranscript() {
    document.getElementById('transcript').style.display = 'none';
  }

  htmlEscape(str) {
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  small(html, className) {
    return `<small class="${className}">${html}</small>`;
  }

  clearTable() {
    const table = document.querySelector('#transcript table');
    const tbody = table.querySelector('tbody');
    tbody.innerHTML = '';
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
    const playbackTime = parseFloat(segment.dataset.playbackTime);
    this.playAt(playbackTime);
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

// The following code is copied from modules/juce_gui_extra/native/javascript/index.js
// This file is intended to be used with a JS build system, but since we do not
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
