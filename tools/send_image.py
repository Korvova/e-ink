"""Send a picture (PNG/JPG/BMP, alpha ok) to an e-ink screen.

Usage:
  python tools/send_image.py <image> --screen 1 [--host 10.0.20.213] [--dither] [--invert] [--margin 40] [--preview out.png]

The image is fitted into 1360x480 on white, converted to 1-bit (threshold or Floyd-Steinberg
dithering), packed as 81600 bytes (bit=1 -> black) and POSTed to /image?screen=N as a file upload.
White-on-transparent logos are detected and inverted automatically (override with --invert / --no-auto).
"""
import argparse, io, sys, urllib.request, uuid
from PIL import Image

sys.stdout.reconfigure(encoding="utf-8", errors="replace")   # status JSON may contain non-cp1251 text

W, H = 1360, 480

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--screen", type=int, default=1)
ap.add_argument("--host", default="10.0.20.213")
ap.add_argument("--dither", action="store_true", help="Floyd-Steinberg instead of threshold")
ap.add_argument("--invert", action="store_true")
ap.add_argument("--no-auto", action="store_true", help="do not auto-invert white logos")
ap.add_argument("--margin", type=int, default=40)
ap.add_argument("--preview", default=None, help="save the 1-bit result as PNG")
ap.add_argument("--dry", action="store_true", help="only build preview, do not send")
a = ap.parse_args()

im = Image.open(a.image).convert("RGBA")
# auto-detect white logo on transparent background
if not a.no_auto and not a.invert:
    alpha = im.getchannel("A")
    if alpha.getextrema()[0] < 255:  # has transparency
        px = [p for p in im.getdata() if p[3] > 128]
        if px and sum(sum(p[:3]) / 3 for p in px) / len(px) > 160:
            a.invert = True
            print("white logo detected -> inverting to black")

if a.invert:
    r, g, b, al = im.split()
    rgb = Image.merge("RGB", (r, g, b))
    rgb = Image.eval(rgb, lambda v: 255 - v)
    im = Image.merge("RGBA", (*rgb.split(), al))

# composite over white
bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
bg.alpha_composite(im)
gray = bg.convert("L")

# fit
tw, th = W - 2 * a.margin, H - 2 * a.margin
scale = min(tw / gray.width, th / gray.height)
nw, nh = max(1, int(gray.width * scale)), max(1, int(gray.height * scale))
gray = gray.resize((nw, nh), Image.LANCZOS)
canvas = Image.new("L", (W, H), 255)
canvas.paste(gray, ((W - nw) // 2, (H - nh) // 2))

bw = canvas.convert("1", dither=Image.FLOYDSTEINBERG if a.dither else Image.NONE)
if a.preview:
    bw.save(a.preview)
    print("preview:", a.preview)

# pack: bit=1 -> black
data = bytearray(W // 8 * H)
pix = bw.load()
for y in range(H):
    row = y * (W // 8)
    for x in range(W):
        if pix[x, y] == 0:
            data[row + (x >> 3)] |= 0x80 >> (x & 7)

if a.dry:
    sys.exit(0)

boundary = uuid.uuid4().hex
body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"image\"; filename=\"frame.bin\"\r\n"
        f"Content-Type: application/octet-stream\r\n\r\n").encode() + bytes(data) + f"\r\n--{boundary}--\r\n".encode()
req = urllib.request.Request(f"http://{a.host}/image?screen={a.screen}", data=body,
                             headers={"Content-Type": f"multipart/form-data; boundary={boundary}"}, method="POST")
resp = urllib.request.urlopen(req, timeout=30)
print(resp.status, resp.read().decode())
