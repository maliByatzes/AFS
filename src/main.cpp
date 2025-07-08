#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Processing a WAVE file into memory

/*
struct WaveFile
{
    std::string chunkID {}; // 1-4
    std::uint32_t chunkSize {}; // 5-8
    std::string fileTypeHeader {}; // 9-12

    std::string fmtCkID {}; // 13-16
    std::uint32_t fmtCkSize {}; // 17-20
    std::uint16_t fmtTag {}; // 21-22
    std::uint16_t nChannels {}; // 23-24
    std::uint32_t nSamplesPerSec {}; // 25-28
    std::uint32_t nAvgBytesPerSec {}; // 29-32
    std::uint16_t nBlockAlign {}; // 33-34
    std::uint16_t bitsPerSample {}; // 35-36

    std::string dataCkID {}; // 37-40
    std::uint32_t dataCkSize {}; // 41-44
};*/


// One big dirty function for now

void processWaveFile(const std::string &fileName)
{

    std::ifstream file (fileName, std::ios_base::binary);

    if (!file.good()) {
        std::cerr << "Opening that file failed badly or it doesn't exist: " << fileName << "\n";
        return;
    }

    std::vector<std::uint8_t> fileData {};
    
    file.unsetf(std::ios::skipws);

    file.seekg(0, std::ios::end);
    std::size_t fileLength = file.tellg();
    file.seekg(0, std::ios::beg);

    fileData.resize(fileLength);

    file.read(reinterpret_cast<char*>(fileData.data()), fileLength);
    file.close();

    if (file.gcount() != fileLength) {
        std::cerr << "Uh oh that didn't read the entire file.\n";
        return;
    }

    // file data size must be at least 12
    if (fileData.size() < 12) {
        std::cerr << "The file data is just too short mate.\n";
        return;
    }

    std::cout << "fileData:\n";
    for (const auto& v : fileData)
        std::cout << v << " ";
    std::cout << "\n"; 
}


int main(int argc, char** argv) {

    std::string fileName { "M1F1-Alaw-AFsp.wav" };
    processWaveFile(fileName);
    
    return 0;
}
