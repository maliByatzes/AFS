#ifndef audio_file_h_
#define audio_file_h_

#include <string>
#include <vector>

namespace asf {

class IAudioFile//NOLINT
{
public:
  virtual ~IAudioFile() = default;

  virtual bool load(const std::string &) = 0;
  [[nodiscard]] virtual std::vector<double> getPCMData() const = 0;
  [[nodiscard]] virtual int getSampleRate() const = 0;
  [[nodiscard]] virtual int getNumChannels() const = 0;
  [[nodiscard]] virtual bool isMono() const = 0;
  [[nodiscard]] virtual bool isStero() const = 0;
  [[nodiscard]] virtual int getBitDepth() const = 0;
  [[nodiscard]] virtual int getNumSamplesPerChannel() const = 0;
  [[nodiscard]] virtual double getDurationSeconds() const = 0;

protected:
  std::string m_file_path;
  std::vector<double> m_pcm_data;
  int m_sample_rate;
  int m_num_channels;
  double m_duration_seconds;
  int m_bit_depth;
  int m_format_tag;
};

}// namespace asf

#endif
