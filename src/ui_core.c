/**
 * @file ui_core.c
 * @brief Core SDL2/TTF rendering implementation
 */

#include "ui_core.h"
#include <stdio.h>
#include <string.h>

/* Font paths */
#ifdef _WIN32
#define FONT_PATH_PRIMARY   "C:/Windows/Fonts/consola.ttf"
#define FONT_PATH_FALLBACK  "C:/Windows/Fonts/cour.ttf"
#else
#define FONT_PATH_PRIMARY   "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
#define FONT_PATH_FALLBACK  "/usr/share/fonts/TTF/DejaVuSansMono.ttf"
#endif

static TTF_Font* load_font(const char* primary, const char* fallback, int size) {
    TTF_Font* font = TTF_OpenFont(primary, size);
    if (!font && fallback) {
        font = TTF_OpenFont(fallback, size);
    }
    if (!font) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
    }
    return font;
}

ui_core_t* ui_core_init(SDL_Renderer* renderer) {
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return NULL;
    }

    ui_core_t* ui = (ui_core_t*)calloc(1, sizeof(ui_core_t));
    if (!ui) return NULL;

    ui->renderer = renderer;
    ui->font_small = load_font(FONT_PATH_PRIMARY, FONT_PATH_FALLBACK, FONT_SIZE_SMALL);
    ui->font_normal = load_font(FONT_PATH_PRIMARY, FONT_PATH_FALLBACK, FONT_SIZE_NORMAL);
    ui->font_large = load_font(FONT_PATH_PRIMARY, FONT_PATH_FALLBACK, FONT_SIZE_LARGE);

    if (!ui->font_small || !ui->font_normal || !ui->font_large) {
        fprintf(stderr, "Warning: Some fonts failed to load\n");
    }

    return ui;
}

void ui_core_shutdown(ui_core_t* ui) {
    if (!ui) return;
    if (ui->font_small) TTF_CloseFont(ui->font_small);
    if (ui->font_normal) TTF_CloseFont(ui->font_normal);
    if (ui->font_large) TTF_CloseFont(ui->font_large);
    TTF_Quit();
    free(ui);
}

static void set_color(SDL_Renderer* renderer, uint32_t rgba) {
    SDL_SetRenderDrawColor(renderer,
        (rgba >> 24) & 0xFF,
        (rgba >> 16) & 0xFF,
        (rgba >> 8) & 0xFF,
        rgba & 0xFF);
}

void ui_draw_rect(ui_core_t* ui, int x, int y, int w, int h, uint32_t color) {
    set_color(ui->renderer, color);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(ui->renderer, &rect);
}

void ui_draw_rect_outline(ui_core_t* ui, int x, int y, int w, int h, uint32_t color) {
    set_color(ui->renderer, color);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderDrawRect(ui->renderer, &rect);
}

int ui_draw_text(ui_core_t* ui, TTF_Font* font, const char* text, int x, int y, uint32_t color) {
    if (!font || !text || !text[0]) return 0;

    SDL_Color sdl_color = {
        (color >> 24) & 0xFF,
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF
    };

    SDL_Surface* surface = TTF_RenderText_Blended(font, text, sdl_color);
    if (!surface) return 0;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(ui->renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return 0;
    }

    SDL_Rect dst = { x, y, surface->w, surface->h };
    SDL_RenderCopy(ui->renderer, texture, NULL, &dst);

    int width = surface->w;
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);

    return width;
}

void ui_draw_text_centered(ui_core_t* ui, TTF_Font* font, const char* text, 
                            int x, int y, int w, uint32_t color) {
    if (!font || !text) return;
    int text_w, text_h;
    TTF_SizeText(font, text, &text_w, &text_h);
    int offset_x = (w - text_w) / 2;
    ui_draw_text(ui, font, text, x + offset_x, y, color);
}

void ui_get_text_size(TTF_Font* font, const char* text, int* w, int* h) {
    if (!font || !text) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    TTF_SizeText(font, text, w, h);
}

bool ui_point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}
