#!/usr/bin/env bash
# Run from any directory. Never sources .env as shell code.
set -euo pipefail
cd "$(dirname "$0")/.."
env_file="${SACURA_ENV_FILE:-.env}"
command -v docker >/dev/null || { echo 'Install Docker Engine and Compose plugin first.' >&2; exit 1; }
[[ -f "$env_file" ]] || { echo 'Copy .env.example to .env and fill S3_SECRET_KEY, DOMAIN and ACME_EMAIL.' >&2; exit 1; }
compose=(docker compose --env-file "$env_file" -f compose.yaml -f compose.production.yaml)
if [[ -n "${SACURA_RELEASE:-}" ]]; then
 compose+=(-f compose.release.yaml)
fi
if [[ -n "${SACURA_COMPOSE_PROJECT:-}" ]]; then
 compose+=(-p "$SACURA_COMPOSE_PROJECT")
fi
"${compose[@]}" config --quiet
case "${1:-deploy}" in
 check)
  docker info >/dev/null
  echo 'Docker and production Compose configuration are ready.'
  ;;
 deploy)
  docker info >/dev/null
  # Build everything before replacing the running application.
  "${compose[@]}" build --pull
  # Back up an existing database before an update, without removing volumes.
  if [[ -n "$("${compose[@]}" ps --status running -q api)" ]]; then
   "${compose[@]}" exec -T api node backup.js
  fi
  "${compose[@]}" up -d --wait --wait-timeout 300
  "${compose[@]}" exec -T web wget -q -O - http://127.0.0.1:8080/api/health
  printf '\n'
  "${compose[@]}" ps
  echo 'Check the public HTTPS URL after DNS propagation and certificate issuance.'
  ;;
 status) "${compose[@]}" ps ;;
 logs) "${compose[@]}" logs --tail=100 ;;
 backup) "${compose[@]}" exec -T api node backup.js ;;
 *) echo 'Usage: bash deploy/production.sh [check|deploy|status|logs|backup]' >&2; exit 2 ;;
esac
