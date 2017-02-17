#include "device.h"
#include "runtime_error.h"
#include <avrt.h>
#include <chrono>
#include <iterator>

using namespace Microsoft::WRL;

namespace wasapi_mic {

device::device()
  : current_status(status::stopped)
{
  ComPtr<IMMDeviceEnumerator> enumerator;
  auto hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    nullptr,
    CLSCTX_ALL,
    IID_PPV_ARGS(&enumerator)
  );
  if (FAILED(hr)) {
    throw runtime_error(L"Device Enumeratorの初期化に失敗しました。");
  }

  hr = enumerator->GetDefaultAudioEndpoint(
    eCapture,
    eConsole,
    &input_device
  );
  if (FAILED(hr)) {
    throw runtime_error(L"既定の録音デバイスの取得に失敗しました。");
  }

  hr = input_device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    &input_client
  );
  if (FAILED(hr)) {
    throw runtime_error(L"録音用Audio Clientの初期化に失敗しました。");
  }

  REFERENCE_TIME capture_device_period;
  hr = input_client->GetDevicePeriod(
    nullptr,
    &capture_device_period
  );
  if (FAILED(hr)) {
    throw runtime_error(L"録音デバイスの処理周期の取得に失敗しました。");
  }

  WAVEFORMATEX capture_format;
  capture_format.wFormatTag = WAVE_FORMAT_PCM;
  capture_format.nChannels = 1;
  capture_format.nSamplesPerSec = 48000;
  capture_format.wBitsPerSample = 16;
  capture_format.nBlockAlign = capture_format.wBitsPerSample / 8 * capture_format.nChannels;
  capture_format.nAvgBytesPerSec = capture_format.nSamplesPerSec * capture_format.nBlockAlign;
  capture_format.cbSize = 0;

  hr = input_client->IsFormatSupported(
    AUDCLNT_SHAREMODE_EXCLUSIVE,
    &capture_format,
    nullptr
  );
  if (FAILED(hr)) {
    throw runtime_error(L"録音デバイスがサポートしていないフォーマットです。");
  }

  hr = input_client->Initialize(
    AUDCLNT_SHAREMODE_EXCLUSIVE,
    0,
    capture_device_period,
    capture_device_period,
    &capture_format,
    nullptr
  );
  if (FAILED(hr)) {
    throw runtime_error(L"録音デバイスの排他モードでの初期化に失敗しました。");
  }

  hr = input_client->GetBufferSize(&num_capture_buffer_frames);
  if (FAILED(hr)) {
    throw runtime_error(L"録音バッファサイズの取得に失敗しました。");
  }

  hr = input_client->GetService(
    IID_PPV_ARGS(&capture_client)
  );
  if (FAILED(hr)) {
    throw runtime_error(L"Capture Clientの取得に失敗しました。");
  }

  hr = enumerator->GetDefaultAudioEndpoint(
    eRender,
    eConsole,
    &output_device
  );
  if (FAILED(hr)) {
    throw runtime_error(L"既定の再生デバイスの取得に失敗しました。");
  }

  hr = output_device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    &output_client
  );
  if (FAILED(hr)) {
    throw runtime_error(L"再生用 Audio Client の初期化に失敗しました。");
  }

  REFERENCE_TIME rendering_device_period;
  hr = output_client->GetDevicePeriod(nullptr, &rendering_device_period);
  if (FAILED(hr)) {
    throw runtime_error(L"再生デバイスの更新周期の取得に失敗しました。");
  }

  WAVEFORMATEXTENSIBLE rendering_format;
  rendering_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  rendering_format.Format.nChannels = 2;
  rendering_format.Format.nSamplesPerSec = 48000;
  rendering_format.Format.wBitsPerSample = 16;
  rendering_format.Format.nBlockAlign = rendering_format.Format.wBitsPerSample / 8 * rendering_format.Format.nChannels;
  rendering_format.Format.nAvgBytesPerSec = rendering_format.Format.nSamplesPerSec * rendering_format.Format.nBlockAlign;
  rendering_format.Format.cbSize = 22;
  rendering_format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
  rendering_format.Samples.wReserved = 0;
  rendering_format.Samples.wSamplesPerBlock = 0;
  rendering_format.Samples.wValidBitsPerSample = rendering_format.Format.wBitsPerSample;
  rendering_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  hr = output_client->IsFormatSupported(
    AUDCLNT_SHAREMODE_EXCLUSIVE,
    reinterpret_cast<WAVEFORMATEX*>(&rendering_format),
    nullptr
  );
  if (FAILED(hr)) {
    throw runtime_error(L"再生デバイスがサポートしていないフォーマットです。");
  }

  hr = output_client->Initialize(
    AUDCLNT_SHAREMODE_EXCLUSIVE,
    0,
    rendering_device_period,
    rendering_device_period,
    reinterpret_cast<WAVEFORMATEX*>(&rendering_format),
    nullptr
  );
  if (FAILED(hr)) {
    throw runtime_error(L"再生デバイスの排他モードでの初期化に失敗しました。");
  }

  hr = output_client->GetService(
    IID_PPV_ARGS(&render_client)
  );
  if (FAILED(hr)) {
    throw runtime_error(L"Render Clientの初期化に失敗しました。");
  }

  hr = output_client->GetBufferSize(&num_rendering_buffer_frames);
  if (FAILED(hr)) {
    throw runtime_error(L"再生バッファサイズの取得に失敗しました。");
  }
}

device::~device()
{
  stop();
}

void device::start()
{
  current_status = status::running;
  auto hr = input_client->Start();
  if (FAILED(hr)) {
    throw runtime_error(L"録音の開始に失敗しました。");
  }
  hr = output_client->Start();
  if (FAILED(hr)) {
    throw runtime_error(L"再生の開始に失敗しました。");
  }
  thread = std::make_unique<std::thread>(&device::run, this);
}

void device::stop()
{
  current_status = status::stopped;
  thread->join();
  thread.reset();
  input_client->Stop();
  output_client->Stop();
}

void device::run()
{
  try {
    DWORD task_index = 0;
    auto task = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &task_index);
    if (task == nullptr) {
      throw runtime_error(L"スレッド優先度の設定に失敗しました。");
    }
    while (current_status == status::running) {
      std::this_thread::sleep_for(
        std::chrono::milliseconds(
          num_capture_buffer_frames / 48000 * 1000 / 4
        )
      );

      // 録音
      UINT32 next_packet_size;
      auto hr = capture_client->GetNextPacketSize(&next_packet_size);
      if (FAILED(hr)) {
        throw runtime_error(L"録音パケットサイズの取得に失敗しました。");
      }

      BYTE *data;
      UINT32 num_frames_available;
      DWORD flags;
      while (next_packet_size != 0) {
        hr = capture_client->GetBuffer(
          &data,
          &num_frames_available,
          &flags,
          nullptr,
          nullptr
        );
        if (FAILED(hr)) {
          throw runtime_error(L"録音バッファ内容の取得に失敗しました。");
        }

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
          std::fill(data, data + sizeof(int16_t) * num_frames_available, 0);
        }

        std::copy(reinterpret_cast<int16_t*>(data),
                  reinterpret_cast<int16_t*>(data) + num_frames_available,
                  std::back_inserter(recording_buffer));

        hr = capture_client->ReleaseBuffer(num_frames_available);
        if (FAILED(hr)) {
          throw runtime_error(L"録音バッファの解放に失敗しました。");
        }

        hr = capture_client->GetNextPacketSize(&next_packet_size);
        if (FAILED(hr)) {
          throw runtime_error(L"録音パケットサイズの取得に失敗しました。");
        }
      }

      // 再生
      UINT32 num_padding_frames;
      hr = output_client->GetCurrentPadding(&num_padding_frames);
      if (FAILED(hr)) {
        throw runtime_error(L"再生パディングサイズの取得に失敗しました。");
      }
      num_frames_available = num_rendering_buffer_frames - num_padding_frames;

      hr = render_client->GetBuffer(
        num_frames_available,
        &data);
      if (FAILED(hr)) {
        throw runtime_error(L"再生バッファの取得に失敗しました。");
      }

      // 1ch -> 2chアップミックス
      int16_t* output_buffer = reinterpret_cast<int16_t*>(data);
      for (size_t i = 0; i < min(num_frames_available, recording_buffer.size()); ++i) {
        output_buffer[i * 2] = static_cast<int16_t>(recording_buffer[i]);
        output_buffer[i * 2 + 1] = static_cast<int16_t>(recording_buffer[i]);
      }
      if (recording_buffer.size() < num_frames_available) {
        // 録音が追いついていない。音途切れ発生。
        std::fill(output_buffer + recording_buffer.size() * 2, output_buffer + num_frames_available * 2, 0);
      }
      for (size_t i = 0; i < min(num_frames_available, recording_buffer.size()); ++i) {
        recording_buffer.pop_front();
      }
      hr = render_client->ReleaseBuffer(num_frames_available, 0);
      if (FAILED(hr)) {
        throw runtime_error(L"再生バッファの解放に失敗しました。");
      }
    }
  } catch (const runtime_error& e) {
    OutputDebugString(e.get_message().c_str());
  }
}

}
