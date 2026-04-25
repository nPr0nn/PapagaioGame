#include "game.h"
#include "systems/transition_system.h"
#include <math.h>
#include <stdio.h>

void all_pos(GameContext *g) {
  g->gaviao_pos.x = 20;
  g->gaviao_pos.y = 50;
  g->gaviao_speed.x = 0;
  g->gaviao_speed.y = 0;

  g->Pedra_pos.x = -100;
  g->Pedra_pos.y = 100;
  g->Pedra_speed.x = -1;
  g->Pedra_speed.y = 0;

  g->Bala_pos.x = -100;
  g->Bala_pos.y = 100;
  g->Bala_speed.x = -1;
  g->Bala_speed.y = 0;

  g->Coco_pos.x = -100;
  g->Coco_pos.y = 100;
  g->Coco_speed.x = -1;
  g->Coco_speed.y = 2;

  g->Coqueiro_pos.x = -100;
  g->Coqueiro_pos.y = 100;
  g->Coqueiro_speed.x = -1;
  g->Coqueiro_speed.y = 0;
}

void all_size(GameContext *g) {
  g->gaviao_size.x = 16;
  g->gaviao_size.y = 16;

  g->Pedra_size.x = 40;
  g->Pedra_size.y = 40;

  g->Bala_size.x = 4;
  g->Bala_size.y = 4;

  g->Coco_size.x = 16;
  g->Coco_size.y = 16;

  g->Coqueiro_size.x = 32;
  g->Coqueiro_size.y = 144;
}

void game_init(GameContext *g) {
#ifndef DEBUG
  rl_set_trace_log_level(LOG_NONE);
#endif
  // Game Title
  g->title = "Raylib Game Template";

  // --- Initialize Game and Transition States ---
  g->current_screen = SCREEN_TITLE;
  g->is_paused = false;
  init_transition_state(&g->transition_state);

  // --- Window Parameters ---
  g->window_width = 1080;
  g->window_height = 720;
  g->window_flags = FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT |
                    FLAG_WINDOW_RESIZABLE;
  g->target_fps = 60;

  rl_set_config_flags(g->window_flags);
  rl_init_window(g->window_width, g->window_height, g->title);
  rl_set_target_fps(g->target_fps);
  rl_init_audio_device();

   // Center Window
#ifdef PLATFORM_DESKTOP
  int monitor_id = rl_get_current_monitor();
  int monitor_width = rl_get_monitor_width(monitor_id);
  int monitor_height = rl_get_monitor_height(monitor_id);

  if (rl_get_monitor_count() == 2) {
    monitor_width = rl_get_monitor_width(1) + 2 * monitor_width;
    monitor_height = rl_get_monitor_height(1);
  }

  int window_pos_x = (monitor_width - rl_get_screen_width()) / 2;
  int window_pos_y = (monitor_height - rl_get_screen_height()) / 2;
  rl_set_window_position(window_pos_x, window_pos_y);
#endif

  // --- Render Screen Texture ---
  int screen_texture_width = 160;
  int screen_texture_height = 144;
  g->screen =
      rl_load_render_texture(screen_texture_width, screen_texture_height);
  rl_set_texture_filter(g->screen.texture, TEXTURE_FILTER_POINT);

  // --- Test Image Draw ---
  Image img = rl_load_image("assets/vaca1.png");
  g->papagaio_image = rl_load_texture_from_image(img);
  rl_unload_image(img);

  // --- Test Sound ---
  g->papagaio_sound = rl_load_sound("assets/bolha.wav");
  
  g->papagaio_pos.x = g->screen.texture.width * 0.5f;
  g->papagaio_pos.y = g->screen.texture.height * 0.5f;
  
  all_pos(g);
  all_size(g);

  g->coco_cai_em = rl_get_random_value(100, 160);
  g->coco_na_arvore = 1;

  g->timer1 = 0;
  
}

void spawn(GameContext *g) {
  // Logic for spawning game objects would go here
  if(g->Pedra_pos.x + g->Pedra_size.x < 0)
  {
    g->Pedra_pos.x = 160+rl_get_random_value(0, 500);
    g->Pedra_pos.y = 144-g->Pedra_size.y;
    g->Pedra_speed.x = -2;
    g->Pedra_speed.y = 0;
  }

  if(g->Coqueiro_pos.x + g->Coqueiro_size.x< 0)
  {
    g->Coqueiro_pos.x = 200+rl_get_random_value(0, 700);
    g->Coqueiro_pos.y = 144-g->Coqueiro_size.y;
    g->Coqueiro_speed.x = -2;
    g->Coqueiro_speed.y = 0;

    g->coco_cai_em = rl_get_random_value(100, 160);
    g->Coco_pos.x = -100;
    g->coco_na_arvore = 1;
  }

  if(g->Coqueiro_pos.x <= g->coco_cai_em && g->coco_na_arvore == 1)
  {
    g->Coco_pos.x = g->Coqueiro_pos.x;
    g->Coco_pos.y = 10;
    g->Coco_speed.x = -2;
    g->Coco_speed.y = 3;
    g->coco_na_arvore = 0;
  }

  if(g->Bala_pos.x + g->Bala_size.x< 0)
  {
    g->Bala_pos.x = 230+rl_get_random_value(0, 1000);
    g->Bala_pos.y = 72 + rl_get_random_value(-72, 72);
    g->Bala_speed.x = -rl_get_random_value(2, 4);
    g->Bala_speed.y = 0;
  }

  
  if(rl_get_random_value(0, 100) < 5)
  {
  
  }
}

void update_positions(GameContext *g) {
  // Logic for updating positions of game objects would go here
  // For example, you could add boundary checks or reset positions when they go off-screen
  g->timer1++;
  g->gaviao_pos.x += g->gaviao_speed.x;
  g->gaviao_pos.y += g->gaviao_speed.y;
  
  g->Pedra_pos.x += g->Pedra_speed.x;
  g->Pedra_pos.y += g->Pedra_speed.y;

  g->Coco_pos.x += g->Coco_speed.x;
  g->Coco_pos.y = fmin(g->Coco_speed.y + g->Coco_pos.y, 144 - g->Coco_size.y);
  // printf("Coco_pos.x: %f Coco_pos.y: %f\n", g->Coco_pos.x, g->Coco_pos.y);

  g->Coqueiro_pos.x += g->Coqueiro_speed.x;
  g->Coqueiro_pos.y += g->Coqueiro_speed.y;

  g->Bala_pos.x += g->Bala_speed.x;
  g->Bala_pos.y += g->Bala_speed.y;
}

void game_update(GameContext *g) {
  // Always update the transition state first
  update_transition(&g->transition_state, &g->current_screen);

  // Only process game logic if we are not in the middle of a transition
  if (!is_transitioning(&g->transition_state)) {
    switch (g->current_screen) {
    case SCREEN_TITLE: {
      // When ENTER is pressed, start a transition to the gameplay screen
      if (rl_is_key_pressed(KEY_ENTER)) {
        transition_to_screen(&g->transition_state, SCREEN_GAMEPLAY,
                             TRANSITION_CIRCLE_EXPAND, BLACK);
      }
    } break;

    case SCREEN_GAMEPLAY: {
      f32 time = rl_get_time();
      g->papagaio_pos.y += 0.25f * sin(time);
      
      // Toggle pause state
      if (rl_is_key_pressed(KEY_P)) {
        g->is_paused = !g->is_paused;
      }

      // Only update gameplay logic if the game is not paused
      if (!g->is_paused) {
        if (rl_is_key_pressed(KEY_M)) {
          rl_play_sound(g->papagaio_sound);
        }
      }

      // Allow transitioning back to title screen
      if (rl_is_key_pressed(KEY_T)) {
        transition_to_screen(&g->transition_state, SCREEN_TITLE,
                             TRANSITION_WIPE_RIGHT, BLACK);
      }

      spawn(g);
      update_positions(g);

    } break;

    case SCREEN_OPTIONS: {
      // Logic for options screen would go here
    } break;
    }
  }
  
}



void game_draw(GameContext *g) {
  // 1. Draw game to screen texture
  rl_begin_texture_mode(g->screen);
  rl_clear_background(RAYWHITE);

  // Draw the content of the current screen
  switch (g->current_screen) {
  case SCREEN_TITLE: {
    rl_clear_background(DARKGRAY);

    rl_draw_text("TITLE SCREEN", 20, 20, 10, WHITE);
    rl_draw_text("Press ENTER to start", 25, 40, 8, LIGHTGRAY);
  } break;
  case SCREEN_GAMEPLAY: {
    rl_clear_background(BROWN);
    rl_draw_texture(
      g->papagaio_image,
      g->papagaio_pos.x - (g->papagaio_image.width) / 2,
      g->papagaio_pos.y - (g->papagaio_image.height) / 2,
      WHITE
    );
    rl_draw_rectangle(g->Bala_pos.x, g->Bala_pos.y, g->Bala_size.x, g->Bala_size.y, RED);
    rl_draw_rectangle(g->Coco_pos.x, g->Coco_pos.y, g->Coco_size.x, g->Coco_size.y, PINK);
    rl_draw_rectangle(g->Coqueiro_pos.x, g->Coqueiro_pos.y, g->Coqueiro_size.x, 20, GREEN);
    rl_draw_rectangle(g->Pedra_pos.x, g->Pedra_pos.y, g->Pedra_size.x, g->Pedra_size.y, LIGHTGRAY);
    rl_draw_rectangle(g->gaviao_pos.x, g->gaviao_pos.y, g->gaviao_size.x, g->gaviao_size.y, BLUE);

    // If the game is paused, draw an overlay
    if (g->is_paused) {
      rl_draw_rectangle(0, 0, g->screen.texture.width, g->screen.texture.height,
                        rl_fade(BLACK, 0.6f));
      rl_draw_text("PAUSED", g->screen.texture.width / 2 - 20,
                   g->screen.texture.height / 2 - 5, 10, WHITE);
    }

  } break;
  case SCREEN_OPTIONS: {
    // Drawing for options screen
  } break;
  }

  // 3. Draw the transition effect on top of everything else
  draw_transition(&g->transition_state, g->screen.texture.width,
                  g->screen.texture.height);

  rl_end_texture_mode();

  // 2. Draw the final composed scene to the window
  rl_begin_drawing();
  rl_clear_background(BLACK);

  // Calculate scale and destination rectangle for the render texture
  f32 scale = fminf((f32)rl_get_render_width() / g->screen.texture.width,
                    (f32)rl_get_render_height() / g->screen.texture.height);
  Rect dest = {
      .x = (rl_get_render_width() - ((f32)g->screen.texture.width * scale)) *
           0.5f,
      .y = (rl_get_render_height() - ((f32)g->screen.texture.height * scale)) *
           0.5f,
      .width = (f32)g->screen.texture.width * scale,
      .height = (f32)g->screen.texture.height * scale};
  Rect source = {0.0f, 0.0f, (f32)g->screen.texture.width,
                 -(f32)g->screen.texture.height};

  // Draw the scaled game texture
  rl_draw_texture_pro(g->screen.texture, source, dest, (Vector2){0, 0}, 0.0f,
                      WHITE);


  rl_end_drawing();
}

void game_loop_step(void *ctx) {
  GameContext *g = (GameContext *)ctx;

  if (rl_is_key_pressed(KEY_F))
    rl_toggle_fullscreen();
  if (rl_is_key_pressed(KEY_ESCAPE))
    g->should_close = true;

  game_update(g);
  game_draw(g);
}

void game_exit(GameContext *g) {
  rl_close_window();
  rl_close_audio_device();
}
