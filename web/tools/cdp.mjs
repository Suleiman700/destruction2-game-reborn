// Minimal Chrome DevTools Protocol driver for testing the web port (no dependencies).
// Usage: node cdp.mjs <port> <command...>
//   goto <url> | key <Code> [holdMs] | wait <ms> | shot <file.png> | eval <js>
// Commands run in sequence, e.g.: node cdp.mjs 9555 key Enter wait 500 shot /tmp/a.png
const port = process.argv[2];
const cmds = process.argv.slice(3);
const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
const page = targets.find(t => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise(r => ws.addEventListener('open', r, { once: true }));
let id = 0;
const pending = new Map();
ws.addEventListener('message', e => {
  const m = JSON.parse(e.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'Runtime.consoleAPICalled') console.log('console:', m.params.args.map(a => a.value ?? a.description).join(' '));
  if (m.method === 'Runtime.exceptionThrown') console.log('exception:', m.params.exceptionDetails.text, m.params.exceptionDetails.exception?.description);
});
const send = (method, params = {}) => new Promise(r => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const sleep = ms => new Promise(r => setTimeout(r, ms));
if (cmds[0] !== 'stack') {
  await send('Runtime.enable');
  await send('Page.enable');
  await send('Emulation.setDeviceMetricsOverride', { width: 800, height: 600, deviceScaleFactor: 1, mobile: false });
  await send('Emulation.setFocusEmulationEnabled', { enabled: true });
}
const keyInfo = code => {
  const special = { Enter: [13, 'Enter'], Escape: [27, 'Escape'], Space: [32, ' '], ArrowUp: [38, 'ArrowUp'], ArrowDown: [40, 'ArrowDown'],
    ArrowLeft: [37, 'ArrowLeft'], ArrowRight: [39, 'ArrowRight'], ControlLeft: [17, 'Control'], ShiftLeft: [16, 'Shift'], ShiftRight: [16, 'Shift'],
    ControlRight: [17, 'Control'], Tab: [9, 'Tab'], F12: [123, 'F12'], Backspace: [8, 'Backspace'] };
  if (special[code]) return special[code];
  if (/^Key[A-Z]$/.test(code)) return [code.charCodeAt(3), code[3].toLowerCase()];
  if (/^Digit\d$/.test(code)) return [code.charCodeAt(5), code[5]];
  return [0, code];
};
for (let i = 0; i < cmds.length; i++) {
  const c = cmds[i];
  if (c === 'goto') { await send('Page.navigate', { url: cmds[++i] }); await sleep(500); }
  else if (c === 'wait') await sleep(+cmds[++i]);
  else if (c === 'key' || c === 'down' || c === 'up') {
    const code = cmds[++i];
    const hold = c === 'key' && /^\d+$/.test(cmds[i + 1] || '') ? +cmds[++i] : 0;
    const [kc, key] = keyInfo(code);
    const ev = type => send('Input.dispatchKeyEvent', { type, code, key, windowsVirtualKeyCode: kc, nativeVirtualKeyCode: kc });
    if (c !== 'up') await ev('rawKeyDown');
    if (hold) await sleep(hold);
    if (c !== 'down') await ev('keyUp');
  }
  else if (c === 'click') {
    const sel = cmds[++i];
    await send('Runtime.evaluate', { expression: `document.querySelector(${JSON.stringify(sel)}).click()`, userGesture: true });
  }
  else if (c === 'shot') {
    const r = await send('Page.captureScreenshot', { format: 'png', clip: { x: 0, y: 0, width: 800, height: 600, scale: 1 } });
    (await import('node:fs')).writeFileSync(cmds[++i], Buffer.from(r.result.data, 'base64'));
  }
  else if (c === 'stack') {
    // Pause the page's main thread and print the current JS/wasm call stack.
    const paused = new Promise(r => ws.addEventListener('message', e => { const m = JSON.parse(e.data); if (m.method === 'Debugger.paused') r(m.params); }));
    await send('Debugger.enable');
    await send('Debugger.pause');
    const p = await paused;
    for (const f of p.callFrames.slice(0, 25)) console.log('  at', f.functionName || '(anon)', f.url.split('/').pop(), f.location.lineNumber);
    await send('Debugger.resume');
  }
  else if (c === 'eval') {
    const r = await send('Runtime.evaluate', { expression: cmds[++i], returnByValue: true, awaitPromise: true });
    console.log('eval:', JSON.stringify(r.result.result?.value ?? r.result));
  }
}
ws.close();
