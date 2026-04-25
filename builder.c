#include <stdio.h>

#define ARENA_IMPLEMENTATION
#define MAK_ARGPARSER_IMPLEMENTATION
#define MAK_BUILDER_IMPLEMENTATION

#include "vendors/mak/arena.h"
#include "vendors/mak/base_types.h"
#include "vendors/mak/mak_argparser.h"
#include "vendors/mak/mak_builder.h"

// --- Build and Run Functions ---

#define MAX_PATH_SIZE 256
#define TARGET_FOLDER "target/"

bool build_vendors(ArenaAllocator *arena_ptr, bool is_web)
{
  // Create output folder
  char output_folder_path[MAX_PATH_SIZE];
  mak_makedir(TARGET_FOLDER, TRUE);

  if (is_web)
  {
    mak_path_join(output_folder_path, TARGET_FOLDER, "web");
  }
  else
  {
    mak_path_join(output_folder_path, TARGET_FOLDER, "desktop");
  }
  mak_makedir(output_folder_path, TRUE);

  printf("[build] vendors - %s version\n", is_web ? "web" : "desktop");

  // Create builder for raylib
  MakBuilder *builder = mak_builder_new(arena_ptr);
  mak_builder_set_output_dir(builder, output_folder_path);

  // Add raylib sources
  mak_builder_add_sources(
      builder, "vendors/raylib/rcore.c", "vendors/raylib/raudio.c",
      "vendors/raylib/rshapes.c", "vendors/raylib/rtext.c",
      "vendors/raylib/rtextures.c", "vendors/raylib/utils.c", NULL);

  if (is_web)
  {
    mak_builder_set_compiler(builder, "emcc");
    mak_builder_add_define(builder, "PLATFORM_WEB");
    mak_builder_add_define(builder, "GRAPHICS_API_OPENGL_ES2");
    mak_builder_add_cflag(builder, "-Wall");
  }
  else
  {
    mak_builder_set_compiler(builder, "gcc");

    // Add rglfw.c for desktop
    mak_builder_add_sources(builder, "vendors/raylib/rglfw.c", NULL);

#ifdef _WIN32
    mak_builder_add_include(builder, "vendors/raylib");
    mak_builder_add_include(builder, "vendors/raylib/external/glfw/include");
    mak_builder_add_define(builder, "PLATFORM_DESKTOP");
    mak_builder_add_define(builder, "GRAPHICS_API_OPENGL_33");
    mak_builder_add_cflag(builder, "-Wall");
    mak_builder_add_cflag(builder, "-Wno-missing-braces");
    mak_builder_add_cflag(builder, "-O2");
#else
    mak_builder_add_include(builder, ".");
    mak_builder_add_include(builder, "vendors/raylib/external/glfw/include");
    mak_builder_add_define(builder, "_GNU_SOURCE");
    mak_builder_add_define(builder, "PLATFORM_DESKTOP_GLFW");
    mak_builder_add_define(builder, "GRAPHICS_API_OPENGL_33");
    mak_builder_add_define(builder, "_GLFW_X11");
    mak_builder_add_cflag(builder, "-Wall");
    mak_builder_add_cflag(builder, "-Werror=pointer-arith");
    mak_builder_add_cflag(builder, "-Werror=implicit-function-declaration");
    mak_builder_add_cflag(builder, "-Wno-missing-braces");
    mak_builder_add_cflag(builder, "-fno-strict-aliasing");
    mak_builder_add_cflag(builder, "-fPIC");
    mak_builder_add_cflag(builder, "-O2");
#endif
  }
  // Compile as static library
  mak_builder_compile_objects(builder);
  MakResult result = mak_builder_create_static_lib(builder, "libraylib.a");
  return result.success;
}

bool build_game(ArenaAllocator *arena_ptr, bool is_web)
{
  // Create output folder
  char output_folder_path[MAX_PATH_SIZE];
  mak_makedir(TARGET_FOLDER, TRUE);

  if (is_web)
  {
    mak_path_join(output_folder_path, TARGET_FOLDER, "web");
  }
  else
  {
    mak_path_join(output_folder_path, TARGET_FOLDER, "desktop");
  }
  mak_makedir(output_folder_path, TRUE);

  printf("[build] game - %s version\n", is_web ? "web" : "desktop");

  MakBuilder *builder = mak_builder_new(arena_ptr);
  mak_builder_set_output_dir(builder, output_folder_path);

  // Add game sources
  mak_builder_add_sources(builder, "src/main.c", "src/game.c", "src/audio_capture.c", NULL);

  // Add raylib include directory
  mak_builder_add_include(builder, "vendors/raylib");
  mak_builder_add_include(builder, "vendors/raylib/external/glfw/include");

  MakResult result;

  if (is_web)
  {
    mak_builder_set_compiler(builder, "em++");
    mak_builder_add_define(builder, "PLATFORM_WEB");
    mak_builder_add_libdir(builder, "target/web");
    mak_builder_add_lib(builder, "raylib");
    mak_builder_add_cflag(builder, "-Os");
    mak_builder_add_cflag(builder, "-Wall");
    mak_builder_add_cflag(builder, "-sUSE_GLFW=3");
    mak_builder_add_cflag(builder, "-sFULL_ES2=1");
    mak_builder_add_cflag(builder, "-sASYNCIFY");
    mak_builder_add_cflag(builder, "-sALLOW_MEMORY_GROWTH");
    mak_builder_add_cflag(builder, "--shell-file");
    mak_builder_add_cflag(builder, "shell.html");
    mak_builder_add_cflag(builder, "--preload-file");
    mak_builder_add_cflag(builder, "assets");
    result = mak_builder_compile_executable(builder, "index.html");
  }
  else
  {
    mak_builder_set_compiler(builder, "gcc");

#ifdef _WIN32
    // Build raylib directly into the game executable on Windows so we do not
    // depend on a separate static archive during linking.
    mak_builder_add_sources(
        builder, "vendors/raylib/rcore.c", "vendors/raylib/raudio.c",
        "vendors/raylib/rshapes.c", "vendors/raylib/rtext.c",
        "vendors/raylib/rtextures.c", "vendors/raylib/utils.c",
        "vendors/raylib/rglfw.c", NULL);
    mak_builder_add_define(builder, "PLATFORM_DESKTOP");
    mak_builder_add_define(builder, "GRAPHICS_API_OPENGL_33");
    mak_builder_add_cflag(builder, "-Wall");
    mak_builder_add_cflag(builder, "-Wno-missing-braces");
    mak_builder_add_cflag(builder, "-O2");
#else
    mak_builder_add_libdir(builder, "target/desktop");
    mak_builder_add_lib(builder, "raylib");
#endif

#ifdef _WIN32
    mak_builder_add_lib(builder, "opengl32");
    mak_builder_add_lib(builder, "gdi32");
    mak_builder_add_lib(builder, "winmm");
#else
    mak_builder_add_lib(builder, "m");
    mak_builder_add_lib(builder, "pthread");
    mak_builder_add_lib(builder, "dl");
    mak_builder_add_lib(builder, "rt");
    mak_builder_add_lib(builder, "X11");
    mak_builder_add_lib(builder, "Xrandr");
    mak_builder_add_lib(builder, "GL");
#endif
    result = mak_builder_compile_executable(builder, "game");
  }

  if (!result.success)
  {
    printf("error: failed to build game: %s\n", result.message);
  }

  return result.success;
}

void run_game(ArenaAllocator *arena_ptr, bool is_web)
{
  if (is_web)
  {
    printf("[run] game - web version\n");
    mak_cmd_exec("emrun target/web/index.html");
  }
  else
  {
    printf("[run] game - desktop version\n");
#ifdef _WIN32
    mak_cmd_exec("target\\desktop\\game.exe");
#else
    mak_cmd_exec("./target/desktop/game");
#endif
  }
}

// --- Main Entry Point ---
i32 main(i32 argc, char **argv)
{
  ArenaAllocator arena = {0};
  arena_alloc(&arena, ARENA_KB(4));

  MakArgParser *argparser =
      mak_argparser_new(&arena, "Builder", "Build system for game and editor");

  mak_argparser_add(
      argparser, (MakArgSpec){.name = "web",
                              .short_name = 'w',
                              .type = MAK_ARG_BOOL,
                              .description = "Build or run the web version"});

  if (!mak_argparser_parse(argparser, argc, argv))
  {
    mak_argparser_print_error(argparser);
    arena_free(&arena);
    return 1;
  }

  // Get information from command line argument parser
  bool is_web = mak_argparser_has_flag(argparser, "web");
  bool should_build = mak_argparser_has_command(argparser, "build");
  bool should_run = mak_argparser_has_command(argparser, "run");
  bool is_vendors = mak_argparser_has_command(argparser, "vendors");
  bool is_game = mak_argparser_has_command(argparser, "game");

  // Validate top-level commands
  if (!should_build && !should_run)
  {
    printf("error: missing command (expected 'build' or 'run')\n");
    arena_free(&arena);
    return 1;
  }

  // Handle invalid combinations early
  if (should_run && is_vendors)
  {
    printf("error: 'vendors' is a library and cannot be run\n");
    arena_free(&arena);
    return 1;
  }

  if (should_run && !is_game)
  {
    printf("error: only 'game' can be run\n");
    arena_free(&arena);
    return 1;
  }

  if (should_build && !is_vendors && !is_game)
  {
    printf("error: unknown build target (expected 'vendors' or 'game')\n");
    arena_free(&arena);
    return 1;
  }

  // Execute actions
  if (should_build && is_vendors)
  {
    build_vendors(&arena, is_web);
  }
  else
  {
    if (should_build && is_game)
    {
      should_run = build_game(&arena, is_web);
    }
    if (should_run && is_game)
    {
      run_game(&arena, is_web);
    }
  }

  arena_free(&arena);
  return 0;
}
