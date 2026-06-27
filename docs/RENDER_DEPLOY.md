# Render.com Backend Deploy Guide

PocketEngine'in bulut senkron backend'i (`server/` altında) Render.com üzerinde
ücretsiz çalışır. Bu doküman iki deploy yolu, gerekli environment değişkenleri,
free tier kısıtlamaları ve upgrade patikasını anlatır.

## İçindekiler

1. [Ön hazırlık](#1-ön-hazırlık)
2. [Yol A — Blueprint (dashboard)](#2-yol-a--blueprint-dashboard)
3. [Yol B — Render API (curl/CI)](#3-yol-b--render-api-curlci)
4. [Environment değişkenleri](#4-environment-değişkenleri)
5. [Free tier kısıtlamaları](#5-free-tier-kısıtlamaları)
6. [Upgrade patikası](#6-upgrade-patikası)
7. [Editör tarafında bağlantı](#7-editör-tarafında-bağlantı)
8. [Güvenlik notları](#8-güvenlik-notları)

---

## 1. Ön hazırlık

Backend kodu `server/` altında. Önce repoyu GitHub'a pushla:

```bash
git remote add origin https://github.com/<user>/pocket-engine.git
git push -u origin main
```

Önkoşullar:

- GitHub hesabı
- Render.com hesabı (ücretsiz)
- Render API key (sadece Yol B için) — `rnd_...` formatında

### Render API key'i nereden alınır?

Render dashboard → **Account Settings → API Keys → Create API key**. Key
şöyle görünür:

```
rnd_E1L4O4ba1yPc7tk4TPkSX28E3xY4
```

> ⚠️ **KRİTİK:** Bu key **asla repoya commitlenmemeli**. `.gitignore`'a
> `.env` ekli. Key'i shell env var olarak export et, kullanımından sonra
> `unset RENDER_API_KEY` ile sil.

---

## 2. Yol A — Blueprint (dashboard)

İlk deploy için en kolay yol. Manuel form doldurmak yok.

### Adımlar

1. Render dashboard → **New → Blueprint**
2. GitHub hesabını bağla, `pocket-engine` repoyu seç
3. Render otomatik olarak `server/render.yaml`'ı okur. Şu service tanımını
   görür:

   ```yaml
   services:
     - type: web
       name: pocketengine-server
       env: node
       plan: free
       rootDir: server
       buildCommand: npm install
       startCommand: node src/index.js
       healthCheckPath: /api/health
       envVars:
         - key: NODE_ENV
           value: production
         - key: JWT_SECRET
           sync: false   # Apply sırasında manuel girilecek
   ```

4. **Apply**'a bas. Render `npm install` + `node src/index.js` çalıştırır.
5. Service **Environment** sekmesine git, `JWT_SECRET` ekle:

   ```bash
   # Yerel olarak üret, sonra Render'a yapıştır:
   node -e "console.log(require('crypto').randomBytes(48).toString('hex'))"
   ```

6. (Opsiyonel) `CORS_ORIGIN` — editörün çağrı yaptığı origin. Free tier'da
   genelde `*` yeterli.

7. **Deploy** → birkaç dk içinde `https://pocketengine-server.onrender.com`
   hazır. Health check: `GET /api/health` → `{ ok: true, time: ... }`

### Manuel deploy tetikleme

```bash
# Service ID'yi dashboard URL'sinden ya da API'den al
curl -s -X POST https://api.render.com/v1/services/<SERVICE_ID>/deploys \
  -H "Authorization: Bearer $RENDER_API_KEY"
```

---

## 3. Yol B — Render API (curl/CI)

CI/CD pipeline'ından deploy etmek için. `render.yaml`'a gerek yok, raw API
çağrısı yaparız.

### 3.1. API key'i export et (asla commitlama)

```bash
# Shell oturumunda geçici olarak export et:
export RENDER_API_KEY='rnd_E1L4O4ba1yPc7tk4TPkSX28E3xY4'

# Doğrula:
echo "Key length: ${#RENDER_API_KEY}"   # 33 karakter (rnd_ + 28)

# CI'da (GitHub Actions) bunu repo secret'ı olarak ekle, adını RENDER_API_KEY yap
```

> **Tekrar:** Bu key'i `git commit`'leme. `.env` dosyası repoda yok. CI'da
> secret olarak kullan; secret log'a yazılmaz ama yine de dikkatli ol.

### 3.2. JWT_SECRET üret

```bash
JWT_SECRET=$(node -e "console.log(require('crypto').randomBytes(48).toString('hex'))")
echo "JWT_SECRET length: ${#JWT_SECRET}"   # 96 karakter
```

### 3.3. Service oluştur

```bash
curl -s -X POST https://api.render.com/v1/services \
  -H "Authorization: Bearer $RENDER_API_KEY" \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -d "{
    \"type\": \"web_service\",
    \"autoDeploy\": \"yes\",
    \"service\": {
      \"name\": \"pocketengine-server\",
      \"environment\": \"node\",
      \"plan\": \"free\",
      \"region\": \"oregon\",
      \"branch\": \"main\",
      \"rootDir\": \"server\",
      \"buildCommand\": \"npm install\",
      \"startCommand\": \"node src/index.js\",
      \"healthCheckPath\": \"/api/health\",
      \"envVars\": [
        { \"key\": \"NODE_ENV\",    \"value\": \"production\" },
        { \"key\": \"PORT\",        \"value\": \"8080\" },
        { \"key\": \"CORS_ORIGIN\", \"value\": \"*\" },
        { \"key\": \"STORAGE_DIR\", \"value\": \"./data\" },
        { \"key\": \"JWT_SECRET\",  \"value\": \"$JWT_SECRET\" }
      ]
    }
  }" | tee /tmp/render-create.json
```

Cevapta `service.id` field'ı var — bunu `SERVICE_ID` olarak sakla.

### 3.4. Service ID al

```bash
SERVICE_ID=$(jq -r '.service.id' /tmp/render-create.json)
echo "Service ID: $SERVICE_ID"
```

### 3.5. Sonraki deploy'lar

```bash
# Yeni bir deploy tetikle (mevcut kodu yeniden build):
curl -s -X POST "https://api.render.com/v1/services/$SERVICE_ID/deploys" \
  -H "Authorization: Bearer $RENDER_API_KEY" \
  -H "Accept: application/json"

# Clear cache + deploy:
curl -s -X POST "https://api.render.com/v1/services/$SERVICE_ID/deploys?clearCache=y" \
  -H "Authorization: Bearer $RENDER_API_KEY"
```

### 3.6. Service durumu

```bash
curl -s "https://api.render.com/v1/services/$SERVICE_ID" \
  -H "Authorization: Bearer $RENDER_API_KEY" \
  -H "Accept: application/json" | jq '.service.status'
# → "suspended" | "build_failed" | "live" | "crashed"
```

### 3.7. Key'i temizle

```bash
unset RENDER_API_KEY
unset JWT_SECRET
```

---

## 4. Environment değişkenleri

| Key | Zorunlu | Default | Açıklama |
|-----|---------|---------|----------|
| `NODE_ENV` | Hayır | `development` | `production` önerilir |
| `PORT` | Hayır | `8080` | Render `PORT` env'yi override eder |
| `JWT_SECRET` | **EVET** | — | JWT imzalama anahtarı, uzun random hex |
| `CORS_ORIGIN` | Hayır | `*` | Editör origin'i, comma-separated |
| `STORAGE_DIR` | Hayır | `./data` | JSON store dizini |
| `DB_TYPE` | Hayır | `json` | Gelecek: `postgres`, `sqlite` |

### JWT_SECRET üretimi

```bash
# 96 karakterlik hex (48 byte random):
node -e "console.log(require('crypto').randomBytes(48).toString('hex'))"

# Veya openssl:
openssl rand -hex 48
```

### CORS_ORIGIN örnek

- Free tier / dev: `*` (herkes)
- Production: `https://editor.pocketengine.app` (tek origin)
- Multi-origin: `https://a.com,https://b.com`

---

## 5. Free tier kısıtlamaları

Render **free web services** planı:

| Özellik | Free | Starter ($7/ay) |
|---------|------|-----------------|
| RAM | 512 MB | 512 MB |
| CPU | 0.1 vCPU | 0.5 vCPU |
| Disk | ❌ (ephemeral) | ✅ (1 GB persistent) |
| Uptime | Spin-down 15 dk idle | 7/24 |
| Cold start | ~30 sn | Anında |
| Bandwidth | 100 GB/ay | 100 GB/ay |
| Build dakika | 750/ay | 3000/ay |

### Ephemeral disk sorunu

Free tier'da `data/` **her deploy/restart'ta sıfırlanır**. Yani:

- Kullanıcı kayıt olur → JWT alır
- 15 dk idle → Render spin-down
- Yeni istek → cold start → `data/db.json` **sıfırdan başlar**
- Eski tokenlar geçersiz, projeler kayıp

### Workaround

1. **Free tier + storage'ı istemci tarafında tut**: Editör, sync pull yapıp
   local SQLite'a yazar. Backend sadece "cache" gibi davranır.
2. **Starter plan + persistent disk**: `render.yaml`'daki `disk:` bloğunu
   uncomment et.
3. **Postgres'e geç**: `src/db.js`'i Postgres backend ile değiştir (sadece
   o dosya değişir, diğer kod aynı).

---

## 6. Upgrade patikası

```
Free (ephemeral)
    │
    │  Proje büyüdü, data kaybı can sıkıcı
    ▼
Starter ($7/ay) + 1 GB persistent disk
    │
    │  Asset'ler > 1 GB, çok kullanıcı
    ▼
Standard ($25/ay) + 10 GB disk
    │
    │  Postgres + S3 gerekli
    ▼
Standard + Render Postgres + Cloudflare R2
```

### Persistent disk ekleme (paid plan)

`server/render.yaml`'daki commented bloğu uncomment et:

```yaml
services:
  - type: web
    name: pocketengine-server
    # ...
    disk:
      name: pocketengine-data
      mountPath: /var/data
      sizeGB: 1   # min 1 GB
```

`STORAGE_DIR=/var/data` set et.

### Postgres'e geçiş

1. Render dashboard → **New → PostgreSQL** (free tier 90 gün, sonra $7/ay)
2. Connection string'i `DATABASE_URL` env'ye ekle
3. `server/src/db.js`'i `pg` paketiyle değiştir (TODO — kod yazılmadı)
4. Schema migration: `server/migrations/001_init.sql` (TODO)

---

## 7. Editör tarafında bağlantı

Editörün backend URL'i runtime'da set edilmeli. Editor config:

```
~/.pocketengine/config.json
{
  "serverUrl": "https://pocketengine-server.onrender.com",
  "deviceId":  "auto-generated-uuid",
  "authToken": "jwt-from-register"
}
```

Editör akışı:

1. İlk açılış → `deviceId` üret (UUID), kayıt ol: `POST /api/auth/register`
2. JWT al → `authToken` olarak sakla
3. Her save → `POST /api/sync/push` (Bearer token)
4. Açılışta → `POST /api/sync/pull` (en güncel state)

> Free tier cold start ~30 sn ilk istek yavaş olur. Editör loading
> indicator göstermeli.

---

## 8. Güvenlik notları

### API key (`rnd_E1L4O4ba1yPc7tk4TPkSX28E3xY4`)

- **Asla** repoya commitlenmemeli
- Shell env var olarak export et, kullanım sonra `unset`
- CI'da secret olarak kullan; log'a yazılmasın
- Render dashboard → Account Settings → API Keys → rotate (sık sık)

### JWT_SECRET

- `rnd_...` ile **karıştırma** — JWT_SECRET uygulama secret'i, kullanıcıya
  gösterilmez
- Her environment için ayrı (dev/staging/prod)
- Rotate etmek → tüm kullanıcı tokenları geçersiz (30 gün ömür)

### CORS

- Production'da `CORS_ORIGIN=*` yerine spesifik origin listele
- `GET /api/assets/:id` unauthenticated — UUID tahmin edilemez ama yine de
  CDN + rate-limit düşünülebilir

### Diğer

- HTTPS Render tarafından otomatik (Let's Encrypt)
- Helmet default header'ları set eder (`X-Frame-Options`, `CSP`, vs.)
- Morgan production'da combined log
- Body limit 40 MB (asset upload için) — DDoS için `express-rate-limit`
  eklenebilir (TODO)

---

## 9. Hızlı troubleshooting

| Belirti | Sebep | Çözüm |
|---------|-------|-------|
| `/api/health` 502 | Service spun down | İlk istek cold-start, bekle |
| `/api/auth/register` 500 | `JWT_SECRET` set değil | Render envVars'a ekle |
| Token 401 | JWT_SECRET değişti | Yeniden register ol |
| `/api/sync/push` 413 | Body > 40 MB | Asset'leri ayrı upload et |
| Data kayboldu | Free tier restart | Starter + disk ya da Postgres |

Detaylı backend dokümantasyonu: `server/README.md`.
