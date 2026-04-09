/**
 * main.js — AgentVST Default WebView UI
 *
 * JUCE 8 WebView bridge integration.
 *
 * When running inside JUCE's WebBrowserComponent:
 *   - window.__JUCE__.initialisationData contains the plugin schema
 *   - Juce.getSliderState(id)     returns a slider state object for float/int params
 *   - Juce.getToggleState(id)     returns a toggle state object for boolean params
 *   - Juce.getComboBoxState(id)   returns a combo state object for enum params
 *
 * When running in a browser (dev mode):
 *   - Falls back to a local mock bridge for layout preview.
 */

'use strict';

// ─────────────────────────────────────────────────────────────────────────────
// Dev-mode mock bridge (used when not inside JUCE)
// ─────────────────────────────────────────────────────────────────────────────
const isBrowser = typeof window.__JUCE__ === 'undefined';

const mockSchema = isBrowser ? {
  name: 'AgentVST Plugin',
  version: '1.0.0',
  parameters: [
    { id: 'gain_db', name: 'Gain',   type: 'float',   min: -60, max: 12,  default: 0,     unit: 'dB',  group: 'main' },
    { id: 'bypass',  name: 'Bypass', type: 'boolean',                      default: false,              group: 'main' }
  ],
  groups: [
    { id: 'main', name: 'Main', parameters: ['gain_db', 'bypass'] }
  ]
} : null;

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────
function escHtml(str) {
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function formatValue(value, param) {
  const num = parseFloat(value);
  const decimals = (param.type === 'int' || (param.step && param.step >= 1)) ? 0 : 2;
  return num.toFixed(decimals) + (param.unit ? ' ' + param.unit : '');
}

// ─────────────────────────────────────────────────────────────────────────────
// Parameter control builders
// ─────────────────────────────────────────────────────────────────────────────
function buildSlider(param) {
  const row = document.createElement('div');
  row.className = 'param-row';

  const labelRow = document.createElement('div');
  labelRow.className = 'param-label-row';

  const nameEl = document.createElement('span');
  nameEl.className = 'param-name';
  nameEl.textContent = param.name;

  const valueEl = document.createElement('span');
  valueEl.className = 'param-value-display';
  valueEl.id = 'val-' + param.id;
  valueEl.textContent = formatValue(param.default, param);

  labelRow.appendChild(nameEl);
  labelRow.appendChild(valueEl);

  const step = param.step > 0 ? param.step : (param.max - param.min) / 1000;
  const input = document.createElement('input');
  input.type  = 'range';
  input.id    = 'ctrl-' + param.id;
  input.min   = param.min;
  input.max   = param.max;
  input.step  = step;
  input.value = param.default;

  row.appendChild(labelRow);
  row.appendChild(input);

  // Wire to JUCE bridge or mock
  if (!isBrowser && typeof Juce !== 'undefined') {
    const state = Juce.getSliderState(param.id);
    // Sync JUCE → HTML
    state.valueChangedEvent.addListener(() => {
      const v = state.getNormalisedValue() * (param.max - param.min) + param.min;
      input.value        = v;
      valueEl.textContent = formatValue(v, param);
    });
    // Sync HTML → JUCE
    input.addEventListener('input', () => {
      const normalised = (parseFloat(input.value) - param.min) / (param.max - param.min);
      state.setNormalisedValue(normalised);
      valueEl.textContent = formatValue(input.value, param);
    });
  } else {
    input.addEventListener('input', () => {
      valueEl.textContent = formatValue(input.value, param);
    });
  }

  return row;
}

function buildToggle(param) {
  const row = document.createElement('div');
  row.className = 'toggle-row';

  const nameEl = document.createElement('span');
  nameEl.className = 'toggle-name';
  nameEl.textContent = param.name;

  const label = document.createElement('label');
  label.className = 'toggle-switch';

  const checkbox = document.createElement('input');
  checkbox.type    = 'checkbox';
  checkbox.id      = 'ctrl-' + param.id;
  checkbox.checked = !!param.default;

  const track = document.createElement('span');
  track.className = 'toggle-track';
  const thumb = document.createElement('span');
  thumb.className = 'toggle-thumb';

  label.appendChild(checkbox);
  label.appendChild(track);
  label.appendChild(thumb);

  row.appendChild(nameEl);
  row.appendChild(label);

  if (!isBrowser && typeof Juce !== 'undefined') {
    const state = Juce.getToggleState(param.id);
    state.valueChangedEvent.addListener(() => {
      checkbox.checked = state.getValue();
    });
    checkbox.addEventListener('change', () => state.setValue(checkbox.checked));
  }

  return row;
}

function buildCombo(param) {
  const row = document.createElement('div');
  row.className = 'combo-row';

  const nameEl = document.createElement('span');
  nameEl.className = 'combo-name';
  nameEl.textContent = param.name;

  const select = document.createElement('select');
  select.id = 'ctrl-' + param.id;
  (param.options || []).forEach((opt, i) => {
    const option = document.createElement('option');
    option.value = i;
    option.textContent = opt;
    select.appendChild(option);
  });
  select.value = param.default || 0;

  row.appendChild(nameEl);
  row.appendChild(select);

  if (!isBrowser && typeof Juce !== 'undefined') {
    const state = Juce.getComboBoxState(param.id);
    state.valueChangedEvent.addListener(() => {
      select.value = state.getChoiceIndex();
    });
    select.addEventListener('change', () => state.setChoiceIndex(parseInt(select.value)));
  }

  return row;
}

// ─────────────────────────────────────────────────────────────────────────────
// Render schema into the DOM
// ─────────────────────────────────────────────────────────────────────────────
function renderSchema(schema) {
  document.getElementById('pluginName').textContent    = schema.name    || 'AgentVST Plugin';
  document.getElementById('pluginVersion').textContent = 'v' + (schema.version || '1.0.0');
  document.title = schema.name || 'AgentVST Plugin';

  const area = document.getElementById('controlsArea');
  area.innerHTML = '';

  // Build a map: groupId → DOM card
  const groupCards = {};
  (schema.groups || []).forEach(g => {
    const card = document.createElement('div');
    card.className = 'param-group';
    const title = document.createElement('div');
    title.className = 'param-group-title';
    title.textContent = g.name;
    card.appendChild(title);
    area.appendChild(card);
    groupCards[g.id] = { card, paramIds: g.parameters || [] };
  });

  // Ungrouped fallback card
  let ungroupedCard = null;
  const allGroupedIds = Object.values(groupCards).flatMap(g => g.paramIds);
  const ungroupedParams = (schema.parameters || []).filter(
    p => !allGroupedIds.includes(p.id)
  );
  if (ungroupedParams.length) {
    ungroupedCard = document.createElement('div');
    ungroupedCard.className = 'param-group';
    area.appendChild(ungroupedCard);
  }

  // Insert controls in group order
  const paramMap = {};
  (schema.parameters || []).forEach(p => { paramMap[p.id] = p; });

  Object.values(groupCards).forEach(({ card, paramIds }) => {
    paramIds.forEach(id => {
      const param = paramMap[id];
      if (!param) return;
      if (param.type === 'boolean') card.appendChild(buildToggle(param));
      else if (param.type === 'enum') card.appendChild(buildCombo(param));
      else card.appendChild(buildSlider(param));
    });
  });

  ungroupedParams.forEach(param => {
    if (param.type === 'boolean') ungroupedCard.appendChild(buildToggle(param));
    else if (param.type === 'enum') ungroupedCard.appendChild(buildCombo(param));
    else ungroupedCard.appendChild(buildSlider(param));
  });

  document.getElementById('statusText').textContent = 'Connected — ' +
    (schema.parameters || []).length + ' parameters';
}

// ─────────────────────────────────────────────────────────────────────────────
// Watchdog violation indicator
// ─────────────────────────────────────────────────────────────────────────────
function flashWatchdog() {
  const el = document.getElementById('watchdogIndicator');
  el.classList.add('violation');
  el.title = 'DSP watchdog triggered — processing time exceeded budget';
  setTimeout(() => el.classList.remove('violation'), 2000);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bootstrap
// ─────────────────────────────────────────────────────────────────────────────
function init() {
  if (isBrowser) {
    // Dev mode: use mock schema
    console.info('[AgentVST] Running in browser dev mode. Using mock schema.');
    renderSchema(mockSchema);
    return;
  }

  // JUCE 8 WebView: read schema from initialisationData
  try {
    const initData = JSON.parse(window.__JUCE__.initialisationData);
    if (initData && initData.schema) {
      renderSchema(initData.schema);
    } else {
      document.getElementById('statusText').textContent =
        'Warning: no schema in initialisationData';
    }
  } catch (e) {
    document.getElementById('statusText').textContent = 'Error: ' + e.message;
  }

  // Register watchdog event listener (Phase 3: exposed by C++ bridge)
  if (typeof Juce !== 'undefined' && Juce.addEventListener) {
    Juce.addEventListener('watchdog_violation', flashWatchdog);
  }
}

document.addEventListener('DOMContentLoaded', init);
