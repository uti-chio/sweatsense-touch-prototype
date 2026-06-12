const baudRate = 115200;
const encoder = new TextEncoder();
const decoder = new TextDecoder();

const state = {
  port: null,
  reader: null,
  writer: null,
  connected: false,
  demo: false,
  readingCsv: false,
  rows: [],
  demoTimer: null,
};

const el = (id) => document.getElementById(id);
const logEl = el("log");
const chart = el("sweep-chart");
const ctx = chart.getContext("2d");

function log(message, tone = "normal") {
  const stamp = new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });
  const prefix = tone === "tx" ? ">>" : tone === "warn" ? "!!" : "<<";
  logEl.textContent += `[${stamp}] ${prefix} ${message}\n`;
  logEl.scrollTop = logEl.scrollHeight;
}

function setConnection(mode, label) {
  const pill = el("connection-pill");
  pill.classList.toggle("connected", mode === "connected");
  pill.classList.toggle("demo", mode === "demo");
  el("connection-label").textContent = label;
  el("connect-btn").disabled = mode === "connected";
  el("disconnect-btn").disabled = mode !== "connected";
  el("stop-btn").disabled = mode === "idle";
}

function pulse(selector) {
  const node = document.querySelector(selector);
  if (!node) return;
  node.classList.remove("pulse");
  void node.offsetWidth;
  node.classList.add("pulse");
}

async function connectSerial() {
  if (!("serial" in navigator)) {
    log("Web Serial is not available here. Open this page in Chrome or Edge from localhost.", "warn");
    return;
  }

  try {
    state.port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: 0x303a }],
    });
    await state.port.open({ baudRate });
    state.writer = state.port.writable.getWriter();
    state.connected = true;
    state.demo = false;
    setConnection("connected", "USB connected");
    log("Connected to ESP32-S3 serial port.");
    readLoop();
    await sendCommand("STATUS");
  } catch (err) {
    log(`Connect failed: ${err.message || err}`, "warn");
  }
}

async function disconnectSerial() {
  stopDemo();
  try {
    if (state.reader) {
      await state.reader.cancel();
      state.reader.releaseLock();
      state.reader = null;
    }
    if (state.writer) {
      state.writer.releaseLock();
      state.writer = null;
    }
    if (state.port) {
      await state.port.close();
      state.port = null;
    }
  } catch (err) {
    log(`Disconnect warning: ${err.message || err}`, "warn");
  }
  state.connected = false;
  setConnection("idle", "Not connected");
}

async function readLoop() {
  let buffer = "";
  while (state.port?.readable && state.connected) {
    state.reader = state.port.readable.getReader();
    try {
      for (;;) {
        const { value, done } = await state.reader.read();
        if (done) break;
        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split(/\r?\n/);
        buffer = lines.pop() || "";
        for (const line of lines) handleLine(line.trim());
      }
    } catch (err) {
      if (state.connected) log(`Read error: ${err.message || err}`, "warn");
    } finally {
      state.reader.releaseLock();
      state.reader = null;
    }
  }
}

function handleLine(line) {
  if (!line) return;
  log(line);

  if (line.includes("=== EIS DATA CSV ===")) {
    state.readingCsv = true;
    state.rows = [];
    updateMetrics();
    drawChart();
    return;
  }

  if (line.includes("=== END CSV ===")) {
    state.readingCsv = false;
    inferSweatMetrics();
    return;
  }

  if (state.readingCsv) parseCsvLine(line);

  if (line.includes("AD5940 appears connected")) pulse(".afe");
  if (line.includes("STATUS")) pulse(".mcu");
  if (line.includes("SWEATUI dashboard updated")) pulse(".display");
}

function parseCsvLine(line) {
  if (!/^\s*[-+0-9.]/.test(line) || !line.includes(",")) return;
  const parts = line.split(",").map((x) => Number.parseFloat(x.trim()));
  if (parts.length < 5 || parts.some((x) => !Number.isFinite(x))) return;
  const [frequency, real, imaginary, magnitude, phase] = parts;
  state.rows.push({ frequency, real, imaginary, magnitude, phase });
  updateMetrics();
  drawChart();
}

async function sendCommand(command) {
  stopDemo();
  if (!state.connected || !state.writer) {
    log(`Not connected. Simulating command: ${command}`, "warn");
    runDemo(command);
    return;
  }

  log(command, "tx");
  await state.writer.write(encoder.encode(`${command}\n`));
}

function customCommand() {
  const start = valueOr("start-freq", "10000");
  const end = valueOr("end-freq", "1000");
  const points = valueOr("points-decade", "2");
  const amp = valueOr("amplitude", "150");
  return `MEASURE:0,${start},${end},${points},0.0,0.0,10000.0,1,1,127000.0,150.0,0,0,${amp}`;
}

function valueOr(id, fallback) {
  const value = el(id).value.trim();
  return value || fallback;
}

function runDemo(command = "DEMO") {
  stopDemo();
  state.demo = true;
  state.rows = [];
  state.readingCsv = true;
  setConnection("demo", "Demo mode");
  log(`Demo response for ${command}`);
  log("AD5940 appears connected");
  pulse(".mcu");
  pulse(".afe");

  const frequencies = [10000, 5623, 3162, 1778, 1000, 562, 316, 178, 100, 56, 32, 18, 10];
  let i = 0;
  state.demoTimer = setInterval(() => {
    const f = frequencies[i];
    const contact = 0.65 + 0.22 * Math.sin(i / 2);
    const real = 310 + 90 * Math.log10(10000 / f + 1) + 18 * Math.sin(i);
    const imaginary = -45 - 120 * contact * Math.sqrt(1000 / f);
    const magnitude = Math.hypot(real, imaginary);
    const phase = Math.atan2(imaginary, real) * 180 / Math.PI;
    const row = { frequency: f, real, imaginary, magnitude, phase };
    state.rows.push(row);
    log(`${f.toFixed(1)},${real.toFixed(2)},${imaginary.toFixed(2)},${magnitude.toFixed(2)},${phase.toFixed(2)}`);
    updateMetrics();
    drawChart();
    i += 1;
    if (i >= frequencies.length) {
      stopDemo(false);
      inferSweatMetrics();
      log("Demo sweep complete.");
    }
  }, 460);
}

function stopDemo(resetConnection = true) {
  if (state.demoTimer) {
    clearInterval(state.demoTimer);
    state.demoTimer = null;
  }
  state.readingCsv = false;
  if (resetConnection && state.demo && !state.connected) {
    state.demo = false;
    setConnection("idle", "Not connected");
  }
}

async function stopBoard() {
  if (state.connected) {
    await sendCommand("STOP");
  } else {
    stopDemo();
    log("Stopped local demo.");
  }
}

function updateMetrics() {
  el("point-count").textContent = `${state.rows.length} point${state.rows.length === 1 ? "" : "s"}`;
  if (!state.rows.length) {
    el("hydration-value").textContent = "--";
    el("contact-value").textContent = "--";
    el("trend-value").textContent = "--";
    el("hydration-note").textContent = "Waiting for sweep";
    return;
  }
  inferSweatMetrics(false);
}

function inferSweatMetrics(final = true) {
  if (!state.rows.length) return;
  const low = [...state.rows].sort((a, b) => a.frequency - b.frequency)[0];
  const high = [...state.rows].sort((a, b) => b.frequency - a.frequency)[0];
  const ratio = high && low ? low.magnitude / Math.max(high.magnitude, 1) : 1;
  const hydration = clamp(Math.round(100 - (ratio - 1) * 30), 25, 96);
  const contact = clamp(Math.round(100 - Math.abs(low.phase) * 1.7), 20, 99);
  const trend = hydration > 72 ? "Stable" : hydration > 52 ? "Watch" : "Low";

  el("hydration-value").textContent = `${hydration}%`;
  el("contact-value").textContent = `${contact}%`;
  el("trend-value").textContent = trend;
  el("hydration-note").textContent = final ? "from latest EIS sweep" : "reading...";
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function drawChart() {
  const width = chart.width;
  const height = chart.height;
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#fbfdfb";
  ctx.fillRect(0, 0, width, height);

  const pad = { left: 62, right: 26, top: 24, bottom: 48 };
  const plotW = width - pad.left - pad.right;
  const plotH = height - pad.top - pad.bottom;

  ctx.strokeStyle = "#d8e1da";
  ctx.lineWidth = 1;
  ctx.beginPath();
  for (let i = 0; i <= 5; i++) {
    const y = pad.top + (plotH * i) / 5;
    ctx.moveTo(pad.left, y);
    ctx.lineTo(width - pad.right, y);
  }
  ctx.stroke();

  ctx.fillStyle = "#68756d";
  ctx.font = "13px Segoe UI, sans-serif";
  ctx.fillText("Frequency response: |Z| and phase", pad.left, 17);
  ctx.fillText("Hz", width - pad.right - 14, height - 16);
  ctx.save();
  ctx.translate(18, height / 2 + 36);
  ctx.rotate(-Math.PI / 2);
  ctx.fillText("|Z| ohms", 0, 0);
  ctx.restore();

  if (!state.rows.length) {
    ctx.fillStyle = "#9aa6a0";
    ctx.font = "18px Segoe UI, sans-serif";
    ctx.fillText("Connect the board or start demo mode", pad.left + 170, pad.top + plotH / 2);
    return;
  }

  const rows = [...state.rows].sort((a, b) => a.frequency - b.frequency);
  const xs = rows.map((r) => Math.log10(r.frequency));
  const mags = rows.map((r) => r.magnitude);
  const minX = Math.min(...xs);
  const maxX = Math.max(...xs);
  const minY = Math.min(...mags) * 0.92;
  const maxY = Math.max(...mags) * 1.08;

  const xScale = (x) => pad.left + ((Math.log10(x) - minX) / Math.max(maxX - minX, 0.001)) * plotW;
  const yScale = (y) => pad.top + plotH - ((y - minY) / Math.max(maxY - minY, 1)) * plotH;

  ctx.strokeStyle = "#2d6cdf";
  ctx.lineWidth = 3;
  ctx.beginPath();
  rows.forEach((r, i) => {
    const x = xScale(r.frequency);
    const y = yScale(r.magnitude);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();

  rows.forEach((r) => {
    const x = xScale(r.frequency);
    const y = yScale(r.magnitude);
    ctx.fillStyle = "#ffffff";
    ctx.strokeStyle = "#2d6cdf";
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(x, y, 4.5, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
  });

  ctx.fillStyle = "#68756d";
  ctx.font = "12px Segoe UI, sans-serif";
  const minFreq = rows[0].frequency;
  const maxFreq = rows[rows.length - 1].frequency;
  ctx.fillText(formatFreq(minFreq), pad.left, height - 22);
  ctx.fillText(formatFreq(maxFreq), width - pad.right - 70, height - 22);
  ctx.fillText(`${Math.round(maxY)} ohm`, 8, pad.top + 5);
  ctx.fillText(`${Math.round(minY)} ohm`, 8, pad.top + plotH);
}

function formatFreq(f) {
  if (f >= 1000) return `${(f / 1000).toFixed(f >= 10000 ? 0 : 1)} kHz`;
  return `${Math.round(f)} Hz`;
}

function wireEvents() {
  el("connect-btn").addEventListener("click", connectSerial);
  el("disconnect-btn").addEventListener("click", disconnectSerial);
  el("demo-btn").addEventListener("click", () => runDemo("presentation demo"));
  el("stop-btn").addEventListener("click", stopBoard);
  el("clear-log-btn").addEventListener("click", () => { logEl.textContent = ""; });
  el("custom-sweep-btn").addEventListener("click", () => sendCommand(customCommand()));

  document.querySelectorAll("[data-command]").forEach((button) => {
    button.addEventListener("click", () => sendCommand(button.dataset.command));
  });

  document.querySelectorAll("[data-action]").forEach((button) => {
    button.addEventListener("click", () => {
      const action = button.dataset.action;
      if (action === "electrode") {
        pulse(".pad-left");
        pulse(".pad-right");
        sendCommand(customCommand());
      } else if (action === "check") {
        pulse(".afe");
        sendCommand("CHECK");
      } else if (action === "status") {
        pulse(".mcu");
        sendCommand("STATUS");
      } else if (action === "display") {
        pulse(".display");
        sendCommand("SWEATUI:92,31.7,78,66");
      } else if (action === "button") {
        sendCommand("MEASURE:SAMPLE");
      }
    });
  });
}

function init() {
  if (!("serial" in navigator)) {
    el("support-note").textContent = "Web Serial is unavailable here. Use Chrome or Edge from localhost; demo mode still works.";
  }
  setConnection("idle", "Not connected");
  wireEvents();
  drawChart();
  log("Ready. Connect USB, then click the board or use Demo mode.");
}

init();
