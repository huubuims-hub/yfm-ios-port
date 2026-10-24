/* The on-screen controller's pictures (touch_pad_art.h).
 *
 * Every button is drawn here as a vector shape (signed distance fields with
 * a one-pixel soft edge) at the size the layout gives it, so it is smooth at
 * any screen size, on a phone or a tablet. The game's own 16 x 16 button
 * sprites it used before were enlarged with nearest-texel sampling, which
 * made the edges coarse and stair-stepped, and needed the disc. A button is
 * drawn once per size and state (up, held) into a small picture, which each
 * frame then only blends onto the canvas. Text is the overlay font's
 * (overlay_text.h), anti-aliased too. */
#include "touch_pad_art.h"
#include "touch_pad.h"
#include "pc/text/overlay_text.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x, y; } V2;
typedef struct { uint32_t *pixels; int w, h; } Sprite;

/* Sprites by button, [0] let go, [1] held. */
static Sprite sprites[TOUCH_BUTTONS][2];

enum {
    BODY = 0x10141C,  /* the dark glass behind a symbol */
    LIGHT = 0xF4F6FA, /* rims, arrows and text */
    GREEN = 0x58D6A4, /* Triangle */
    RED = 0xFF6F7D,   /* Circle */
    BLUE = 0x6FAEFF,  /* Cross */
    PINK = 0xF0A3DA   /* Square */
};

/* --- distances, in pixels; negative inside ----------------------------- */

static V2 v2(float x, float y)
{
    V2 v = {x, y};
    return v;
}
static V2 sub(V2 a, V2 b) { return v2(a.x - b.x, a.y - b.y); }
static float dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float length(V2 a) { return sqrtf(dot(a, a)); }

static float sd_circle(V2 p, float r) { return length(p) - r; }

/* A box of half sizes hx, hy with corners rounded by r. */
static float sd_box(V2 p, float hx, float hy, float r)
{
    V2 q = v2(fabsf(p.x) - (hx - r), fabsf(p.y) - (hy - r));
    V2 o = v2(q.x > 0 ? q.x : 0, q.y > 0 ? q.y : 0);
    float inner = q.x > q.y ? q.x : q.y;
    return length(o) + (inner < 0 ? inner : 0) - r;
}

static float sd_segment(V2 p, V2 a, V2 b)
{
    V2 pa = sub(p, a), ba = sub(b, a);
    float h = clampf(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
    return length(v2(pa.x - ba.x * h, pa.y - ba.y * h));
}

/* A triangle's distance (negative inside), whichever way it is wound. */
static float sd_triangle(V2 p, V2 a, V2 b, V2 c)
{
    V2 e0 = sub(b, a), e1 = sub(c, b), e2 = sub(a, c);
    V2 v0 = sub(p, a), v1 = sub(p, b), v2_ = sub(p, c);
    float h0 = clampf(dot(v0, e0) / dot(e0, e0), 0.0f, 1.0f);
    float h1 = clampf(dot(v1, e1) / dot(e1, e1), 0.0f, 1.0f);
    float h2 = clampf(dot(v2_, e2) / dot(e2, e2), 0.0f, 1.0f);
    V2 q0 = v2(v0.x - e0.x * h0, v0.y - e0.y * h0);
    V2 q1 = v2(v1.x - e1.x * h1, v1.y - e1.y * h1);
    V2 q2 = v2(v2_.x - e2.x * h2, v2_.y - e2.y * h2);
    float s = e0.x * e2.y - e0.y * e2.x;
    float s0 = v0.x * e0.y - v0.y * e0.x, s1 = v1.x * e1.y - v1.y * e1.x, s2 = v2_.x * e2.y - v2_.y * e2.x;
    float d2 = fminf(fminf(dot(q0, q0), dot(q1, q1)), dot(q2, q2));
    float side = fminf(fminf(s * s0, s * s1), s * s2);
    return side > 0 ? -sqrtf(d2) : sqrtf(d2);
}

/* --- compositing -------------------------------------------------------- */

/* Color accumulated in premultiplied floats. */
typedef struct { float r, g, b, a; } Pixel;

/* Coverage of a pixel whose centre is d away from an edge (a one-pixel ramp). */
static float cover(float d) { return clampf(0.5f - d, 0.0f, 1.0f); }

static void over(Pixel *p, uint32_t color, float coverage, float opacity)
{
    float a = coverage * opacity, keep = 1.0f - a;
    if (a <= 0.0f) return;
    p->r = (float)(color >> 16 & 255) / 255.0f * a + p->r * keep;
    p->g = (float)(color >> 8 & 255) / 255.0f * a + p->g * keep;
    p->b = (float)(color & 255) / 255.0f * a + p->b * keep;
    p->a = a + p->a * keep;
}

/* 0xAARRGGBB with straight (not premultiplied) color. */
static uint32_t pack(const Pixel *p)
{
    float a = p->a;
    unsigned ia, r, g, b;
    if (a <= 0.002f) return 0;
    ia = (unsigned)(a * 255.0f + 0.5f);
    r = (unsigned)clampf(p->r / a * 255.0f + 0.5f, 0.0f, 255.0f);
    g = (unsigned)clampf(p->g / a * 255.0f + 0.5f, 0.0f, 255.0f);
    b = (unsigned)clampf(p->b / a * 255.0f + 0.5f, 0.0f, 255.0f);
    return (ia > 255 ? 255u : ia) << 24 | r << 16 | g << 8 | b;
}

/* The D-pad's arrow is one triangle pointing up; a point is turned so that
 * the button's direction lands on it. */
static V2 turn_to_up(V2 p, TouchButton direction)
{
    switch (direction) {
    case TOUCH_DOWN: return v2(p.x, -p.y);
    case TOUCH_LEFT: return v2(p.y, p.x);
    case TOUCH_RIGHT: return v2(p.y, -p.x);
    default: return p;
    }
}

/* --- one button's pixels ------------------------------------------------ */

typedef struct {
    TouchButton button;
    int down;
    int w, h;
    float s; /* the layout's button size */
} Shape;

static uint32_t face_color(TouchButton b)
{
    return b == TOUCH_TRIANGLE ? GREEN : b == TOUCH_CIRCLE ? RED : b == TOUCH_CROSS ? BLUE : PINK;
}

/* A face button: dark glass in a ring of its color, and its symbol. Held, the
 * glass fills with the color and the symbol turns dark, a little smaller. */
static uint32_t face_pixel(const Shape *sh, V2 p)
{
    Pixel px = {0, 0, 0, 0};
    float s = sh->s, radius = s * (sh->down ? 0.43f : 0.46f), ring = s * 0.05f, r = s * 0.19f, t = s * 0.055f, d;
    uint32_t color = face_color(sh->button), symbol = sh->down ? BODY : color;
    over(&px, sh->down ? color : BODY, cover(sd_circle(p, radius)), sh->down ? 0.88f : 0.55f);
    d = fabsf(sd_circle(p, radius - ring * 0.5f)) - ring * 0.5f;
    over(&px, sh->down ? LIGHT : color, cover(d), sh->down ? 0.9f : 0.95f);
    switch (sh->button) {
    case TOUCH_TRIANGLE: {
        float k = r * 0.93f, rr = s * 0.025f; /* rounded: the triangle grown by rr */
        V2 a = v2(0, -k * 1.05f + rr), b = v2(k * 0.95f - rr, k * 0.7f - rr * 0.6f),
           c = v2(-k * 0.95f + rr, k * 0.7f - rr * 0.6f);
        V2 q = v2(p.x, p.y + s * 0.015f);
        d = fabsf(sd_triangle(q, a, b, c) - rr) - t * 0.5f;
        break;
    }
    case TOUCH_CIRCLE: d = fabsf(sd_circle(p, r * 0.98f)) - t * 0.5f; break;
    case TOUCH_CROSS: {
        float k = r * 0.86f;
        float d1 = sd_segment(p, v2(-k, -k), v2(k, k)), d2 = sd_segment(p, v2(-k, k), v2(k, -k));
        d = fminf(d1, d2) - t * 0.5f;
        break;
    }
    default: d = fabsf(sd_box(p, r * 0.92f, r * 0.92f, s * 0.03f)) - t * 0.5f; break;
    }
    over(&px, symbol, cover(d), 0.98f);
    return pack(&px);
}

/* A D-pad key: a rounded square of dark glass with a light rim and an arrow
 * pointing out; held, it fills light and the arrow turns dark. */
static uint32_t key_pixel(const Shape *sh, V2 p)
{
    Pixel px = {0, 0, 0, 0};
    float s = sh->s, half = s * 0.5f - s * 0.05f, corner = s * 0.2f, rim = s * 0.035f, d, k = s * 0.17f, rr = s * 0.03f;
    V2 q = turn_to_up(p, sh->button);
    over(&px, sh->down ? LIGHT : BODY, cover(sd_box(p, half, half, corner)), sh->down ? 0.82f : 0.55f);
    d = fabsf(sd_box(p, half - rim * 0.5f, half - rim * 0.5f, corner - rim * 0.5f)) - rim * 0.5f;
    over(&px, LIGHT, cover(d), sh->down ? 0.95f : 0.55f);
    d = sd_triangle(q, v2(0, -k * 1.1f + rr), v2(k - rr, k * 0.75f - rr * 0.6f), v2(-k + rr, k * 0.75f - rr * 0.6f)) - rr;
    over(&px, sh->down ? BODY : LIGHT, cover(d), 0.97f);
    return pack(&px);
}

/* A pill: a shoulder tab, or SELECT, START and MENU, centred in the
 * button's rectangle. */
static uint32_t pill_pixel(const Shape *sh, V2 p, float width, float height, float corner)
{
    Pixel px = {0, 0, 0, 0};
    float rim = sh->s * 0.035f, d;
    over(&px, sh->down ? LIGHT : BODY, cover(sd_box(p, width * 0.5f, height * 0.5f, corner)), sh->down ? 0.82f : 0.55f);
    d = fabsf(sd_box(p, width * 0.5f - rim * 0.5f, height * 0.5f - rim * 0.5f, corner - rim * 0.5f)) - rim * 0.5f;
    over(&px, LIGHT, cover(d), sh->down ? 0.95f : 0.55f);
    return pack(&px);
}

static const char *label_of(TouchButton b)
{
    switch (b) {
    case TOUCH_L1: return "L1";
    case TOUCH_L2: return "L2";
    case TOUCH_R1: return "R1";
    case TOUCH_R2: return "R2";
    case TOUCH_SELECT: return "SELECT";
    case TOUCH_START: return "START";
    case TOUCH_MENU: return "MENU";
    default: return NULL;
    }
}

static int is_pill(TouchButton b) { return b == TOUCH_SELECT || b == TOUCH_START || b == TOUCH_MENU; }
static int is_shoulder(TouchButton b) { return b == TOUCH_L1 || b == TOUCH_L2 || b == TOUCH_R1 || b == TOUCH_R2; }

/* The pill's measures inside the button's rectangle. */
static void pill_measures(const Shape *sh, float *width, float *height, float *corner)
{
    float s = sh->s;
    if (is_shoulder(sh->button)) {
        *width = s * 0.94f;
        *height = s * 0.58f;
        *corner = s * 0.2f;
    } else {
        *width = (float)sh->w - s * 0.06f;
        *height = fminf((float)sh->h - s * 0.06f, s * 0.56f);
        *corner = *height * 0.5f;
    }
}

static void free_sprite(Sprite *sprite)
{
    free(sprite->pixels);
    memset(sprite, 0, sizeof(*sprite));
}

static void paint_label(Sprite *sprite, const Shape *sh, float width, float height)
{
    const char *text = label_of(sh->button);
    MenuCanvas tmp;
    int size, tw;
    if (!text) return;
    size = (int)(is_shoulder(sh->button) ? sh->s * 0.30f : height * 0.5f + 0.5f);
    if (size < 6) size = 6;
    while ((tw = OverlayText_Width(text, size)) > (int)(width * 0.86f) && size > 6) size--;
    tmp.pixels = sprite->pixels;
    tmp.stride = tmp.width = sprite->w;
    tmp.height = sprite->h;
    tmp.alpha = 1;
    OverlayText_Draw(&tmp, (sprite->w - tw) / 2, sprite->h / 2, sprite->w, text, size, sh->down ? BODY : LIGHT);
}

static int build(Sprite *sprite, TouchButton button, int down, int w, int h, float s)
{
    Shape sh = {button, down, w, h, s};
    float pill_w = 0, pill_h = 0, pill_corner = 0;
    int i, j;
    if (w <= 0 || h <= 0) return 0;
    sprite->pixels = malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!sprite->pixels) return 0;
    sprite->w = w;
    sprite->h = h;
    if (is_pill(button) || is_shoulder(button)) pill_measures(&sh, &pill_w, &pill_h, &pill_corner);
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            V2 p = v2((float)i + 0.5f - (float)w * 0.5f, (float)j + 0.5f - (float)h * 0.5f);
            uint32_t color;
            if (button >= TOUCH_TRIANGLE && button <= TOUCH_SQUARE) color = face_pixel(&sh, p);
            else if (button <= TOUCH_RIGHT) color = key_pixel(&sh, p);
            else color = pill_pixel(&sh, p, pill_w, pill_h, pill_corner);
            sprite->pixels[j * w + i] = color;
        }
    }
    if (label_of(button)) paint_label(sprite, &sh, pill_w, pill_h);
    return 1;
}

void TouchPadArt_Reset(void)
{
    int i;
    for (i = 0; i < TOUCH_BUTTONS; i++) {
        free_sprite(&sprites[i][0]);
        free_sprite(&sprites[i][1]);
    }
}

/* The button's picture in a state, made again when the layout's size changed. */
static const Sprite *sprite_for(TouchButton button, int down, int w, int h, float s)
{
    Sprite *sprite = &sprites[button][down ? 1 : 0];
    if (sprite->pixels && sprite->w == w && sprite->h == h) return sprite;
    free_sprite(sprite);
    return build(sprite, button, down, w, h, s) ? sprite : NULL;
}

static void blit(MenuCanvas *canvas, const Sprite *sprite, int x, int y)
{
    int i, j;
    for (j = 0; j < sprite->h; j++) {
        if (y + j < 0 || y + j >= canvas->height) continue;
        for (i = 0; i < sprite->w; i++) {
            uint32_t texel = sprite->pixels[j * sprite->w + i];
            if (texel >> 24) OverlayText_Blend(canvas, x + i, y + j, texel & 0xFFFFFF, texel >> 24);
        }
    }
}

static void grow(int *x0, int *y0, int *x1, int *y1, int x, int y, int w, int h)
{
    if (x < *x0) *x0 = x;
    if (y < *y0) *y0 = y;
    if (x + w > *x1) *x1 = x + w;
    if (y + h > *y1) *y1 = y + h;
}

void TouchPadArt_Draw(MenuCanvas *canvas, int *x, int *y, int *w, int *h)
{
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30), i, bx, by, bw, bh, cx, cy, cw, ch;
    float s;
    *x = *y = *w = *h = 0;
    if (!TouchPad_Shown() || !TouchPad_Rect(TOUCH_CROSS, &cx, &cy, &cw, &ch)) return;
    s = (float)(cw < ch ? cw : ch);
    for (i = 0; i < TOUCH_BUTTONS; i++) {
        const Sprite *sprite;
        if (!TouchPad_Rect((TouchButton)i, &bx, &by, &bw, &bh)) return;
        grow(&x0, &y0, &x1, &y1, bx, by, bw, bh);
        sprite = sprite_for((TouchButton)i, TouchPad_Held((TouchButton)i), bw, bh, s);
        if (sprite) blit(canvas, sprite, bx, by);
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > canvas->width) x1 = canvas->width;
    if (y1 > canvas->height) y1 = canvas->height;
    if (x1 <= x0 || y1 <= y0) return;
    *x = x0;
    *y = y0;
    *w = x1 - x0;
    *h = y1 - y0;
}
