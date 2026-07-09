// AudioWorklet wrapper around the engine. All control traffic arrives as
// port messages from the UI; display norms stream back at ~30 Hz.

import { Engine } from './engine.js';
import { defaultParams, D, S } from './params.js';

class VapeProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.engine = new Engine(sampleRate);
    this.displayCountdown = 0;
    this.port.onmessage = (e) => this.handle(e.data);
  }

  handle(m) {
    const e = this.engine;
    switch (m.type) {
      case 'param': e.setParam(m.id, m.value); break;
      case 'table': e.tableIndex = m.value | 0; break;
      case 'filterType': e.filterType = m.value | 0; break;
      case 'lfoShape': e.lfoShapes[m.which] = m.value | 0; break;
      case 'lfoMode': e.lfoModes[m.which] = m.value | 0; break;
      case 'matrix': e.setMatrix(m.routes); break;
      case 'noteOn': e.noteOn(m.note, m.vel ?? 0.8); break;
      case 'noteOff': e.noteOff(m.note); break;
      case 'sustain': e.sustain(!!m.down); break;
      case 'wheel': e.wheel = m.value; break;
      case 'bend': e.pitchBend = m.value * 2; break; // +/-2 st
      case 'init': {
        const defs = defaultParams();
        for (const id in defs) e.setParam(id, defs[id]);
        e.tableIndex = 0; e.filterType = 0;
        e.lfoShapes = [0, 0]; e.lfoModes = [0, 0];
        e.setMatrix([{ src: S.LFO1, dst: D.position, depth: 0.25 }]);
        break;
      }
    }
  }

  process(inputs, outputs) {
    const out = outputs[0];
    const L = out[0];
    const R = out.length > 1 ? out[1] : out[0];
    this.engine.process(L, R, L.length);

    if (--this.displayCountdown <= 0) {
      this.displayCountdown = 12; // ~31 Hz at 128-sample blocks / 48k
      const norms = this.engine.displayNorms();
      this.port.postMessage({ type: 'display', norms: norms ? Array.from(norms) : null });
    }
    return true;
  }
}

registerProcessor('vape-station', VapeProcessor);
