#include "afsproject/db.h"
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <afsproject/afs.h>
#include <afsproject/audio_file.h>
#include <afsproject/fft.h>
#include <afsproject/spectrum.h>
#include <afsproject/wave.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace afs {

void AFS::stereoToMono(IAudioFile &audio_file)
{
  // Compute simple averaging to chnage from stereo to mon
  // M(t) = (L(t) + R(t)) / 2

  if (audio_file.isStereo()) {
    std::vector<double> pcm_data = audio_file.getPCMData();
    std::vector<double> avg_pcm_data{};

    for (size_t i = 0; i < pcm_data.size() - 1; i += 2) {
      avg_pcm_data.push_back((pcm_data.at(i) + pcm_data.at(i + 1)) / 2);
    }

    audio_file.setPCMData(avg_pcm_data, audio_file.getSampleRate(), 1);
  }
}

void AFS::normalizePCMData(IAudioFile &audio_file)
{
  auto pcm_data = audio_file.getPCMData();

  if (pcm_data.empty()) { return; }

  double max_sample = 0.0;
  std::ranges::for_each(pcm_data, [&max_sample](double sample) {
    auto abs_sample = std::abs(sample);
    max_sample = std::fmax(max_sample, abs_sample);
  });

  std::vector<double> normalized_pcm_data(pcm_data.size());

  if (max_sample == 0.0) {
    std::ranges::for_each(normalized_pcm_data, [](double &sample) { sample = 0.0; });
  } else {
    for (size_t i = 0; i < pcm_data.size(); ++i) { normalized_pcm_data[i] = pcm_data[i] / max_sample; }
  }

  audio_file.setPCMData(normalized_pcm_data, audio_file.getSampleRate(), audio_file.getNumChannels());
}

void AFS::applyLowPassFilter(IAudioFile &audio_file)
{
  std::vector<double> pcm_data = audio_file.getPCMData();

  const nc::NdArray<double> ys(pcm_data.begin(), pcm_data.end());
  Wave wave(ys, int(audio_file.getSampleRate()));

  Spectrum spectrum = wave.makeSpectrum();
  spectrum.lowPass(5000);// NOLINT

  const Wave filtered = spectrum.makeWave();
  const std::vector<double> pcm_data2 = filtered.getYs().toStlVector();

  audio_file.setPCMData(pcm_data2, audio_file.getSampleRate(), audio_file.getNumChannels());
}

void AFS::downSampling(IAudioFile &audio_file)
{
  // Downsample for sample rate @ 44100 Hz

  if (audio_file.getSampleRate() == 44100) {// NOLINT
    std::vector<double> pcm_data = audio_file.getPCMData();
    std::vector<double> new_pcm_data;

    for (size_t i = 0; i < pcm_data.size(); i += 4) { new_pcm_data.push_back(pcm_data[i]); }

    audio_file.setPCMData(new_pcm_data, audio_file.getSampleRate() / 4, audio_file.getNumChannels());
  }
}

void AFS::storingFingerprints(IAudioFile &audio_file, long long song_id, SQLiteDB &db)// NOLINT
{
  Matrix matrix{ shortTimeFourierTransform(audio_file) };
  filtering(matrix);
  const Fingerprint fingerprints{ generateFingerprints(matrix, song_id) };

  const std::string insert_fingerprint_sql = "INSERT INTO fingerprints (id, song_id) VALUES (?, ?);";
  const std::string insert_data_sql = "INSERT INTO fingerprint_data (fingerprint_id, value) VALUES (?, ?);";

  try {
    db.execute("BEGIN;");

    SQLiteDB::Statement fg_stmt(db, insert_fingerprint_sql);
    SQLiteDB::Statement data_stmt(db, insert_data_sql);

    for (const auto &pair : fingerprints) {
      const unsigned int fg_id = pair.first;
      const auto &values = pair.second;

      fg_stmt.bindInt(1, static_cast<int>(fg_id));
      fg_stmt.bindLongLong(2, song_id);
      fg_stmt.step();
      fg_stmt.reset();

      for (const unsigned long value : values) {
        data_stmt.bindInt(1, static_cast<int>(fg_id));
        data_stmt.bindLongLong(2, static_cast<long long>(value));
        data_stmt.step();
        data_stmt.reset();
      }
    }

    db.execute("COMMIT;");
    std::cout << "Successfully inserted fingerprints for song ID: " << song_id << "\n";
  } catch (const SQLiteException &e) {
    db.execute("ROLLBACK;");
    std::cerr << "Failed to insert fingerprints. Rolled back transaction.\n" << e.what() << "\n";
  }
}

Matrix AFS::shortTimeFourierTransform(IAudioFile &audio_file)
{
  stereoToMono(audio_file);
  applyLowPassFilter(audio_file);
  downSampling(audio_file);

  std::vector<double> pcm_data = audio_file.getPCMData();

  // 1. Define the Hamming window for all calculations
  const int sample_window = 1024;
  std::vector<double> window(sample_window);

  for (size_t i = 0; i < window.size(); ++i) {
    double ans = 0.54 - (0.46 * std::cos((2.0 * M_PI * static_cast<double>(i)) / (sample_window - 1)));// NOLINT
    window[i] = ans;
  }

  // 2. Slide the window and perform calculations, apply FFT on each data block
  const int half_window = sample_window / 2;

  int end_of_last_win = 0;
  for (size_t x = 0; x < pcm_data.size(); x += half_window) {// NOLINT
    if (x + sample_window > pcm_data.size()) {
      end_of_last_win = int(x) + (sample_window - 1);
      break;
    }
  }

  const int num_of_zeros = end_of_last_win - (int(pcm_data.size()) - 1);

  std::vector<double> zeros_arr(static_cast<size_t>(num_of_zeros));
  pcm_data.append_range(zeros_arr);

  Matrix matrix{};

  for (size_t i = 0; i < pcm_data.size(); i += half_window) {
    const std::vector<double> data_block(pcm_data.begin() + int(i), pcm_data.begin() + int(i) + sample_window);
    std::vector<double> ys(sample_window);

    for (size_t j = 0; j < window.size(); ++j) { ys[j] = data_block[j] * window[j]; }

    const VecComplexDoub hs_vec = FFT::convertToFrequencyDomain(ys);
    std::vector<std::pair<int, double>> magnitudes(hs_vec.size());
    for (size_t x = 0; x < hs_vec.size(); ++x) { magnitudes[x] = std::make_pair(int(x), std::abs(hs_vec[x])); }// NOLINT
    matrix.push_back(magnitudes);
  }

  return matrix;
}

void AFS::filtering(Matrix &matrix)
{
  Matrix filtered_matrix;

  for (auto &bins : matrix) {
    // 1. Divide the bins int logarithmic bands
    // NOLINTBEGIN
    std::vector<std::pair<int, double>> very_low_sound_bins(bins.begin(), bins.begin() + 10);
    std::vector<std::pair<int, double>> low_sound_bins(bins.begin() + 10, bins.begin() + 20);
    std::vector<std::pair<int, double>> low_mid_sound_bins(bins.begin() + 20, bins.begin() + 40);
    std::vector<std::pair<int, double>> mid_sound_bins(bins.begin() + 40, bins.begin() + 80);
    std::vector<std::pair<int, double>> mid_high_sound_bins(bins.begin() + 80, bins.begin() + 160);
    std::vector<std::pair<int, double>> high_sound_bins(bins.begin() + 160, bins.begin() + 513);
    // NOLINTEND

    // 2. Keep the strongest bin in each band
    std::vector<std::pair<int, double>> strongest_bins;
    auto cmp_fn = [](const std::pair<int, double> &a, const std::pair<int, double> &b) {// NOLINT
      return a.second < b.second;
    };
    strongest_bins.push_back(*std::ranges::max_element(very_low_sound_bins, cmp_fn));
    strongest_bins.push_back(*std::ranges::max_element(low_sound_bins, cmp_fn));
    strongest_bins.push_back(*std::ranges::max_element(low_mid_sound_bins, cmp_fn));
    strongest_bins.push_back(*std::ranges::max_element(mid_sound_bins, cmp_fn));
    strongest_bins.push_back(*std::ranges::max_element(mid_high_sound_bins, cmp_fn));
    strongest_bins.push_back(*std::ranges::max_element(high_sound_bins, cmp_fn));

    // 3. Compute the average of these 6 powerful bins
    double sum = 0.0;
    for (const auto &bin : strongest_bins) { sum += bin.second; }
    const double average = sum / 6;// NOLINT

    // 4. Keep the bins that are above the mean
    const double coeff = 1.2;
    const double threshold = average * coeff;
    std::vector<std::pair<int, double>> filtered_strongest_bins;

    for (const auto &val : strongest_bins) {
      if (val.second > threshold) { filtered_strongest_bins.push_back(val); }
    }

    filtered_matrix.push_back(filtered_strongest_bins);
  }

  matrix.swap(filtered_matrix);
}

Fingerprint AFS::generateFingerprints(Matrix &matrix, long long rsong_id)
{
  // tuple -> index, time, bin
  std::vector<std::tuple<int, int, int>> points;

  int index = 0;
  for (size_t i = 0; i < matrix.size(); ++i) {
    int current_time = static_cast<int>(double(i) * TIME_STEP * 1000);// NOLINT
    std::ranges::for_each(matrix[i], [&](std::pair<int, double> &bin) {
      points.emplace_back(index, current_time, bin.first);
      index++;
    });
  }

  // Fingerprint database blueprint
  std::unordered_map<uint32_t, std::vector<uint64_t>> fingerprint_table;

  for (size_t idx = 0; idx < points.size(); ++idx) {
    if (idx + 3 >= points.size()) { break; }
    if (idx + 3 + 4 >= points.size()) { break; }

    auto anchor_point = points[idx];
    const size_t starting_freq_idx = idx + 3;
    const std::vector<std::tuple<int, int, int>> target_zone(
      points.begin() + int(starting_freq_idx), points.begin() + int(starting_freq_idx) + 5);// NOLINT

    for (const auto &point : target_zone) {
      int delta_time = std::get<1>(point) - std::get<1>(anchor_point);// NOLINT

      // Packing into one value
      uint16_t freq_anchor = std::get<2>(anchor_point) & NINE_BITS_MASK;// NOLINT
      uint16_t freq_point = std::get<2>(point) & NINE_BITS_MASK;// NOLINT
      uint16_t delta_bits = delta_time & FOURTEEN_BITS_MASK;// NOLINT

      uint32_t address = 0;
      address |= freq_anchor;
      address |= (static_cast<uint32_t>(freq_point) << 9);// NOLINT
      address |= (static_cast<uint32_t>(delta_bits) << 18);// NOLINT

      uint32_t time_of_anchor = static_cast<uint32_t>(std::get<1>(anchor_point)) & THIRTY_TWO_BITS_MASK;// NOLINT
      // TODO: Change this >.<
      uint32_t song_id = static_cast<uint64_t>(rsong_id) & THIRTY_TWO_BITS_MASK;// NOLINT

      uint64_t couple = 0;
      couple |= time_of_anchor;
      couple |= (static_cast<uint64_t>(song_id) << 32);// NOLINT

      if (fingerprint_table.contains(address)) {
        fingerprint_table[address].push_back(couple);
      } else {
        fingerprint_table[address] = { couple };
      }
    }
  }

  return fingerprint_table;
}

}// namespace afs
