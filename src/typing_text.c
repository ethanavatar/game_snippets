#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"

#include "common.h"

#include "stdlib/allocators.h"
#include "stdlib/strings.h"

void *init   (struct Game_Context *);
void  update (struct Game_Context *, void *, float);
void  destroy(struct Game_Context *, void *);

extern struct Scene_Functions __declspec(dllexport) get_scene_functions(void);
struct Scene_Functions get_scene_functions(void) {
    return (struct Scene_Functions) {
        .init    = &init,
        .update  = &update,
        .destroy = &destroy,
    };
}

static void DrawTextBoxed(Font font, const char *text, size_t text_length, Rectangle rec, float fontSize, float spacing, bool wordWrap, Color tint);   // Draw text using font inside rectangle limits
static void DrawTextBoxedSelectable(Font font, const char *text, size_t text_length, Rectangle rec, float fontSize, float spacing, bool wordWrap, Color tint, int selectStart, int selectLength, Color selectTint, Color selectBackTint);    // Draw text using font inside rectangle limits with support for text selection

static const char *lorem2p = "Lorem ipsum odor amet, consectetuer adipiscing elit. Per nunc accumsan nostra aliquam neque hendrerit sem aliquet. Leo pretium vel molestie dis donec habitasse. Nunc velit adipiscing ante turpis sollicitudin justo vitae erat? Nam finibus libero velit auctor inceptos. Egestas gravida ultrices erat aenean, inceptos justo. Laoreet facilisis velit lectus vehicula facilisis etiam phasellus facilisis. Finibus tristique suspendisse convallis, nisl fermentum interdum inceptos. Massa ultricies sit dis magna curabitur ultrices conubia nunc sed. Duis venenatis fames nec sapien luctus pellentesque, urna tristique netus.";

enum Typing_Text_Animation_State {
    Typing_Text_Animation_State_ChooseLetter,
    Typing_Text_Animation_State_DeleteTypo,
    Typing_Text_Animation_State_FixTypo,
    Typing_Text_Animation_State_Finished,
    Typing_Text_Animation_State_COUNT,
};

struct Typing_Text {
    enum Typing_Text_Animation_State state;

    const char *source;
    size_t source_length;
    size_t cursor;

    char *workspace;

    bool had_typo;
    char correct_letter;

    float typing_delay;
    float typing_timer;
    float next_letter_speed_modifier;
};

static void typing_animation_process(struct Typing_Text *text, float delta_time);

enum Text_Skip_Mode {
    Text_Skip_Mode_FastForward,
    Text_Skip_Mode_JumpToEnd,
    Text_Skip_Mode_COUNT,
};

struct Settings {
    float text_chars_per_second;
    enum Text_Skip_Mode text_skip_mode;
};

struct Scene_Context {
    struct Typing_Text text;

    Font font;
    struct Settings settings;
    Rectangle container;
    float default_typing_delay;
};

void *init(struct Game_Context *game) {
    struct Scene_Context *self = allocator_allocate(game->scene_allocator, sizeof(struct Scene_Context));
    memset(self, 0, sizeof(struct Scene_Context));

    self->container = (Rectangle) { 25.0f, 25.0f, game->screen_width - 50.0f, game->screen_height - 250.0f };

    // Get default system font
    self->font = GetFontDefault();

    self->settings = (struct Settings) {
        .text_chars_per_second = 20,
        .text_skip_mode = Text_Skip_Mode_JumpToEnd,
    };

    self->default_typing_delay = 1.f / self->settings.text_chars_per_second;

    self->text.source  = lorem2p;
    self->text.source_length = TextLength(self->text.source);
    self->text.workspace = format_cstring(game->scene_allocator, "%s", self->text.source);
    self->text.typing_delay = self->default_typing_delay;

    return self;
}

void update(
    struct Game_Context *game,
    void *scene_context, float delta_time
) {
    struct Scene_Context *self = (struct Scene_Context *) scene_context;

    if (IsKeyPressed(KEY_R)) {
        self->text.cursor = 0;
        self->text.state  = Typing_Text_Animation_State_ChooseLetter;
    }

    BeginDrawing(); {

        ClearBackground(RAYWHITE);

        // Draw container border
        DrawRectangleLinesEx(self->container, 3, MAROON);

        // Draw text in container (add some padding)
        DrawTextBoxed(
            self->font,
            self->text.workspace, self->text.cursor,
            (Rectangle){
                self->container.x + 5,     self->container.y + 5,
                self->container.width - 5, self->container.height - 5
            }, 20.0f, 2.0f,
            true,
            GRAY
        );

        typing_animation_process(&self->text, delta_time);

        float space_bar_width  = game->screen_width / 3.f;
        float space_bar_height = 50;
        float space_bar_x = (game->screen_width / 2.f) - (space_bar_width / 2.f);
        float space_bar_y = game->screen_height - space_bar_height - 25;

        DrawRectangleLinesEx((Rectangle) {
            space_bar_x, space_bar_y + 5,
            space_bar_width, space_bar_height
        }, 3, MAROON);

        if (IsKeyDown(KEY_SPACE)) {
            DrawRectangleRec((Rectangle) {
                space_bar_x, space_bar_y + 5,
                space_bar_width, space_bar_height
            }, WHITE);

            DrawRectangleLinesEx((Rectangle) {
                space_bar_x, space_bar_y + 5,
                space_bar_width, space_bar_height
            }, 3, MAROON);

        } else {
            DrawRectangleRec((Rectangle) {
                space_bar_x, space_bar_y,
                space_bar_width, space_bar_height
            }, RAYWHITE);

            DrawRectangleLinesEx((Rectangle) {
                space_bar_x, space_bar_y,
                space_bar_width, space_bar_height
            }, 3, MAROON);
        }

        self->text.typing_delay = self->default_typing_delay;
        if (IsKeyDown(KEY_SPACE)) {
            static_assert(Text_Skip_Mode_COUNT == 2);
            if (self->settings.text_skip_mode == Text_Skip_Mode_JumpToEnd) {
                self->text.cursor = self->text.source_length;

            } else if (self->settings.text_skip_mode == Text_Skip_Mode_FastForward) {
                self->text.typing_delay /= 5.f;
            }
        }

        const char *mode_text = "<mode_text>";
        int mode_text_width = 0;
        static_assert(Text_Skip_Mode_COUNT == 2);
        if (self->settings.text_skip_mode == Text_Skip_Mode_JumpToEnd) {
            mode_text = "Mode: Jump to End";

        } else if (self->settings.text_skip_mode == Text_Skip_Mode_FastForward) {
            mode_text = "Mode: Fast Forward";
        }

        DrawText(
            mode_text,
            (game->screen_width / 2.f) - (MeasureText(mode_text, 20.f) / 2.f), space_bar_y - 100,
            20.f, GRAY
        );


        if (IsKeyPressed(KEY_TAB)) {
            self->settings.text_skip_mode = self->settings.text_skip_mode == Text_Skip_Mode_FastForward
                ? Text_Skip_Mode_JumpToEnd
                : Text_Skip_Mode_FastForward;
        }

    } EndDrawing();
}

void destroy(struct Game_Context *game_context, void *scene_context) { }

static void typing_animation_process(struct Typing_Text *text, float delta_time) {
    static_assert(Typing_Text_Animation_State_COUNT == 4);
    if (text->state == Typing_Text_Animation_State_Finished) {
        return;
    }

    bool should_type = text->typing_timer >= text->typing_delay + text->next_letter_speed_modifier;
    if (should_type) {
        text->typing_timer = 0;
        text->had_typo = false;
    }

    if (should_type && text->state == Typing_Text_Animation_State_ChooseLetter) {
        text->correct_letter = text->source[text->cursor++];
        text->next_letter_speed_modifier = GetRandomValue(-1, 1) * (text->typing_delay * 0.6f);

        int typo_distance = 0;
        bool is_typo = false;
        if (!text->had_typo) {
            typo_distance = GetRandomValue(0, 5);
            is_typo       = GetRandomValue(0, 30) == 0;
        }

        char chosen_letter = text->correct_letter;
        if (is_typo) {
            chosen_letter += typo_distance;
            text->next_letter_speed_modifier = text->typing_delay * 3.0f;
            text->state = Typing_Text_Animation_State_DeleteTypo;
        }

        if (text->cursor >= text->source_length) {
            text->state = Typing_Text_Animation_State_Finished;
        }

    } else if (should_type && text->state == Typing_Text_Animation_State_DeleteTypo) {
        text->cursor--;
        text->next_letter_speed_modifier = text->typing_delay * 2.0f;
        text->state = Typing_Text_Animation_State_FixTypo;

    } else if (should_type && text->state == Typing_Text_Animation_State_FixTypo) {
        text->workspace[text->cursor] = text->correct_letter;
        text->cursor++;
        text->state = Typing_Text_Animation_State_ChooseLetter;
        text->had_typo = true;

    }

    text->typing_timer += delta_time;
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
