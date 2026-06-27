/**
 * @file routes/assets.js
 * @description Cloud asset storage — textures, audio, etc.
 *
 *  POST   /api/assets/upload   — multipart `file` field  -> { url, id }
 *  GET    /api/assets          — list user's assets
 *  GET    /api/assets/:id      — serve the raw file
 *
 * Files are written to <STORAGE_DIR>/assets/<id>-<originalname>.
 */

import { Router } from 'express';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import { v4 as uuidv4 } from 'uuid';
import multer from 'multer';
import { getStore, persist, getAssetsDir } from '../db.js';
import { requireAuth } from '../middleware/auth.js';

const router = Router();
router.use(requireAuth);

/**
 * Multer storage — disk, filename `<uuid>-<safe-originalname>`.
 * Files are kept inside the assets directory so they survive restarts.
 */
const storage = multer.diskStorage({
  destination: (_req, _file, cb) => cb(null, getAssetsDir()),
  filename: (_req, file, cb) => {
    const id = uuidv4();
    const safe = (file.originalname || 'asset').replace(/[^\w.\-]+/g, '_').slice(0, 128);
    cb(null, `${id}-${safe}`);
  },
});

/** 32 MB per file cap (Render free tier friendly). */
const upload = multer({
  storage,
  limits: { fileSize: 32 * 1024 * 1024 },
});

/**
 * Upload a single file. Field name must be `file`.
 *
 * @route POST /api/assets/upload
 * @multipart file
 * @returns { id: string, url: string, filename: string, size: number, mimetype: string }
 */
router.post('/upload', upload.single('file'), (req, res, next) => {
  try {
    if (!req.file) {
      const err = new Error('no file uploaded (field name must be "file")');
      err.status = 400;
      err.expose = true;
      throw err;
    }

    const id = path.basename(req.file.filename).split('-')[0];
    const asset = {
      id,
      userId: req.user.userId,
      filename: req.file.filename,
      originalName: req.file.originalname,
      mimetype: req.file.mimetype,
      size: req.file.size,
      path: req.file.path,
      url: `/api/assets/${id}`,
      createdAt: Date.now(),
    };

    const store = getStore();
    store.assets.push(asset);
    persist();
    res.status(201).json({
      id: asset.id,
      url: asset.url,
      filename: asset.originalName,
      size: asset.size,
      mimetype: asset.mimetype,
    });
  } catch (err) {
    next(err);
  }
});

/**
 * List the authenticated user's assets (metadata only).
 * @route GET /api/assets
 * @returns { assets: Array<{ id, url, filename, size, mimetype, createdAt }> }
 */
router.get('/', (req, res, next) => {
  try {
    const store = getStore();
    const list = store.assets
      .filter((a) => a.userId === req.user.userId)
      .map(({ id, url, originalName, size, mimetype, createdAt }) => ({
        id,
        url,
        filename: originalName,
        size,
        mimetype,
        createdAt,
      }));
    res.json({ assets: list });
  } catch (err) {
    next(err);
  }
});

/**
 * Serve the raw file for an asset. Public-ish (no auth) so the engine can
 * fetch textures via plain HTTP. The id is a UUID — guessing is impractical.
 * To lock down, attach `requireAuth` here and pass the token from the engine.
 *
 * @route GET /api/assets/:id
 */
router.get('/:id', async (req, res, next) => {
  try {
    const store = getStore();
    const asset = store.assets.find((a) => a.id === req.params.id);
    if (!asset) {
      const err = new Error('asset not found');
      err.status = 404;
      err.expose = true;
      throw err;
    }
    // Resolve path defensively in case storage dir moved between deploys.
    const filePath = asset.path && (await fileExists(asset.path))
      ? asset.path
      : path.join(getAssetsDir(), asset.filename);
    if (!(await fileExists(filePath))) {
      const err = new Error('asset file missing on disk');
      err.status = 410;
      err.expose = true;
      throw err;
    }
    res.setHeader('Content-Type', asset.mimetype || 'application/octet-stream');
    res.setHeader('Cache-Control', 'public, max-age=31536000, immutable');
    res.sendFile(filePath);
  } catch (err) {
    next(err);
  }
});

/**
 * @param {string} p
 * @returns {Promise<boolean>}
 */
async function fileExists(p) {
  try {
    await fs.access(p);
    return true;
  } catch {
    return false;
  }
}

export default router;
