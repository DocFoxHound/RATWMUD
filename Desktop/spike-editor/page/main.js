// The main window's one check: the `editor` scheme refuses any webview not labelled "editor" (doc 78, section 6.6).
const invoke = window.__TAURI__.core.invoke;
const editorOrigin = navigator.userAgent.includes('Windows') ? 'http://editor.localhost' : 'editor://localhost';
const report = (line) => invoke('spike_report', { line }).catch(() => {});
(async () => {
  await report({ test: 'main-origin', origin: location.origin, secure: window.isSecureContext, ua: navigator.userAgent });
  try {
    const r = await fetch(editorOrigin + '/spike/hello');
    await report({ test: 'main-to-editor-scheme', reached: true, status: r.status, body: (await r.text()).slice(0, 200) });
  } catch (e) {
    await report({ test: 'main-to-editor-scheme', reached: false, error: String(e) });
  }
})();
