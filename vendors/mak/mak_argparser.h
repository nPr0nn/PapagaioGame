#ifndef MAK_ARGPARSER_H
#define MAK_ARGPARSER_H

#include "arena.h"
#include <stdbool.h>

// --- Public Types ---

typedef enum {
  MAK_ARG_BOOL,   // Flag: --verbose, -v
  MAK_ARG_STRING, // Value: --output <path>, -o <path>
  MAK_ARG_INT,    // Value: --jobs <n>, -j <n>
} MakArgType;

typedef struct {
  const char *name; // Long name (e.g., "verbose")
  char short_name;  // Short name (e.g., 'v'), 0 if none
  MakArgType type;
  const char *description;
  const char *default_value; // For string/int types
  bool required;
} MakArgSpec;

typedef struct MakArgParser MakArgParser;

// --- Public API ---

// Create new argument parser
MakArgParser *mak_argparser_new(ArenaAllocator *arena, const char *program_name,
                                const char *description);

// Add argument specification
void mak_argparser_add(MakArgParser *parser, MakArgSpec spec);

// Parse command line arguments
bool mak_argparser_parse(MakArgParser *parser, int argc, char **argv);

// Returns true if a boolean flag (--verbose, -v) was explicitly set
bool mak_argparser_has_flag(MakArgParser *parser, const char *name);

// Returns true if a specific positional command (e.g., "build") was passed
bool mak_argparser_has_command(MakArgParser *parser, const char *command);

// Get parsed values
bool mak_argparser_get_bool(MakArgParser *parser, const char *name);
const char *mak_argparser_get_string(MakArgParser *parser, const char *name);
int mak_argparser_get_int(MakArgParser *parser, const char *name);

// Get positional arguments (after all flags)
const char **mak_argparser_get_positional(MakArgParser *parser, int *count);

// Print help message
void mak_argparser_print_help(MakArgParser *parser);

// Print error message
void mak_argparser_print_error(MakArgParser *parser);

// --- Implementation ---

#ifdef MAK_ARGPARSER_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  MakArgSpec spec;
  bool is_set;
  union {
    bool bool_val;
    const char *string_val;
    int int_val;
  } value;
} MakParsedArg;

struct MakArgParser {
  ArenaAllocator *arena;
  const char *program_name;
  const char *description;

  MakParsedArg *args;
  int arg_count;
  int arg_capacity;

  const char **positional;
  int positional_count;
  int positional_capacity;

  const char *error_message;
};

// --- Internal Helpers ---

static MakParsedArg *mak_find_arg(MakArgParser *parser, const char *name) {
  for (int i = 0; i < parser->arg_count; i++) {
    if (strcmp(parser->args[i].spec.name, name) == 0) {
      return &parser->args[i];
    }
  }
  return NULL;
}

static MakParsedArg *mak_find_arg_by_short(MakArgParser *parser, char c) {
  for (int i = 0; i < parser->arg_count; i++) {
    if (parser->args[i].spec.short_name == c) {
      return &parser->args[i];
    }
  }
  return NULL;
}

static void mak_add_positional(MakArgParser *parser, const char *arg) {
  if (parser->positional_count >= parser->positional_capacity) {
    int new_cap = parser->positional_capacity * 2;
    const char **new_pos =
        arena_alloc_array(parser->arena, const char *, new_cap);
    if (parser->positional_count > 0) {
      memcpy(new_pos, parser->positional,
             parser->positional_count * sizeof(const char *));
    }
    parser->positional = new_pos;
    parser->positional_capacity = new_cap;
  }
  parser->positional[parser->positional_count++] = arg;
}

// --- Public Implementation ---

MakArgParser *mak_argparser_new(ArenaAllocator *arena, const char *program_name,
                                const char *description) {
  MakArgParser *parser = arena_alloc_type(arena, MakArgParser);
  if (!parser)
    return NULL;

  memset(parser, 0, sizeof(*parser));
  parser->arena = arena;
  parser->program_name = program_name;
  parser->description = description;

  parser->arg_capacity = 16;
  parser->args = arena_alloc_array(arena, MakParsedArg, parser->arg_capacity);

  parser->positional_capacity = 16;
  parser->positional =
      arena_alloc_array(arena, const char *, parser->positional_capacity);

  return parser;
}

void mak_argparser_add(MakArgParser *parser, MakArgSpec spec) {
  if (parser->arg_count >= parser->arg_capacity) {
    int new_cap = parser->arg_capacity * 2;
    MakParsedArg *new_args =
        arena_alloc_array(parser->arena, MakParsedArg, new_cap);
    memcpy(new_args, parser->args, parser->arg_count * sizeof(MakParsedArg));
    parser->args = new_args;
    parser->arg_capacity = new_cap;
  }

  MakParsedArg *arg = &parser->args[parser->arg_count++];
  arg->spec = spec;
  arg->is_set = false;

  // Set default values
  if (spec.type == MAK_ARG_BOOL) {
    arg->value.bool_val = false;
  } else if (spec.type == MAK_ARG_STRING && spec.default_value) {
    arg->value.string_val = spec.default_value;
  } else if (spec.type == MAK_ARG_INT && spec.default_value) {
    arg->value.int_val = atoi(spec.default_value);
  }
}

bool mak_argparser_parse(MakArgParser *parser, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];

    // Check for long option (--name)
    if (arg[0] == '-' && arg[1] == '-') {
      const char *name = arg + 2;

      // Check for --name=value format
      const char *equals = strchr(name, '=');
      char name_buf[64];
      if (equals) {
        size_t len = equals - name;
        if (len >= sizeof(name_buf))
          len = sizeof(name_buf) - 1;
        memcpy(name_buf, name, len);
        name_buf[len] = '\0';
        name = name_buf;
      }

      MakParsedArg *parsed = mak_find_arg(parser, name);
      if (!parsed) {
        char error_buf[256];
        snprintf(error_buf, sizeof(error_buf), "Unknown option: --%s", name);
        parser->error_message = arena_strdup(parser->arena, error_buf);
        return false;
      }

      if (parsed->spec.type == MAK_ARG_BOOL) {
        parsed->value.bool_val = true;
        parsed->is_set = true;
      } else {
        // Get value
        const char *value = NULL;
        if (equals) {
          value = equals + 1;
        } else if (i + 1 < argc) {
          value = argv[++i];
        } else {
          char error_buf[256];
          snprintf(error_buf, sizeof(error_buf), "Option --%s requires a value",
                   name);
          parser->error_message = arena_strdup(parser->arena, error_buf);
          return false;
        }

        if (parsed->spec.type == MAK_ARG_STRING) {
          parsed->value.string_val = value;
        } else if (parsed->spec.type == MAK_ARG_INT) {
          parsed->value.int_val = atoi(value);
        }
        parsed->is_set = true;
      }
    }
    // Check for short option (-n)
    else if (arg[0] == '-' && arg[1] != '\0' && arg[1] != '-') {
      // Handle multiple short flags like -abc
      for (int j = 1; arg[j] != '\0'; j++) {
        char c = arg[j];
        MakParsedArg *parsed = mak_find_arg_by_short(parser, c);

        if (!parsed) {
          char error_buf[256];
          snprintf(error_buf, sizeof(error_buf), "Unknown option: -%c", c);
          parser->error_message = arena_strdup(parser->arena, error_buf);
          return false;
        }

        if (parsed->spec.type == MAK_ARG_BOOL) {
          parsed->value.bool_val = true;
          parsed->is_set = true;
        } else {
          // Value must be next argument or after '='
          const char *value = NULL;
          if (arg[j + 1] == '=') {
            value = arg + j + 2;
            j = strlen(arg); // Break loop
          } else if (arg[j + 1] != '\0') {
            value = arg + j + 1;
            j = strlen(arg); // Break loop
          } else if (i + 1 < argc) {
            value = argv[++i];
          } else {
            char error_buf[256];
            snprintf(error_buf, sizeof(error_buf),
                     "Option -%c requires a value", c);
            parser->error_message = arena_strdup(parser->arena, error_buf);
            return false;
          }

          if (parsed->spec.type == MAK_ARG_STRING) {
            parsed->value.string_val = value;
          } else if (parsed->spec.type == MAK_ARG_INT) {
            parsed->value.int_val = atoi(value);
          }
          parsed->is_set = true;
        }
      }
    }
    // Positional argument
    else {
      mak_add_positional(parser, arg);
    }
  }

  // Check required arguments
  for (int i = 0; i < parser->arg_count; i++) {
    if (parser->args[i].spec.required && !parser->args[i].is_set) {
      char error_buf[256];
      snprintf(error_buf, sizeof(error_buf), "Required option missing: --%s",
               parser->args[i].spec.name);
      parser->error_message = arena_strdup(parser->arena, error_buf);
      return false;
    }
  }

  return true;
}

bool mak_argparser_has_flag(MakArgParser *parser, const char *name) {
  MakParsedArg *arg = mak_find_arg(parser, name);
  if (!arg || arg->spec.type != MAK_ARG_BOOL)
    return false;
  return arg->is_set && arg->value.bool_val;
}

bool mak_argparser_has_command(MakArgParser *parser, const char *command) {
  for (int i = 0; i < parser->positional_count; i++) {
    if (strcmp(parser->positional[i], command) == 0)
      return true;
  }
  return false;
}

bool mak_argparser_get_bool(MakArgParser *parser, const char *name) {
  MakParsedArg *arg = mak_find_arg(parser, name);
  if (!arg || arg->spec.type != MAK_ARG_BOOL)
    return false;
  return arg->value.bool_val;
}

const char *mak_argparser_get_string(MakArgParser *parser, const char *name) {
  MakParsedArg *arg = mak_find_arg(parser, name);
  if (!arg || arg->spec.type != MAK_ARG_STRING)
    return NULL;
  return arg->value.string_val;
}

int mak_argparser_get_int(MakArgParser *parser, const char *name) {
  MakParsedArg *arg = mak_find_arg(parser, name);
  if (!arg || arg->spec.type != MAK_ARG_INT)
    return 0;
  return arg->value.int_val;
}

const char **mak_argparser_get_positional(MakArgParser *parser, int *count) {
  *count = parser->positional_count;
  return parser->positional;
}

void mak_argparser_print_help(MakArgParser *parser) {
  printf("Usage: %s [OPTIONS]\n", parser->program_name);
  if (parser->description) {
    printf("\n%s\n", parser->description);
  }

  if (parser->arg_count > 0) {
    printf("\nOptions:\n");
    for (int i = 0; i < parser->arg_count; i++) {
      MakParsedArg *arg = &parser->args[i];

      // Print short name
      if (arg->spec.short_name) {
        printf("  -%c, ", arg->spec.short_name);
      } else {
        printf("      ");
      }

      // Print long name
      printf("--%-20s", arg->spec.name);

      // Print description
      if (arg->spec.description) {
        printf("  %s", arg->spec.description);
      }

      // Print default value
      if (arg->spec.default_value) {
        printf(" (default: %s)", arg->spec.default_value);
      }

      // Print required marker
      if (arg->spec.required) {
        printf(" [REQUIRED]");
      }

      printf("\n");
    }
  }

  printf("\n");
}

void mak_argparser_print_error(MakArgParser *parser) {
  if (parser->error_message) {
    fprintf(stderr, "Error: %s\n\n", parser->error_message);
    mak_argparser_print_help(parser);
  }
}

#endif // MAK_ARGPARSER_IMPLEMENTATION
#endif // MAK_ARGPARSER_H
