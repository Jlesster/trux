namespace trux::async {

template <typename WorkFn, typename Callback>
void spawn(WorkFn&& work, Callback&& on_done);
}
