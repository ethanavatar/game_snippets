#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include "raylib.h"

#include "stdlib/thread_context.h"
#include "stdlib/scratch_memory.h"
#include "stdlib/string_builder.h"
#include "stdlib/win32_platform.h"
#include "stdlib/strings.h"

#include "common.h"

const int screen_width  = 800;
const int screen_height = 600;

static struct Scene scene_load_from_dll(
    const char *dll_path,
    const char *temp_dll_path,
    const char *pdb_path,
    const char *temp_pdb_path
);

static void scene_unload(struct Scene *scene);

int main(void) {
    struct Thread_Context tctx;
    thread_context_init_and_equip(&tctx);
    struct Allocator persistent = scratch_begin();

    SetRandomSeed(time(0));

    InitWindow(screen_width, screen_height, "raylib [core] example - basic window");
    SetTargetFPS(60);

    struct Game_Context game = {
        .scene_allocator = &persistent,
        .screen_width    = screen_width,
        .screen_height   = screen_height,
    };

    struct Scene current_scene_info = { 0 };
    current_scene_info = scene_load_from_dll(
        "bin/typing_text.dll",
        "bin/typing_text_loaded.dll",

        "bin/typing_text.pdb",
        "bin/typing_text_loaded.pdb"
    );

    float reload_timer = 0.0f;

    struct Scene_Functions *current_scene = &current_scene_info.functions;
    void *scene_data = current_scene->init(&game);

    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();

        reload_timer += delta_time;
        if ((reload_timer >= 1.0f) || IsKeyPressed(KEY_F5)) {

            long long dll_mod_time = win32_get_file_last_modified_time("bin/typing_text.dll");

            if (dll_mod_time > current_scene_info.last_library_write_time || IsKeyPressed(KEY_F5)) {
                scene_unload(&current_scene_info);
                current_scene_info = scene_load_from_dll(
                    "bin/typing_text.dll",
                    "bin/typing_text_loaded.dll",

                    "bin/typing_text.pdb",
                    "bin/typing_text_loaded.pdb"
                );
                current_scene = &current_scene_info.functions;

                if (IsKeyPressed(KEY_F5)) {
                    current_scene->destroy(&game, scene_data);
                    scene_data = current_scene->init(&game);
                    fprintf(stderr, "Hard reloaded!\n");

                } else {
                    fprintf(stderr, "Reloaded! (%lld)\n", current_scene_info.last_library_write_time);
                }
            }

            reload_timer = 0.0f;
        }

        current_scene->update(&game, scene_data, delta_time);
    }

    current_scene->destroy(&game, scene_data);

    CloseWindow();

    scratch_end(&persistent);
    thread_context_release();
    return 0;
}

static void *empty_init   (struct Game_Context *) { return NULL; }
static void  empty_update (struct Game_Context *, void  *scene_data, float delta_time) { }
static void  empty_destroy(struct Game_Context *, void *scene_data) { }

const struct Scene_Functions EMPTY_SCENE_FUNCTIONS = {
    .init    = &empty_init,
    .update  = &empty_update,
    .destroy = &empty_destroy,
};

static struct Scene scene_load_from_dll(
    const char *dll_path, const char *temp_dll_path,
    const char *pdb_path, const char *temp_pdb_path
) {
    struct Scene scene = { 0 };
    scene.functions = EMPTY_SCENE_FUNCTIONS;
    scene.last_library_write_time = win32_get_file_last_modified_time(dll_path);

    win32_copy_file(dll_path, temp_dll_path);
    win32_copy_file(pdb_path, temp_pdb_path);

    scene.library = win32_load_library(temp_dll_path);
    if (scene.library) {
        scene.is_valid = true;
        Scene_Get_Function get_scene_functions =
            (Scene_Get_Function) win32_get_symbol_address(scene.library, "get_scene_functions");

        scene.functions = get_scene_functions();
    }

    return scene;
}

static void scene_unload(struct Scene *scene) {
    if (scene->library) {
        win32_free_library(scene->library);
        scene->library = NULL;
        scene->functions = EMPTY_SCENE_FUNCTIONS;
    }

    scene->is_valid = false;
}
