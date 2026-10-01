#!/usr/bin/env bash
# Run once as root on Ubuntu after installing Docker Engine and the Compose plugin.
set -euo pipefail
[[ "$EUID" == 0 ]] || { echo 'Run as root' >&2; exit 1; }
key_file="${1:?Path to deployment PUBLIC SSH key required}"
email="${2:?ACME contact email required}"
[[ -f "$key_file" ]] && ssh-keygen -l -f "$key_file" >/dev/null
docker info >/dev/null
docker compose version >/dev/null
id sacura-deploy >/dev/null 2>&1 || useradd --create-home --shell /bin/bash sacura-deploy
usermod -aG docker sacura-deploy
root=/opt/sacura/staging
install -d -m 750 -o sacura-deploy -g sacura-deploy "$root" "$root/releases" "$root/shared"
install -d -m 700 -o sacura-deploy -g sacura-deploy /home/sacura-deploy/.ssh
authorized=/home/sacura-deploy/.ssh/authorized_keys
touch "$authorized"
while IFS= read -r key; do
 [[ "$key" == ssh-ed25519\ * ]] || { echo 'Expected an Ed25519 public key' >&2; exit 1; }
 grep -Fq -- "$key" "$authorized" || printf 'restrict %s\n' "$key" >> "$authorized"
done < "$key_file"
chown sacura-deploy:sacura-deploy "$authorized"
chmod 600 "$authorized"
if [[ ! -e "$root/shared/.env" ]]; then
 umask 077
 secret=$(openssl rand -hex 32)
 printf 'S3_ACCESS_KEY=sacura-staging\nS3_SECRET_KEY=%s\nS3_BUCKET=sacura-meshes\nS3_REGION=us-east-1\nS3_ENDPOINT=http://minio:9000\nS3_FORCE_PATH_STYLE=true\nS3_CREATE_BUCKET=true\nDOMAIN=sacura-test.duckdns.org\nACME_EMAIL=%s\n' "$secret" "$email" > "$root/shared/.env"
 chown sacura-deploy:sacura-deploy "$root/shared/.env"
fi
echo 'Staging directories, deployment user, SSH key and shared environment prepared.'
