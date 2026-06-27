/**
 * @file routes/sync.js
 * @description Bulk push/pull of the user's full state.
 *
 *  POST /api/sync/push  { projects:[...], assets:[...] } -> { ok, pushed: {...} }
 *  POST /api/sync/pull  (empty body)                     -> { projects, assets }
 *
 * `projects` items in push may include `id` — if it exists for this user we
 * update it; otherwise we create a new project and return the new id in the
 * mapping. This keeps the client free to manage ids but lets it reuse
 * server-issued ids after the first push.
 *
 * `assets` in push are base64-encoded blobs:
 *   { id?, filename, mimetype, dataB64 } -> { id, url }
 * (Server re-uses the client id if provided and not yet taken.)
 */

import { Router } from 'express';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import { v4 as uuidv4 } from 'uuid';
import { getStore, persist, getAssetsDir } from '../db.js';
import { requireAuth } from '../middleware/auth.js';

const router = Router();
router.use(requireAuth);

/**
 * @route POST /api/sync/push
 * @body { projects?: Array, assets?: Array }
 * @returns { ok: true, pushed: { projects: Array<{localId, id}>, assets: Array<{localId, id, url}> } }
 */
router.post('/push', async (req, res, next) => {
  try {
    const { userId } = req.user;
    const { projects = [], assets = [] } = req.body || {};
    const store = getStore();

    /** @type {{localId:string,id:string}[]} */
    const projectMap = [];
    /** @type {{localId:string,id:string,url:string}[]} */
    const assetMap = [];

    // --- upsert projects ---
    for (const p of projects) {
      if (!p || typeof p !== 'object') continue;
      const name = typeof p.name === 'string' ? p.name.slice(0, 256) : 'Untitled';
      const manifest = p.manifest && typeof p.manifest === 'object' ? p.manifest : {};

      let project = null;
      if (p.id) {
        project = store.projects.find((x) => x.id === p.id && x.userId === userId);
      }
      if (project) {
        project.name = name;
        project.manifest = manifest;
        project.updatedAt = Date.now();
        projectMap.push({ localId: p.localId || p.id, id: project.id });
      } else {
        const newProject = {
          id: uuidv4(),
          userId,
          name,
          manifest,
          createdAt: Date.now(),
          updatedAt: Date.now(),
        };
        store.projects.push(newProject);
        projectMap.push({ localId: p.localId || p.id || null, id: newProject.id });
      }
    }

    // --- upsert assets (base64) ---
    for (const a of assets) {
      if (!a || typeof a !== 'object' || typeof a.dataB64 !== 'string') continue;
      const filename = typeof a.filename === 'string' ? a.filename : 'asset';
      const mimetype = typeof a.mimetype === 'string' ? a.mimetype : 'application/octet-stream';

      let buf;
      try {
        buf = Buffer.from(a.dataB64, 'base64');
      } catch {
        continue; // skip malformed entries
      }
      if (buf.length === 0) continue;

      // Re-use client-provided id if absent for this user; else mint new.
      let id = typeof a.id === 'string' ? a.id : null;
      if (id && store.assets.find((x) => x.id === id && x.userId === userId)) {
        // overwrite existing — remove old file entry first
        const idx = store.assets.findIndex((x) => x.id === id);
        if (idx !== -1) {
          const old = store.assets[idx];
          try { await fs.unlink(old.path); } catch { /* ignore */ }
          store.assets.splice(idx, 1);
        }
      } else if (id && store.assets.find((x) => x.id === id)) {
        // collision with another user's id — mint fresh
        id = null;
      }
      if (!id) id = uuidv4();

      const safe = filename.replace(/[^\w.\-]+/g, '_').slice(0, 128);
      const storedName = `${id}-${safe}`;
      const filePath = path.join(getAssetsDir(), storedName);
      await fs.writeFile(filePath, buf);

      const asset = {
        id,
        userId,
        filename: storedName,
        originalName: filename,
        mimetype,
        size: buf.length,
        path: filePath,
        url: `/api/assets/${id}`,
        createdAt: Date.now(),
      };
      store.assets.push(asset);
      assetMap.push({ localId: a.localId || a.id || null, id, url: asset.url });
    }

    persist();
    res.json({ ok: true, pushed: { projects: projectMap, assets: assetMap } });
  } catch (err) {
    next(err);
  }
});

/**
 * @route POST /api/sync/pull
 * @returns { projects: Array, assets: Array }
 */
router.post('/pull', (req, res, next) => {
  try {
    const { userId } = req.user;
    const store = getStore();
    const projects = store.projects
      .filter((p) => p.userId === userId)
      .map(({ id, name, manifest, updatedAt, createdAt }) => ({
        id, name, manifest, updatedAt, createdAt,
      }));
    const assets = store.assets
      .filter((a) => a.userId === userId)
      .map(({ id, url, originalName, size, mimetype, createdAt }) => ({
        id, url, filename: originalName, size, mimetype, createdAt,
      }));
    res.json({ projects, assets });
  } catch (err) {
    next(err);
  }
});

export default router;
