// The staged economy's live acts (Docs/Design/79-a-staged-economy.md, 12; Phase 9): the dm.actions kinds the DM's
// Prosperity tab and the studio queue, applied by the game and journalled before anyone is told.
#include "RatwGame.h"

namespace ratw::game
{
bool Game::economyCommand(const std::string&) { return false; }

Result Game::applyEconomyCommand(const std::string&, const std::string& target, const json::Value&)
{
    return {false, "not built", target};
}
} // namespace ratw::game
