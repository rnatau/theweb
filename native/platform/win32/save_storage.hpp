#pragma once
#include "../../save_storage.hpp"

namespace web {
std::unique_ptr<SaveStorage> make_win32_save_storage();
} // namespace web
