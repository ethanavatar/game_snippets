#include <stdio.h>
#include <stddef.h>
#include <assert.h>

#include "self_build/self_build.h"
#include "self_build/self_build.c"

#include "stdlib/win32_platform.c"
#include "stdlib/strings.c"
#include "stdlib/allocators.c"
#include "stdlib/arena.c"
#include "stdlib/thread_context.c"
#include "stdlib/managed_arena.c"
#include "stdlib/scratch_memory.c"
#include "stdlib/string_builder.c"

extern struct Build __declspec(dllexport) build(struct Build_Context *, enum Build_Kind);

struct Build build(
    struct Build_Context *context, enum Build_Kind requested_kind
) {
    struct Build stdlib = build_submodule(context, "selfbuild", Build_Kind_Shared_Library);
    struct Build raylib = build_submodule(context, "raylib", Build_Kind_Shared_Library);

    // @TODO: It would be nice to be able to "install" individual headers by copying them to the build directory.
    // And maybe dependent modules can automatically include the build directory of their dependencies
    static char *includes[] = { "src", "raylib/src" };

    // @TODO: Abstract linking libraries because depending on if the target is static or shared,
    // the flags can either be like `-lwinmm` or `winmm.lib`
    static char *link_flags[] = { "-lwinmm", "-lgdi32", "-lopengl32" };

    static char *lib_files[] = { "src/typing_text.c" };

    static struct Build lib = {
        .kind = Build_Kind_Shared_Library,
        .name = "typing_text",

        .sources          = lib_files,
        .sources_count    = sizeof(lib_files) / sizeof(char *),

        .link_flags       = link_flags,
        .link_flags_count = sizeof(link_flags) / sizeof(char *),

        .includes         = includes,
        .includes_count   = sizeof(includes) / sizeof(char *),
    };

    lib.dependencies = calloc(2, sizeof(struct Build));
    add_dependency(&lib, stdlib);
    add_dependency(&lib, raylib);
    lib.root_dir = ".";

    static char *exe_files[] = { "src/main.c" };

    static struct Build exe = {
        .kind = Build_Kind_Executable,
        .name = "game_snippets",

        .sources          = exe_files,
        .sources_count    = sizeof(exe_files) / sizeof(char *),

        .link_flags       = link_flags,
        .link_flags_count = sizeof(link_flags) / sizeof(char *),

        .includes         = includes,
        .includes_count   = sizeof(includes) / sizeof(char *),
    };

    exe.dependencies = calloc(3, sizeof(struct Build));
    add_dependency(&exe, stdlib);
    add_dependency(&exe, raylib);
    add_dependency(&exe, lib);

    return exe;
}

int main(void) {
    struct Thread_Context tctx;
    thread_context_init_and_equip(&tctx);

    struct Allocator scratch = scratch_begin();

    char *artifacts_directory = "bin";
    char *self_build_path     = "selfbuild";

    if (!win32_dir_exists(artifacts_directory)) win32_create_directories(artifacts_directory);
    char *cwd = win32_get_current_directory(&scratch);
    
    bootstrap("build.c", "build.exe", "bin/build.old", self_build_path);

    struct Build_Context context = {
        .self_build_path     = self_build_path,
        .artifacts_directory = artifacts_directory,
        .current_directory   = cwd,
    };

    struct Build module = build(&context, 0);
    module.root_dir = ".";
    build_module(&context, &module);

    scratch_end(&scratch);
    thread_context_release();
    return 0;
}
