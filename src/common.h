#ifndef COMMON_H
#define COMMON_H

#include "stdlib/allocators.h"

struct Game_Context {
    struct Allocator *scene_allocator;
    int screen_width, screen_height;
};

typedef void *(*Scene_Init_Function)    (struct Game_Context *);
typedef void  (*Scene_Update_Function)  (struct Game_Context *, void *, float);
typedef void  (*Scene_Destroy_Function) (struct Game_Context *, void *);

struct Scene_Functions {
    Scene_Init_Function    init;
    Scene_Update_Function  update;
    Scene_Destroy_Function destroy;
};

typedef struct Scene_Functions Scene_Functions_T;
typedef Scene_Functions_T (*Scene_Get_Function)(void);

struct Scene {
    void *library;
    long long last_library_write_time;
    bool  is_valid;

    void *scene_data;
    struct Scene_Functions functions;
};

#endif // COMMON_H
