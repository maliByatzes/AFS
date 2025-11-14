CREATE TABLE IF NOT EXISTS fingerprints (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    hash        INTEGER NOT NULL,
    song_id     INTEGER NOT NULL,
    time_offset INTEGER NOT NULL,
    FOREIGN KEY(song_id) REFERENCES songs(id)
);

CREATE INDEX IF NOT EXISTS idx_hash ON fingerprints(hash);
CREATE INDEX IF NOT EXISTS idx_song ON fingerprints(song_id);
