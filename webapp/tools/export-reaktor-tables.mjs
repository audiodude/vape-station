// Renders VapeStation's 5 graintables x 4 mip levels to 20 WAV files for
// import into a Reaktor Sample Map. Each WAV is the table's 64 frames
// concatenated back-to-back (2048 samples/frame, wrap-guard sample dropped),
// declared at 48000 Hz so frame boundaries land at frameIndex * 2048/48000*1000 ms.
// Run: node webapp/tools/export-reaktor-tables.mjs [outputDir]
import { grainTables } from '../js/engine.js';
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const FRAME_LEN = 2048;
const NUM_FRAMES = 64;
const SAMPLE_RATE = 48000;
const BITS = 24;
const BYTES_PER_SAMPLE = BITS / 8;

function floatTo24BitPCM(samples) {
  const buf = Buffer.alloc(samples.length * BYTES_PER_SAMPLE);
  for (let i = 0; i < samples.length; ++i) {
    const v = Math.max(-1, Math.min(1, samples[i]));
    let ival = Math.round(v * 8388607); // 2^23 - 1
    if (ival < 0) ival += 0x1000000; // two's complement in 24 bits
    buf[i * 3] = ival & 0xff;
    buf[i * 3 + 1] = (ival >> 8) & 0xff;
    buf[i * 3 + 2] = (ival >> 16) & 0xff;
  }
  return buf;
}

function writeWav(filePath, samples) {
  const data = floatTo24BitPCM(samples);
  const blockAlign = BYTES_PER_SAMPLE; // mono
  const byteRate = SAMPLE_RATE * blockAlign;
  const header = Buffer.alloc(44);
  header.write('RIFF', 0);
  header.writeUInt32LE(36 + data.length, 4);
  header.write('WAVE', 8);
  header.write('fmt ', 12);
  header.writeUInt32LE(16, 16); // PCM fmt chunk size
  header.writeUInt16LE(1, 20); // PCM format
  header.writeUInt16LE(1, 22); // mono
  header.writeUInt32LE(SAMPLE_RATE, 24);
  header.writeUInt32LE(byteRate, 28);
  header.writeUInt16LE(blockAlign, 32);
  header.writeUInt16LE(BITS, 34);
  header.write('data', 36);
  header.writeUInt32LE(data.length, 40);
  writeFileSync(filePath, Buffer.concat([header, data]));
}

function extractFrames(mip) {
  // mip is laid out as NUM_FRAMES * (FRAME_LEN + 1), each frame followed by
  // a wrap-guard sample (== frame's own first sample) that must be dropped.
  const stride = FRAME_LEN + 1;
  const out = new Float32Array(NUM_FRAMES * FRAME_LEN);
  for (let f = 0; f < NUM_FRAMES; ++f) {
    for (let i = 0; i < FRAME_LEN; ++i) {
      out[f * FRAME_LEN + i] = mip[f * stride + i];
    }
  }
  return out;
}

function main() {
  const outDir = process.argv[2]
    ? path.resolve(process.argv[2])
    : path.join(path.dirname(fileURLToPath(import.meta.url)), 'reaktor-tables');
  mkdirSync(outDir, { recursive: true });

  const tables = grainTables();
  let count = 0;
  for (const table of tables) {
    table.mips.forEach((mip, mipIndex) => {
      const samples = extractFrames(mip);
      const outPath = path.join(outDir, `${table.name}_mip${mipIndex}.wav`);
      writeWav(outPath, samples);
      count++;
      console.log(`wrote ${outPath} (${samples.length} samples)`);
    });
  }
  console.log(`\n${count} WAV files written to ${outDir}`);
}

main();
