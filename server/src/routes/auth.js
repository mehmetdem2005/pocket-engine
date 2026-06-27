/**
 * @file routes/auth.js
 * @description Device-based auth — no password.
 *
 *  POST /api/auth/register  { deviceId } -> { token, userId }
 *  POST /api/auth/verify    { token }    -> { valid, userId }
 */

import { Router } from 'express';
import jwt from 'jsonwebtoken';
import { v4 as uuidv4 } from 'uuid';
import { getStore, persist } from '../db.js';
import { signToken, requireAuth } from '../middleware/auth.js';

const router = Router();

/**
 * Register (or reuse) a device. If the deviceId already exists, we issue a
 * fresh token for the existing user instead of creating a duplicate.
 *
 * @route POST /api/auth/register
 * @body { deviceId: string }
 * @returns { token: string, userId: string }
 */
router.post('/register', (req, res, next) => {
  try {
    const { deviceId } = req.body || {};
    if (!deviceId || typeof deviceId !== 'string' || deviceId.length < 4) {
      const err = new Error('deviceId is required (min 4 chars)');
      err.status = 400;
      err.expose = true;
      throw err;
    }

    const store = getStore();
    let user = store.users.find((u) => u.deviceId === deviceId);

    if (!user) {
      user = {
        id: uuidv4(),
        deviceId,
        createdAt: Date.now(),
      };
      store.users.push(user);
      persist();
    }

    const token = signToken({ userId: user.id, deviceId: user.deviceId });
    res.status(201).json({ token, userId: user.id });
  } catch (err) {
    next(err);
  }
});

/**
 * Verify a token (used by the editor to validate a cached token on launch).
 *
 * @route POST /api/auth/verify
 * @body { token: string }
 * @returns { valid: boolean, userId?: string }
 */
router.post('/verify', (req, res, next) => {
  try {
    const { token } = req.body || {};
    if (!token) {
      const err = new Error('token is required');
      err.status = 400;
      err.expose = true;
      throw err;
    }

    try {
      const decoded = jwt.verify(token, process.env.JWT_SECRET || 'dev-insecure-secret');
      res.json({ valid: true, userId: decoded.userId });
    } catch {
      res.json({ valid: false });
    }
  } catch (err) {
    next(err);
  }
});

/**
 * Optional: whoami — requires an existing valid token.
 *
 * @route GET /api/auth/me
 * @returns { userId: string, deviceId: string }
 */
router.get('/me', requireAuth, (req, res) => {
  res.json({ userId: req.user.userId, deviceId: req.user.deviceId });
});

export default router;
