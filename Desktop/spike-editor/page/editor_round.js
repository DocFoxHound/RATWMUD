// Evaluated by the shell in the Editor window (the built studio app, loaded through the `editor` scheme), after
// `const ROUND = ...; const FULL = ...;`. Every result goes to /spike/report, which the scheme handler keeps in Rust.
(async () => {
  const report = (o) => fetch('/spike/report', { method: 'POST', headers: { 'Content-Type': 'application/json' },
                                                 body: JSON.stringify(Object.assign({ round: ROUND }, o)) });
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const table = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
  const crc32 = (bytes) => { let c = 0xFFFFFFFF; for (let i = 0; i < bytes.length; i++) c = table[(c ^ bytes[i]) & 255] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
  const step = async (name, fn) => {
    const t = performance.now();
    try { await report(Object.assign({ test: name }, await fn(), { ms: Math.round(performance.now() - t) })); }
    catch (e) { await report({ test: name, error: String(e), ms: Math.round(performance.now() - t) }); }
  };
  try {
    const root = document.getElementById('root');
    await report({ test: 'page', origin: location.origin, href: location.href, secure: window.isSecureContext,
                   title: document.title, rootChildren: root ? root.children.length : -1,
                   text: document.body.innerText.slice(0, 300), scripts: document.scripts.length,
                   stylesheets: document.styleSheets.length, violations: (window.__spikeViolations || []).slice(0, 20),
                   tauriGlobal: typeof window.__TAURI__, tauriInternals: typeof window.__TAURI_INTERNALS__,
                   cryptoSubtle: typeof (window.crypto && window.crypto.subtle) });
    let before = null;
    try { before = localStorage.getItem('spike-storage'); } catch (e) { before = 'error: ' + e; }
    await report({ test: 'storage-before', value: before });
    try { localStorage.setItem('spike-storage', 'set in round ' + ROUND); } catch (e) { await report({ test: 'storage-set', error: String(e) }); }

    if (FULL) {
      for (const mb of [1, 16]) {
        await step(`post-${mb}mb`, async () => {
          const n = mb * 1024 * 1024, body = new Uint8Array(n);
          for (let i = 0; i < n; i++) body[i] = (i * 2654435761) >>> 24;
          const r = await fetch('/spike/echo', { method: 'POST', headers: { 'Content-Type': 'application/octet-stream' }, body });
          return { status: r.status, sent: n, crc: crc32(body), server: await r.json().catch(() => null) };
        });
      }
      await step('post-16mb-json-string', async () => {
        const text = JSON.stringify({ cells: 'x'.repeat(16 * 1024 * 1024 - 20) });
        const r = await fetch('/spike/echo', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: text });
        return { status: r.status, sent: text.length, server: await r.json().catch(() => null) };
      });
      await step('post-17mb-over-cap', async () => {
        const r = await fetch('/spike/echo', { method: 'POST', body: new Uint8Array(17 * 1024 * 1024) });
        return { status: r.status, body: (await r.text()).slice(0, 200) };
      });
      for (const [mb, gz] of [[32, 0], [32, 1], [33, 0], [33, 1]]) {
        await step(`get-${mb}mb-${gz ? 'gzip' : 'raw'}`, async () => {
          const r = await fetch(`/spike/big?mb=${mb}&gz=${gz}`);
          const bytes = new Uint8Array(await r.arrayBuffer());
          return { status: r.status, bytes: bytes.length, crc: crc32(bytes), serverCrc: r.headers.get('x-spike-crc'),
                   wire: r.headers.get('x-spike-wire'), type: r.headers.get('content-type') };
        });
      }
      await step('tauri-own-command', async () => {
        if (!window.__TAURI_INTERNALS__) return { available: false };
        try { await window.__TAURI_INTERNALS__.invoke('spike_report', { line: { test: 'leak' } }); return { refused: false }; }
        catch (e) { return { refused: true, error: String(e) }; }
      });
      await step('tauri-builtin-command', async () => {
        if (!window.__TAURI_INTERNALS__) return { available: false };
        try { await window.__TAURI_INTERNALS__.invoke('plugin:window|set_title', { label: 'editor', value: 'pwned' }); return { refused: false }; }
        catch (e) { return { refused: true, error: String(e) }; }
      });
      await step('download-link', async () => {
        const a = document.createElement('a'); a.href = '/spike/download.json'; a.download = 'spike.json';
        document.body.appendChild(a); a.click(); a.remove(); await sleep(2500); return {};
      });
      await step('download-blob', async () => {
        const url = URL.createObjectURL(new Blob(['{"spike":1}'], { type: 'application/json' }));
        const a = document.createElement('a'); a.href = url; a.download = 'blob.json';
        document.body.appendChild(a); a.click(); a.remove(); await sleep(2500); return {};
      });
      await step('download-exe', async () => {
        const a = document.createElement('a'); a.href = '/spike/download.exe'; a.download = 'tool.exe';
        document.body.appendChild(a); a.click(); a.remove(); await sleep(2500); return {};
      });
    }
  } catch (e) {
    await report({ test: 'round-error', error: String(e) });
  }
  await report({ test: 'round-done' });
})();
