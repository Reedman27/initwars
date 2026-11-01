#include "render.h"
#include "font.h"

#include <stdio.h>
#include <string.h>

uint32_t color_argb(int pal)
{
    if (pal < 0 || pal >= C_NCOLORS) pal = C_BLACK;
    const Color *c = &PALETTE[pal];
    return 0xFF000000u | ((uint32_t)c->r << 16) | ((uint32_t)c->g << 8) | c->b;
}

int color_dark(int pal)
{
    switch (pal) {
    case C_BGREEN: return C_GREEN;
    case C_GREEN:  return C_DKGREEN;
    case C_AMBER:  return C_DKAMBER;
    case C_LTGREY: return C_GREY;
    case C_WHITE:  return C_LTGREY;
    case C_RED:    return C_DKRED;
    case C_CYAN:   return C_GREY;
    case C_GREY:   return C_DKGREEN;
    default:       return pal;
    }
}

void fr_clear(Frame *f, int pal)
{
    uint32_t c = color_argb(pal);
    for (int i = 0; i < LW * LH; i++) f->px[i] = c;
}

void fr_pixel(Frame *f, int x, int y, int pal)
{
    if (x < 0 || y < 0 || x >= LW || y >= LH) return;
    f->px[y * LW + x] = color_argb(pal);
}

void fr_rect(Frame *f, int x, int y, int w, int h, int pal)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > LW ? LW : x + w, y1 = y + h > LH ? LH : y + h;
    uint32_t c = color_argb(pal);
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) f->px[j * LW + i] = c;
}

void fr_box(Frame *f, int x, int y, int w, int h, int pal)
{
    if (w < 1 || h < 1) return;
    fr_rect(f, x, y, w, 1, pal);
    fr_rect(f, x, y + h - 1, w, 1, pal);
    fr_rect(f, x, y, 1, h, pal);
    fr_rect(f, x + w - 1, y, 1, h, pal);
}

void fr_line(Frame *f, int x0, int y0, int x1, int y1, int pal)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 1024; guard++) {
        fr_pixel(f, x0, y0, pal);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fr_hline_dotted(Frame *f, int x0, int x1, int y, int pal, int phase)
{
    for (int x = x0; x < x1; x++)
        if (((x + phase) & 3) < 2) fr_pixel(f, x, y, pal);
}

void fr_vline_dotted(Frame *f, int x, int y0, int y1, int pal, int phase)
{
    for (int y = y0; y < y1; y++)
        if (((y + phase) & 3) < 2) fr_pixel(f, x, y, pal);
}

int text_width(const char *s)
{
    int n = 0;
    for (; *s; s++) n++;
    return n * FONT_W - 1;
}

static void draw_char(Frame *f, int x, int y, int ch, uint32_t c)
{
    const uint8_t *g = font_glyph(ch);
    for (int col = 0; col < 5; col++) {
        int px = x + col;
        if (px < 0 || px >= LW) continue;
        for (int row = 0; row < 7; row++) {
            int py = y + row;
            if (py < 0 || py >= LH) continue;
            if ((g[col] >> row) & 1) f->px[py * LW + px] = c;
        }
    }
}

void fr_text(Frame *f, int x, int y, const char *s, int pal)
{
    uint32_t c = color_argb(pal);
    for (; *s; s++, x += FONT_W) {
        if (x > LW) break;
        draw_char(f, x, y, (unsigned char)*s, c);
    }
}

void fr_text_center(Frame *f, int cx, int y, const char *s, int pal)
{
    fr_text(f, cx - text_width(s) / 2, y, s, pal);
}

void fr_text_right(Frame *f, int rx, int y, const char *s, int pal)
{
    fr_text(f, rx - text_width(s), y, s, pal);
}

static int sprite_color(char ch, int main_pal)
{
    switch (ch) {
    case 'k': return C_BLACK;
    case 'd': return C_DKGREEN;
    case 'g': return C_GREEN;
    case 'G': return C_BGREEN;
    case 'a': return C_AMBER;
    case 'A': return C_DKAMBER;
    case 'x': return C_GREY;
    case 'X': return C_LTGREY;
    case 'r': return C_RED;
    case 'R': return C_DKRED;
    case 'w': return C_WHITE;
    case 'c': return C_CYAN;
    case '1': return main_pal;
    case '2': return color_dark(main_pal);
    default:  return -1;
    }
}

void fr_sprite(Frame *f, const Sprite *s, int x, int y, int main_pal, Tint tint, bool flip)
{
    for (int j = 0; j < s->h; j++) {
        const char *row = s->rows[j];
        for (int i = 0; i < s->w; i++) {
            char ch = row[flip ? s->w - 1 - i : i];
            int pal = sprite_color(ch, main_pal);
            if (pal < 0) continue;
            switch (tint) {
            case T_FLASH: pal = C_WHITE; break;
            case T_DIM:   pal = color_dark(pal); break;
            case T_GHOST: if (((i + j) & 1) == 0) continue; break;
            case T_WORN:  if (ch == '1' || ch == 'X' || ch == 'w') pal = color_dark(pal); break;
            default: break;
            }
            fr_pixel(f, x + i, y + j, pal);
        }
    }
}

void fr_sprite_scaled(Frame *f, const Sprite *s, int x, int y, int main_pal, Tint tint, int scale)
{
    for (int j = 0; j < s->h; j++)
        for (int i = 0; i < s->w; i++) {
            char ch = s->rows[j][i];
            int pal = sprite_color(ch, main_pal);
            if (pal < 0) continue;
            if (tint == T_DIM) pal = color_dark(pal);
            else if (tint == T_FLASH) pal = C_WHITE;
            else if (tint == T_WORN && (ch == '1' || ch == 'X' || ch == 'w')) pal = color_dark(pal);
            fr_rect(f, x + i * scale, y + j * scale, scale, scale, pal);
        }
}

void fr_dim(Frame *f, int num, int den)
{
    for (int i = 0; i < LW * LH; i++) {
        uint32_t p = f->px[i];
        uint32_t r = ((p >> 16) & 0xFF) * (uint32_t)num / (uint32_t)den;
        uint32_t g = ((p >> 8) & 0xFF) * (uint32_t)num / (uint32_t)den;
        uint32_t b = (p & 0xFF) * (uint32_t)num / (uint32_t)den;
        f->px[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}

void fr_scanlines(Frame *f)
{
    for (int y = 1; y < LH; y += 2) {
        for (int x = 0; x < LW; x++) {
            uint32_t p = f->px[y * LW + x];
            uint32_t r = ((p >> 16) & 0xFF) * 3 / 4;
            uint32_t g = ((p >> 8) & 0xFF) * 3 / 4;
            uint32_t b = (p & 0xFF) * 3 / 4;
            f->px[y * LW + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

bool fr_save_ppm(const Frame *f, const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;
    if (fprintf(fp, "P6\n%d %d\n255\n", LW, LH) < 0) { fclose(fp); return false; }
    for (int i = 0; i < LW * LH; i++) {
        uint32_t p = f->px[i];
        uint8_t rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
        if (fwrite(rgb, 1, 3, fp) != 3) { fclose(fp); return false; }
    }
    return fclose(fp) == 0;
}
