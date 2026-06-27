# PocketEngine Server

Node.js / Express backend for the PocketEngine C++ desktop editor (Termux:X11 on
Android). Provides device-based auth, cloud project sync (`.pocketproj` JSON
manifests), and asset storage with a CDN-style URL.

Deployable to Render.com free tier. No external database — state is kept in a
file-persisted JSON store under `data/`.

## Stack

- Express 4.x (ESM)
- `jsonwebtoken` (30-day tokens)
- `multer` (multipart asset uploads)
- `helmet` + `cors` + `morgan`
- In-memory + disk JSON store (`src/db.js`)

## Routes

| Method | Path                    | Auth | Description |
|--------|-------------------------|------|-------------|
| GET    | `/api/health`           | —    | `{ ok, time }` |
| POST   | `/api/auth/register`    | —    | `{ deviceId }` → `{ token, userId }` |
| POST   | `/api/auth/verify`      | —    | `{ token }` → `{ valid, userId? }` |
| GET    | `/api/auth/me`          | ✔    | `{ userId, deviceId }` |
| GET    | `/api/projects`         | ✔    | List user's projects (no manifests) |
| POST   | `/api/projects`         | ✔    | Create `{ name, manifest }` → `{ id }` |
| GET    | `/api/projects/:id`     | ✔    | Fetch manifest |
| PUT    | `/api/projects/:id`     | ✔    | Update `{ name?, manifest? }` |
| DELETE | `/api/projects/:id`     | ✔    | Delete |
| POST   | `/api/assets/upload`    | ✔    | Multipart `file` → `{ url, id, ... }` |
| GET    | `/api/assets`           | ✔    | List user's assets |
| GET    | `/api/assets/:id`       | —*   | Serve raw file (id is a UUID) |
| POST   | `/api/sync/push`        | ✔    | Bulk upsert `{ projects, assets }` |
| POST   | `/api/sync/pull`        | ✔    | Bulk fetch `{ projects, assets }` |

\* `GET /api/assets/:id` is intentionally unauthenticated so the engine can
fetch textures/audio over plain HTTP. The id is a UUID — guessing is impractical.
To lock down, add `requireAuth` to that route in `src/routes/assets.js`.

## Local dev

```bash
cd server
cp .env.example .env       # then edit JWT_SECRET
npm install
npm run dev                # node --watch src/index.js  → http://localhost:8080
```

Quick smoke test:

```bash
# Register a device
curl -s localhost:8080/api/auth/register \
  -H 'Content-Type: application/json' \
  -d '{"deviceId":"test-device-001"}' | jq

# Health
curl -s localhost:8080/api/health
```

## Deploy to Render.com

### Option A — Blueprint (recommended for first deploy)

1. Push this repo to GitHub.
2. On Render dashboard: **New → Blueprint** → pick the repo → Render reads
   `server/render.yaml`.
3. In the service's **Environment** tab, set:
   - `JWT_SECRET` — a long random hex string:
     ```bash
     node -e "console.log(require('crypto').randomBytes(48).toString('hex'))"
     ```
   - (optional) `CORS_ORIGIN` — your editor's origin(s), comma-separated.
4. Deploy.

### Option B — Render API (CI/CD script)

The Render API key (`rnd_...`) is a secret — **never commit it**. Pass it at
deploy time via env var or interactive prompt:

```bash
# Export your key in the shell (do NOT save to the repo):
export RENDER_API_KEY='rnd_E1L4O4ba1yPc7tk4TPkSX28E3xY4'  # example only

# Create a new web service from this repo via the Render API:
curl -s -X POST https://api.render.com/v1/services \
  -H "Authorization: Bearer $RENDER_API_KEY" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -d '{
    "type": "web_service",
    "autoDeploy": "yes",
    "service": {
      "name": "pocketengine-server",
      "environment": "node",
      "plan": "free",
      "region": "oregon",
      "branch": "main",
      "rootDir": "server",
      "buildCommand": "npm install",
      "startCommand": "node src/index.js",
      "healthCheckPath": "/api/health",
      "envVars": [
        { "key": "NODE_ENV",      "value": "production" },
        { "key": "PORT",          "value": "8080" },
        { "key": "CORS_ORIGIN",   "value": "*" },
        { "key": "STORAGE_DIR",   "value": "./data" },
        { "key": "JWT_SECRET",    "value": "REPLACE_WITH_LONG_RANDOM_HEX" }
      ]
    }
  }'
```

> Replace the `JWT_SECRET` value with real randomness. To trigger deploys on
> future commits:
> ```bash
> curl -s -X POST https://api.render.com/v1/services/<SERVICE_ID>/deploys \
>   -H "Authorization: Bearer $RENDER_API_KEY"
> ```

### Persistence caveat (free tier)

Render **free web services** do not get persistent disks — `data/` is reset on
each deploy/restart. For real persistence either:

1. Upgrade to a paid plan and add a `disk:` block to `render.yaml`
   (template is commented out inside the file), or
2. Swap `src/db.js` for an external store (Postgres on Render, SQLite on a
   persistent disk, or S3-compatible object storage for assets).

The JSON-store design is intentionally swappable — only `db.js` needs to change.

## Project layout

```
server/
├── package.json
├── render.yaml
├── .env.example
├── README.md
└── src/
    ├── index.js              # Express app entry
    ├── db.js                 # JSON file store
    ├── middleware/
    │   ├── auth.js           # bearer token + signToken
    │   └── error.js          # 404 + error handler
    └── routes/
        ├── auth.js
        ├── projects.js
        ├── assets.js
        └── sync.js
```

## License

MIT.
