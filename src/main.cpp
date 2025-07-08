#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

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

enum class Endianness
{
    LittleEndian,
    BigEndian
};

enum WaveFormat {
    PCM = 0x0001,
    IEEE_FLOAT = 0x0003,
    ALAW = 0x0006,
    MULAW = 0x0007,
    EXTENSIBLE = 0xFFFE
};

int32_t fourBytesToInt(const std::vector<std::uint8_t> &data, int startIdx, Endianness endianness)
{
    if (data.size() >= startIdx + 4) {
        int32_t res;

        if (endianness == Endianness::LittleEndian)
            res = (data[startIdx + 3] << 24) |
                  (data[startIdx + 2] << 16) |
                  (data[startIdx + 1] << 8) |
                  data[startIdx];    
        else
            res = (data[startIdx] << 24) |
                  (data[startIdx + 1] << 16) |
                  (data[startIdx + 2] << 8) |
                  data[startIdx + 3];

        return res;
    } else {
        std::cerr << "The data is just not structured correctly!\n";
        return 0;
    }
}

int16_t twoBytesToInt(const std::vector<std::uint8_t> &data, int startIdx, Endianness endianness) {
    int16_t res;

    if (endianness == Endianness::LittleEndian)
        res = (data[startIdx + 1] << 8) | (data[startIdx]);
    else
        res = (data[startIdx] << 8) | (data[startIdx + 1]);

    return res;
}

int idxOfChunk(const std::vector<std::uint8_t> &data, const std::string &ckID, int startIdx, Endianness endianness)
{
    const int reqLength = 4;

    if (ckID.size() != reqLength) {
        std::cerr << "The chunk ID you passed is invalid bruh.\n";
        return -1;
    }

    int idx = startIdx;
    while (idx < data.size() - reqLength) {
        if (std::memcmp(&data[idx], ckID.data(), reqLength) == 0) {
            return idx;
        }

        idx += reqLength;

        if ((idx + 4) >= data.size()) {
            std::cerr << "It seems like we ran out of data to read.\n";
            return -1;
        }

        // skip a whole chunk to the next chunk
        int32_t ckSize = fourBytesToInt(data, idx, endianness);
        if (ckSize > (data.size() - idx - reqLength) || (ckSize < 0)) {
            std::cerr << "The chunk size is somehow invalid.\n";
            return -1;
        }

        idx += (reqLength + ckSize);
    }

    return -1;
}

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

    // Header chunk
    std::string headerChunkID (fileData.begin(), fileData.begin() + 4);
    std::int32_t headerChunkSize = fourBytesToInt(fileData, 4, Endianness::LittleEndian);
    std::string fileTypeHeader (fileData.begin() + 8, fileData.begin() + 12);

    // std::cout << "headerChunkID: " << headerChunkID << ", headerChunkSize: " << headerChunkSize << ", fileTypeHeader: " << fileTypeHeader << "\n";

    int idxOfFmtChunk = idxOfChunk(fileData, "fmt ", 12, Endianness::LittleEndian);
    int idxOfDataChunk = idxOfChunk(fileData, "data", 12, Endianness::LittleEndian);

    // std::cout << "index of fmt chunk: " << idxOfFmtChunk << "\n";
    // std::cout << "index of data chunk: " << idxOfDataChunk << "\n";

    // Check if required value for this to be a wave file are there
    if (idxOfFmtChunk == -1 || idxOfDataChunk == -1 || headerChunkID != "RIFF" || fileTypeHeader != "WAVE") {
        std::cerr << "This is not a valid .WAV file.\n";
        return;
    }

    
    // Format Chunk

    std::string fmtCkID (fileData.begin() + idxOfFmtChunk, fileData.begin() + idxOfFmtChunk + 4);
    std::int32_t fmtCkSize = fourBytesToInt(fileData, idxOfFmtChunk + 4, Endianness::LittleEndian);
    std::uint16_t fmtTag = twoBytesToInt(fileData, idxOfFmtChunk + 8, Endianness::LittleEndian);
    std::uint16_t nChannels = twoBytesToInt(fileData, idxOfFmtChunk + 10, Endianness::LittleEndian);
    std::uint32_t nSamplesPerSec = fourBytesToInt(fileData, idxOfFmtChunk + 12, Endianness::LittleEndian);
    std::uint32_t nAvgBytesPerSec = fourBytesToInt(fileData, idxOfFmtChunk + 16, Endianness::LittleEndian);
    std::uint16_t nBlockAlign = twoBytesToInt(fileData, idxOfFmtChunk + 20, Endianness::LittleEndian);
    std::uint16_t bitsPerSample = twoBytesToInt(fileData, idxOfFmtChunk + 22, Endianness::LittleEndian);

    /*
    std::cout << "fmtCkID: " << fmtCkID << ", fmtCkSize: " << fmtCkSize << ", fmtTag: " << fmtTag << ", nChannels: "
    << nChannels << ", nSamplePerSec: " << nSamplesPerSec << ", nAvgBytesPerSec: " << nAvgBytesPerSec << ", nBlockAlign: "
    << nBlockAlign << ", bitsPerSample: " << bitsPerSample << "\n";*/

    // Checks to verify the consistency of the header chunk
    
    if (fmtTag != WaveFormat::PCM &&
        fmtTag != WaveFormat::IEEE_FLOAT &&
        fmtTag != WaveFormat::ALAW &&
        fmtTag != WaveFormat::MULAW &&
        fmtTag != WaveFormat::EXTENSIBLE
    ) {
        std::cerr << "That wave format is not supported or not a wave format at all.\n";
        return;
    }

    if (nChannels < 1 || nChannels > 128) {
        std::cerr << "Number of channels is just too high or too low, who knows.\n";
        return;
    }

    if (nAvgBytesPerSec != static_cast<std::uint32_t>((nChannels * nSamplesPerSec * bitsPerSample) / 8)) {
        std::cerr << "The calculation is not mathing bruh, it ain't right.\n";
        return;    
    }

    if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) {
        std::cerr << "Somehow this bit depth is not valid, HOW!?\n";
        return;
    }
}


int main(int argc, char** argv) {

    std::string fileName { "M1F1-Alaw-AFsp.wav" };
    processWaveFile(fileName);
    
    return 0;
}
