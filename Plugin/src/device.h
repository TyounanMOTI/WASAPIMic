#pragma once
#include <wrl/client.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <thread>
#include <memory>
#include <atomic>
#include <deque>

namespace wasapi_mic {

class device
{
public:
  device();
  ~device();

  void start();
  void stop();
  void run();

private:
  enum class status
  {
    stopped,
    running,
  };

  Microsoft::WRL::ComPtr<IMMDevice> input_device;
  Microsoft::WRL::ComPtr<IMMDevice> output_device;
  Microsoft::WRL::ComPtr<IAudioClient> input_client;
  Microsoft::WRL::ComPtr<IAudioClient> output_client;
  Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_client;
  Microsoft::WRL::ComPtr<IAudioRenderClient> render_client;
  UINT32 num_capture_buffer_frames;
  UINT32 num_rendering_buffer_frames;

  std::unique_ptr<std::thread> thread;
  std::atomic<status> current_status;
  std::deque<int16_t> recording_buffer;
};

}
