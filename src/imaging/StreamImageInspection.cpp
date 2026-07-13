#include "imaging/StreamImageInspection.hpp"

#include "imaging/PhysicalSize.hpp"

#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <vector>

namespace ips {
namespace {

using Microsoft::WRL::ComPtr;

constexpr unsigned long kMaxMetadataScanBytes = 16u * 1024u * 1024u;

std::wstring HResultText(HRESULT hr) {
  wchar_t buffer[32] = {};
  swprintf_s(buffer, L"0x%08X", static_cast<unsigned int>(hr));
  return buffer;
}

bool SeekStream(IStream* stream, LARGE_INTEGER offset, DWORD origin, ULARGE_INTEGER* newPosition = nullptr) {
  return stream && SUCCEEDED(stream->Seek(offset, origin, newPosition));
}

bool CloneOrResetStream(IStream* stream, ComPtr<IStream>& clone, std::wstring& error) {
  if (!stream) {
    error = L"Stream is null.";
    return false;
  }

  if (SUCCEEDED(stream->Clone(&clone)) && clone) {
    LARGE_INTEGER zero{};
    SeekStream(clone.Get(), zero, STREAM_SEEK_SET);
    return true;
  }

  LARGE_INTEGER zero{};
  if (!SeekStream(stream, zero, STREAM_SEEK_SET)) {
    error = L"Stream does not support Clone or Seek.";
    return false;
  }

  clone = stream;
  return true;
}

std::vector<std::uint8_t> ReadPrefix(IStream* stream, std::wstring& error) {
  STATSTG stat{};
  unsigned long bytesToRead = kMaxMetadataScanBytes;
  if (SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME)) && stat.cbSize.QuadPart > 0) {
    bytesToRead = static_cast<unsigned long>(
        std::min<unsigned long long>(stat.cbSize.QuadPart, kMaxMetadataScanBytes));
  }

  std::vector<std::uint8_t> data(bytesToRead);
  ULONG read = 0;
  HRESULT hr = stream->Read(data.data(), bytesToRead, &read);
  if (FAILED(hr)) {
    error = L"IStream::Read failed: " + HResultText(hr);
    return {};
  }
  data.resize(read);
  if (data.empty()) {
    error = L"Stream is empty.";
  }
  return data;
}

}

bool InspectImageStream(IStream* stream, ImageInspection& inspection, std::wstring& error) {
  inspection = ImageInspection{};

  ComPtr<IStream> metadataStream;
  if (!CloneOrResetStream(stream, metadataStream, error)) {
    return false;
  }

  const auto bytes = ReadPrefix(metadataStream.Get(), error);
  if (bytes.empty()) {
    if (error.empty()) {
      error = L"Could not read image metadata prefix.";
    }
    return false;
  }

  inspection.format = DetectContainerFormat(bytes);
  inspection.embeddedDpi = DetectEmbeddedDpi(bytes);

  ComPtr<IStream> wicStream;
  if (!CloneOrResetStream(stream, wicStream, error)) {
    return false;
  }

  ComPtr<IWICImagingFactory> factory;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    error = L"CoCreateInstance(CLSID_WICImagingFactory) failed: " + HResultText(hr);
    return false;
  }

  ComPtr<IWICBitmapDecoder> decoder;
  hr = factory->CreateDecoderFromStream(wicStream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
  if (FAILED(hr)) {
    error = L"WIC CreateDecoderFromStream failed: " + HResultText(hr);
    return false;
  }

  ComPtr<IWICBitmapFrameDecode> frame;
  hr = decoder->GetFrame(0, &frame);
  if (FAILED(hr)) {
    error = L"WIC GetFrame(0) failed: " + HResultText(hr);
    return false;
  }

  UINT width = 0;
  UINT height = 0;
  hr = frame->GetSize(&width, &height);
  if (FAILED(hr)) {
    error = L"WIC GetSize failed: " + HResultText(hr);
    return false;
  }
  inspection.widthPixels = width;
  inspection.heightPixels = height;

  double dpiX = 0.0;
  double dpiY = 0.0;
  hr = frame->GetResolution(&dpiX, &dpiY);
  if (SUCCEEDED(hr)) {
    inspection.wicDpiX = dpiX;
    inspection.wicDpiY = dpiY;
  }

  if (inspection.embeddedDpi.found) {
    auto physicalSize =
        CalculatePhysicalSizeCm(width, height, inspection.embeddedDpi.dpiX, inspection.embeddedDpi.dpiY);
    if (physicalSize) {
      inspection.hasPhysicalSize = true;
      inspection.widthCm = physicalSize->widthCm;
      inspection.heightCm = physicalSize->heightCm;
    }
  }

  return true;
}

}

