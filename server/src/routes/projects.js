/**
 * @file routes/projects.js
 * @description CRUD for cloud projects (`.pocketproj` manifests).
 *
 *  GET    /api/projects        — list current user's projects
 *  POST   /api/projects        — create  { name, manifest }
 *  GET    /api/projects/:id    — fetch manifest
 *  PUT    /api/projects/:id    — update manifest
 *  DELETE /api/projects/:id    — delete
 *
 * All routes require a valid bearer token.
 */

import { Router } from 'express';
import { v4 as uuidv4 } from 'uuid';
import { getStore, persist } from '../db.js';
import { requireAuth } from '../middleware/auth.js';

const router = Router();

router.use(requireAuth);

/**
 * List the authenticated user's projects (manifests omitted for size).
 * @route GET /api/projects
 * @returns { projects: Array<{ id, name, updatedAt, createdAt }> }
 */
router.get('/', (req, res, next) => {
  try {
    const { userId } = req.user;
    const store = getStore();
    const list = store.projects
      .filter((p) => p.userId === userId)
      .map(({ id, name, updatedAt, createdAt }) => ({ id, name, updatedAt, createdAt }));
    res.json({ projects: list });
  } catch (err) {
    next(err);
  }
});

/**
 * Create a new project.
 * @route POST /api/projects
 * @body { name: string, manifest?: object }
 * @returns { id: string }
 */
router.post('/', (req, res, next) => {
  try {
    const { userId } = req.user;
    const { name, manifest } = req.body || {};

    if (!name || typeof name !== 'string') {
      const err = new Error('name is required');
      err.status = 400;
      err.expose = true;
      throw err;
    }

    const project = {
      id: uuidv4(),
      userId,
      name: name.slice(0, 256),
      manifest: manifest && typeof manifest === 'object' ? manifest : {},
      createdAt: Date.now(),
      updatedAt: Date.now(),
    };

    const store = getStore();
    store.projects.push(project);
    persist();
    res.status(201).json({ id: project.id });
  } catch (err) {
    next(err);
  }
});

/**
 * Helper: find a project owned by the user or 404.
 * @param {string} userId
 * @param {string} id
 */
function findOwned(userId, id) {
  const store = getStore();
  const project = store.projects.find((p) => p.id === id && p.userId === userId);
  if (!project) {
    const err = new Error('project not found');
    err.status = 404;
    err.expose = true;
    throw err;
  }
  return project;
}

/**
 * Fetch a single project's full manifest.
 * @route GET /api/projects/:id
 * @returns { id, name, manifest, updatedAt, createdAt }
 */
router.get('/:id', (req, res, next) => {
  try {
    const project = findOwned(req.user.userId, req.params.id);
    res.json(project);
  } catch (err) {
    next(err);
  }
});

/**
 * Update a project's manifest (and optionally name).
 * @route PUT /api/projects/:id
 * @body { name?: string, manifest?: object }
 * @returns { id, updatedAt }
 */
router.put('/:id', (req, res, next) => {
  try {
    const project = findOwned(req.user.userId, req.params.id);
    const { name, manifest } = req.body || {};

    if (name !== undefined) {
      if (typeof name !== 'string' || !name) {
        const err = new Error('name must be a non-empty string');
        err.status = 400;
        err.expose = true;
        throw err;
      }
      project.name = name.slice(0, 256);
    }
    if (manifest !== undefined) {
      if (typeof manifest !== 'object' || manifest === null) {
        const err = new Error('manifest must be an object');
        err.status = 400;
        err.expose = true;
        throw err;
      }
      project.manifest = manifest;
    }
    project.updatedAt = Date.now();
    persist();
    res.json({ id: project.id, updatedAt: project.updatedAt });
  } catch (err) {
    next(err);
  }
});

/**
 * Delete a project.
 * @route DELETE /api/projects/:id
 * @returns { ok: true }
 */
router.delete('/:id', (req, res, next) => {
  try {
    const { userId } = req.user;
    const store = getStore();
    const idx = store.projects.findIndex((p) => p.id === req.params.id && p.userId === userId);
    if (idx === -1) {
      const err = new Error('project not found');
      err.status = 404;
      err.expose = true;
      throw err;
    }
    store.projects.splice(idx, 1);
    persist();
    res.json({ ok: true });
  } catch (err) {
    next(err);
  }
});

export default router;
