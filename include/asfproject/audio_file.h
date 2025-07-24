#ifndef audio_file_h_
#define audio_file_h_

#include <cstdint>
#include <string>
#include <vector>

namespace asf {

class IAudioFile//NOLINT
{
public:
  virtual ~IAudioFile() = default;

  virtual bool load(const std::string &) = 0;
  [[nodiscard]] virtual bool save(const std::string &) const = 0;
  [[nodiscard]] virtual std::vector<double> getPCMData() const = 0;
  [[nodiscard]] virtual int32_t getSampleRate() const = 0;
  [[nodiscard]] virtual int16_t getNumChannels() const = 0;
  [[nodiscard]] virtual bool isMono() const = 0;
  [[nodiscard]] virtual bool isStero() const = 0;
  [[nodiscard]] virtual int16_t getBitDepth() const = 0;
  [[nodiscard]] virtual int getNumSamplesPerChannel() const = 0;
  [[nodiscard]] virtual double getDurationSeconds() const = 0;

protected:
  std::string m_file_path;
  std::vector<double> m_pcm_data;
  int32_t m_sample_rate;
  int16_t m_num_channels;
  double m_duration_seconds;
  int16_t m_bit_depth;
  int16_t m_format_tag;
};

}// namespace asf

#endif
