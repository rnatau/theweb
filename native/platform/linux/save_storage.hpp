#pragma once
#include "../../save_storage.hpp"

namespace web {
std::unique_ptr<SaveStorage> make_posix_save_storage();
}
