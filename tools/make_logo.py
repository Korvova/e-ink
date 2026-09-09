"""Draw the RMS sign (stylised R: three thick diagonal strokes, zig-zag) as a PNG.

Usage: python tools/make_logo.py out.png [--ms] [--height 400]
  --ms   add the letters "MS" to the right of the sign (full RMS logo)
Black on transparent, ready for tools/send_image.py or the web page.
"""
import argparse, os
from PIL import Image, ImageDraw, ImageFont

ap = argparse.ArgumentParser()
ap.add_argument("out")
ap.add_argument("--ms", action="store_true")
ap.add_argument("--height", type=int, default=400)
a = ap.parse_args()

H = a.height
W = int(H * 0.62)                 # sign proportions ~ 0.62 : 1
T = H * 0.19                      # stroke thickness (vertical)
# zig-zag centre line: top-left -> right -> left -> bottom-right
pts = [(0.03 * W, 0.17 * H), (0.97 * W, 0.40 * H), (0.03 * W, 0.62 * H), (0.97 * W, 0.86 * H)]

SS = 4                            # supersampling for clean edges
img = Image.new("L", (W * SS * 5 if a.ms else W * SS, H * SS), 0)
d = ImageDraw.Draw(img)

def band(p, q):
    (x1, y1), (x2, y2) = p, q
    poly = [(x1, y1 - T / 2), (x2, y2 - T / 2), (x2, y2 + T / 2), (x1, y1 + T / 2)]
    d.polygon([(x * SS, y * SS) for x, y in poly], fill=255)

for i in range(3):
    band(pts[i], pts[i + 1])
# square off the outer ends so the strokes finish with vertical cuts inside the box
d.rectangle([0, 0, 0.03 * W * SS - 1, H * SS], fill=0)
d.rectangle([0.97 * W * SS + 1, 0, W * SS, H * SS], fill=0)

if a.ms:
    font_path = r"C:\Windows\Fonts\pt sans_bold.ttf"
    size = int(H * 0.50 * SS)          # letters ~ half the sign height, like the original
    f = ImageFont.truetype(font_path, size)
    x = W * SS + int(0.16 * W * SS)
    bbox = d.textbbox((0, 0), "MS", font=f)
    y = (H * SS - (bbox[3] - bbox[1])) // 2 - bbox[1]
    d.text((x, y), "MS", font=f, fill=255)
    img = img.crop((0, 0, x + (bbox[2] - bbox[0]) + int(0.03 * W * SS), H * SS))

img = img.resize((img.width // SS, img.height // SS), Image.LANCZOS)
rgba = Image.new("RGBA", img.size, (0, 0, 0, 0))
rgba.putalpha(img)                # black where drawn, transparent elsewhere
rgba.save(a.out)
print(a.out, rgba.size)
