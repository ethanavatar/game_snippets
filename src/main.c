#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include "raylib.h"

#include "stdlib/thread_context.h"
#include "stdlib/scratch_memory.h"
#include "stdlib/string_builder.h"

const int screen_width  = 800;
const int screen_height = 600;

static void DrawTextBoxed(Font font, const char *text, size_t text_length, Rectangle rec, float fontSize, float spacing, bool wordWrap, Color tint);   // Draw text using font inside rectangle limits
static void DrawTextBoxedSelectable(Font font, const char *text, size_t text_length, Rectangle rec, float fontSize, float spacing, bool wordWrap, Color tint, int selectStart, int selectLength, Color selectTint, Color selectBackTint);    // Draw text using font inside rectangle limits with support for text selection

const char *lorem2p = "Lorem ipsum odor amet, consectetuer adipiscing elit. Per nunc accumsan nostra aliquam neque hendrerit sem aliquet. Leo pretium vel molestie dis donec habitasse. Nunc velit adipiscing ante turpis sollicitudin justo vitae erat? Nam finibus libero velit auctor inceptos. Egestas gravida ultrices erat aenean, inceptos justo. Laoreet facilisis velit lectus vehicula facilisis etiam phasellus facilisis. Finibus tristique suspendisse convallis, nisl fermentum interdum inceptos. Massa ultricies sit dis magna curabitur ultrices conubia nunc sed. Duis venenatis fames nec sapien luctus pellentesque, urna tristique netus.";

enum Typing_Text_Animation_State {
    Typing_Text_Animation_State_ChooseLetter,
    Typing_Text_Animation_State_DeleteTypo,
    Typing_Text_Animation_State_FixTypo,
    Typing_Text_Animation_State_Finished,
    Typing_Text_Animation_State_COUNT,
};

struct Typing_Text {
    enum Typing_Text_Animation_State state;
    char *source;
    size_t source_length;
    size_t cursor;

    char correct_letter;

    float typing_delay;
    float typing_timer;

    bool had_typo;

    float next_letter_speed_modifier;

    struct String_Builder builder;
};

void typing_animation_process(struct Typing_Text *text, float delta_time) {
    static_assert(Typing_Text_Animation_State_COUNT == 4);

    if (text->typing_timer >= text->typing_delay + text->next_letter_speed_modifier) {
        text->typing_timer = 0;
        text->had_typo = false;

        if (text->state == Typing_Text_Animation_State_ChooseLetter) {
            text->correct_letter = text->source[text->cursor++];
            text->next_letter_speed_modifier = GetRandomValue(-1, 1) * (text->typing_delay * 0.6f);

            int typo_distance = 0;
            bool is_typo = false;
            if (!text->had_typo) {
                typo_distance = GetRandomValue(0, 5);
                is_typo = GetRandomValue(0, 25) == 0;
            }

            char chosen_letter = text->correct_letter;
            if (is_typo) {
                chosen_letter += typo_distance;
                text->next_letter_speed_modifier = text->typing_delay * 3.0f;
                text->state = Typing_Text_Animation_State_DeleteTypo;
            }

            string_builder_append(&text->builder, "%c", chosen_letter);

            if (text->cursor >= text->source_length) {
                text->state = Typing_Text_Animation_State_Finished;
            }

        } else if (text->state == Typing_Text_Animation_State_DeleteTypo) {
            text->cursor--;
            text->next_letter_speed_modifier = text->typing_delay * 2.0f;
            text->state = Typing_Text_Animation_State_FixTypo;

        } else if (text->state == Typing_Text_Animation_State_FixTypo) {
            text->builder.buffer[text->builder.length - 1] = text->correct_letter;
            text->cursor++;
            text->state = Typing_Text_Animation_State_ChooseLetter;
            text->had_typo = true;

        } else if (text->state == Typing_Text_Animation_State_Finished) {

        }
    }

    text->typing_timer += delta_time;
}

int main(void) {

    struct Thread_Context tctx;
    thread_context_init_and_equip(&tctx);
    struct Allocator persistent = scratch_begin();

    SetRandomSeed(time(0));

    InitWindow(screen_width, screen_height, "raylib [core] example - basic window");
    SetTargetFPS(60);

    Rectangle container = { 25.0f, 25.0f, screen_width - 50.0f, screen_height - 250.0f };

    // Get default system font
    Font font = GetFontDefault();

    bool word_wrap = true;

    struct Typing_Text text = { 0 };
    text.source  = lorem2p;
    text.source_length = TextLength(text.source);
    text.builder = string_builder_create(&persistent, 0);
    text.typing_delay = 0.05f;

    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();

        if (IsKeyPressed(KEY_R)) {
            text.cursor = 0;
            text.state  = Typing_Text_Animation_State_ChooseLetter;
            string_builder_clear(&text.builder);
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Draw container border
            DrawRectangleLinesEx(container, 3, MAROON);

            struct String_View view = string_builder_as_string(&text.builder);

            // Draw text in container (add some padding)
            DrawTextBoxed(
                font,
                view.data, text.cursor,
                (Rectangle){
                    container.x + 4,
                    container.y + 4,
                    container.width - 4,
                    container.height - 4
                }, 20.0f, 2.0f,
                word_wrap,
                GRAY
            );

            typing_animation_process(&text, delta_time);
        EndDrawing();
    }

    CloseWindow();

    scratch_end(&persistent);
    thread_context_release();
    return 0;
}

// Draw text using font inside rectangle limits
static void DrawTextBoxed(
    Font font,
    const char *text, size_t text_length,
    Rectangle rec, float fontSize, float spacing, bool wordWrap, Color tint)
{
    DrawTextBoxedSelectable(font, text, text_length, rec, fontSize, spacing, wordWrap, tint, 0, 0, WHITE, WHITE);
}

// Draw text using font inside rectangle limits with support for text selection
static void DrawTextBoxedSelectable(
    Font font,
    const char *text, size_t text_length,
    Rectangle rec,
    float fontSize, float spacing, bool wordWrap,
    Color tint,
    int selectStart, int selectLength,
    Color selectTint, Color selectBackTint
) {
    float textOffsetY = 0;          // Offset between lines (on line break '\n')
    float textOffsetX = 0.0f;       // Offset X to next character to draw

    float scaleFactor = fontSize/(float)font.baseSize;     // Character rectangle scaling factor

    // Word/character wrapping mechanism variables
    enum { MEASURE_STATE = 0, DRAW_STATE = 1 };
    int state = wordWrap? MEASURE_STATE : DRAW_STATE;

    int startLine = -1;         // Index where to begin drawing (where a line begins)
    int endLine = -1;           // Index where to stop drawing (where a line ends)
    int lastk = -1;             // Holds last value of the character position

    for (int i = 0, k = 0; i < text_length; i++, k++)
    {
        // Get next codepoint from byte string and glyph index in font
        int codepointByteCount = 0;
        int codepoint = GetCodepoint(&text[i], &codepointByteCount);
        int index = GetGlyphIndex(font, codepoint);

        // NOTE: Normally we exit the decoding sequence as soon as a bad byte is found (and return 0x3f)
        // but we need to draw all of the bad bytes using the '?' symbol moving one byte
        if (codepoint == 0x3f) codepointByteCount = 1;
        i += (codepointByteCount - 1);

        float glyphWidth = 0;
        if (codepoint != '\n')
        {
            glyphWidth = (font.glyphs[index].advanceX == 0) ? font.recs[index].width*scaleFactor : font.glyphs[index].advanceX*scaleFactor;

            if (i + 1 < text_length) glyphWidth = glyphWidth + spacing;
        }

        // NOTE: When wordWrap is ON we first measure how much of the text we can draw before going outside of the rec container
        // We store this info in startLine and endLine, then we change states, draw the text between those two variables
        // and change states again and again recursively until the end of the text (or until we get outside of the container).
        // When wordWrap is OFF we don't need the measure state so we go to the drawing state immediately
        // and begin drawing on the next line before we can get outside the container.
        if (state == MEASURE_STATE)
        {
            // TODO: There are multiple types of spaces in UNICODE, maybe it's a good idea to add support for more
            // Ref: http://jkorpela.fi/chars/spaces.html
            if ((codepoint == ' ') || (codepoint == '\t') || (codepoint == '\n')) endLine = i;

            if ((textOffsetX + glyphWidth) > rec.width)
            {
                endLine = (endLine < 1)? i : endLine;
                if (i == endLine) endLine -= codepointByteCount;
                if ((startLine + codepointByteCount) == endLine) endLine = (i - codepointByteCount);

                state = !state;
            }
            else if ((i + 1) == text_length)
            {
                endLine = i;
                state = !state;
            }
            else if (codepoint == '\n') state = !state;

            if (state == DRAW_STATE)
            {
                textOffsetX = 0;
                i = startLine;
                glyphWidth = 0;

                // Save character position when we switch states
                int tmp = lastk;
                lastk = k - 1;
                k = tmp;
            }
        }
        else
        {
            if (codepoint == '\n')
            {
                if (!wordWrap)
                {
                    textOffsetY += (font.baseSize + font.baseSize/2)*scaleFactor;
                    textOffsetX = 0;
                }
            }
            else
            {
                if (!wordWrap && ((textOffsetX + glyphWidth) > rec.width))
                {
                    textOffsetY += (font.baseSize + font.baseSize/2)*scaleFactor;
                    textOffsetX = 0;
                }

                // When text overflows rectangle height limit, just stop drawing
                if ((textOffsetY + font.baseSize*scaleFactor) > rec.height) break;

                // Draw selection background
                bool isGlyphSelected = false;
                if ((selectStart >= 0) && (k >= selectStart) && (k < (selectStart + selectLength)))
                {
                    DrawRectangleRec((Rectangle){ rec.x + textOffsetX - 1, rec.y + textOffsetY, glyphWidth, (float)font.baseSize*scaleFactor }, selectBackTint);
                    isGlyphSelected = true;
                }

                // Draw current character glyph
                if ((codepoint != ' ') && (codepoint != '\t'))
                {
                    DrawTextCodepoint(font, codepoint, (Vector2){ rec.x + textOffsetX, rec.y + textOffsetY }, fontSize, isGlyphSelected? selectTint : tint);
                }
            }

            if (wordWrap && (i == endLine))
            {
                textOffsetY += (font.baseSize + font.baseSize/2)*scaleFactor;
                textOffsetX = 0;
                startLine = endLine;
                endLine = -1;
                glyphWidth = 0;
                selectStart += lastk - k;
                k = lastk;

                state = !state;
            }
        }

        if ((textOffsetX != 0) || (codepoint != ' ')) textOffsetX += glyphWidth;  // avoid leading spaces
    }
}
