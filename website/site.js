// Cosmic Chords site: the library demo (piano roll + Web Audio synth) and the Spread diagram.
(() => {
  "use strict";

  const NAMES = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"];
  const noteName = (n) => NAMES[n % 12] + (Math.floor(n / 12) - 1);
  const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();

  // ---------------------------------------------------------------- demo data
  const progressions = window.COSMIC_DEMO || [];
  const BPM = 92;
  const BEAT = 60 / BPM;   // seconds per beat

  // Chord start times (in beats) and total length of a progression.
  const timeline = (p) => {
    let t = 0;
    const starts = p.chords.map((c) => { const s = t; t += c.beats || 4; return s; });
    return { starts, total: t };
  };

  const list = document.getElementById("demo-list");
  const canvas = document.getElementById("roll-canvas");
  const ctx2d = canvas.getContext("2d");
  const chordsEl = document.getElementById("roll-chords");
  const nameEl = document.getElementById("roll-name");
  const metaEl = document.getElementById("roll-meta");
  const playBtn = document.getElementById("play");

  let current = 0;
  let playing = null;   // { startTime, nodes, master }
  let audio = null;

  const describe = (p) => {
    const key = p.key.replace(" Minor", " minor").replace(" Major", " major");
    const withTop = p.chords.some((c) => c.melody) ? ", with top line" : "";
    return `${p.genre} in ${key}${withTop}`;
  };

  progressions.forEach((p, i) => {
    const li = document.createElement("li");
    const b = document.createElement("button");
    b.type = "button";
    b.className = "demo-item";
    b.setAttribute("role", "option");
    b.innerHTML = `<span class="name"></span><span class="chords"></span>`;
    b.querySelector(".name").textContent = p.name;
    if (p.chords.some((c) => c.melody)) {
      const tag = document.createElement("span");
      tag.className = "topline-tag";
      tag.textContent = "Top line";
      b.querySelector(".name").appendChild(tag);
    }
    b.querySelector(".chords").textContent = p.chords.map((c) => c.name).join("  ");
    b.addEventListener("click", () => select(i, true));
    li.appendChild(b);
    list.appendChild(li);
  });

  function select(i, autoplay) {
    stop();
    current = i;
    const p = progressions[i];
    list.querySelectorAll(".demo-item").forEach((b, k) => b.setAttribute("aria-selected", String(k === i)));
    nameEl.textContent = p.name;
    metaEl.textContent = describe(p);
    chordsEl.innerHTML = "";
    p.chords.forEach((c) => {
      const li = document.createElement("li");
      li.textContent = c.name;
      const small = document.createElement("small");
      small.textContent = c.roman.replace(/b(?=[ivIV])/, "♭").replace(/#(?=[ivIV])/, "♯");
      li.appendChild(small);
      chordsEl.appendChild(li);
    });
    draw(-1);
    if (autoplay) play();
  }

  // ---------------------------------------------------------------- piano roll
  function sizeCanvas() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = canvas.clientWidth || 600;
    const h = Math.round(w * 0.5);
    canvas.width = Math.round(w * dpr);
    canvas.height = Math.round(h * dpr);
    canvas.style.height = h + "px";
    ctx2d.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  function draw(playheadSec) {
    const p = progressions[current];
    if (!p) return;
    const w = canvas.clientWidth, h = parseFloat(canvas.style.height) || canvas.height;
    const notes = p.chords.flatMap((c) => c.notes.concat((c.melody || []).map((m) => m[0])));
    const lo = Math.min(...notes) - 2, hi = Math.max(...notes) + 2;
    const rows = hi - lo + 1;
    const rowH = h / rows;
    const gutter = 34;
    const { starts, total } = timeline(p);
    const beatW = (w - gutter) / total;
    const beat = playheadSec >= 0 ? playheadSec / BEAT : -1;
    let activeChord = -1;
    starts.forEach((s, k) => { if (beat >= s) activeChord = k; });

    ctx2d.clearRect(0, 0, w, h);

    // Octave lines at every C, labelled.
    ctx2d.font = "11px 'Bricolage Grotesque', sans-serif";
    ctx2d.textBaseline = "middle";
    for (let n = lo; n <= hi; n++) {
      const y = h - (n - lo + 1) * rowH;
      if ([1, 3, 6, 8, 10].includes(n % 12)) {
        ctx2d.fillStyle = "rgba(255,255,255,0.018)";
        ctx2d.fillRect(gutter, y, w - gutter, rowH);
      }
      if (n % 12 === 0) {
        ctx2d.fillStyle = "rgba(255,255,255,0.08)";
        ctx2d.fillRect(gutter, y + rowH - 1, w - gutter, 1);
        ctx2d.fillStyle = css("--on-graphite-dim");
        ctx2d.fillText(noteName(n), 4, y + rowH / 2);
      }
    }
    // Bar lines.
    for (let k = 1; k < p.chords.length; k++) {
      ctx2d.fillStyle = "rgba(255,255,255,0.07)";
      ctx2d.fillRect(gutter + starts[k] * beatW, 0, 1, h);
    }

    const amber = css("--amber");
    p.chords.forEach((c, k) => {
      const on = k === activeChord;
      const colW = (c.beats || 4) * beatW;
      c.notes.forEach((n) => {
        const x = gutter + starts[k] * beatW + 3, y = h - (n - lo + 1) * rowH + 1;
        const bw = colW - 6, bh = Math.max(5, rowH - 1.5);
        ctx2d.save();
        if (on) { ctx2d.shadowColor = amber; ctx2d.shadowBlur = 12; }
        ctx2d.fillStyle = on ? amber : (activeChord > k ? "rgba(230,231,234,0.35)" : "rgba(230,231,234,0.72)");
        roundRect(x, y, bw, bh, Math.min(4, bh / 2));
        ctx2d.fill();
        ctx2d.restore();
      });
    });

    // Top line: outlined notes, filled while they sound.
    p.chords.forEach((c, k) => {
      (c.melody || []).forEach(([n, s, len]) => {
        const start = starts[k] + s;
        const x = gutter + start * beatW + 1.5, y = h - (n - lo + 1) * rowH + 1;
        const bw = Math.max(4, len * beatW - 3), bh = Math.max(5, rowH - 1.5);
        const sounding = beat >= start && beat < start + len;
        ctx2d.save();
        roundRect(x, y, bw, bh, Math.min(4, bh / 2));
        if (sounding) { ctx2d.shadowColor = amber; ctx2d.shadowBlur = 12; ctx2d.fillStyle = amber; ctx2d.fill(); }
        ctx2d.strokeStyle = amber;
        ctx2d.lineWidth = 1.5;
        ctx2d.globalAlpha = sounding ? 1 : (beat > start ? 0.45 : 0.9);
        ctx2d.stroke();
        ctx2d.restore();
      });
    });

    if (playheadSec >= 0) {
      const x = gutter + beat * beatW;
      ctx2d.fillStyle = amber;
      ctx2d.fillRect(x, 0, 2, h);
    }

    chordsEl.querySelectorAll("li").forEach((li, k) => li.classList.toggle("is-on", k === activeChord));
  }

  function roundRect(x, y, w, h, r) {
    ctx2d.beginPath();
    ctx2d.moveTo(x + r, y);
    ctx2d.arcTo(x + w, y, x + w, y + h, r);
    ctx2d.arcTo(x + w, y + h, x, y + h, r);
    ctx2d.arcTo(x, y + h, x, y, r);
    ctx2d.arcTo(x, y, x + w, y, r);
    ctx2d.closePath();
  }

  // ---------------------------------------------------------------- synth
  const freq = (n) => 440 * Math.pow(2, (n - 69) / 12);

  function play() {
    stop();
    audio = audio || new (window.AudioContext || window.webkitAudioContext)();
    if (audio.state === "suspended") audio.resume();
    const p = progressions[current];
    const t0 = audio.currentTime + 0.06;

    const master = audio.createGain();
    master.gain.value = 0.9;
    const filter = audio.createBiquadFilter();
    filter.type = "lowpass";
    filter.frequency.value = 2400;
    filter.Q.value = 0.4;
    filter.connect(master);
    master.connect(audio.destination);

    const nodes = [];
    const { starts, total } = timeline(p);
    const hasTop = p.chords.some((c) => c.melody);
    p.chords.forEach((c, k) => {
      const start = t0 + starts[k] * BEAT, end = start + (c.beats || 4) * BEAT * 0.96;
      const level = (hasTop ? 0.11 : 0.16) / Math.sqrt(c.notes.length);
      c.notes.forEach((n, i) => {
        const g = audio.createGain();
        g.gain.setValueAtTime(0, start);
        g.gain.linearRampToValueAtTime(level * (i === 0 ? 1.2 : 1), start + 0.025);
        g.gain.exponentialRampToValueAtTime(level * 0.55, start + 0.9);
        g.gain.setValueAtTime(level * 0.55, end - 0.05);
        g.gain.exponentialRampToValueAtTime(0.0001, end + 0.45);
        g.connect(filter);
        for (const [type, detune, mix] of [["triangle", -4, 1], ["sine", 5, 0.7]]) {
          const o = audio.createOscillator();
          o.type = type;
          o.frequency.value = freq(n);
          o.detune.value = detune;
          const m = audio.createGain();
          m.gain.value = mix;
          o.connect(m).connect(g);
          o.start(start);
          o.stop(end + 0.5);
          nodes.push(o);
        }
      });
    });

    // Top line: a soft sine lead with a little vibrato, above the pad.
    p.chords.forEach((c, k) => {
      (c.melody || []).forEach(([n, s, len]) => {
        const start = t0 + (starts[k] + s) * BEAT, end = start + len * BEAT * 0.92;
        const g = audio.createGain();
        g.gain.setValueAtTime(0, start);
        g.gain.linearRampToValueAtTime(0.13, start + 0.02);
        g.gain.exponentialRampToValueAtTime(0.08, start + 0.35);
        g.gain.setValueAtTime(0.08, Math.max(start + 0.36, end - 0.03));
        g.gain.exponentialRampToValueAtTime(0.0001, end + 0.25);
        g.connect(master);
        const o = audio.createOscillator();
        o.type = "sine";
        o.frequency.value = freq(n);
        const lfo = audio.createOscillator(), depth = audio.createGain();
        lfo.frequency.value = 5;
        depth.gain.value = 6;
        lfo.connect(depth).connect(o.detune);
        o.connect(g);
        for (const osc of [o, lfo]) { osc.start(start); osc.stop(end + 0.3); nodes.push(osc); }
      });
    });

    playing = { startTime: t0, master, total: total * BEAT };
    playBtn.classList.add("is-playing");
    playBtn.setAttribute("aria-label", "Stop");
    requestAnimationFrame(tick);
    playing.nodes = nodes;
  }

  function stop() {
    if (!playing) return;
    const { master, nodes } = playing;
    const now = audio.currentTime;
    master.gain.cancelScheduledValues(now);
    master.gain.setValueAtTime(master.gain.value, now);
    master.gain.linearRampToValueAtTime(0, now + 0.08);
    nodes.forEach((o) => { try { o.stop(now + 0.1); } catch (e) { /* already stopped */ } });
    playing = null;
    playBtn.classList.remove("is-playing");
    playBtn.setAttribute("aria-label", "Play");
    draw(-1);
  }

  function tick() {
    if (!playing) return;
    const t = audio.currentTime - playing.startTime;
    if (t >= playing.total + 0.3) { stop(); return; }
    draw(Math.max(0, Math.min(t, playing.total - 0.001)));
    requestAnimationFrame(tick);
  }

  playBtn.addEventListener("click", () => (playing ? stop() : play()));

  // Arrow keys move through the list.
  list.addEventListener("keydown", (e) => {
    if (e.key !== "ArrowDown" && e.key !== "ArrowUp" && e.key !== "ArrowRight" && e.key !== "ArrowLeft") return;
    e.preventDefault();
    const dir = e.key === "ArrowDown" || e.key === "ArrowRight" ? 1 : -1;
    const next = (current + dir + progressions.length) % progressions.length;
    select(next, false);
    list.querySelectorAll(".demo-item")[next].focus();
  });

  window.addEventListener("resize", () => { sizeCanvas(); draw(-1); });
  if (progressions.length) {
    sizeCanvas();
    select(0, false);
    if (document.fonts) document.fonts.ready.then(() => draw(-1));
  }

  // ---------------------------------------------------------------- copy buttons
  document.querySelectorAll("[data-copy]").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const text = document.getElementById(btn.dataset.copy).textContent;
      try {
        await navigator.clipboard.writeText(text);
        btn.textContent = "Copied";
        btn.classList.add("is-done");
        setTimeout(() => { btn.textContent = "Copy"; btn.classList.remove("is-done"); }, 1800);
      } catch (e) {
        btn.textContent = "Select and copy";
      }
    });
  });

  // ---------------------------------------------------------------- spread diagram
  const levels = [
    ["Close", [48, 52, 55, 59]],
    ["Bass", [48, 64, 67, 71]],
    ["Open", [48, 64, 71, 79]],
    ["Deep", [36, 64, 71, 79]],
    ["Wide", [36, 43, 64, 71, 79]],
  ];
  const grid = document.getElementById("spread-grid");
  if (grid) {
    const lo = 34, hi = 82, W = 120, H = 300, rowH = H / (hi - lo + 1);
    levels.forEach(([label, notes]) => {
      const col = document.createElement("div");
      col.className = "spread-col";
      let svg = `<svg viewBox="0 0 ${W} ${H}" aria-hidden="true">`;
      for (let n = lo; n <= hi; n++) {
        if (n % 12 === 0) {
          const y = H - (n - lo + 1) * rowH + rowH;
          svg += `<line x1="0" x2="${W}" y1="${y}" y2="${y}" stroke="#b9c0c8" stroke-width="1"/>`;
          svg += `<text x="5" y="${y - 3}" font-size="10" fill="#4a505b" font-family="Bricolage Grotesque, sans-serif">${noteName(n)}</text>`;
        }
      }
      notes.forEach((n, i) => {
        const y = H - (n - lo + 1) * rowH;
        const bass = i === 0 && label !== "Close";
        svg += `<rect x="34" y="${y + 0.5}" width="${W - 44}" height="${rowH * 1.7}" rx="3" fill="${bass ? "#e8b04b" : "#16181d"}"/>`;
      });
      svg += `</svg>`;
      col.innerHTML = svg + `<div><span class="lvl">${label}</span><span class="notes">${notes.map(noteName).join(" ")}</span></div>`;
      grid.appendChild(col);
    });
  }
})();
