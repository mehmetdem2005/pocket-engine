/**
 * @file middleware/error.js
 * @description Centralized error-handling middleware + 404 catcher.
 */

/**
 * 404 catcher — runs when no route matched.
 * @param {import('express').Request} _req
 * @param {import('express').Response} res
 * @returns {void}
 */
export function notFound(_req, res) {
  res.status(404).json({ error: 'not found' });
}

/**
 * Final error handler. Must have arity 4 to be recognized by Express.
 * @param {Error} err
 * @param {import('express').Request} _req
 * @param {import('express').Response} res
 * @param {import('express').NextFunction} _next
 * @returns {void}
 */
// eslint-disable-next-line no-unused-vars
export function errorHandler(err, _req, res, _next) {
  const status = err.status || err.statusCode || 500;
  const message = err.expose ? err.message : (status >= 500 ? 'internal server error' : err.message);

  if (status >= 500) {
    console.error('[error]', err);
  }

  res.status(status).json({
    error: message,
    ...(process.env.NODE_ENV !== 'production' && err.stack ? { stack: err.stack } : {}),
  });
}
