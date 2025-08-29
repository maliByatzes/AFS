#include <afsproject/afs.h>
#include <afsproject/audio_engine.h>
#include <afsproject/audio_file.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

using namespace afs;
namespace fs = std::filesystem;

void processAudioFile(const std::string &filepath)
{
  const AudioEngine engine;
  const std::unique_ptr<IAudioFile> audio_file = engine.loadAudioFile(filepath);

  if (!audio_file) {
    std::cerr << "Failed to load audio file: " << filepath << "\n";
    return;
  }

  std::cout << "Processing: " << filepath << " ...\n";
  AFS::storingFingerprints(*audio_file);
  std::cout << "Fingerprints stored successfully.\n";
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
  } else {
    std::cerr << "Unknown command: " << command << "\n";
    printHelp();
    return 1;
  }

  return 0;
}
