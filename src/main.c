
#include "game.h"

int main(void) {
  GameContext game_context;

  game_init(&game_context);

#if defined(PLATFORM_WEB)
  // For the web, we set a callback and let the browser run it
  emscripten_set_main_loop_arg(game_loop_step, &game_context, -1, 1);
#else
  // For desktop, we have a standard while loop with full control
  // The loop condition is now fully visible and not hidden in a macro
  while (!rl_window_should_close() && !game_context.should_close) {
    game_loop_step(&game_context);
  }
#endif

  game_exit(&game_context);

#if defined(PLATFORM_WEB)
  emscripten_cancel_main_loop();
#endif

  return 0;
}
