#include "raylib.h"

const int screen_width  = 800;
const int screen_height = 600;

int main(void) {
    InitWindow(screen_width, screen_height, "raylib [core] example - basic window");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Hello, Sailor!", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
