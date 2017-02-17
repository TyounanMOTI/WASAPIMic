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
    throw runtime_error(L"Device Enumerator�̏������Ɏ��s���܂����B");
  }

  hr = enumerator->GetDefaultAudioEndpoint(
    eCapture,
    eConsole,
    &input_device
  );
  if (FAILED(hr)) {
    throw runtime_error(L"����̘^���f�o�C�X�̎擾�Ɏ��s���܂����B");
  }

  hr = input_device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    &input_client
  );
  if (FAILED(hr)) {
    throw runtime_error(L"�^���pAudio Client�̏������Ɏ��s���܂����B");
  }

  REFERENCE_TIME capture_device_period;
  hr = input_client->GetDevicePeriod(
    nullptr,
    &capture_device_period
  );
  if (FAILED(hr)) {
    throw runtime_error(L"�^���f�o�C�X�̏��������̎擾�Ɏ��s���܂����B");
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
    throw runtime_error(L"�^���f�o�C�X���T�|�[�g���Ă��Ȃ��t�H�[�}�b�g�ł��B");
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
    throw runtime_error(L"�^���f�o�C�X�̔r�����[�h�ł̏������Ɏ��s���܂����B");
  }

  hr = input_client->GetBufferSize(&num_capture_buffer_frames);
  if (FAILED(hr)) {
    throw runtime_error(L"�^���o�b�t�@�T�C�Y�̎擾�Ɏ��s���܂����B");
  }

  hr = input_client->GetService(
    IID_PPV_ARGS(&capture_client)
  );
  if (FAILED(hr)) {
    throw runtime_error(L"Capture Client�̎擾�Ɏ��s���܂����B");
  }

  hr = enumerator->GetDefaultAudioEndpoint(
    eRender,
    eConsole,
    &output_device
  );
  if (FAILED(hr)) {
    throw runtime_error(L"����̍Đ��f�o�C�X�̎擾�Ɏ��s���܂����B");
  }

  hr = output_device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    &output_client
  );
  if (FAILED(hr)) {
    throw runtime_error(L"�Đ��p Audio Client �̏������Ɏ��s���܂����B");
  }

  REFERENCE_TIME rendering_device_period;
  hr = output_client->GetDevicePeriod(nullptr, &rendering_device_period);
  if (FAILED(hr)) {
    throw runtime_error(L"�Đ��f�o�C�X�̍X�V�����̎擾�Ɏ��s���܂����B");
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
    throw runtime_error(L"�Đ��f�o�C�X���T�|�[�g���Ă��Ȃ��t�H�[�}�b�g�ł��B");
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
    throw runtime_error(L"�Đ��f�o�C�X�̔r�����[�h�ł̏������Ɏ��s���܂����B");
  }

  hr = output_client->GetService(
    IID_PPV_ARGS(&render_client)
  );
  if (FAILED(hr)) {
    throw runtime_error(L"Render Client�̏������Ɏ��s���܂����B");
  }

  hr = output_client->GetBufferSize(&num_rendering_buffer_frames);
  if (FAILED(hr)) {
    throw runtime_error(L"�Đ��o�b�t�@�T�C�Y�̎擾�Ɏ��s���܂����B");
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
    throw runtime_error(L"�^���̊J�n�Ɏ��s���܂����B");
  }
  hr = output_client->Start();
  if (FAILED(hr)) {
    throw runtime_error(L"�Đ��̊J�n�Ɏ��s���܂����B");
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
      throw runtime_error(L"�X���b�h�D��x�̐ݒ�Ɏ��s���܂����B");
    }
    while (current_status == status::running) {
      std::this_thread::sleep_for(
        std::chrono::milliseconds(
          num_capture_buffer_frames / 48000 * 1000 / 4
        )
      );

      // �^��
      UINT32 next_packet_size;
      auto hr = capture_client->GetNextPacketSize(&next_packet_size);
      if (FAILED(hr)) {
        throw runtime_error(L"�^���p�P�b�g�T�C�Y�̎擾�Ɏ��s���܂����B");
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
          throw runtime_error(L"�^���o�b�t�@���e�̎擾�Ɏ��s���܂����B");
        }

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
          std::fill(data, data + sizeof(int16_t) * num_frames_available, 0);
        }

        std::copy(reinterpret_cast<int16_t*>(data),
                  reinterpret_cast<int16_t*>(data) + num_frames_available,
                  std::back_inserter(recording_buffer));

        hr = capture_client->ReleaseBuffer(num_frames_available);
        if (FAILED(hr)) {
          throw runtime_error(L"�^���o�b�t�@�̉���Ɏ��s���܂����B");
        }

        hr = capture_client->GetNextPacketSize(&next_packet_size);
        if (FAILED(hr)) {
          throw runtime_error(L"�^���p�P�b�g�T�C�Y�̎擾�Ɏ��s���܂����B");
        }
      }

      // �Đ�
      UINT32 num_padding_frames;
      hr = output_client->GetCurrentPadding(&num_padding_frames);
      if (FAILED(hr)) {
        throw runtime_error(L"�Đ��p�f�B���O�T�C�Y�̎擾�Ɏ��s���܂����B");
      }
      num_frames_available = num_rendering_buffer_frames - num_padding_frames;

      hr = render_client->GetBuffer(
        num_frames_available,
        &data);
      if (FAILED(hr)) {
        throw runtime_error(L"�Đ��o�b�t�@�̎擾�Ɏ��s���܂����B");
      }

      // 1ch -> 2ch�A�b�v�~�b�N�X
      int16_t* output_buffer = reinterpret_cast<int16_t*>(data);
      for (size_t i = 0; i < min(num_frames_available, recording_buffer.size()); ++i) {
        output_buffer[i * 2] = static_cast<int16_t>(recording_buffer[i]);
        output_buffer[i * 2 + 1] = static_cast<int16_t>(recording_buffer[i]);
      }
      if (recording_buffer.size() < num_frames_available) {
        // �^�����ǂ����Ă��Ȃ��B���r�؂ꔭ���B
        std::fill(output_buffer + recording_buffer.size() * 2, output_buffer + num_frames_available * 2, 0);
      }
      for (size_t i = 0; i < min(num_frames_available, recording_buffer.size()); ++i) {
        recording_buffer.pop_front();
      }
      hr = render_client->ReleaseBuffer(num_frames_available, 0);
      if (FAILED(hr)) {
        throw runtime_error(L"�Đ��o�b�t�@�̉���Ɏ��s���܂����B");
      }
    }
  } catch (const runtime_error& e) {
    OutputDebugString(e.get_message().c_str());
  }
}

}
