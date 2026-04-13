const state = {
  settings: null,
  preview: null,
  wifiStatus: null,
  saveTimer: null,
  previewTimer: null,
  wifiStatusTimer: null,
  eventSource: null,
  eventStreamOpen: false,
  saveInFlight: false,
  queuedSave: false,
  lastSerialized: '',
  debugPayload: null,
};

const elements = {
  form: document.getElementById('settingsForm'),
  message: document.getElementById('formMessage'),
  wifiMessage: document.getElementById('wifiMessage'),
  wifiSsid: document.getElementById('wifiSsid'),
  wifiNetworkList: document.getElementById('wifiNetworkList'),
  wifiPassword: document.getElementById('wifiPassword'),
  wifiAutoConnect: document.getElementById('wifiAutoConnect'),
  wifiScanButton: document.getElementById('wifiScanButton'),
  wifiConnectButton: document.getElementById('wifiConnectButton'),
  wifiStatusText: document.getElementById('wifiStatusText'),
  wifiStatusSsid: document.getElementById('wifiStatusSsid'),
  wifiStatusIp: document.getElementById('wifiStatusIp'),
  wifiStatusRssi: document.getElementById('wifiStatusRssi'),
  locationQuery: document.getElementById('locationQuery'),
  apiKey: document.getElementById('apiKey'),
  units: document.getElementById('units'),
  theme: document.getElementById('theme'),
  radarMode: document.getElementById('radarMode'),
  radarAutoContrast: document.getElementById('radarAutoContrast'),
  radarInterpolation: document.getElementById('radarInterpolation'),
  radarStormOverlays: document.getElementById('radarStormOverlays'),
  radarInterpolationSteps: document.getElementById('radarInterpolationSteps'),
  radarSmoothingPasses: document.getElementById('radarSmoothingPasses'),
  debugMode: document.getElementById('debugMode'),
  updateIntervalMinutes: document.getElementById('updateIntervalMinutes'),
  resolvedLocation: document.getElementById('resolvedLocation'),
  locationKey: document.getElementById('locationKey'),
  syncPill: document.getElementById('syncPill'),
  deviceThemePill: document.getElementById('deviceThemePill'),
  wifiPill: document.getElementById('wifiPill'),
  timePill: document.getElementById('timePill'),
  previewTitle: document.getElementById('previewTitle'),
  previewSubtitle: document.getElementById('previewSubtitle'),
  previewTemp: document.getElementById('previewTemp'),
  previewLocation: document.getElementById('previewLocation'),
  previewSummary: document.getElementById('previewSummary'),
  previewUnits: document.getElementById('previewUnits'),
  previewRadar: document.getElementById('previewRadar'),
  previewInterval: document.getElementById('previewInterval'),
  previewRadarFx: document.getElementById('previewRadarFx'),
  previewTheme: document.getElementById('previewTheme'),
  previewDownload: document.getElementById('previewDownload'),
  previewWarning: document.getElementById('previewWarning'),
  previewConnectivity: document.getElementById('previewConnectivity'),
  previewBle: document.getElementById('previewBle'),
  alertsList: document.getElementById('alertsList'),
  debugStatus: document.getElementById('debugStatus'),
  debugConsoleOutput: document.getElementById('debugConsoleOutput'),
  exportDebugLogs: document.getElementById('exportDebugLogs'),
  clearDebugLogs: document.getElementById('clearDebugLogs'),
};

function setMessage(text, kind = '') {
  elements.message.textContent = text || '';
  elements.message.className = `message ${kind}`.trim();
}

function setWifiMessage(text, kind = '') {
  if (!elements.wifiMessage) {
    setMessage(text, kind);
    return;
  }
  elements.wifiMessage.textContent = text || '';
  elements.wifiMessage.className = `message ${kind}`.trim();
}

function resolveTheme(themeSetting, deviceTheme) {
  return themeSetting === 'device' ? deviceTheme : themeSetting;
}

function applyPageTheme(themeSetting, deviceTheme) {
  const resolved = resolveTheme(themeSetting, deviceTheme);
  document.body.dataset.theme = resolved === 'light' ? 'light' : 'dark';
}

function themeLabel(theme) {
  switch (theme) {
    case 'light':
      return 'Light';
    case 'dark':
      return 'Dark';
    case 'future1':
      return 'Future Theme';
    default:
      return 'Match Device';
  }
}

function radarLabel(mode) {
  switch (mode) {
    case 'composite':
      return 'Composite';
    case 'echotop':
      return 'Echo Top';
    default:
      return 'Base';
  }
}

function formatTemperature(temperatureC, units) {
  if (!Number.isFinite(temperatureC)) {
    return '--';
  }

  if (units === 'imperial') {
    return `${Math.round((temperatureC * 9) / 5 + 32)} F`;
  }
  return `${Math.round(temperatureC)} C`;
}

function serializeForm() {
  const draft = readDraftSettings();
  const params = new URLSearchParams();
  params.set('locationQuery', draft.locationQuery || '');
  params.set('apiKey', draft.apiKey || '');
  params.set('units', draft.units || 'metric');
  params.set('theme', draft.theme || 'device');
  params.set('radarMode', draft.radarMode || 'base');
  params.set('radarAutoContrast', draft.radarAutoContrast ? 'true' : 'false');
  params.set('radarInterpolation', draft.radarInterpolation ? 'true' : 'false');
  params.set('radarStormOverlays', draft.radarStormOverlays ? 'true' : 'false');
  params.set('radarInterpolationSteps', String(draft.radarInterpolationSteps || 0));
  params.set('radarSmoothingPasses', String(draft.radarSmoothingPasses || 0));
  params.set('debugMode', draft.debugMode ? 'true' : 'false');
  params.set('updateIntervalMinutes', String(draft.updateIntervalMinutes || 5));
  return params.toString();
}

function readDraftSettings() {
  return {
    locationQuery: elements.locationQuery.value.trim(),
    apiKey: elements.apiKey.value,
    units: elements.units.value,
    theme: elements.theme.value,
    radarMode: elements.radarMode.value,
    radarAutoContrast: elements.radarAutoContrast.checked,
    radarInterpolation: elements.radarInterpolation.checked,
    radarStormOverlays: elements.radarStormOverlays.checked,
    radarInterpolationSteps: Number.parseInt(elements.radarInterpolationSteps.value || '0', 10) || 0,
    radarSmoothingPasses: Number.parseInt(elements.radarSmoothingPasses.value || '0', 10) || 0,
    debugMode: elements.debugMode.checked,
    updateIntervalMinutes: Number.parseInt(elements.updateIntervalMinutes.value || '0', 10) || 0,
  };
}

function ensureWifiOption(value, label = value) {
  if (!value) {
    return;
  }
  const existing = Array.from(elements.wifiSsid.options).find((option) => option.value === value);
  if (existing) {
    existing.textContent = label;
    return;
  }
  const option = document.createElement('option');
  option.value = value;
  option.textContent = label;
  elements.wifiSsid.appendChild(option);
}

function renderWifiNetworks(networks = []) {
  const selected = elements.wifiSsid.value;
  elements.wifiSsid.innerHTML = '<option value="">Scan for networks</option>';
  if (elements.wifiNetworkList) {
    elements.wifiNetworkList.innerHTML = '';
  }

  if (!networks.length) {
    if (elements.wifiNetworkList) {
      const empty = document.createElement('p');
      empty.className = 'wifi-network-empty';
      empty.textContent = 'No nearby WiFi networks found. Move closer to your router and scan again.';
      elements.wifiNetworkList.appendChild(empty);
    }
  }

  networks.forEach((network) => {
    if (!network?.ssid) {
      return;
    }
    const strength = Number.isFinite(network.rssi) ? ` (${network.rssi} dBm)` : '';
    const secure = network.secure ? ' lock' : '';
    ensureWifiOption(network.ssid, `${network.ssid}${strength}${secure}`);

    if (elements.wifiNetworkList) {
      const item = document.createElement('button');
      item.type = 'button';
      item.className = 'wifi-network-item';

      const name = document.createElement('span');
      name.className = 'wifi-network-name';
      name.textContent = network.ssid;

      const meta = document.createElement('span');
      meta.className = 'wifi-network-meta';
      meta.textContent = `${Number.isFinite(network.rssi) ? `${network.rssi} dBm` : 'n/a'}${network.secure ? ' lock' : ''}`;

      item.appendChild(name);
      item.appendChild(meta);
      item.addEventListener('click', () => {
        elements.wifiSsid.value = network.ssid;
        Array.from(elements.wifiNetworkList.querySelectorAll('.wifi-network-item')).forEach((node) => {
          node.classList.toggle('selected', node === item);
        });
      });
      elements.wifiNetworkList.appendChild(item);
    }
  });

  const preferred = selected || state.settings?.wifiSsid || state.wifiStatus?.ssid;
  if (preferred) {
    ensureWifiOption(preferred, preferred);
    elements.wifiSsid.value = preferred;

    if (elements.wifiNetworkList) {
      Array.from(elements.wifiNetworkList.querySelectorAll('.wifi-network-item')).forEach((node) => {
        const name = node.querySelector('.wifi-network-name')?.textContent || '';
        node.classList.toggle('selected', name === preferred);
      });
    }
  }
}

function renderWifiStatus(status = {}) {
  state.wifiStatus = status;
  elements.wifiStatusText.textContent = status.status || 'Unknown';
  elements.wifiStatusSsid.textContent = status.ssid || status.savedSsid || 'None';
  elements.wifiStatusIp.textContent = status.ipAddress || 'Unavailable';
  elements.wifiStatusRssi.textContent = Number.isFinite(status.rssi) && status.rssi > -127 ? `${status.rssi} dBm` : 'n/a';
  elements.wifiPill.textContent = status.connected
    ? `WiFi ${status.ssid || 'Connected'}`
    : (status.connecting ? 'WiFi Connecting' : 'WiFi Offline');

  if (status.savedSsid) {
    ensureWifiOption(status.savedSsid, status.savedSsid);
  }
  if (status.ssid) {
    ensureWifiOption(status.ssid, status.ssid);
  }
  if (document.activeElement !== elements.wifiSsid) {
    elements.wifiSsid.value = status.savedSsid || status.ssid || elements.wifiSsid.value;
  }
  if (document.activeElement !== elements.wifiAutoConnect) {
    elements.wifiAutoConnect.checked = status.autoConnect !== false;
  }
}

function populateForm(settings) {
  ensureWifiOption(settings.wifiSsid || '', settings.wifiSsid || 'Saved network');
  if (document.activeElement !== elements.wifiSsid) {
    elements.wifiSsid.value = settings.wifiSsid || '';
  }
  if (document.activeElement !== elements.wifiAutoConnect) {
    elements.wifiAutoConnect.checked = settings.wifiAutoConnect !== false;
  }

  elements.locationQuery.value = settings.locationQuery || '';
  elements.apiKey.value = settings.apiKey || '';
  elements.units.value = settings.units || 'metric';
  elements.theme.value = settings.theme || 'device';
  elements.radarMode.value = settings.radarMode || 'base';
  elements.radarAutoContrast.checked = settings.radarAutoContrast !== false;
  elements.radarInterpolation.checked = settings.radarInterpolation !== false;
  elements.radarStormOverlays.checked = settings.radarStormOverlays !== false;
  elements.radarInterpolationSteps.value = settings.radarInterpolationSteps ?? 1;
  elements.radarSmoothingPasses.value = settings.radarSmoothingPasses ?? 1;
  elements.debugMode.checked = settings.debugMode === true;
  elements.updateIntervalMinutes.value = settings.updateIntervalMinutes || 5;
  elements.resolvedLocation.textContent = settings.locationName || settings.locationQuery || 'Not configured';
  elements.locationKey.textContent = settings.locationKey || 'None';
  elements.deviceThemePill.textContent = `Device Theme: ${settings.deviceTheme || 'dark'}`;
  applyPageTheme(settings.theme, settings.deviceTheme || 'dark');
  state.lastSerialized = serializeForm();
}

function renderDebugConsole(debugPayload = null) {
  if (debugPayload) {
    state.debugPayload = debugPayload;
  }

  const debugEnabled = (state.settings?.debugMode === true) || elements.debugMode.checked;
  elements.debugStatus.textContent = debugEnabled ? 'Enabled' : 'Disabled';
  elements.exportDebugLogs.disabled = !debugEnabled;
  elements.clearDebugLogs.disabled = !debugEnabled;

  if (!debugEnabled) {
    elements.debugConsoleOutput.textContent = 'Debug mode is disabled.';
    return;
  }

  const entries = (debugPayload || state.debugPayload)?.entries || [];
  if (!entries.length) {
    elements.debugConsoleOutput.textContent = 'No debug events yet.';
    return;
  }

  const lines = entries.slice(-30).map((entry) => {
    const seconds = (Number(entry.ms || 0) / 1000).toFixed(2);
    return `[${seconds}s][${entry.category || 'misc'}] ${entry.message || ''}`;
  });
  elements.debugConsoleOutput.textContent = lines.join('\n');
  elements.debugConsoleOutput.scrollTop = elements.debugConsoleOutput.scrollHeight;
}

function exportDebugLogs() {
  const debugEnabled = (state.settings?.debugMode === true) || elements.debugMode.checked;
  if (!debugEnabled) {
    setMessage('Enable debug mode before exporting logs.', 'error');
    return;
  }

  const entries = state.debugPayload?.entries || [];
  if (!entries.length) {
    setMessage('No debug logs available to export.', 'error');
    return;
  }

  const lines = entries.map((entry) => {
    const ms = Number(entry.ms || 0);
    const seconds = (ms / 1000).toFixed(3);
    return `[${seconds}s][${entry.category || 'misc'}] ${entry.message || ''}`;
  });

  const now = new Date();
  const stamp = `${now.getFullYear()}${String(now.getMonth() + 1).padStart(2, '0')}${String(now.getDate()).padStart(2, '0')}_${String(now.getHours()).padStart(2, '0')}${String(now.getMinutes()).padStart(2, '0')}${String(now.getSeconds()).padStart(2, '0')}`;
  const blob = new Blob([lines.join('\n') + '\n'], { type: 'text/plain;charset=utf-8' });
  const url = URL.createObjectURL(blob);

  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = `core_weather_debug_logs_${stamp}.txt`;
  document.body.appendChild(anchor);
  anchor.click();
  document.body.removeChild(anchor);
  URL.revokeObjectURL(url);
  setMessage('Debug logs exported.', 'success');
}

async function clearDebugLogs() {
  const confirmed = window.confirm('Clear all debug logs from device memory?');
  if (!confirmed) {
    return;
  }
  elements.clearDebugLogs.disabled = true;
  try {
    const response = await fetch('/api/debug/clear', { method: 'POST' });
    const payload = await response.json();
    if (!response.ok) {
      setMessage(payload.error || 'Failed to clear debug logs.', 'error');
      return;
    }
    renderDebugConsole(payload);
    setMessage(payload.message || 'Debug logs cleared.', 'success');
  } catch (error) {
    setMessage(error?.message || 'Failed to clear debug logs.', 'error');
  } finally {
    renderDebugConsole();
  }
}

async function loadDebugLogs() {
  const response = await fetch('/api/debug/logs');
  const payload = await response.json();
  renderDebugConsole(payload);
}

async function loadWifiStatus() {
  const response = await fetch('/wifi/status');
  const payload = await response.json();
  renderWifiStatus(payload);
}

async function scanWifiNetworks() {
  elements.wifiScanButton.disabled = true;
  setWifiMessage('Scanning for WiFi networks...', 'syncing');
  try {
    let payload = null;
    for (let attempt = 0; attempt < 14; attempt += 1) {
      try {
        const response = await fetch(attempt === 0 ? '/wifi/scan?start=1' : '/wifi/scan');
        payload = await response.json();
        if (!response.ok) {
          setWifiMessage(payload.error || 'WiFi scan failed.', 'error');
          return;
        }
      } catch (error) {
        // SoftAP may briefly hop channels during scan; retry before failing.
        if (attempt >= 12) {
          throw error;
        }
        await new Promise((resolve) => setTimeout(resolve, 300));
        continue;
      }

      if (!payload?.inProgress) {
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, 350));
    }

    const networks = Array.isArray(payload?.networks) ? payload.networks : [];
    renderWifiNetworks(networks);
    if (networks.length === 0) {
      setWifiMessage('Scan completed. No WiFi networks were found.', 'error');
      return;
    }
    setWifiMessage(`Found ${networks.length} WiFi network${networks.length === 1 ? '' : 's'}.`, 'success');
  } catch (error) {
    setWifiMessage(error?.message || 'WiFi scan failed.', 'error');
  } finally {
    elements.wifiScanButton.disabled = false;
  }
}

async function connectWifi() {
  const ssid = elements.wifiSsid.value.trim();
  if (!ssid) {
    setWifiMessage('Select a WiFi network first.', 'error');
    return;
  }

  elements.wifiConnectButton.disabled = true;
  setWifiMessage(`Connecting to ${ssid}...`, 'syncing');

  const params = new URLSearchParams();
  params.set('ssid', ssid);
  params.set('password', elements.wifiPassword.value || '');
  params.set('autoConnect', elements.wifiAutoConnect.checked ? 'true' : 'false');

  try {
    const response = await fetch('/wifi/connect', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8',
      },
      body: params.toString(),
    });
    const payload = await response.json();
    if (!response.ok && response.status !== 202) {
      setWifiMessage(payload.error || 'WiFi connection failed.', 'error');
      return;
    }

    renderWifiStatus(payload);
    if (state.settings) {
      state.settings.wifiSsid = ssid;
      state.settings.wifiAutoConnect = elements.wifiAutoConnect.checked;
    }
    elements.wifiPassword.value = '';
    setWifiMessage(payload.warning || payload.message || 'WiFi connection started.', payload.warning ? 'syncing' : 'success');
    await loadPreview();
  } catch (error) {
    setWifiMessage(error?.message || 'WiFi connection failed.', 'error');
  } finally {
    elements.wifiConnectButton.disabled = false;
    loadWifiStatus().catch(() => {});
  }
}

function renderAlerts(alerts = []) {
  elements.alertsList.innerHTML = '';
  if (!alerts.length) {
    const empty = document.createElement('div');
    empty.className = 'alert-item';
    empty.innerHTML = '<strong>No active alerts</strong><span>The weather engine has not reported any warnings.</span>';
    elements.alertsList.appendChild(empty);
    return;
  }

  alerts.forEach((alert) => {
    const item = document.createElement('div');
    item.className = 'alert-item';
    item.innerHTML = `<strong>${alert.title || 'Weather alert'}</strong><span>${alert.severity || 'Unknown severity'}</span>`;
    elements.alertsList.appendChild(item);
  });
}

function renderPreview(preview, draft = readDraftSettings()) {
  state.preview = preview;

  const savedSettings = state.settings || {};
  const deviceTheme = savedSettings.deviceTheme || 'dark';
  const effectiveTheme = resolveTheme(draft.theme || savedSettings.theme || 'device', deviceTheme);
  const displayLocation = draft.locationQuery || preview.locationName || savedSettings.locationName || savedSettings.locationQuery || 'Unknown location';
  const isDraftLocation = draft.locationQuery && draft.locationQuery !== (savedSettings.locationQuery || '');
  const units = draft.units || preview.units || savedSettings.units || 'metric';
  const radarMode = draft.radarMode || preview.radarMode || savedSettings.radarMode || 'base';
  const radarAutoContrast = draft.radarAutoContrast ?? preview.radarAutoContrast ?? savedSettings.radarAutoContrast ?? true;
  const radarInterpolation = draft.radarInterpolation ?? preview.radarInterpolation ?? savedSettings.radarInterpolation ?? true;
  const radarStormOverlays = draft.radarStormOverlays ?? preview.radarStormOverlays ?? savedSettings.radarStormOverlays ?? true;
  const radarInterpolationSteps = draft.radarInterpolationSteps || preview.radarInterpolationSteps || savedSettings.radarInterpolationSteps || 0;
  const radarSmoothingPasses = draft.radarSmoothingPasses || preview.radarSmoothingPasses || savedSettings.radarSmoothingPasses || 0;
  const updateInterval = draft.updateIntervalMinutes || preview.updateIntervalMinutes || savedSettings.updateIntervalMinutes || 5;
  const summary = preview.current?.summary || 'Weather data has not arrived yet.';
  const warningLabel = preview.warningState ? 'Attention Needed' : 'Clear';
  const connectivityLabel = preview.wifiConnected
    ? `${preview.wifiSsid || 'Online'}${preview.timeSynced ? ' | synced' : ' | time pending'}`
    : (preview.wifiConnecting ? 'Connecting...' : (preview.wifiStatus || 'Offline'));
  const bleLabel = preview.bleConnected
    ? 'Connected'
    : (preview.bleAdvertising ? `Advertising${preview.bleLastEvent ? ` | ${preview.bleLastEvent}` : ''}` : 'Idle');

  applyPageTheme(draft.theme || savedSettings.theme || 'device', deviceTheme);
  elements.syncPill.textContent = state.saveInFlight
    ? 'Syncing...'
    : (state.eventStreamOpen ? 'Live Sync SSE' : 'Live Sync Polling');
  elements.wifiPill.textContent = preview.wifiConnected
    ? `WiFi ${preview.wifiSsid || 'Connected'}`
    : (preview.wifiConnecting ? 'WiFi Connecting' : 'WiFi Offline');
  elements.timePill.textContent = preview.timeSynced ? 'Time Synced' : 'Time Unsynced';
  elements.previewTitle.textContent = preview.current?.valid ? 'Current Weather' : 'Waiting for weather';
  elements.previewSubtitle.textContent = isDraftLocation
    ? 'Showing your draft location until the firmware confirms the resolved provider key.'
    : 'Draft changes update the web preview immediately.';
  elements.previewTemp.textContent = formatTemperature(preview.current?.temperatureC, units);
  elements.previewLocation.textContent = isDraftLocation ? `${displayLocation} | draft` : displayLocation;
  elements.previewSummary.textContent = summary;
  elements.previewUnits.textContent = `Units: ${units}`;
  elements.previewRadar.textContent = `Radar: ${radarLabel(radarMode)}`;
  elements.previewInterval.textContent = `Interval: ${updateInterval} min`;
  elements.previewRadarFx.textContent = `FX: ${radarAutoContrast ? 'contrast' : 'flat'} | ${radarInterpolation ? `${radarInterpolationSteps}x interp` : 'no interp'} | ${radarSmoothingPasses} blur | ${radarStormOverlays ? 'cells' : 'no cells'}`;
  elements.previewTheme.textContent = themeLabel(effectiveTheme);
  elements.previewDownload.textContent = preview.radarDownloading
    ? `Downloading ${preview.radarFrames || 0} frames`
    : `${preview.radarFrames || 0} frames ready`;
  elements.previewWarning.textContent = warningLabel;
  elements.previewConnectivity.textContent = connectivityLabel;
  elements.previewBle.textContent = bleLabel;
  renderAlerts(preview.alerts || []);
  renderDebugConsole();
}

async function loadSettings() {
  const response = await fetch('/api/settings');
  const settings = await response.json();
  state.settings = settings;
  populateForm(settings);
}

async function loadPreview() {
  const response = await fetch('/api/preview');
  const preview = await response.json();
  renderPreview(preview);
}

function startPreviewPolling() {
  if (state.previewTimer) {
    return;
  }
  state.previewTimer = setInterval(() => {
    loadPreview().catch(() => {});
  }, 4000);
}

function stopPreviewPolling() {
  if (!state.previewTimer) {
    return;
  }
  clearInterval(state.previewTimer);
  state.previewTimer = null;
}

function startWifiStatusPolling() {
  if (state.wifiStatusTimer) {
    return;
  }
  state.wifiStatusTimer = setInterval(() => {
    loadWifiStatus().catch(() => {});
  }, 3000);
}

function stopWifiStatusPolling() {
  if (!state.wifiStatusTimer) {
    return;
  }
  clearInterval(state.wifiStatusTimer);
  state.wifiStatusTimer = null;
}

function connectEventStream() {
  if (!('EventSource' in window) || state.eventSource) {
    if (!('EventSource' in window)) {
      startPreviewPolling();
    }
    return;
  }

  const eventSource = new EventSource('/api/events');
  state.eventSource = eventSource;

  eventSource.addEventListener('open', () => {
    state.eventStreamOpen = true;
    stopPreviewPolling();
    renderPreview(state.preview || {}, readDraftSettings());
  });

  eventSource.addEventListener('settings', (event) => {
    const settings = JSON.parse(event.data);
    state.settings = settings;
    populateForm(settings);
    renderPreview(state.preview || {}, readDraftSettings());
  });

  eventSource.addEventListener('preview', (event) => {
    const preview = JSON.parse(event.data);
    renderPreview(preview, readDraftSettings());
  });

  eventSource.addEventListener('debug', (event) => {
    const payload = JSON.parse(event.data);
    renderDebugConsole(payload);
  });

  eventSource.addEventListener('error', () => {
    state.eventStreamOpen = false;
    startPreviewPolling();
    renderPreview(state.preview || {}, readDraftSettings());
  });
}

async function persistSettings(reason = 'manual') {
  if (state.saveInFlight) {
    state.queuedSave = true;
    return;
  }

  const serialized = serializeForm();
  if (state.settings && serialized === state.lastSerialized && reason !== 'manual') {
    renderPreview(state.preview || {}, readDraftSettings());
    return;
  }

  state.saveInFlight = true;
  renderPreview(state.preview || {}, readDraftSettings());
  setMessage(reason === 'manual' ? 'Saving settings...' : 'Syncing changes...', reason === 'manual' ? '' : 'syncing');

  const response = await fetch('/api/settings/live', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8',
    },
    body: serialized,
  });

  const result = await response.json();
  state.saveInFlight = false;

  if (!response.ok) {
    setMessage(result.error || 'Failed to save settings.', 'error');
    renderPreview(state.preview || {}, readDraftSettings());
    return;
  }

  state.settings = result;
  populateForm(result);
  setMessage(result.message || 'Settings saved.', 'success');
  await loadPreview();

  if (state.queuedSave) {
    state.queuedSave = false;
    await persistSettings('auto');
  }
}

function queueSave(delay = 0, reason = 'auto') {
  if (state.saveTimer) {
    clearTimeout(state.saveTimer);
  }

  state.saveTimer = setTimeout(() => {
    persistSettings(reason).catch((error) => {
      state.saveInFlight = false;
      const msg = (error?.message || '').toLowerCase().includes('failed to fetch')
        ? 'Could not reach the device service. Stay on setup WiFi and try again.'
        : (error?.message || 'Failed to save settings.');
      setMessage(msg, 'error');
      renderPreview(state.preview || {}, readDraftSettings());
    });
  }, delay);
}

function refreshPreviewFromDraft() {
  renderPreview(state.preview || {}, readDraftSettings());
}

function bindEvents() {
  elements.form.addEventListener('submit', (event) => {
    event.preventDefault();
    queueSave(0, 'manual');
  });

  elements.clearDebugLogs.addEventListener('click', () => {
    clearDebugLogs().catch(() => {
      setMessage('Failed to clear debug logs.', 'error');
      renderDebugConsole();
    });
  });

  elements.exportDebugLogs.addEventListener('click', () => {
    exportDebugLogs();
  });

  elements.wifiScanButton.addEventListener('click', () => {
    scanWifiNetworks().catch(() => {
      setMessage('WiFi scan failed.', 'error');
    });
  });

  elements.wifiConnectButton.addEventListener('click', () => {
    connectWifi().catch(() => {
      setMessage('WiFi connection failed.', 'error');
    });
  });

  elements.wifiSsid.addEventListener('change', () => {
    if (!elements.wifiNetworkList) {
      return;
    }
    const selected = elements.wifiSsid.value;
    Array.from(elements.wifiNetworkList.querySelectorAll('.wifi-network-item')).forEach((node) => {
      const name = node.querySelector('.wifi-network-name')?.textContent || '';
      node.classList.toggle('selected', name === selected);
    });
  });

  [elements.units,
   elements.theme,
   elements.radarMode,
   elements.radarAutoContrast,
   elements.radarInterpolation,
   elements.radarStormOverlays,
   elements.radarInterpolationSteps,
   elements.radarSmoothingPasses,
   elements.debugMode,
   elements.updateIntervalMinutes].forEach((element) => {
    element.addEventListener('input', refreshPreviewFromDraft);
    element.addEventListener('change', () => {
      refreshPreviewFromDraft();
      queueSave(0, 'auto');
    });
  });

  [elements.locationQuery, elements.apiKey].forEach((element) => {
    element.addEventListener('input', refreshPreviewFromDraft);
    element.addEventListener('blur', () => {
      refreshPreviewFromDraft();
      queueSave(0, 'auto');
    });
  });

  elements.theme.addEventListener('change', () => {
    if (!state.settings) {
      return;
    }
    applyPageTheme(elements.theme.value, state.settings.deviceTheme || 'dark');
  });

  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      stopPreviewPolling();
      stopWifiStatusPolling();
      return;
    }

    if (!state.eventStreamOpen) {
      startPreviewPolling();
    }
    startWifiStatusPolling();
    loadPreview().catch(() => {});
    loadWifiStatus().catch(() => {});
  });

  elements.form.addEventListener('reset', () => {
    setTimeout(refreshPreviewFromDraft, 0);
  });

  elements.form.addEventListener('change', () => {
    elements.resolvedLocation.textContent = state.settings?.locationName || readDraftSettings().locationQuery || 'Not configured';
  });

  elements.form.addEventListener('input', () => {
    const draft = readDraftSettings();
    if (draft.locationQuery && draft.locationQuery !== state.settings?.locationQuery) {
      elements.resolvedLocation.textContent = `${draft.locationQuery} (pending resolve)`;
      elements.locationKey.textContent = 'Pending save';
    }
  });
}

async function init() {
  bindEvents();
  try {
    await loadSettings();
    await loadWifiStatus();
    await loadPreview();
    await loadDebugLogs();
    connectEventStream();
    if (!state.eventStreamOpen) {
      startPreviewPolling();
    }
    startWifiStatusPolling();
  } catch (error) {
    setMessage(error?.message || 'Failed to load device state.', 'error');
  }
}

init();
