#ifndef GAME_H
#define GAME_H

#include "../vendors/mak/arena.h"
#include "../vendors/mak/base_types.h"
#include "../vendors/raylib/sc_raylib.h"
#include "../vendors/raylib/sc_raymath.h"

#include "web_utils.h"

#include "systems/transition_system.h"

#include <math.h>

#define DEBUG 1

typedef struct GameContext {
  // Game Defining
  const char *title;
  // We replace the old 'GameState' with 'GameScreen' and a pause flag
  GameScreen current_screen;
  bool is_paused;

  // Window Parameter
  i32 window_height;
  i32 window_width;
  i32 window_flags;
  i32 target_fps;

  // Screen Texture
  RenderTexture2D screen;

  // Close Events
  bool should_close;

  // Game Specifics
  // Papagaio
  Vec2 papagaio_pos;
  Sound papagaio_sound;
  Texture2D papagaio_image;

  // Gaviao
  Vec2 gaviao_pos;
  Vec2 gaviao_speed;
  Vec2 gaviao_size;

  Vec2 Pedra_pos;
  Vec2 Pedra_speed;
  Vec2 Pedra_size;

  Vec2 Bala_pos;
  Vec2 Bala_speed;
  Vec2 Bala_size;

  Vec2 Coco_pos;
  Vec2 Coco_speed;
  Vec2 Coco_size;


  Vec2 Coqueiro_pos;
  Vec2 Coqueiro_speed;
  Vec2 Coqueiro_size;


  i32 coco_cai_em;
  i32 coco_na_arvore;
  i32 timer1;
  



  // The state for our transition system
  TransitionState transition_state;
} GameContext;

void game_loop_step(void *ctx);

void game_init(GameContext *ctx);
void game_update(GameContext *ctx);
void game_draw(GameContext *ctx);
void game_exit(GameContext *ctx);

#endif
