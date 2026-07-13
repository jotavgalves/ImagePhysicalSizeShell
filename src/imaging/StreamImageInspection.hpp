#pragma once

#include "imaging/ImageInspection.hpp"

#include <objidl.h>

namespace ips {

bool InspectImageStream(IStream* stream, ImageInspection& inspection, std::wstring& error);

}

