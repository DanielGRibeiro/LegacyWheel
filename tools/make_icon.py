"""Gera o icone do LegacyWheel (volante generico de simulador) em varios tamanhos.

Uso: python tools/make_icon.py
Saida: legacywheel.ico (16..256 px) e docs/icon.png (512 px) para o README.
"""
import math
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BG_TOP = (36, 72, 120)
BG_BOTTOM = (14, 28, 52)
WHEEL = (240, 244, 248)
ACCENT = (255, 138, 0)


def render(size, bold=False):
    """Desenha em 8x e reduz (antisserrilhado). bold = tracos grossos para 16/24 px."""
    s = size * 8
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))

    # fundo: quadrado arredondado com degrade vertical
    grad = Image.new("RGBA", (s, s))
    gd = ImageDraw.Draw(grad)
    for y in range(s):
        t = y / (s - 1)
        c = tuple(int(BG_TOP[i] + (BG_BOTTOM[i] - BG_TOP[i]) * t) for i in range(3))
        gd.line([(0, y), (s, y)], fill=c + (255,))
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, s - 1, s - 1], radius=int(s * 0.22), fill=255)
    img.paste(grad, (0, 0), mask)

    d = ImageDraw.Draw(img)
    cx, cy = s / 2, s / 2 + s * 0.02
    r = s * 0.36                       # raio do aro
    ring = s * (0.11 if bold else 0.075)
    spoke = s * (0.11 if bold else 0.08)
    hub = s * (0.14 if bold else 0.11)

    # aro
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=WHEEL, width=int(ring))

    # tres raios: esquerda, direita e baixo (volante de corrida classico)
    for ang in (180, 0, 90):
        a = math.radians(ang)
        x2 = cx + math.cos(a) * (r - ring * 0.4)
        y2 = cy + math.sin(a) * (r - ring * 0.4)
        d.line([(cx, cy), (x2, y2)], fill=WHEEL, width=int(spoke))

    # cubo
    d.ellipse([cx - hub, cy - hub, cx + hub, cy + hub], fill=WHEEL)
    if not bold:
        inner = hub * 0.45
        d.ellipse([cx - inner, cy - inner, cx + inner, cy + inner], fill=BG_BOTTOM + (255,))

    # marca laranja do centro (12 horas), como nos volantes de corrida
    mark_w = math.radians(14 if bold else 11)
    ro, ri = r + ring / 2, r - ring / 2
    pts = []
    for k in range(13):
        a = -math.pi / 2 - mark_w + 2 * mark_w * k / 12
        pts.append((cx + math.cos(a) * ro, cy + math.sin(a) * ro))
    for k in range(13):
        a = -math.pi / 2 + mark_w - 2 * mark_w * k / 12
        pts.append((cx + math.cos(a) * ri, cy + math.sin(a) * ri))
    d.polygon(pts, fill=ACCENT)

    return img.resize((size, size), Image.LANCZOS)


def main():
    sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
    frames = [render(n, bold=n <= 24) for n in sizes]
    ico = os.path.join(ROOT, "legacywheel.ico")
    frames[-1].save(ico, format="ICO", sizes=[(n, n) for n in sizes], append_images=frames[:-1])
    os.makedirs(os.path.join(ROOT, "docs"), exist_ok=True)
    render(512).save(os.path.join(ROOT, "docs", "icon.png"))
    # folha de previa com os tamanhos lado a lado
    sheet = Image.new("RGBA", (sum(sizes) + 10 * len(sizes) + 10, 276), (245, 245, 245, 255))
    x = 10
    for n, f in zip(sizes, frames):
        sheet.paste(f, (x, 266 - n), f)
        x += n + 10
    sheet.save(os.path.join(ROOT, "docs", "icon-sizes.png"))
    print("ok:", ico)


if __name__ == "__main__":
    main()
