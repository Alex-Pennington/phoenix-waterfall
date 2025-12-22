/**
 * @file ui_core.h
 * @brief Core SDL2/TTF rendering and window management
 * 
 * Simplified from phoenix_sdr_controller for waterfall display.
 */

#ifndef UI_CORE_H
#define UI_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include <SDL_ttf.h>

/* UI Colors (SDRuno-inspired dark theme) */
#define COLOR_BG_DARK       0x1A1A2EFF
#define COLOR_BG_PANEL      0x16213EFF
#define COLOR_BG_WIDGET     0x0F3460FF
#define COLOR_ACCENT        0x00D9FFFF
#define COLOR_ACCENT_DIM    0x007799FF
#define COLOR_TEXT          0xE8E8E8FF
#define COLOR_TEXT_DIM      0x888888FF
#define COLOR_GREEN         0x00FF88FF
#define COLOR_RED           0xFF4444FF
#define COLOR_ORANGE        0xFFA500FF
#define COLOR_YELLOW        0xFFFF00FF
#define COLOR_BUTTON        0x2D4A7CFF
#define COLOR_BUTTON_HOVER  0x3D5A8CFF
#define COLOR_BUTTON_ACTIVE 0x4D6A9CFF
#define COLOR_SLIDER_BG     0x333355FF
#define COLOR_SLIDER_FG     0x00AAFFFF
#define COLOR_INPUT_BG      0x222244FF
#define COLOR_INPUT_BORDER  0x444466FF
#define COLOR_INPUT_FOCUS   0x00AAFFFF

/* Font sizes */
#define FONT_SIZE_SMALL  11
#define FONT_SIZE_NORMAL 13
#define FONT_SIZE_LARGE  16

/* UI Core context */
typedef struct {
    SDL_Renderer* renderer;
    TTF_Font* font_small;
    TTF_Font* font_normal;
    TTF_Font* font_large;
} ui_core_t;

/* Mouse state */
typedef struct {
    int x, y;
    bool left_down;
    bool left_clicked;
    bool left_released;
    int wheel_y;
} mouse_state_t;

/* Initialize UI core (fonts only - renderer passed in) */
ui_core_t* ui_core_init(SDL_Renderer* renderer);

/* Shutdown UI core */
void ui_core_shutdown(ui_core_t* ui);

/* Draw filled rectangle */
void ui_draw_rect(ui_core_t* ui, int x, int y, int w, int h, uint32_t color);

/* Draw rectangle outline */
void ui_draw_rect_outline(ui_core_t* ui, int x, int y, int w, int h, uint32_t color);

/* Draw text (returns width) */
int ui_draw_text(ui_core_t* ui, TTF_Font* font, const char* text, int x, int y, uint32_t color);

/* Draw text centered in width */
void ui_draw_text_centered(ui_core_t* ui, TTF_Font* font, const char* text, int x, int y, int w, uint32_t color);

/* Get text size */
void ui_get_text_size(TTF_Font* font, const char* text, int* w, int* h);

/* Check if point is in rectangle */
bool ui_point_in_rect(int px, int py, int x, int y, int w, int h);

#endif /* UI_CORE_H */
