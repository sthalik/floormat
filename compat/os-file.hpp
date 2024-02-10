#pragma once

namespace floormat::fs {

[[nodiscard]] bool file_exists(StringView name);

} // namespace floormat::fs

#ifndef fm_FILENAME_MAX
#define fm_FILENAME_MAX (260)
#endif
