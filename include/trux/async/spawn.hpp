/// @file spawn.hpp
/// @brief Forward declaration of async::spawn(), letting headers
///        reference it without pulling in executor.hpp's system
///        headers (see executor.hpp for the definition and docs).

namespace trux::async {

/// @brief Runs `work` on a background thread, then delivers its
///        result to `on_done` on the calling thread.
/// @see   executor.hpp for the full definition.
template <typename WorkFn, typename Callback>
void spawn(WorkFn&& work, Callback&& on_done);
}  // namespace trux::async
