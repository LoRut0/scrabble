PRAGMA foreign_keys = ON;

/* =======================
   Пользователи и доступ
   ======================= */

CREATE TABLE users (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    username        TEXT NOT NULL UNIQUE,
    display_name    TEXT,
    email           TEXT NOT NULL UNIQUE,
    email_verified_at DATETIME,
    is_active       INTEGER NOT NULL DEFAULT 1,
    avatar_url      TEXT,
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

--TODO: failed_login_count add
-- Хеши паролей и настройки MFA
CREATE TABLE user_credentials (
    user_id             INTEGER PRIMARY KEY,
    password_hash       BLOB NOT NULL,
    last_login_at       DATETIME,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Управляемые сессии/refresh-токены (revocation, logout, device list)
CREATE TABLE auth_tokens (
    jti         TEXT PRIMARY KEY,
    user_id     INTEGER NOT NULL,
    created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at  DATETIME NOT NULL,
    revoked_at  DATETIME,
    user_agent  TEXT,
    ip_hash     BLOB,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE INDEX idx_auth_tokens_user
    ON auth_tokens(user_id);

CREATE INDEX idx_auth_tokens_expires
    ON auth_tokens(expires_at);

/* =======================
   Игровые сущности
   ======================= */

-- Партия (метаданные). Текущее «живое» состояние — в Redis.
CREATE TABLE games (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    host_user_id INTEGER,
    FOREIGN KEY (host_user_id) REFERENCES users(id)
    --TODO: add ongoing bool column
    --TODO: add max_players in game
    --TODO: add lang column
);

CREATE TABLE game_users (
    game_id INTEGER NOT NULL,
    user_id INTEGER NOT NULL,
    PRIMARY KEY (game_id, user_id),
    FOREIGN KEY (game_id) REFERENCES games(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

/* =======================
   Статистика и рейтинги
   ======================= */

-- Агрегированная статистика пользователя
CREATE TABLE user_stats (
    user_id           INTEGER PRIMARY KEY,
    games_played      INTEGER NOT NULL DEFAULT 0,
    wins              INTEGER NOT NULL DEFAULT 0,
    total_points      INTEGER NOT NULL DEFAULT 0,
    avg_points        REAL NOT NULL DEFAULT 0.0,
    best_word_score   INTEGER NOT NULL DEFAULT 0,
    longest_word_len  INTEGER NOT NULL DEFAULT 0,
    last_game_at      DATETIME,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- История результатов по партиям (для отчётов/таблиц лидеров)
CREATE TABLE game_results (
    game_id   INTEGER NOT NULL,
    user_id   INTEGER NOT NULL,
    place     INTEGER NOT NULL,
    score     INTEGER NOT NULL,
    PRIMARY KEY (game_id, user_id),
    FOREIGN KEY (game_id) REFERENCES games(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE INDEX idx_results_user
    ON game_results(user_id);
