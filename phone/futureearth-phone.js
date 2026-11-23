// FutureEarth phone window. Loaded after Epic's Pixel Streaming player (player.js),
// which exposes the stream as window.pixelStreaming. Sends the phone's orientation
// to Unreal so turning the phone looks around the scene.
(function () {
  'use strict';
  const D2R = Math.PI / 180;
  const SEND_HZ = 30;
  let latest = null;      // {f:[e,n,u], u:[e,n,u]}
  let dragYaw = 0, dragPitch = 0, usingDrag = false;

  // W3C device orientation -> where the back camera points (forward) and the top edge (up),
  // in East-North-Up axes. alpha is degrees counter-clockwise from north.
  function fromAngles(alpha, beta, gamma) {
    const a = alpha * D2R, b = beta * D2R, g = gamma * D2R;
    const cA = Math.cos(a), sA = Math.sin(a), cB = Math.cos(b), sB = Math.sin(b), cG = Math.cos(g), sG = Math.sin(g);
    return {
      f: [-(cG * sA * sB + cA * sG), -(sA * sG - cA * cG * sB), -(cB * cG)],
      u: [-cB * sA, cA * cB, sB],
    };
  }

  function onOrientation(e) {
    if (e.beta == null || e.gamma == null) return;
    // iOS reports a true compass heading (clockwise from north); elsewhere alpha is used as is.
    const alpha = typeof e.webkitCompassHeading === 'number' ? 360 - e.webkitCompassHeading : (e.alpha || 0);
    latest = fromAngles(alpha, e.beta, e.gamma);
  }

  function fromDrag() {
    const y = dragYaw * D2R, p = dragPitch * D2R;
    return {
      f: [Math.sin(y) * Math.cos(p), Math.cos(y) * Math.cos(p), Math.sin(p)],
      u: [-Math.sin(y) * Math.sin(p), -Math.cos(y) * Math.sin(p), Math.cos(p)],
    };
  }

  function send() {
    const ps = window.pixelStreaming;
    const view = usingDrag ? fromDrag() : latest;
    if (!ps || !view) return;
    const r = (v) => v.map((x) => Math.round(x * 10000) / 10000);
    try { ps.emitUIInteraction({ f: r(view.f), u: r(view.u) }); } catch (_) { /* stream not ready yet */ }
  }

  // --- small overlay ----------------------------------------------------------
  const bar = document.createElement('div');
  bar.style.cssText = 'position:fixed;top:12px;right:12px;z-index:9999;display:flex;gap:8px;font:600 14px system-ui,sans-serif';
  const button = (label) => {
    const b = document.createElement('button');
    b.textContent = label;
    b.style.cssText = 'padding:10px 14px;border-radius:999px;border:1px solid rgba(255,255,255,.35);background:rgba(8,12,15,.72);color:#fff;font:inherit';
    bar.appendChild(b);
    return b;
  };
  const motion = button('Enable motion');
  const note = document.createElement('div');
  note.style.cssText = 'position:fixed;top:60px;right:12px;z-index:9999;max-width:70vw;padding:8px 12px;border-radius:12px;background:rgba(8,12,15,.72);color:#fff;font:13px system-ui,sans-serif;display:none';
  const say = (text) => { note.textContent = text; note.style.display = text ? 'block' : 'none'; };

  function startDrag(reason) {
    usingDrag = true;
    motion.textContent = 'Drag sky to look';
    say(reason + ' Drag the upper half of the picture to look around.');
    setTimeout(() => say(''), 6000);
  }

  motion.addEventListener('click', async () => {
    try {
      if (typeof DeviceOrientationEvent === 'undefined') return startDrag('This browser has no motion sensors.');
      if (typeof DeviceOrientationEvent.requestPermission === 'function') {
        const answer = await DeviceOrientationEvent.requestPermission();
        if (answer !== 'granted') return startDrag('Motion access was not granted.');
      }
      window.addEventListener('deviceorientation', onOrientation, true);
      usingDrag = false;
      motion.textContent = 'Motion on';
      say('');
      // No sensor data within two seconds (e.g. a desktop browser): fall back to dragging.
      setTimeout(() => { if (!latest) startDrag('No motion data from this device.'); }, 2000);
    } catch (err) {
      startDrag(window.isSecureContext ? 'Motion is unavailable.' : 'Motion needs an https:// address.');
    }
  });

  // Drag-to-look fallback: only drags that start in the upper 55% of the screen, so the
  // interface at the bottom still receives touches. Captured before the player sees them.
  let last = null;
  const point = (e) => (e.touches ? e.touches[0] : e);
  function down(e) {
    if (!usingDrag) return;
    const p = point(e);
    if (!p || p.clientY > window.innerHeight * 0.55 || e.target.closest('button')) return;
    last = { x: p.clientX, y: p.clientY };
    e.stopPropagation(); e.preventDefault();
  }
  function move(e) {
    if (!last) return;
    const p = point(e);
    dragYaw += (p.clientX - last.x) * -0.25;
    dragPitch = Math.max(-80, Math.min(80, dragPitch + (p.clientY - last.y) * 0.25));
    last = { x: p.clientX, y: p.clientY };
    e.stopPropagation(); e.preventDefault();
  }
  function up(e) { if (last) { last = null; e.stopPropagation(); } }
  for (const [type, fn] of [['touchstart', down], ['touchmove', move], ['touchend', up], ['mousedown', down], ['mousemove', move], ['mouseup', up]]) {
    window.addEventListener(type, fn, { capture: true, passive: false });
  }

  window.addEventListener('DOMContentLoaded', () => { document.body.appendChild(bar); document.body.appendChild(note); });
  setInterval(send, 1000 / SEND_HZ);
})();
