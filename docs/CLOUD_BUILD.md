**English** · [简体中文](CLOUD_BUILD.zh_CN.md)

# Cloud build and deploy to the official AI Passport platform

Get a firmware image without ESP-IDF installed locally: build in the cloud with GitHub, download the bin, upload to the official platform.

---

## 1. Create a GitHub repository

1. Sign in to GitHub, click `+` → `New repository`.
2. Pick any name, e.g. `ai-passport-storyteller`.
3. **Important: do NOT check** "Add a README file" / ".gitignore" / "license" — it must be empty.
4. Copy the repository URL, e.g. `https://github.com/<your-user>/ai-passport-storyteller.git`.

## 2. Edit the script and push

Open `tools/push_to_github.sh` and set your URL:
```bash
REPO_URL="https://github.com/<your-user>/ai-passport-storyteller.git"
```

Then run in the project directory (Git Bash):
```bash
bash tools/push_to_github.sh
```

On success you'll see the `pushed` confirmation message. The script also pushes tag `storyteller-v1` to trigger the build.

> If prompted to log in: GitHub uses a Personal Access Token as the password. Create a token with `repo` scope (Settings → Developer settings → Personal access tokens) and paste it; username is still your GitHub username.

## 3. Wait for the cloud build

1. Open your repo → **Actions**.
2. Find **Build firmware**, open the latest run.
3. Wait until all steps are green (3–8 min). It uses the repo's ESP-IDF 5.5.3; artifact name is fixed as `FoloToy-AI-Passport-full.bin`.
4. If a step fails (red), open the log and send me the error so I can fix the code.

## 4. Download the bin

**Option A (Artifact)**: run page → **Artifacts** → `passport-storyteller-v1` → download ZIP → extract `FoloToy-AI-Passport-full.bin`.

**Option B (Release)**: the tag triggers a Release → **Releases** on the right → download `FoloToy-AI-Passport-full.bin`.

## 5. Upload to the official platform

1. Open the official AI Passport firmware flashing page.
2. Choose "**single merged firmware**" and upload `FoloToy-AI-Passport-full.bin`.
3. Follow the platform instructions to deploy.

> The merged firmware is a complete image starting at 0x0 — one file is enough; no separate partition bins needed.

---

## FAQ

**Why push a tag?** The repo's `Build firmware` workflow triggers on tag push. You can also click `Run workflow` manually in Actions.

**Build failed?** Send me the red error from the Actions log — with no local compiler, cloud logs are how we catch real compile errors.

**Device unresponsive after upload?** Confirm you downloaded the merged `FoloToy-AI-Passport-full.bin`, use a data-capable USB cable, and follow the device checklist in `docs/DEPLOY.md`.

**Why a chime instead of speech?** No prerecorded audio yet; it falls back to "chime + on-screen text" by design. See `docs/DEPLOY.md` step 3 to add WAV recordings and rebuild.

**Before going live?** Two red lines: (1) review all stories in `content/stories.csv` (`verified=true`, add source URLs); (2) confirm font/mascot licensing. See `docs/ASSETS.md`.
