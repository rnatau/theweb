#pragma once
#include "../../rules/collection_policy.hpp"

namespace web::enhanced {
bool collection_allowed(const rules::TurnState&, const rules::SharedGameState&);
} // namespace web::enhanced
