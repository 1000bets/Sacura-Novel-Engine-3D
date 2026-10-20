#!/usr/bin/env bash
set -euo pipefail
release="${1:?Commit SHA required}"
[[ "$release" =~ ^[a-f0-9]{40}$ ]] || { echo 'Invalid commit SHA' >&2; exit 2; }
root=/opt/sacura/staging
exec 9>"$root/deploy.lock"
flock -w 600 9
directory="$root/releases/$release/engine-web-prototype"
[[ -f "$directory/deploy/production.sh" ]] || { echo 'Release archive missing' >&2; exit 1; }
previous=$(readlink -f "$root/current" || true)
export SACURA_ENV_FILE="$root/shared/.env" SACURA_COMPOSE_PROJECT=sacura-staging SACURA_RELEASE="$release"
if ! bash "$directory/deploy/production.sh" deploy; then
 if [[ -n "$previous" && -f "$previous/deploy/production.sh" ]]; then
  echo 'Deployment failed; restoring previous container images.' >&2
  export SACURA_RELEASE=$(basename "$(dirname "$previous")")
  docker compose --env-file "$SACURA_ENV_FILE" -p "$SACURA_COMPOSE_PROJECT" -f "$previous/compose.yaml" -f "$previous/compose.production.yaml" -f "$previous/compose.release.yaml" up -d --no-build --wait --wait-timeout 300
 fi
 exit 1
fi
# Verify the public endpoint, including trusted TLS and the exact deployed commit.
if ! python3 - "$release" <<'PY'
import json,sys,time,urllib.request
base='https://sacura-test.duckdns.org'
for attempt in range(12):
 try:
  with urllib.request.urlopen(base+'/api/health',timeout=15) as response:
   assert json.load(response)['status']=='ok'
  with urllib.request.urlopen(base+'/version.json',timeout=15) as response:
   assert json.load(response)['commit']==sys.argv[1]
  print('Public HTTPS health and deployed commit verified.')
  break
 except Exception:
  if attempt==11:sys.exit('Public health/version verification failed')
  time.sleep(5)
PY
then
 if [[ -n "$previous" && -f "$previous/compose.release.yaml" ]]; then
  export SACURA_RELEASE=$(basename "$(dirname "$previous")")
  docker compose --env-file "$SACURA_ENV_FILE" -p "$SACURA_COMPOSE_PROJECT" -f "$previous/compose.yaml" -f "$previous/compose.production.yaml" -f "$previous/compose.release.yaml" up -d --no-build --wait --wait-timeout 300
 fi
 exit 1
fi
ln -sfn "$directory" "$root/current.next"
mv -Tf "$root/current.next" "$root/current"
echo "Staging release active: $release"
