#ifndef WEB_UTILS_H
#define WEB_UTILS_H

#if defined(__EMSCRIPTEN__)
#define PLATFORM_WEB 1
#include <emscripten/emscripten.h>

static inline void toggle_browser_fullscreen(void) {
  EM_ASM_({
    // Check if an element is currently in fullscreen mode
    if (!document.fullscreenElement && !document.mozFullScreenElement &&
        !document.webkitFullscreenElement && !document.msFullscreenElement) {
      // If NOT fullscreen, request it on the canvas
      var canvas = document.querySelector("#canvas");
      if (canvas.requestFullscreen) {
        canvas.requestFullscreen();
      } else if (canvas.mozRequestFullScreen) {
        canvas.mozRequestFullScreen();
      } else if (canvas.webkitRequestFullscreen) {
        canvas.webkitRequestFullscreen(Element.ALLOW_KEYBOARD_INPUT);
      } else if (canvas.msRequestFullscreen) {
        canvas.msRequestFullscreen();
      }
    } else {
      // If already fullscreen, exit
      if (document.exitFullscreen) {
        document.exitFullscreen();
      } else if (document.msExitFullscreen) {
        document.msExitFullscreen();
      } else if (document.mozCancelFullScreen) {
        document.mozCancelFullScreen();
      } else if (document.webkitExitFullscreen) {
        document.webkitExitFullscreen();
      }
    }
  });
}
#define rl_toggle_fullscreen() toggle_browser_fullscreen()
#else
#define PLATFORM_DESKTOP 1
#endif // __EMSCRIPTEN__

#endif // WEB_UTILS_H
