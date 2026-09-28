// flash.js — A Dark Room onto an Xteink, over Web Serial, with esptool-js.
//
// The page offers one factory image per device (firmware/manifest.json: bootloader, partition
// table, OTA selector and app, merged to be written at 0x0). With "keep the save" on, the NVS
// partition (where the game keeps its save and settings) is left out of the write, so an update
// goes on from where the game was. "Save what is on it" reads the whole flash to a file first.

import { ESPLoader, Transport } from './vendor/esptool-js-0.7.0.js';
import { md5 } from './md5.js';

const DEVICES = {
  x4pro: { name: 'X4 Pro', chip: 'ESP32-S3', build: 'x4pro' },
  x4c: { name: 'X4 Classic', chip: 'ESP32-S3', build: 'x4c' },
  x3: { name: 'X3', chip: 'ESP32-C3', build: 'x3' },
  x4: { name: 'X4', chip: 'ESP32-C3', build: 'x3' }, // the first X4 runs the X3's binary
};
const ESPRESSIF_VID = 0x303a;
const FLASH_BYTES = 16 * 1024 * 1024;

const $ = (sel) => document.querySelector(sel);
const $$ = (sel) => [...document.querySelectorAll(sel)];

const store = {
  get(key) { try { return localStorage.getItem(key); } catch { return null; } },
  set(key, value) { try { localStorage.setItem(key, value); } catch { /* private window */ } },
};

// ---- the log: newest first, the older lines fading, as the game's notifications do ------------

const logEl = $('#log');
function say(text) {
  const p = document.createElement('p');
  p.textContent = text;
  logEl.prepend(p);
  while (logEl.children.length > 14) logEl.lastElementChild.remove();
}

// esptool-js's own words, for when something goes wrong.
const termEl = $('#terminal');
const terminal = {
  clean() { termEl.textContent = ''; },
  writeLine(s) { termEl.textContent += s + '\n'; termEl.scrollTop = termEl.scrollHeight; },
  write(s) { termEl.textContent += s; },
};

// ---- lights ------------------------------------------------------------------------------------

function setLights(on) {
  document.documentElement.dataset.theme = on ? 'light' : 'dark';
  $('#lights').textContent = on ? 'lights off.' : 'lights on.';
}
{
  const saved = store.get('lights');
  setLights(saved ? saved === 'on' : !matchMedia('(prefers-color-scheme: dark)').matches);
  $('#lights').addEventListener('click', () => {
    const on = document.documentElement.dataset.theme !== 'light';
    setLights(on);
    store.set('lights', on ? 'on' : 'off');
  });
}

// ---- choices -----------------------------------------------------------------------------------

let device = DEVICES[store.get('device')] ? store.get('device') : null;
const options = { keep: true, backup: false };
let manifest = null;
let busy = false;
let backup = null; // a file chosen to write back, in place of the game

function idle() { return backup ? 'write it back' : 'flash'; }

function kb(n) { return Math.round(n / 1024).toLocaleString() + ' KB'; }

function render() {
  for (const b of $$('[data-device]')) b.setAttribute('aria-checked', String(b.dataset.device === device));
  for (const b of $$('[data-option]')) {
    const on = options[b.dataset.option];
    b.setAttribute('aria-checked', String(on));
    b.querySelector('i').textContent = on ? 'on' : 'off';
  }
  const flash = $('#flash');
  const serial = 'serial' in navigator;
  flash.disabled = busy || !serial || (!backup && (!device || !manifest));
  for (const b of $$('[data-device], [data-option]')) b.disabled = busy;
  $('[data-option="keep"]').disabled = busy || !!backup;
  $('#restore').disabled = busy || !serial;
  $('#restore').textContent = backup ? 'cancel.' : 'write a backup back…';
  if (!busy) flash.querySelector('.label').textContent = idle();

  const d = device && DEVICES[device];
  const fw = d && manifest && manifest.firmware[d.build];
  $('#status').textContent = !serial ? 'this browser cannot reach a device: use Chrome or Edge, on a computer.'
    : backup ? `${backup.name} · ${kb(backup.size)} · written over everything, from 0x0`
    : !manifest ? 'the firmware is not here yet.'
    : !d ? 'choose the device.'
    : `${d.name} · ${d.chip.toLowerCase()} · ${fw.file} · ${kb(fw.size)}`;
  $('#download').hidden = !fw || !!backup;
  if (fw) $('#download').href = 'firmware/' + fw.file;
}

for (const b of $$('[data-device]')) {
  b.addEventListener('click', () => {
    device = b.dataset.device;
    backup = null;
    store.set('device', device);
    render();
  });
}
for (const b of $$('[data-option]')) {
  b.addEventListener('click', () => {
    options[b.dataset.option] = !options[b.dataset.option];
    render();
  });
}

// ---- the button's bar: the game's cooldown, filling instead of emptying -------------------------

function progress(label, fraction) {
  const flash = $('#flash');
  flash.querySelector('.label').textContent = label;
  flash.querySelector('.bar').style.width = fraction == null ? '0' : `${Math.max(0, Math.min(1, fraction)) * 100}%`;
}

// ---- the firmware ------------------------------------------------------------------------------

async function loadManifest() {
  try {
    const r = await fetch('firmware/manifest.json', { cache: 'no-cache' });
    if (!r.ok) throw new Error(r.status);
    manifest = await r.json();
    $('#built').textContent = 'built ' + manifest.built + (manifest.version ? ' · ' + manifest.version : '');
  } catch {
    say('the firmware could not be found.');
  }
  render();
}

function hex(bytes) { return Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join(''); }

async function fetchFirmware(build) {
  const entry = manifest.firmware[build];
  // GitHub Pages lets files be cached for ten minutes; ask again, so a new manifest never meets
  // an old image.
  const r = await fetch('firmware/' + entry.file, { cache: 'no-cache' });
  if (!r.ok) throw new Told(`the firmware could not be fetched (${r.status}).`);
  const data = new Uint8Array(await r.arrayBuffer());
  const sum = hex(new Uint8Array(await crypto.subtle.digest('SHA-256', data)));
  if (sum !== entry.sha256) throw new Told('the firmware came down damaged. reload the page and try again.');
  return data;
}

// The NVS partition, from the image's own partition table (at 0x8000; 32-byte entries, magic
// 0xAA 0x50; type 1 is data, subtype 2 is nvs).
function findNvs(image) {
  for (let off = 0x8000; off + 32 <= image.length && off < 0x9000; off += 32) {
    if (image[off] !== 0xaa || image[off + 1] !== 0x50) break;
    if (image[off + 2] === 0x01 && image[off + 3] === 0x02) {
      const dv = new DataView(image.buffer, image.byteOffset + off, 32);
      return { offset: dv.getUint32(4, true), size: dv.getUint32(8, true) };
    }
  }
  return null;
}

// What to write: the whole image at 0x0, or, keeping the save, all of it but the NVS partition.
function pieces(image, keep) {
  const nvs = keep ? findNvs(image) : null;
  if (!nvs) return [{ address: 0, data: image }];
  const end = nvs.offset + nvs.size;
  return [
    { address: 0, data: image.slice(0, nvs.offset) },
    { address: end, data: image.slice(end) },
  ].filter((p) => p.data.length > 0);
}

// The whole flash, in pieces: esptool-js grows its buffer a packet at a time, which over 16 MB
// would copy gigabytes.
async function readAll(loader, bytes, onProgress) {
  const out = new Uint8Array(bytes);
  const CHUNK = 256 * 1024;
  for (let at = 0; at < bytes; at += CHUNK) {
    const n = Math.min(CHUNK, bytes - at);
    const part = await loader.readFlash(at, n, (_packet, got) => onProgress((at + got) / bytes));
    out.set(part.subarray(0, n), at);
  }
  return out;
}

function download(data, name) {
  const url = URL.createObjectURL(new Blob([data], { type: 'application/octet-stream' }));
  const a = Object.assign(document.createElement('a'), { href: url, download: name });
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60_000);
}

// An error whose message is meant for the page as it is.
class Told extends Error {}

function tell(e) {
  if (e instanceof Told) return e.message;
  const m = String((e && e.message) || e);
  if (e && e.name === 'NotFoundError') return 'no device was chosen.';
  if (/Failed to open|already open|InvalidStateError|NetworkError/i.test(m) || (e && e.name === 'InvalidStateError')) {
    return 'the port is busy. close whatever else is using it (a serial monitor, another tab) and try again.';
  }
  if (/Failed to connect|timeout|timed out/i.test(m)) {
    return 'the device did not answer. wake it, try another cable or port, and try again.';
  }
  if (/MD5/i.test(m)) return 'what was written does not match. try again.';
  return 'something went wrong: ' + m;
}

// ---- the flashing ------------------------------------------------------------------------------

async function run() {
  const file = backup;
  const kind = file ? 'restore' : 'install';
  const d = DEVICES[device];
  busy = true;
  terminal.clean();
  let transport = null;
  try {
    // The port first: the browser shows its list only while the click is still fresh.
    say('choose the device in the list.');
    const port = navigator.serial.requestPort({ filters: [{ usbVendorId: ESPRESSIF_VID }] });
    render();
    progress('choosing', null);
    const chosen = await port;

    let image;
    if (kind === 'install') {
      progress('fetching', null);
      image = await fetchFirmware(d.build);
    } else {
      image = new Uint8Array(await file.arrayBuffer());
    }

    transport = new Transport(chosen, false);
    // Over the chips' own USB the baud rate means nothing, and leaving it alone spares a port
    // reopen that can knock the chip out of its bootloader.
    const loader = new ESPLoader({ transport, baudrate: 115200, romBaudrate: 115200, terminal });
    progress('waking', null);
    say('the device stirs.');
    await loader.main();
    const chip = loader.chip.CHIP_NAME;
    say(`it answers: an ${chip.toLowerCase()}.`);
    if (kind === 'install' && chip !== d.chip) {
      throw new Told(`that is an ${chip}, and the ${d.name} is an ${d.chip}. choose the device it is.`);
    }

    const sizeName = await loader.detectFlashSize();
    const flashBytes = sizeName ? loader.flashSizeBytes(sizeName) : FLASH_BYTES;
    if (kind === 'install' && flashBytes < FLASH_BYTES) {
      throw new Told(`its flash holds ${sizeName}; this firmware is laid out for 16MB.`);
    }
    if (image.length > flashBytes) throw new Told('that file is larger than the device\'s flash.');

    if (options.backup) {
      say(`reading what is on it (${sizeName || '16MB'}). this takes a few minutes.`);
      const old = await readAll(loader, flashBytes, (f) => progress(`reading ${Math.floor(f * 100)}%`, f));
      progress('checking', 1);
      if ((await loader.flashMd5sum(0, flashBytes)) !== md5(old)) {
        throw new Told('the copy came back damaged, so nothing was written. try again.');
      }
      const date = new Date().toISOString().slice(0, 10);
      download(old, `xteink-${chip.toLowerCase()}-backup-${date}.bin`);
      say('the old firmware is kept, in a file.');
    }

    const files = kind === 'install' ? pieces(image, options.keep) : [{ address: 0, data: image }];
    const total = files.reduce((n, f) => n + f.data.length, 0);
    const before = files.map((_, i) => files.slice(0, i).reduce((n, f) => n + f.data.length, 0));
    say(kind === 'install'
      ? (files.length > 1 ? 'writing. the save is left where it is.' : 'writing.')
      : `writing ${file.name}.`);
    progress('writing 0%', 0);
    await loader.writeFlash({
      fileArray: files,
      flashMode: 'keep',
      flashFreq: 'keep',
      flashSize: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (i, written, of) => {
        const f = (before[i] + (of ? written / of : 1) * files[i].data.length) / total;
        progress(`writing ${Math.floor(f * 100)}%`, f);
      },
      calculateMD5Hash: md5,
    });
    say('written, and checked.');
    await loader.after('hard_reset');
    progress('done', 1);
    if (kind === 'install') {
      say('the fire is lit.');
      say('it wakes into a dark room. if the glass stays blank, unplug it and press power.');
    } else {
      say('it restarts into what was written.');
      backup = null;
    }
  } catch (e) {
    console.error(e);
    say(tell(e));
    progress(idle(), null);
  } finally {
    if (transport) {
      try { await transport.disconnect(); } catch { /* already gone */ }
    }
    busy = false;
    render();
    setTimeout(() => { if (!busy) progress(idle(), null); }, 4000);
  }
}

$('#flash').addEventListener('click', run);

$('#restore').addEventListener('click', () => {
  if (backup) {
    backup = null;
    render();
  } else {
    $('#file').click();
  }
});
$('#file').addEventListener('change', () => {
  const file = $('#file').files[0];
  $('#file').value = '';
  if (!file) return;
  const sure = confirm(`write ${file.name} (${kb(file.size)}) over everything on the device, from 0x0?`
    + '\n\nonly do this with a whole-flash image, such as a backup this page saved.');
  if (!sure) return;
  backup = file;
  say(`${file.name} waits to be written.`);
  render();
});

addEventListener('beforeunload', (e) => {
  if (busy) e.preventDefault();
});

if (!('serial' in navigator)) say('this browser cannot reach a device.');
render();
loadManifest();
