(()=>{var S=Object.create;var y=Object.defineProperty;var k=Object.getOwnPropertyDescriptor;var C=Object.getOwnPropertyNames;var T=Object.getPrototypeOf,x=Object.prototype.hasOwnProperty;var L=(a,e)=>()=>(e||a((e={exports:{}}).exports,e),e.exports);var I=(a,e,t,s)=>{if(e&&typeof e=="object"||typeof e=="function")for(let n of C(e))!x.call(a,n)&&n!==t&&y(a,n,{get:()=>e[n],enumerable:!(s=k(e,n))||s.enumerable});return a};var J=(a,e,t)=>(t=a!=null?S(T(a)):{},I(e||!a||!a.__esModule?y(t,"default",{value:a,enumerable:!0}):t,a));var b=L((exports,module)=>{typeof window.__JUCE__!="undefined"&&typeof window.__JUCE__.getAndroidUserScripts!="undefined"&&typeof window.inAndroidUserScriptEval=="undefined"&&(window.inAndroidUserScriptEval=!0,eval(window.__JUCE__.getAndroidUserScripts()),delete window.inAndroidUserScriptEval);{typeof window.__JUCE__=="undefined"&&(console.warn("The 'window.__JUCE__' object is undefined. Native integration features will not work. Defining a placeholder 'window.__JUCE__' object."),window.__JUCE__={postMessage:function(){}}),typeof window.__JUCE__.initialisationData=="undefined"&&(window.__JUCE__.initialisationData={__juce__platform:[],__juce__functions:[],__juce__registeredGlobalEventIds:[],__juce__sliders:[],__juce__toggles:[],__juce__comboBoxes:[]});class a{constructor(){this.listeners=new Map,this.listenerId=0}addListener(n){let i=this.listenerId++;return this.listeners.set(i,n),i}removeListener(n){this.listeners.has(n)&&this.listeners.delete(n)}callListeners(n){for(let[,i]of this.listeners)i(n)}}class e{constructor(){this.eventListeners=new Map}addEventListener(n,i){this.eventListeners.has(n)||this.eventListeners.set(n,new a);let o=this.eventListeners.get(n).addListener(i);return[n,o]}removeEventListener([n,i]){this.eventListeners.has(n)&&this.eventListeners.get(n).removeListener(i)}emitEvent(n,i){this.eventListeners.has(n)&&this.eventListeners.get(n).callListeners(i)}}class t{constructor(){this.listeners=new e}addEventListener(n,i){return this.listeners.addEventListener(n,i)}removeEventListener([n,i]){this.listeners.removeEventListener(n,i)}emitEvent(n,i){window.__JUCE__.postMessage(JSON.stringify({eventId:n,payload:i}))}emitByBackend(n,i){this.listeners.emitEvent(n,JSON.parse(i))}}typeof window.__JUCE__.backend=="undefined"&&(window.__JUCE__.backend=new t)}});var $=J(b());var _=class{constructor(){this.lastPromiseId=0,this.promises=new Map,window.__JUCE__.backend.addEventListener("__juce__complete",({promiseId:e,result:t})=>{this.promises.has(e)&&(this.promises.get(e).resolve(t),this.promises.delete(e))})}createPromise(){let e=this.lastPromiseId++,t=new Promise((s,n)=>{this.promises.set(e,{resolve:s,reject:n})});return[e,t]}},M=new _;function l(a){return window.__JUCE__.initialisationData.__juce__functions.includes(a)||console.warn(`Creating native function binding for '${a}', which is unknown to the backend`),function(){let[t,s]=M.createPromise();return window.__JUCE__.backend.emitEvent("__juce__invoke",{name:a,params:Array.prototype.slice.call(arguments),resultId:t}),s}}var h=class{constructor(){this.listeners=new Map,this.listenerId=0}addListener(e){let t=this.listenerId++;return this.listeners.set(t,e),t}removeListener(e){this.listeners.has(e)&&this.listeners.delete(e)}callListeners(e){for(let[,t]of this.listeners)t(e)}},u="valueChanged",w="propertiesChanged",U="sliderDragStarted",P="sliderDragEnded",g=class{constructor(e){window.__JUCE__.initialisationData.__juce__sliders.includes(e)||console.warn("Creating SliderState for '"+e+"', which is unknown to the backend"),this.name=e,this.identifier="__juce__slider"+this.name,this.scaledValue=0,this.properties={start:0,end:1,skew:1,name:"",label:"",numSteps:100,interval:0,parameterIndex:-1},this.valueChangedEvent=new h,this.propertiesChangedEvent=new h,window.__JUCE__.backend.addEventListener(this.identifier,t=>this.handleEvent(t)),window.__JUCE__.backend.emitEvent(this.identifier,{eventType:"requestInitialUpdate"})}setNormalisedValue(e){this.scaledValue=this.snapToLegalValue(this.normalisedToScaledValue(e)),window.__JUCE__.backend.emitEvent(this.identifier,{eventType:u,value:this.scaledValue})}sliderDragStarted(){window.__JUCE__.backend.emitEvent(this.identifier,{eventType:U})}sliderDragEnded(){window.__JUCE__.backend.emitEvent(this.identifier,{eventType:P})}handleEvent(e){if(e.eventType==u&&(this.scaledValue=e.value,this.valueChangedEvent.callListeners()),e.eventType==w){let{eventType:t,...s}=e;this.properties=s,this.propertiesChangedEvent.callListeners()}}getScaledValue(){return this.scaledValue}getNormalisedValue(){return Math.pow((this.scaledValue-this.properties.start)/(this.properties.end-this.properties.start),this.properties.skew)}normalisedToScaledValue(e){return Math.pow(e,1/this.properties.skew)*(this.properties.end-this.properties.start)+this.properties.start}snapToLegalValue(e){let t=this.properties.interval;if(t==0)return e;let s=this.properties.start;return((i,o=0,c=1)=>Math.max(o,Math.min(c,i)))(s+t*Math.floor((e-s)/t+.5),this.properties.start,this.properties.end)}},B=new Map;for(let a of window.__JUCE__.initialisationData.__juce__sliders)B.set(a,new g(a));var f=class{constructor(e){window.__JUCE__.initialisationData.__juce__toggles.includes(e)||console.warn("Creating ToggleState for '"+e+"', which is unknown to the backend"),this.name=e,this.identifier="__juce__toggle"+this.name,this.value=!1,this.properties={name:"",parameterIndex:-1},this.valueChangedEvent=new h,this.propertiesChangedEvent=new h,window.__JUCE__.backend.addEventListener(this.identifier,t=>this.handleEvent(t)),window.__JUCE__.backend.emitEvent(this.identifier,{eventType:"requestInitialUpdate"})}getValue(){return this.value}setValue(e){this.value=e,window.__JUCE__.backend.emitEvent(this.identifier,{eventType:u,value:this.value})}handleEvent(e){if(e.eventType==u&&(this.value=e.value,this.valueChangedEvent.callListeners()),e.eventType==w){let{eventType:t,...s}=e;this.properties=s,this.propertiesChangedEvent.callListeners()}}},A=new Map;for(let a of window.__JUCE__.initialisationData.__juce__toggles)A.set(a,new f(a));var E=class{constructor(e){window.__JUCE__.initialisationData.__juce__comboBoxes.includes(e)||console.warn("Creating ComboBoxState for '"+e+"', which is unknown to the backend"),this.name=e,this.identifier="__juce__comboBox"+this.name,this.value=0,this.properties={name:"",parameterIndex:-1,choices:[]},this.valueChangedEvent=new h,this.propertiesChangedEvent=new h,window.__JUCE__.backend.addEventListener(this.identifier,t=>this.handleEvent(t)),window.__JUCE__.backend.emitEvent(this.identifier,{eventType:"requestInitialUpdate"})}getChoiceIndex(){return Math.round(this.value*(this.properties.choices.length-1))}setChoiceIndex(e){let t=this.properties.choices.length;this.value=t>1?e/(t-1):0,window.__JUCE__.backend.emitEvent(this.identifier,{eventType:u,value:this.value})}handleEvent(e){if(e.eventType==u&&(this.value=e.value,this.valueChangedEvent.callListeners()),e.eventType==w){let{eventType:t,...s}=e;this.properties=s,this.propertiesChangedEvent.callListeners()}}},j=new Map;for(let a of window.__JUCE__.initialisationData.__juce__comboBoxes)j.set(a,new E(a));var v=class{constructor(){this.canCreateMarkers=l("canCreateMarkers");this.createMarkers=l("createMarkers");this.getAudioSources=l("getAudioSources");this.getModels=l("getModels");this.getRegionSequences=l("getRegionSequences");this.getTranscriptionStatus=l("getTranscriptionStatus");this.getWhisperLanguages=l("getWhisperLanguages");this.play=l("play");this.stop=l("stop");this.setPlaybackPosition=l("setPlaybackPosition");this.setWebState=l("setWebState");this.transcribeAudioSource=l("transcribeAudioSource")}},m=class{constructor(){this.native=new v,this.state={modelName:"small",language:"",translate:!1,transcript:null}}init(){this.loadState().then(()=>{this.initModels(),this.initLanguages(),this.initTranscript()}),document.getElementById("process-button").onclick=()=>{this.handleProcess()},document.getElementById("create-markers").onclick=()=>{this.handleCreateMarkers("markers")},document.getElementById("create-regions").onclick=()=>{this.handleCreateMarkers("regions")},document.getElementById("create-notes").onclick=()=>{this.handleCreateMarkers("notes")},setInterval(()=>{this.update()},500)}loadState(){if(!window.__JUCE__.initialisationData.webState||!window.__JUCE__.initialisationData.webState[0])return Promise.resolve();try{this.state=JSON.parse(window.__JUCE__.initialisationData.webState[0])}catch(e){console.warn("Failed to parse web state:",e)}return Promise.resolve()}saveState(){return this.state?this.native.setWebState(JSON.stringify(this.state)):Promise.resolve()}initModels(){this.native.getModels().then(e=>{let t=document.getElementById("model-select");e.forEach(s=>{let n=document.createElement("option");n.selected=this.state.modelName===s.name,n.value=s.name,n.innerText=s.label,t.appendChild(n)}),t.onchange=()=>{this.state.modelName=t.options[t.selectedIndex].value,this.saveState()}})}initLanguages(){this.native.getWhisperLanguages().then(t=>{let s=document.getElementById("language-select");t.forEach(n=>{let i=document.createElement("option");i.selected=this.state.language===n.code,i.value=n.code,i.innerText=n.name.charAt(0).toUpperCase()+n.name.slice(1),s.appendChild(i)}),s.onchange=()=>{this.state.language=s.options[s.selectedIndex].value,this.saveState()}});let e=document.getElementById("translate-checkbox");e.checked=this.state.translate,e.onchange=()=>{this.state.translate=e.checked,this.saveState()}}initTranscript(){if(this.state.transcript){let e=this.state.transcript.groups;e&&e.length>0&&(this.showTranscript(),e.forEach(t=>{this.addSegments(t.segments,t.audioSource)}))}}handleProcess(){this.disableProcessButton(),this.showSpinner(),this.setProcessText("Processing..."),this.clearTranscript(),this.hideTranscript();let e=document.getElementById("language-select"),t=e.options[e.selectedIndex].value,s=document.getElementById("translate-checkbox").checked,n={modelName:this.state.modelName,language:t,translate:s};this.native.getAudioSources().then(i=>{let o=()=>{if(i.length===0){this.enableProcessButton(),this.hideSpinner(),this.setProcessText("Process"),this.saveState();return}let c=i.shift();this.native.transcribeAudioSource(c.persistentID,n).then(r=>{r.segments&&r.segments.length>0?(this.showTranscript(),this.addSegments(r.segments,c),this.state.transcript=this.state.transcript||{groups:[]},this.state.transcript.groups.push({segments:r.segments,audioSource:c})):r.error&&(this.showAlert("danger","<b>Error:</b> "+this.htmlEscape(r.error)),i.length=0),o()})};o()})}handleCreateMarkers(e){let t=document.querySelectorAll(".segment"),s=[];for(let n=0;n<t.length;n++){let i=t[n],o=i.querySelector(".segment-start"),c=i.querySelector(".segment-end");if(o.dataset.playbackStart){let r=o.dataset.playbackStart,p=c.dataset.playbackEnd,d=i.querySelector(".segment-text").innerText;s.push({start:r,end:p,name:d})}}s.length>0&&this.native.createMarkers(s,e).then(n=>{n&&n.error&&this.showAlert("danger","<b>Error:</b> "+this.htmlEscape(n.error))})}update(){this.updateTranscriptionStatus(),this.updatePlaybackRegions()}updateTranscriptionStatus(){this.native.getTranscriptionStatus().then(e=>{e!==""&&this.setProcessText(e+"...")})}updatePlaybackRegions(){this.native.getRegionSequences().then(e=>{this.updatePlaybackForRegionSequences(e)})}updatePlaybackForRegionSequences(e){let t=this.collectPlaybackRegionsByAudioSource(e);for(let s of document.querySelectorAll(".segment")){let i=s.querySelector(".segment-source").dataset.persistentId,o=t[i]||[];this.updatePlaybackForSegment(s,o)}}updatePlaybackForSegment(e,t){let s=e.querySelector(".segment-start"),n=e.querySelector(".segment-end"),i=e.querySelector(".segment-text"),o=parseFloat(s.dataset.segmentTime),c=parseFloat(n.dataset.segmentTime),r=this.findPlayableRegion(t,o,c);r?(s.dataset.playbackStart=r.start,n.dataset.playbackEnd=r.end,i.dataset.playbackStart=r.start,s.innerText=this.timestampToString(r.start),n.innerText=this.timestampToString(r.end)):(s.dataset.playbackStart="",n.dataset.playbackEnd="",i.dataset.playbackStart="",s.innerText="",n.innerText="")}collectPlaybackRegionsByAudioSource(e){let t={};for(let s of e)for(let n of s.playbackRegions){let i=n.audioSourcePersistentID;t[i]||(t[i]=[]),t[i].push(n)}return t}findPlayableRegion(e,t,s){for(let n of e){let i=n.playbackStart,o=n.playbackEnd,c=n.modificationStart,r=i+t-c,p=i+s-c;if(r>=i&&r<=o)return{start:r,end:p}}return null}enableProcessButton(){document.getElementById("process-button").disabled=!1}disableProcessButton(){document.getElementById("process-button").disabled=!0}setProcessText(e){document.getElementById("process-text").innerText=e}showSpinner(){document.getElementById("spinner").style.display="inline-block"}hideSpinner(){document.getElementById("spinner").style.display="none"}showAlert(e,t){let s=document.getElementById("alerts"),n=document.createElement("div");n.innerHTML=[`<div class="alert alert-${e} alert-dismissible mb-2" role="alert">`,`   <div>${t}</div>`,'   <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>',"</div>"].join(""),s.append(n)}showCreateMenu(){document.getElementById("create-menu").style.display="block"}showTranscript(){document.getElementById("transcript").style.display="block",this.native.canCreateMarkers().then(e=>{e&&this.showCreateMenu()})}hideTranscript(){document.getElementById("transcript").style.display="none"}clearTranscript(){let t=document.querySelector("#transcript table").querySelector("tbody");t.innerHTML="",this.state.transcript=null,this.saveState()}addSegments(e,t){this.addRows(e.map(s=>[this.formatStartTime(s.start,"segment-start small"),this.formatEndTime(s.end,"segment-end small text-muted"),this.formatText(s.text,"segment-text"),this.formatScore(s,"segment-score"),this.formatSource(t.name,"segment-source small",t.persistentID)]))}addRows(e){let s=document.querySelector("#transcript table").querySelector("tbody"),n=document.createDocumentFragment();e.forEach(i=>{let o=document.createElement("tr");o.className="segment align-middle";let c=i.map((r,p)=>{let d=document.createElement("td");return d.innerHTML=r,p===4&&(d.className="text-muted text-truncate",d.style="max-width: 200px;",d.title=d.textContent),d});o.append(...c),n.appendChild(o)}),s.appendChild(n)}formatStartTime(e,t){return`<a href="javascript:" onclick="app.playSegment(this)" class="${t} link-offset-2 link-underline link-underline-opacity-0 link-underline-opacity-50-hover" data-segment-time="${e}">${this.timestampToString(e)}</a>`}formatEndTime(e,t){return`<span class="${t}" data-segment-time="${e}">${this.timestampToString(e)}</span>`}formatText(e,t){return`<a href="javascript:" onclick="app.playSegment(this)" class="${t} link-light link-offset-2 link-underline link-underline-opacity-0 link-underline-opacity-50-hover">${this.htmlEscape(e)}</a>`}formatScore(e,t){let s=e.score,n=this.scoreColor(s),i=s*100;return['<div class="progress" style="height: 2px">',`  <div class="progress-bar ${t}" style="width: ${i}%; background-color: ${n}"></div>`,"</div>"].join("")}formatSource(e,t,s){return`<span class="${t}" data-persistent-id="${this.htmlEscape(s)}">${this.htmlEscape(e)}</span>`}htmlEscape(e){return e.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;")}playSegment(e){if(e.dataset.playbackStart){let t=parseFloat(e.dataset.playbackStart);this.playAt(t)}}playAt(e){this.native.stop(),this.native.setPlaybackPosition(e).then(()=>{this.native.play()})}scoreColor(e){return e>.9?"#a3ff00":e>.8?"#2cba00":e>.7?"#ffa700":e>0?"#ff2c2f":"transparent"}timestampToString(e){let t=Math.floor(e/60),s=e-t*60,n=Math.round((s-Math.floor(s))*1e3);return`${t}:${String(Math.floor(s)).padStart(2,"0")}.${String(n).padStart(3,"0")}`}};window.App=m;})();
