CREATE TABLE IF NOT EXISTS fingerprints (
    id        INTEGER PRIMARY KEY,
    song_id   INTEGER NOT NULL,
    FOREIGN KEY(song_id) REFERENCES songs(id)
);

CREATE TABLE IF NOT EXISTS fingerprint_data (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    fingerprint_id   INTEGER NOT NULL,
    value            INTEGER NOT NULL,
    FOREIGN KEY(fingerprint_id) REFERENCES fingerprints(id)
);
