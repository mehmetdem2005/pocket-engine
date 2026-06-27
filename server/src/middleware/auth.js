/**
 * @file middleware/auth.js
 * @description Bearer token middleware. Verifies JWT and attaches
 * `req.user = { userId, deviceId }` on success.
 */

import jwt from 'jsonwebtoken';

const SECRET = process.env.JWT_SECRET || 'dev-insecure-secret';
const TOKEN_TTL = '30d';

/**
 * Sign a JWT for the given user.
 * @param {{ userId: string, deviceId: string }} payload
 * @returns {string} signed JWT
 */
export function signToken({ userId, deviceId }) {
  return jwt.sign({ userId, deviceId }, SECRET, { expiresIn: TOKEN_TTL });
}

/**
 * Express middleware that requires a valid `Authorization: Bearer <token>`
 * header. On success attaches `req.user`.
 *
 * @param {import('express').Request} req
 * @param {import('express').Response} res
 * @param {import('express').NextFunction} next
 * @returns {void}
 */
export function requireAuth(req, res, next) {
  const header = req.headers.authorization || '';
  const [scheme, token] = header.split(' ');

  if (scheme !== 'Bearer' || !token) {
    return res.status(401).json({ error: 'missing or malformed Authorization header' });
  }

  try {
    const decoded = jwt.verify(token, SECRET);
    req.user = { userId: decoded.userId, deviceId: decoded.deviceId };
    next();
  } catch (err) {
    return res.status(401).json({ error: 'invalid or expired token' });
  }
}
