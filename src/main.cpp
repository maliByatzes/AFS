#include <afsproject/afs.h>
#include <afsproject/audio_engine.h>
#include <afsproject/audio_file.h>
#include <afsproject/db.h>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sqlite3.h>
#include <string>

using namespace afs;
namespace fs = std::filesystem;

long long storeSongMetadata(IAudioFile &audio_file, SQLiteDB &db, const std::string &filepath)// NOLINT
{
  const std::string sql =
    "INSERT INTO songs (title, artist, album, genre, release_date, duration_seconds, file_path, sample_rate_hz, "
    "bitrate_kbps) VALUES (?, ?, ?, ?, "
    "?, ?, ?, ?, ?);";
  long long song_id = -1;

  try {
    db.execute("BEGIN;");

    SQLiteDB::Statement stmt(db, sql);

    auto metadata = audio_file.getMetadata();

    std::cout << "Metadata in storeSongMetadata:\n"
              << " Title: " << metadata.title << "\n"
              << " Artist: " << metadata.artist << "\n"
              << " Album: " << metadata.album << "\n"
              << " Date: " << metadata.date << "\n"
              << " Genre: " << metadata.genre << "\n";

    const std::filesystem::path path(filepath);

    stmt.bindText(1, metadata.title);
    stmt.bindText(2, metadata.artist);
    stmt.bindText(3, metadata.album);
    stmt.bindText(4, metadata.genre);
    stmt.bindText(5, metadata.date);
    stmt.bindText(6, std::to_string(audio_file.getDurationSeconds()));
    stmt.bindText(7, filepath);
    stmt.bindText(8, std::to_string(audio_file.getSampleRate()));// NOLINT
    stmt.bindText(9, std::to_string(audio_file.getBitDepth()));// NOLINT


    stmt.step();

    song_id = sqlite3_last_insert_rowid(db.get());

    db.execute("COMMIT;");
    std::cout << "Successfully inserted song: " << path.filename() << " by mah balls.\n";
  } catch (const SQLiteException &e) {
    db.execute("ROLLBACK;");
    std::cerr << "Failed to insert song. Rolled back transaction.\n" << e.what() << "\n";
  }

  return song_id;
}

void processAudioFile(const std::string &filepath)
{
  const AudioEngine engine;
  const std::unique_ptr<IAudioFile> audio_file = engine.loadAudioFile(filepath);

  if (!audio_file) {
    std::cerr << "Failed to load audio file: " << filepath << "\n";
    return;
  }

  try {
    SQLiteDB my_db("afs.db");

    if (afs::run(my_db, "db/migration")) { std::cout << "Database migration completed successfuly.\n"; }

    std::cout << "Processing: " << filepath << " ...\n";
    // 1. Store the song metadata
    const long long song_id = storeSongMetadata(*audio_file, my_db, filepath);
    // 2. Store fingerprints of said song
    AFS::storingFingerprints(*audio_file, song_id, my_db);
    std::cout << "Fingerprints stored successfully.\n";
  } catch (const std::exception &e) {
    std::cerr << "An unrecoverable error occurred: " << e.what() << "\n";
    return;
  }
}

void searchAudioFile(const std::string &file)
{
  const AudioEngine engine;
  const std::unique_ptr<IAudioFile> audio = engine.loadAudioFile(file);

  if (!audio) {
    std::cerr << "Failed to load audio file: " << file << "\n";
    return;
  }

  try {
    SQLiteDB my_db("afs.db");

    std::cout << "Searching for " << file << "...\n";
    AFS::searchForRecord(*audio, my_db);
  } catch (const std::exception &e) {
    std::cerr << "An unrecoverable error occurred: " << e.what() << "\n";
    return;
  }
}

void printHelp()
{
  // TODO: Format this better by NOT using spaces like this
  std::cout << "Usage: [options]\n";
  std::cout << "Options:\n";
  std::cout << "  --help                       Display this information.\n";
  std::cout << "  --version                    Display tool version.\n";
  std::cout << "  --populate <directory_path>  Process audio files in directory_path.\n";
  std::cout << "  --server                     Run a server.\n";
  std::cout << "  --search <file>              Search for the audio file.\n";
}

void printVersion() { std::cout << "AFS v0.0.1\n"; }

void runServerMode()
{
  std::cout << "Starting server mode...\n";
  // TODO
  std::cout << "Server listening on http://localhost:8080\n";
}

void runCLIMode(const std::string &directory_path)
{
  std::cout << "Starting CLI database population mode...\n";
  const fs::path dir_path(directory_path);

  if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
    std::cerr << "Error: Directory not found or is not a directory.\n";
    return;
  }

  for (const auto &entry : fs::directory_iterator(dir_path)) {
    if (entry.is_regular_file()) { processAudioFile(entry.path().string()); }
  }

  std::cout << "CLI mode finished. All supported files processed.\n";
}

// The main entry point now checks for command-line arguments:
// To run in CLI mode:
// ./AFS --populate /path/to/audio_files
// To run in server mode:
// ./AFS --server
int main(int argc, char *argv[])
{
  if (argc < 2) {
    printHelp();
    return 1;
  }

  std::string command = argv[1];// NOLINT
  if (command == "--help") {
    printHelp();
  } else if (command == "--version") {
    printVersion();
  } else if (command == "--populate") {
    if (argc <= 2) {
      std::cerr << "Missing path for audio file.\n";
      return 1;
    }
    runCLIMode(argv[2]);// NOLINT
  } else if (command == "--server") {
    runServerMode();
  } else if (command == "--search") {
    searchAudioFile(argv[2]);// NOLINT
  } else {
    std::cerr << "Unknown command: " << command << "\n";
    printHelp();
    return 1;
  }

  return 0;
}
