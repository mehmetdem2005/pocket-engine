/**
 * @file index.js
 * @description Express app entry — CORS, helmet, morgan logging, route wiring,
 * graceful shutdown, health check.
 */

import express from 'express';
import cors from 'cors';
import helmet from 'helmet';
import morgan from 'morgan';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { initStorage, flushNow, getAssetsDir } from './db.js';
import authRoutes from './routes/auth.js';
import projectRoutes from './routes/projects.js';
import assetRoutes from './routes/assets.js';
import syncRoutes from './routes/sync.js';
import { notFound, errorHandler } from './middleware/error.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PORT = Number(process.env.PORT || 8080);
const CORS_ORIGIN = process.env.CORS_ORIGIN || '*';

const app = express();

// --- security + logging ---
app.use(helmet({ crossOriginResourcePolicy: { policy: 'cross-origin' } }));
app.use(morgan(process.env.NODE_ENV === 'production' ? 'combined' : 'dev'));

// CORS — editor runs on-device so allow any origin.
app.use(
  cors({
    origin: CORS_ORIGIN === '*' ? true : CORS_ORIGIN.split(','),
    methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
    allowedHeaders: ['Authorization', 'Content-Type'],
    credentials: false,
  }),
);

// Body parsers — bump json limit to handle base64 asset payloads in /sync/push.
app.use(express.json({ limit: '40mb' }));
app.use(express.urlencoded({ extended: true, limit: '40mb' }));

// --- health check (no auth) ---
app.get('/api/health', (_req, res) => {
  res.json({ ok: true, time: Date.now() });
});

// --- routes ---
app.use('/api/auth', authRoutes);
app.use('/api/projects', projectRoutes);
app.use('/api/assets', assetRoutes);
app.use('/api/sync', syncRoutes);

// --- 404 + error handler (must be last) ---
app.use(notFound);
app.use(errorHandler);

// --- boot ---
async function boot() {
  await initStorage();
  const server = app.listen(PORT, () => {
    console.log(`[pocketengine-server] listening on :${PORT}`);
    console.log(`[pocketengine-server] storage dir: ${path.resolve(getAssetsDir(), '..')}`);
  });

  const shutdown = async (sig) => {
    console.log(`\n[pocketengine-server] ${sig} received, shutting down...`);
    server.close(async () => {
      await flushNow();
      process.exit(0);
    });
    // Force-exit after 10s if close hangs.
    setTimeout(() => process.exit(1), 10_000).unref();
  };

  process.on('SIGINT', () => shutdown('SIGINT'));
  process.on('SIGTERM', () => shutdown('SIGTERM'));
}

boot().catch((err) => {
  console.error('[pocketengine-server] fatal boot error:', err);
  process.exit(1);
});
