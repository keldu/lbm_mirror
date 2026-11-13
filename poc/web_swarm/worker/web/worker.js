// The browser worker, transport half.
//
// This file owns the WebSocket, the RTCPeerConnection, the data channel and
// nothing else. Every decision about what a message means lives in the wasm
// module, so the wire format has exactly one definition -- the schemas in
// modules/swarm -- rather than one in C++ and a hand maintained copy here.
//
// The rule to keep: if you find yourself about to read a field out of a parsed
// message in this file, it belongs on the other side of the boundary.

'use strict';

const ui = {
  state: null,
  detail: null,
  log: null,
};

function setState(text, detail) {
  if (ui.state) ui.state.textContent = text;
  if (ui.detail && detail !== undefined) ui.detail.textContent = detail;
}

function log(line) {
  console.log(line);
  if (!ui.log) return;
  const stamp = new Date().toLocaleTimeString();
  ui.log.textContent += `[${stamp}] ${line}\n`;
  ui.log.scrollTop = ui.log.scrollHeight;
}

// Normalises an ICE candidate into the shape modules/swarm declares. The
// browser hands out {candidate, sdpMid, sdpMLineIndex}; the wire carries
// {type, candidate, mid}. Doing the mapping here keeps every message on the
// wire uniform -- all of them are objects with a "type".
function candidateMessage(candidate) {
  return JSON.stringify({
    type: 'candidate',
    candidate: candidate.candidate,
    mid: candidate.sdpMid === null ? '' : candidate.sdpMid,
  });
}

function descriptionMessage(description) {
  return JSON.stringify({
    type: description.type,
    sdp: description.sdp,
  });
}

class Worker {
  constructor(mod, url, workerId) {
    this.mod = mod;
    this.url = url;
    this.workerId = workerId;
    this.ws = null;
    this.pc = null;
    this.control = null;

    // Bound once so the wasm exports are called by name rather than through
    // ccall at every message, which would re-resolve them each time.
    this.hello = mod.cwrap('kel_worker_hello', 'string', []);
    this.handle = mod.cwrap('kel_worker_handle', 'string', ['string']);
    this.pongs = mod.cwrap('kel_worker_pongs', 'number', []);
  }

  connect() {
    setState('connecting', this.url);
    log(`connecting to ${this.url}`);

    this.ws = new WebSocket(this.url);

    this.ws.onopen = () => {
      setState('signalling');
      log('signalling socket open');
    };

    this.ws.onerror = () => {
      setState('error', 'signalling socket failed');
      log('signalling socket error');
    };

    this.ws.onclose = () => {
      setState('disconnected');
      log('signalling socket closed');
    };

    this.ws.onmessage = (event) => this.onSignal(event.data);

    this.startPeerConnection();
  }

  startPeerConnection() {
    // No ICE servers: host candidates are enough on localhost or a LAN, and
    // a STUN server is only needed to get through NAT.
    this.pc = new RTCPeerConnection({ iceServers: [] });

    this.pc.onicecandidate = (event) => {
      if (!event.candidate) return;
      this.send(candidateMessage(event.candidate));
    };

    this.pc.onconnectionstatechange = () => {
      log(`peer connection ${this.pc.connectionState}`);
      if (this.pc.connectionState === 'failed') {
        setState('error', 'peer connection failed');
      }
    };

    // The orchestrator creates the channel, so it arrives rather than being
    // made here. That fixes the ordering: it is already in the offer, so
    // there is no race between it opening and the first send.
    this.pc.ondatachannel = (event) => {
      if (event.channel.label !== 'control') return;
      this.attachControl(event.channel);
    };
  }

  attachControl(channel) {
    this.control = channel;

    channel.onopen = () => {
      setState('registering');
      log('control channel open');

      const hello = this.hello();
      if (!hello) {
        setState('error', 'could not build hello');
        return;
      }
      log(`-> hello (${hello.length} bytes)`);
      channel.send(hello);
    };

    channel.onmessage = (event) => {
      if (typeof event.data !== 'string') return;

      // The one place the transport looks inside a message, and only to say
      // what happened in the log. The reply itself is decided by wasm.
      const reply = this.handle(event.data);

      let kind = '?';
      try {
        kind = JSON.parse(event.data).type;
      } catch (err) {
        kind = 'unparseable';
      }

      if (kind === 'welcome') {
        setState('registered', `pongs 0`);
        log('<- welcome, registered');
      } else if (kind === 'ping') {
        setState('registered', `pongs ${this.pongs()}`);
      } else {
        log(`<- ${kind}`);
      }

      if (reply && channel.readyState === 'open') {
        channel.send(reply);
      }
    };

    channel.onclose = () => {
      setState('disconnected', 'control channel closed');
      log('control channel closed');
    };
  }

  onSignal(text) {
    let message;
    try {
      message = JSON.parse(text);
    } catch (err) {
      log('malformed signalling message');
      return;
    }

    if (message.type === 'offer' || message.type === 'answer') {
      this.pc
        .setRemoteDescription({ type: message.type, sdp: message.sdp })
        .then(() => {
          if (message.type !== 'offer') return null;
          return this.pc.createAnswer();
        })
        .then((answer) => {
          if (!answer) return null;
          return this.pc.setLocalDescription(answer).then(() => {
            this.send(descriptionMessage(this.pc.localDescription));
          });
        })
        .catch((err) => log(`signalling failed: ${err}`));
      return;
    }

    if (message.type === 'candidate') {
      this.pc
        .addIceCandidate({
          candidate: message.candidate,
          sdpMid: message.mid === '' ? null : message.mid,
        })
        .catch((err) => log(`bad candidate: ${err}`));
      return;
    }

    log(`unknown signalling type: ${message.type}`);
  }

  send(text) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(text);
    }
  }

  disconnect() {
    if (this.control) this.control.close();
    if (this.pc) this.pc.close();
    if (this.ws) this.ws.close();
    setState('disconnected');
  }
}

function defaultUrl() {
  const params = new URLSearchParams(window.location.search);
  const fromQuery = params.get('orchestrator');
  if (fromQuery) return fromQuery;
  // The page is served by something else -- a WebSocket server is not an HTTP
  // server -- so the orchestrator is not on this port. 8080 is its default.
  return `ws://${window.location.hostname || '127.0.0.1'}:8080/`;
}

window.addEventListener('DOMContentLoaded', () => {
  ui.state = document.getElementById('state');
  ui.detail = document.getElementById('detail');
  ui.log = document.getElementById('log');

  const urlInput = document.getElementById('url');
  const idInput = document.getElementById('worker-id');
  const connectButton = document.getElementById('connect');
  const disconnectButton = document.getElementById('disconnect');

  urlInput.value = defaultUrl();
  idInput.value = `browser-${Math.floor(Math.random() * 10000)}`;

  setState('loading', 'instantiating wasm');

  Module().then((mod) => {
    const init = mod.cwrap('kel_worker_init', 'number', ['string', 'string']);
    const budget = mod.cwrap('kel_worker_budget_bytes', 'number', []);

    log('wasm ready, probing device');

    const usable = init(idInput.value, navigator.userAgent);
    const mib = Math.round(budget() / (1024 * 1024));

    if (usable) {
      setState('probed', `${mib} MiB usable`);
      log(`probe: usable, ${mib} MiB safe budget`);
    } else {
      // Still worth connecting. An orchestrator that knows why a device is
      // unusable is better off than one that never heard from it.
      setState('probed', 'device cannot run a solver');
      log('probe: device unusable (no float render target, or no context)');
    }

    let worker = null;

    connectButton.disabled = false;
    connectButton.onclick = () => {
      if (worker) worker.disconnect();
      worker = new Worker(mod, urlInput.value, idInput.value);
      worker.connect();
      connectButton.disabled = true;
      disconnectButton.disabled = false;
    };

    disconnectButton.onclick = () => {
      if (worker) worker.disconnect();
      worker = null;
      connectButton.disabled = false;
      disconnectButton.disabled = true;
    };
  }).catch((err) => {
    setState('error', 'wasm failed to load');
    log(`wasm load failed: ${err}`);
  });
});
