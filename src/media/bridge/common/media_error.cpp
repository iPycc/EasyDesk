#include "common/media_error.h"

#include <algorithm>
#include <cstring>

namespace easydesk::media {

void setMediaError(EasyDeskMediaError *error,
                   EasyDeskMediaStatus status,
                   std::string_view message) {
    if (error == nullptr || error->size < sizeof(EasyDeskMediaError))
        return;

    error->code = static_cast<int32_t>(status);
    const std::size_t length =
        std::min(message.size(), sizeof(error->message) - std::size_t{1});
    std::memcpy(error->message, message.data(), length);
    error->message[length] = '\0';
}

}  // namespace easydesk::media
