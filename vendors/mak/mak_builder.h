#ifndef MAK_BUILDER_H
#define MAK_BUILDER_H

#include "arena.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef MAK_PLATFORM_WINDOWS
#define MAK_PATH_SEPARATOR '\\'
#else
#define MAK_PATH_SEPARATOR '/'
#endif

// --- Public Types ---

typedef struct {
  bool success;
  const char *message;
  const char *context;
} MakResult;

typedef struct MakBuilder MakBuilder;

// --- Public API ---

// Create a new builder (allocates from arena)
MakBuilder *mak_builder_new(ArenaAllocator *arena);

// Set compiler (default: "gcc" on Linux, "cl" on Windows)
void mak_builder_set_compiler(MakBuilder *b, const char *compiler);

// Set archiver for static libraries (default: "ar" on Linux, "lib" on Windows)
void mak_builder_set_archiver(MakBuilder *b, const char *archiver);

// Set output directory (default: "build/")
void mak_builder_set_output_dir(MakBuilder *b, const char *dir);

// Global flags (apply to all compilations)
void mak_builder_add_cflag(MakBuilder *b, const char *flag);
void mak_builder_add_include(MakBuilder *b, const char *dir);
void mak_builder_add_define(MakBuilder *b, const char *define);
void mak_builder_add_libdir(MakBuilder *b, const char *dir);
void mak_builder_add_lib(MakBuilder *b, const char *lib);

// Add source files
// NULL-terminated variadic: mak_builder_add_sources(b, "a.c", "b.c", NULL)
void mak_builder_add_sources(MakBuilder *b, ...);

// Compile all sources to object files
MakResult mak_builder_compile_objects(MakBuilder *b);

// Create static library from compiled objects
MakResult mak_builder_create_static_lib(MakBuilder *b, const char *lib_name);

// Compile and link executable in one step
MakResult mak_builder_compile_executable(MakBuilder *b, const char *exe_name);

// Execute a command
MakResult mak_cmd_exec(const char *command);

// Create the folder folder_path
MakResult mak_makedir(const char *folder_path, bool skip_if_exists);

// Concatenate two path strings
const char *mak_path_join(char *buffer, const char *a, const char *b);

// --- Implementation ---

#ifdef MAK_BUILDER_IMPLEMENTATION

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform detection
#ifdef _WIN32
#include <windows.h>
#define MAK_PLATFORM_WINDOWS
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAK_PLATFORM_UNIX
#endif

// --- Internal Structures ---

typedef struct {
  const char *source;
  const char *flags; // Per-file flags (can be NULL)
  char *object_path; // Generated after compilation
} MakSourceEntry;

typedef struct {
  char **items;
  int count;
  int capacity;
} MakStringList;

struct MakBuilder {
  ArenaAllocator *arena;

  // Configuration
  const char *compiler;
  const char *archiver;
  const char *output_dir;

  // Global flags
  MakStringList cflags;
  MakStringList includes;
  MakStringList defines;
  MakStringList libdirs;
  MakStringList libs;

  // Sources
  MakSourceEntry *sources;
  int source_count;
  int source_capacity;

  // Last error
  MakResult last_error;
};

// --- Internal Helpers ---

static void mak_string_list_init(MakStringList *list, ArenaAllocator *arena,
                                 int initial_cap) {
  list->capacity = initial_cap;
  list->count = 0;
  list->items = arena_alloc_array(arena, char *, initial_cap);
}

static void mak_string_list_add(MakStringList *list, ArenaAllocator *arena,
                                const char *str) {
  if (list->count >= list->capacity) {
    int new_cap = list->capacity * 2;
    char **new_items = arena_alloc_array(arena, char *, new_cap);
    memcpy(new_items, list->items, list->count * sizeof(char *));
    list->items = new_items;
    list->capacity = new_cap;
  }
  list->items[list->count++] = (char *)arena_strdup(arena, str);
}

static MakResult mak_ok(void) {
  MakResult r = {.success = true, .message = NULL, .context = NULL};
  return r;
}

static MakResult mak_error(const char *message, const char *context) {
  MakResult r = {.success = false, .message = message, .context = context};
  return r;
}

// Execute command with better error reporting
static bool mak_exec(const char **argv, int argc, const char **error_msg) {
  printf("[MAK] Executing: ");
  for (int i = 0; i < argc; i++) {
    printf("%s ", argv[i]);
  }
  printf("\n");

#ifdef MAK_PLATFORM_WINDOWS
  // Build command line
  size_t total_len = 0;
  for (int i = 0; i < argc; i++) {
    total_len += strlen(argv[i]) + 3; // +3 for quotes and space
  }

  char *cmd_line = malloc(total_len + 1);
  if (!cmd_line) {
    *error_msg = "Memory allocation failed";
    return false;
  }

  cmd_line[0] = '\0';
  for (int i = 0; i < argc; i++) {
    if (i > 0)
      strcat(cmd_line, " ");
    strcat(cmd_line, "\"");
    strcat(cmd_line, argv[i]);
    strcat(cmd_line, "\"");
  }

  STARTUPINFOA si = {0};
  PROCESS_INFORMATION pi = {0};
  si.cb = sizeof(si);

  BOOL success = CreateProcessA(NULL, cmd_line, NULL, NULL, FALSE, 0, NULL,
                                NULL, &si, &pi);
  free(cmd_line);

  if (!success) {
    *error_msg = "Failed to create process";
    return false;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exit_code;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  if (exit_code != 0) {
    *error_msg = "Command failed with non-zero exit code";
    return false;
  }

#else // UNIX
  char **exec_argv = malloc((argc + 1) * sizeof(char *));
  if (!exec_argv) {
    *error_msg = "Memory allocation failed";
    return false;
  }

  for (int i = 0; i < argc; i++) {
    exec_argv[i] = (char *)argv[i];
  }
  exec_argv[argc] = NULL;

  pid_t pid = fork();
  if (pid == -1) {
    free(exec_argv);
    *error_msg = "Fork failed";
    return false;
  }

  if (pid == 0) {
    execvp(exec_argv[0], exec_argv);
    fprintf(stderr, "[MAK] Error: execvp failed for '%s': %s\n", exec_argv[0],
            strerror(errno));
    exit(127);
  }

  free(exec_argv);

  int status;
  waitpid(pid, &status, 0);

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    *error_msg = "Command failed";
    return false;
  }
#endif

  return true;
}

// Build argument list for compilation
static int mak_build_compile_args(MakBuilder *b, const char *source,
                                  const char *per_file_flags,
                                  const char *output, const char **argv,
                                  int max_args) {
  int argc = 0;

  argv[argc++] = b->compiler;

  // Global cflags
  for (int i = 0; i < b->cflags.count && argc < max_args; i++) {
    argv[argc++] = b->cflags.items[i];
  }

  // Per-file flags
  if (per_file_flags) {
    // Parse per-file flags (simple space splitting)
    static char flag_buf[256];
    const char *p = per_file_flags;
    while (*p && argc < max_args) {
      while (*p && isspace(*p))
        p++;
      if (!*p)
        break;

      const char *start = p;
      while (*p && !isspace(*p))
        p++;

      size_t len = p - start;
      if (len >= sizeof(flag_buf))
        len = sizeof(flag_buf) - 1;
      memcpy(flag_buf, start, len);
      flag_buf[len] = '\0';

      argv[argc++] = flag_buf;
    }
  }

  // Includes
  for (int i = 0; i < b->includes.count && argc < max_args; i++) {
    argv[argc++] = b->includes.items[i];
  }

  // Defines
  for (int i = 0; i < b->defines.count && argc < max_args; i++) {
    argv[argc++] = b->defines.items[i];
  }

  argv[argc++] = "-c";
  argv[argc++] = source;
  argv[argc++] = "-o";
  argv[argc++] = output;

  return argc;
}

// Get basename from path
static const char *mak_basename(const char *path) {
  const char *base = strrchr(path, '/');
  if (!base)
    base = strrchr(path, '\\');
  return base ? base + 1 : path;
}

// --- Public Implementation ---

MakBuilder *mak_builder_new(ArenaAllocator *arena) {
  MakBuilder *b = arena_alloc_type(arena, MakBuilder);
  if (!b)
    return NULL;

  memset(b, 0, sizeof(*b));
  b->arena = arena;

  // Set defaults
#ifdef MAK_PLATFORM_WINDOWS
  b->compiler = "cl";
  b->archiver = "lib";
#else
  b->compiler = "gcc";
  b->archiver = "ar";
#endif
  b->output_dir = "build/";

  // Initialize lists
  mak_string_list_init(&b->cflags, arena, 8);
  mak_string_list_init(&b->includes, arena, 8);
  mak_string_list_init(&b->defines, arena, 8);
  mak_string_list_init(&b->libdirs, arena, 8);
  mak_string_list_init(&b->libs, arena, 8);

  // Initialize sources
  b->source_capacity = 16;
  b->sources = arena_alloc_array(arena, MakSourceEntry, b->source_capacity);

  return b;
}

void mak_builder_set_compiler(MakBuilder *b, const char *compiler) {
  b->compiler = compiler;
}

void mak_builder_set_archiver(MakBuilder *b, const char *archiver) {
  b->archiver = archiver;
}

void mak_builder_set_output_dir(MakBuilder *b, const char *dir) {
  b->output_dir = dir;
}

void mak_builder_add_cflag(MakBuilder *b, const char *flag) {
  mak_string_list_add(&b->cflags, b->arena, flag);
}

void mak_builder_add_include(MakBuilder *b, const char *dir) {
  // Build -I flag
  char buf[512];
  snprintf(buf, sizeof(buf), "-I%s", dir);
  mak_string_list_add(&b->includes, b->arena, buf);
}

void mak_builder_add_define(MakBuilder *b, const char *define) {
  char buf[256];
  snprintf(buf, sizeof(buf), "-D%s", define);
  mak_string_list_add(&b->defines, b->arena, buf);
}

void mak_builder_add_libdir(MakBuilder *b, const char *dir) {
  char buf[512];
  snprintf(buf, sizeof(buf), "-L%s", dir);
  mak_string_list_add(&b->libdirs, b->arena, buf);
}

void mak_builder_add_lib(MakBuilder *b, const char *lib) {
  char buf[256];
  snprintf(buf, sizeof(buf), "-l%s", lib);
  mak_string_list_add(&b->libs, b->arena, buf);
}

// Add one or more source files (NULL-terminated variadic)
void mak_builder_add_sources(MakBuilder *b, ...) {
  va_list args;
  va_start(args, b);

  const char *source;
  while ((source = va_arg(args, const char *)) != NULL) {
    // Ensure capacity
    if (b->source_count >= b->source_capacity) {
      int new_cap = b->source_capacity * 2;
      MakSourceEntry *new_sources =
          arena_alloc_array(b->arena, MakSourceEntry, new_cap);
      memcpy(new_sources, b->sources, b->source_count * sizeof(MakSourceEntry));
      b->sources = new_sources;
      b->source_capacity = new_cap;
    }

    // Add source
    MakSourceEntry *entry = &b->sources[b->source_count++];
    entry->source = (char *)arena_strdup(b->arena, source);
    entry->object_path = NULL;
  }

  va_end(args);
}

MakResult mak_builder_compile_objects(MakBuilder *b) {
  printf("[MAK] Compiling %d source files...\n", b->source_count);
  for (int i = 0; i < b->source_count; i++) {
    MakSourceEntry *entry = &b->sources[i];
    // Generate object file path
    char obj_path[512];
    const char *base = mak_basename(entry->source);
    snprintf(obj_path, sizeof(obj_path), "%s%s", b->output_dir, base);

    // Replace extension with .o (find the LAST dot in the basename part)
    char *dot = strrchr(obj_path + strlen(b->output_dir), '.');
    if (dot) {
      strcpy(dot, ".o");
    } else {
      // No extension found, just append .o
      strcat(obj_path, ".o");
    }

    entry->object_path = (char *)arena_strdup(b->arena, obj_path);
    // Build compile command
    const char *argv[128];
    int argc = mak_build_compile_args(b, entry->source, entry->flags,
                                      entry->object_path, argv, 128);
    const char *error_msg = NULL;
    if (!mak_exec(argv, argc, &error_msg)) {
      return mak_error(error_msg, entry->source);
    }
  }
  printf("[MAK] Compilation successful!\n");
  return mak_ok();
}

MakResult mak_builder_create_static_lib(MakBuilder *b, const char *lib_name) {
  printf("[MAK] Creating static library: %s\n", lib_name);

  // Build library path
  char lib_path[512];
  snprintf(lib_path, sizeof(lib_path), "%s%s", b->output_dir, lib_name);

  // Build archiver command
  const char *argv[256];
  int argc = 0;

  argv[argc++] = b->archiver;

#ifdef MAK_PLATFORM_WINDOWS
  argv[argc++] = "/OUT:";
  argv[argc++] = lib_path;
#else
  argv[argc++] = "rcs";
  argv[argc++] = lib_path;
#endif

  for (int i = 0; i < b->source_count && argc < 256; i++) {
    if (b->sources[i].object_path) {
      argv[argc++] = b->sources[i].object_path;
    }
  }

  const char *error_msg = NULL;
  if (!mak_exec(argv, argc, &error_msg)) {
    return mak_error(error_msg, lib_name);
  }

  printf("[MAK] Static library created successfully!\n");
  return mak_ok();
}

MakResult mak_builder_compile_executable(MakBuilder *b, const char *exe_name) {
  printf("[MAK] Compiling executable: %s\n", exe_name);

  // Build executable path
  char exe_path[512];
#ifdef MAK_PLATFORM_WINDOWS
  snprintf(exe_path, sizeof(exe_path), "%s%s.exe", b->output_dir, exe_name);
#else
  snprintf(exe_path, sizeof(exe_path), "%s%s", b->output_dir, exe_name);
#endif

  // Build compile command
  const char *argv[256];
  int argc = 0;

  argv[argc++] = b->compiler;

  // Global cflags
  for (int i = 0; i < b->cflags.count && argc < 256; i++) {
    argv[argc++] = b->cflags.items[i];
  }

  // Includes
  for (int i = 0; i < b->includes.count && argc < 256; i++) {
    argv[argc++] = b->includes.items[i];
  }

  // Defines
  for (int i = 0; i < b->defines.count && argc < 256; i++) {
    argv[argc++] = b->defines.items[i];
  }

  argv[argc++] = "-o";
  argv[argc++] = exe_path;

  // Source files
  for (int i = 0; i < b->source_count && argc < 256; i++) {
    argv[argc++] = b->sources[i].source;
  }

  // Library dirs
  for (int i = 0; i < b->libdirs.count && argc < 256; i++) {
    argv[argc++] = b->libdirs.items[i];
  }

  // Libraries
  for (int i = 0; i < b->libs.count && argc < 256; i++) {
    argv[argc++] = b->libs.items[i];
  }

  const char *error_msg = NULL;
  if (!mak_exec(argv, argc, &error_msg)) {
    return mak_error(error_msg, exe_name);
  }

  printf("[MAK] Executable created successfully!\n");
  return mak_ok();
}

MakResult mak_cmd_exec(const char *command) {
  if (!command || !*command)
    return mak_error("Invalid command", command);

  printf("[MAK] Running command: %s\n", command);

  // Simple space-splitting for arguments
  const char *argv[64];
  int argc = 0;
  static char buf[512];
  strncpy(buf, command, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *token = strtok(buf, " ");
  while (token && argc < 64) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }

  if (argc == 0)
    return mak_error("Empty command", command);

  const char *error_msg = NULL;
  if (!mak_exec(argv, argc, &error_msg)) {
    return mak_error(error_msg, command);
  }

  return mak_ok();
}

MakResult mak_makedir(const char *folder_path, bool skip_if_exists) {
  if (!folder_path || !*folder_path)
    return mak_error("Invalid folder path", folder_path);

  printf("[MAK] Creating directory: %s\n", folder_path);

#ifdef MAK_PLATFORM_WINDOWS
  DWORD attrs = GetFileAttributesA(folder_path);
  if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
    if (skip_if_exists) {
      // Directory already exists, skipping creation
      return mak_ok();
    }
  }

  BOOL result = CreateDirectoryA(folder_path, NULL);
  if (!result) {
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS && skip_if_exists) {
      return mak_ok();
    }
    if (err != ERROR_ALREADY_EXISTS) {
      return mak_error("Failed to create directory", folder_path);
    }
  }

#else
  struct stat st;
  if (stat(folder_path, &st) == 0 && S_ISDIR(st.st_mode)) {
    if (skip_if_exists) {
      // Directory already exists, skipping creation
      return mak_ok();
    }
  }

  // mode 0755
  int ret = mkdir(folder_path, 0755);
  if (ret != 0) {
    if (errno == EEXIST && skip_if_exists) {
      return mak_ok();
    }
    return mak_error(strerror(errno), folder_path);
  }
#endif

  return mak_ok();
}

const char *mak_path_join(char *buffer, const char *a, const char *b) {
  if (!a || !b || !buffer)
    return NULL;

  // Copy first part
  size_t len_a = strlen(a);
  bool a_has_sep = len_a > 0 && (a[len_a - 1] == '/' || a[len_a - 1] == '\\');
  strcpy(buffer, a);

  // Add separator if missing at end of a
  if (!a_has_sep) {
    buffer[len_a] = MAK_PATH_SEPARATOR;
    buffer[len_a + 1] = '\0';
  }

  // Concatenate second part
  size_t len_buf = strlen(buffer);
  size_t len_b = strlen(b);
  bool b_has_leading_sep = len_b > 0 && (b[0] == '/' || b[0] == '\\');

  if (b_has_leading_sep) {
    // skip the leading separator in b to avoid double separator
    strcat(buffer, b + 1);
  } else {
    strcat(buffer, b);
  }

  // Optional: ensure result ends with separator
  len_buf = strlen(buffer);
  if (len_buf > 0 && buffer[len_buf - 1] != '/' &&
      buffer[len_buf - 1] != '\\') {
    buffer[len_buf] = MAK_PATH_SEPARATOR;
    buffer[len_buf + 1] = '\0';
  }

  return buffer;
}

#endif // MAK_BUILDER_IMPLEMENTATION
#endif // MAK_BUILDER_H
