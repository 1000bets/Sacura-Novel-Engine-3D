-- Applied idempotently by createAuth, alongside the existing startup schema migrations.
CREATE TABLE IF NOT EXISTS invites (
 id TEXT PRIMARY KEY,
 token_hash TEXT NOT NULL UNIQUE,
 created_by_user_id TEXT NOT NULL REFERENCES users(id),
 created_at INTEGER NOT NULL,
 expires_at INTEGER NOT NULL,
 used_at INTEGER,
 used_by_user_id TEXT REFERENCES users(id)
);
CREATE TABLE IF NOT EXISTS auth_rate_limits (
 key TEXT PRIMARY KEY,
 attempts INTEGER NOT NULL,
 expires_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS auth_rate_limits_expiry ON auth_rate_limits(expires_at);
