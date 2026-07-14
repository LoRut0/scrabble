import os
import sqlite3
import pathlib
import pytest

pytest_plugins = ['pytest_userver.plugins.core']

DB_PATH = pathlib.Path('/workspace/data/sql/key-json.db')
SCHEMA_PATH = pathlib.Path(__file__).parent.parent / 'sqlite' / 'users_1.sql'


@pytest.fixture(scope='session', autouse=True)
def init_db():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    if DB_PATH.exists():
        DB_PATH.unlink()
    conn = sqlite3.connect(DB_PATH)
    conn.executescript(SCHEMA_PATH.read_text())
    conn.commit()
    conn.close()
