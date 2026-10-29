#pragma once

#include "easydesk_media_abi.h"

#include <string_view>

namespace easydesk::media {

void setMediaError(EasyDeskMediaError *error,
                   EasyDeskMediaStatus status,
                   std::string_view message);

}  // namespace easydesk::media
