import numpy as np, math, random
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

S = 1024
random.seed(7)

PALETTES = {
 "violet": dict(
    bg=[(0, (8, 3, 28)), (0.45, (24, 9, 70)), (0.72, (52, 12, 96)), (1, (14, 6, 44))],
    halo=(110, 40, 230), horizon_glow=(255, 40, 170), corner_glow=(0, 200, 255), stars=(170, 200, 255),
    grid_glow=(255, 60, 190), grid_core=(255, 120, 230), hz_glow=(255, 90, 200), hz_core=(255, 200, 255),
    title_glow_outer=(255, 60, 200), title_glow_inner=(60, 220, 255), offset_a=(255, 40, 190), offset_b=(40, 230, 255),
    outline=(10, 4, 30),
    title_fill=[(0, (255, 255, 255)), (0.35, (150, 245, 255)), (0.62, (200, 150, 255)), (1, (255, 70, 190))],
    title_cut=(60, 10, 110), ul_glow=(60, 220, 255), ul_core=(190, 250, 255),
    sub_glow=(30, 200, 255), sub_outline=(8, 4, 26), sub_fill=(170, 250, 255), bar_glow=(255, 60, 190), bar_core=(255, 110, 215),
    br_glow=(60, 220, 255), br_core=(150, 245, 255)),
 "red": dict(
    bg=[(0, (14, 3, 3)), (0.45, (52, 9, 6)), (0.72, (104, 22, 8)), (1, (24, 5, 3))],
    halo=(255, 70, 15), horizon_glow=(255, 110, 20), corner_glow=(255, 190, 60), stars=(255, 220, 170),
    grid_glow=(255, 85, 20), grid_core=(255, 165, 70), hz_glow=(255, 100, 25), hz_core=(255, 235, 175),
    title_glow_outer=(255, 45, 15), title_glow_inner=(255, 170, 40), offset_a=(235, 25, 25), offset_b=(255, 205, 70),
    outline=(22, 4, 2),
    title_fill=[(0, (255, 252, 225)), (0.35, (255, 214, 90)), (0.62, (255, 128, 35)), (1, (235, 38, 22))],
    title_cut=(110, 14, 6), ul_glow=(255, 170, 40), ul_core=(255, 236, 190),
    sub_glow=(255, 120, 30), sub_outline=(22, 4, 2), sub_fill=(255, 226, 172), bar_glow=(255, 70, 25), bar_core=(255, 120, 55),
    br_glow=(255, 150, 40), br_core=(255, 205, 120)),
}
import sys
THEME = sys.argv[1] if len(sys.argv) > 1 else "violet"
P = PALETTES[THEME]
OUT = sys.argv[2] if len(sys.argv) > 2 else "icon-1024.png"


# ---------- helpers ----------
def lerp(a, b, t): return a + (b - a) * t
def to_img(arr): return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")

def vgrad(stops):
    """vertical gradient from [(pos 0..1, (r,g,b)), ...]"""
    ys = np.linspace(0, 1, S)[:, None]
    out = np.zeros((S, S, 3), np.float32)
    for c in range(3):
        out[:, :, c] = np.interp(ys, [p for p, _ in stops], [col[c] for _, col in stops])
    return out

def radial(cx, cy, r, color, strength):
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    d = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / r
    a = np.clip(1 - d, 0, 1) ** 2 * strength
    return a[:, :, None] * np.array(color, np.float32)[None, None, :]

def mask_text(text, font, center, shear=0.0, spacing=0):
    m = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(m)
    d.text(center, text, font=font, fill=255, anchor="mm")
    if shear:
        cy = center[1]
        m = m.transform((S, S), Image.AFFINE, (1, shear, -shear * cy, 0, 1, 0), Image.BICUBIC)
    return m

def mask_arr(m): return np.asarray(m, np.float32)[:, :, None] / 255.0

def add_glow(canvas, m, color, radius, strength):
    g = mask_arr(m.filter(ImageFilter.GaussianBlur(radius)))
    canvas += g * strength * np.array(color, np.float32)[None, None, :]

def paint(canvas, m, color):
    a = mask_arr(m)
    if isinstance(color, np.ndarray) and color.shape == (S, S, 3):
        src = color
    else:
        src = np.array(color, np.float32)[None, None, :]
    canvas[:] = canvas * (1 - a) + src * a

# ---------- background ----------
bg = vgrad(P["bg"])
bg += radial(512, 330, 560, P["halo"], 0.55)       # violet halo behind the title
bg += radial(512, 700, 520, P["horizon_glow"], 0.30)       # pink glow at the horizon
bg += radial(150, 960, 380, P["corner_glow"], 0.18)        # cyan glow bottom-left

# stars
st = Image.new("L", (S, S), 0); sd = ImageDraw.Draw(st)
for _ in range(70):
    x, y = random.randint(40, S - 40), random.randint(40, 520)
    r = random.choice([1, 1, 1, 2, 2, 3])
    sd.ellipse((x - r, y - r, x + r, y + r), fill=random.randint(120, 255))
st_b = st.filter(ImageFilter.GaussianBlur(1.2))
bg += mask_arr(st_b) * np.array(P["stars"], np.float32)[None, None, :] * 0.9

# synthwave grid floor
grid = Image.new("L", (S, S), 0); gd = ImageDraw.Draw(grid)
hz, vx = 640, 512
for i in range(-22, 23):                                  # lines to the vanishing point
    x_bottom = vx + i * 150
    gd.line([(vx + i * 6, hz), (x_bottom, S + 40)], fill=255, width=3)
y, step = hz, 8
while y < S + 40:                                         # horizontal lines, spacing grows
    gd.line([(0, int(y)), (S, int(y))], fill=255, width=3)
    step *= 1.30; y += step
grid = ImageChops.multiply(grid, Image.fromarray((np.clip((np.mgrid[0:S, 0:S][0] - hz) / 220.0, 0, 1) * 255).astype(np.uint8)))
bg += mask_arr(grid.filter(ImageFilter.GaussianBlur(5))) * np.array(P["grid_glow"], np.float32)[None, None, :] * 0.9
bg += mask_arr(grid) * np.array(P["grid_core"], np.float32)[None, None, :] * 0.55
# horizon line
hl = Image.new("L", (S, S), 0); ImageDraw.Draw(hl).line([(0, hz), (S, hz)], fill=255, width=4)
bg += mask_arr(hl.filter(ImageFilter.GaussianBlur(8))) * np.array(P["hz_glow"], np.float32)[None, None, :] * 1.2
bg += mask_arr(hl) * np.array(P["hz_core"], np.float32)[None, None, :] * 0.8

canvas = bg.astype(np.float32)

# dark band behind the bottom-left text (legibility)
band = np.clip((np.mgrid[0:S, 0:S][0] - 800) / 120.0, 0, 1)[:, :, None]
canvas *= 1 - 0.55 * band * np.clip(1.15 - np.mgrid[0:S, 0:S][1][:, :, None] / 900.0, 0, 1)

# ---------- title: YFM1 ----------
title_font = ImageFont.truetype("Orbitron.ttf", 280); title_font.set_variation_by_name("Black")
tm = mask_text("YFM1", title_font, (512, 340), shear=0.16)
bb = tm.getbbox()
tm = ImageChops.offset(tm, int(512 - (bb[0] + bb[2]) / 2), 0)   # centre the sheared word
bbox = tm.getbbox(); print("title bbox", bbox)

add_glow(canvas, tm, P["title_glow_outer"], 42, 1.1)
add_glow(canvas, tm, P["title_glow_inner"], 20, 1.0)
# chromatic offsets (magenta right/down, cyan left/up)
paint(canvas, ImageChops.offset(tm, 9, 7), P["offset_a"])
paint(canvas, ImageChops.offset(tm, -9, -7), P["offset_b"])
# dark outline
outline = tm.filter(ImageFilter.MaxFilter(15))
paint(canvas, ImageChops.subtract(outline, tm), P["outline"])
# gradient fill, white-cyan on top to hot pink at the bottom
x0, y0, x1, y1 = bbox
ys = np.clip((np.arange(S)[:, None] - y0) / max(1, (y1 - y0)), 0, 1)
fill = np.zeros((S, S, 3), np.float32)
stops = P["title_fill"]
for c in range(3):
    fill[:, :, c] = np.interp(ys, [p for p, _ in stops], [col[c] for _, col in stops])
paint(canvas, tm, fill)
# glossy cut: a darker lower band inside letters for a metallic look
cut = Image.new("L", (S, S), 0); ImageDraw.Draw(cut).rectangle((0, int(y0 + (y1 - y0) * 0.54), S, int(y0 + (y1 - y0) * 0.58)), fill=255)
paint(canvas, ImageChops.multiply(tm, cut), P["title_cut"])

# neon underline under the title
ul = Image.new("L", (S, S), 0); ud = ImageDraw.Draw(ul)
uy = y1 + 34
ud.polygon([(x0 + 40, uy), (x1 - 24, uy), (x1 - 4, uy + 12), (x0 + 20, uy + 12)], fill=255)
add_glow(canvas, ul, P["ul_glow"], 14, 1.2)
paint(canvas, ul, P["ul_core"])

# ---------- subtitle: iOSRecompile (bottom-left) ----------
sub_font = ImageFont.truetype("Rajdhani-Bold.ttf", 108)
sm = Image.new("L", (S, S), 0)
ImageDraw.Draw(sm).text((104, 900), "iOSRecompile", font=sub_font, fill=255, anchor="ls")
sbbox = sm.getbbox(); print("subtitle bbox", sbbox)
add_glow(canvas, sm, P["sub_glow"], 16, 1.1)
paint(canvas, sm.filter(ImageFilter.MaxFilter(7)), P["sub_outline"])
paint(canvas, sm, P["sub_fill"])
# accent bar over the subtitle
bar = Image.new("L", (S, S), 0)
ImageDraw.Draw(bar).polygon([(104, sbbox[1] - 30), (360, sbbox[1] - 30), (344, sbbox[1] - 18), (104, sbbox[1] - 18)], fill=255)
add_glow(canvas, bar, P["bar_glow"], 8, 1.2)
paint(canvas, bar, P["bar_core"])

# ---------- HUD corner brackets ----------
br = Image.new("L", (S, S), 0); bd = ImageDraw.Draw(br)
m, L, w = 100, 74, 7
for (cx, cy, dx, dy) in [(m, m, 1, 1), (S - m, m, -1, 1), (S - m, S - m, -1, -1)]:
    bd.line([(cx, cy), (cx + dx * L, cy)], fill=255, width=w)
    bd.line([(cx, cy), (cx, cy + dy * L)], fill=255, width=w)
add_glow(canvas, br, P["br_glow"], 10, 1.0)
paint(canvas, br, P["br_core"])

# ---------- scanlines + vignette ----------
yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
scan = 1 - 0.07 * ((yy % 4) < 2)
vig = 1 - 0.38 * np.clip(((xx - 512) ** 2 + (yy - 512) ** 2) / (720.0 ** 2), 0, 1)
canvas *= (scan * vig)[:, :, None]

icon = to_img(canvas)
icon.save(OUT, optimize=True)

