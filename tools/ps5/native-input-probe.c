/* ABI hooks retained for the existing frontend/SDL archives. No clocks,
 * counters, per-frame/per-draw log writes, or button traces in normal builds. */
void pemu_native_input_probe(unsigned phase) { (void)phase; }
void pemu_native_game_input(unsigned buttons) { (void)buttons; }
