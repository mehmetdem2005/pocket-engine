/**
 * @file db.js
 * @description In-memory + file-persisted JSON store.
 *
 * The store loads all collections from <STORAGE_DIR>/db.json on boot and
 * flushes mutations back to disk (debounced). This avoids any external DB
 * dependency and works fine on Render's free tier disk.
 *
 * Collections:
 *  - users:    { id, deviceId, createdAt }
 *  - projects: { id, userId, name, manifest, updatedAt, createdAt }
 *  - assets:   { id, userId, filename, mimetype, size, path, url, createdAt }
 */

import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const STORAGE_DIR = process.env.STORAGE_DIR || path.join(__dirname, '..', 'data');
const DB_FILE = path.join(STORAGE_DIR, 'db.json');
const ASSETS_DIR = path.join(STORAGE_DIR, 'assets');

/**
 * In-memory data shape.
 * @typedef {Object} DBShape
 * @property {Object[]} users
 * @property {Object[]} projects
 * @property {Object[]} assets
 */

/** @type {DBShape} */
const data = {
  users: [],
  projects: [],
  assets: [],
};

let flushScheduled = false;
let flushTimer = null;

/**
 * Ensure storage directories exist.
 * @returns {Promise<void>}
 */
export async function initStorage() {
  await fs.mkdir(STORAGE_DIR, { recursive: true });
  await fs.mkdir(ASSETS_DIR, { recursive: true });
  await load();
}

/**
 * Load the JSON store from disk into memory.
 * @returns {Promise<void>}
 */
async function load() {
  try {
    const raw = await fs.readFile(DB_FILE, 'utf8');
    const parsed = JSON.parse(raw);
    data.users = Array.isArray(parsed.users) ? parsed.users : [];
    data.projects = Array.isArray(parsed.projects) ? parsed.projects : [];
    data.assets = Array.isArray(parsed.assets) ? parsed.assets : [];
  } catch (err) {
    if (err.code !== 'ENOENT') {
      console.error('[db] failed to load db.json:', err.message);
    }
    // First boot — file doesn't exist yet; that's fine.
  }
}

/**
 * Persist the in-memory store to disk. Debounced so bursts of writes
 * only hit the filesystem once.
 * @returns {Promise<void>}
 */
function scheduleFlush() {
  if (flushScheduled) return;
  flushScheduled = true;
  if (flushTimer) clearTimeout(flushTimer);
  flushTimer = setTimeout(async () => {
    flushScheduled = false;
    flushTimer = null;
    try {
      const tmp = DB_FILE + '.tmp';
      await fs.writeFile(tmp, JSON.stringify(data, null, 2), 'utf8');
      await fs.rename(tmp, DB_FILE);
    } catch (err) {
      console.error('[db] failed to flush db.json:', err.message);
    }
  }, 250);
}

/**
 * Force-flush immediately (used on shutdown).
 * @returns {Promise<void>}
 */
export async function flushNow() {
  if (flushTimer) {
    clearTimeout(flushTimer);
    flushTimer = null;
  }
  try {
    const tmp = DB_FILE + '.tmp';
    await fs.writeFile(tmp, JSON.stringify(data, null, 2), 'utf8');
    await fs.rename(tmp, DB_FILE);
  } catch (err) {
    console.error('[db] flushNow failed:', err.message);
  }
}

/** @returns {DBShape} the live in-memory store (mutate then call scheduleFlush) */
export function getStore() {
  return data;
}

/** Trigger debounced disk persistence. */
export function persist() {
  scheduleFlush();
}

/** @returns {string} absolute path to the assets directory */
export function getAssetsDir() {
  return ASSETS_DIR;
}

/** @returns {string} absolute path to the storage root */
export function getStorageDir() {
  return STORAGE_DIR;
}
