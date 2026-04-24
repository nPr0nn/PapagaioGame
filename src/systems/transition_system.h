#ifndef TRANSITION_SYSTEM_H
#define TRANSITION_SYSTEM_H

#include "../../vendors/mak/base_types.h"
#include "../../vendors/raylib/sc_raylib.h"
#include <math.h>

// Define the different screens your game can have.
typedef enum GameScreen {
  SCREEN_TITLE,
  SCREEN_GAMEPLAY,
  SCREEN_OPTIONS
} GameScreen;

// Define the types of transitions available.
typedef enum {
  TRANSITION_FADE,
  TRANSITION_WIPE_RIGHT,
  TRANSITION_WIPE_LEFT,
  TRANSITION_CIRCLE_EXPAND
} TransitionType;

// A generic struct to hold all state information for any screen transition.
typedef struct {
  TransitionType type;
  bool onTransition;
  bool transPhaseOut;
  GameScreen transToScreen;
  f32 transDuration;
  f32 transTimer;

  // A union to hold data specific to each transition type
  union {
    // Data for transitions that use a solid color (Fade, Wipe, Circle)
    struct {
      Color color;
    } fade;
  } data;

} TransitionState;

// Helper Functions

static inline f32 ease_in_out_cubic(f32 t) {
  if (t < 0.5f) {
    return 4.0f * t * t * t;
  } else {
    f32 f = -2.0f * t + 2.0f;
    return 1.0f - (f * f * f) / 2.0f;
  }
}

static inline void init_transition_state(TransitionState *ts) {
  ts->type = TRANSITION_FADE;
  ts->onTransition = false;
  ts->transPhaseOut = false;
  ts->transToScreen = (GameScreen)0;
  ts->transDuration = 0.5f; // A good default duration
  ts->transTimer = 0.0f;
  ts->data.fade.color = BLACK;
}

static inline bool is_transitioning(const TransitionState *ts) {
  return ts->onTransition;
}
static inline void transition_to_screen(TransitionState *ts, GameScreen screen,
                                        TransitionType type, Color color) {
  if (!ts->onTransition) {
    ts->onTransition = true;
    ts->transPhaseOut = true;
    ts->transToScreen = screen;
    ts->transTimer = 0.0f;
    ts->type = type;

    // Initialize transition-specific data
    // For all current types, we just set the color.
    ts->data.fade.color = color;
  }
}

static inline void update_transition(TransitionState *ts,
                                     GameScreen *currentScreen) {
  if (!ts->onTransition)
    return;

  if (ts->transPhaseOut) {
    // Update the timer for the "out" phase
    ts->transTimer += rl_get_frame_time();
    if (ts->transTimer >= ts->transDuration) {
      ts->transTimer = ts->transDuration;
      *currentScreen = ts->transToScreen;
      ts->transPhaseOut = false;
    }
  } else {
    // Update the timer for the "in" phase
    ts->transTimer -= rl_get_frame_time();
    if (ts->transTimer <= 0.0f) {
      ts->transTimer = 0.0f;
      ts->onTransition = false;
    }
  }
}

static inline void draw_transition(const TransitionState *ts, i32 screenWidth,
                                   i32 screenHeight) {
  if (!ts->onTransition)
    return;

  f32 rawProgress = ts->transTimer / ts->transDuration;
  f32 progress = ease_in_out_cubic(rawProgress);

  switch (ts->type) {
  case TRANSITION_FADE: {
    rl_draw_rectangle(0, 0, screenWidth, screenHeight,
                      rl_fade(ts->data.fade.color, progress));
  } break;

  case TRANSITION_WIPE_RIGHT: {
    rl_draw_rectangle(0, 0, (i32)(screenWidth * progress), screenHeight,
                      ts->data.fade.color);
  } break;

  case TRANSITION_WIPE_LEFT: {
    rl_draw_rectangle((i32)(screenWidth - (screenWidth * progress)), 0,
                      (i32)(screenWidth * progress), screenHeight,
                      ts->data.fade.color);
  } break;

  case TRANSITION_CIRCLE_EXPAND: {
    Vec2 center = {(f32)screenWidth / 2.0f, (f32)screenHeight / 2.0f};
    f32 maxRadius =
        sqrtf(powf((f32)screenWidth, 2) + powf((f32)screenHeight, 2)) / 2.0f;
    rl_draw_circle_v(center, maxRadius * progress, ts->data.fade.color);
  } break;
  }
}

#endif // TRANSITION_SYSTEM_H
