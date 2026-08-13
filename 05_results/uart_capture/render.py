from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf"
UI   = "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"

BG, FG = (0,0,0), (187,187,187)
TITLE_BG, TITLE_FG = (240,240,240), (30,30,30)
FS = 17           # terminal font size
PAD = 10
TITLEBAR = 30

def render(lines, out, title="COM6 - PuTTY", cols=None):
    f  = ImageFont.truetype(FONT, FS)
    fu = ImageFont.truetype(UI, 13)
    # cell metrics
    cw = f.getlength("M")
    asc, desc = f.getmetrics()
    ch = asc + desc + 3
    ncols = cols or max(80, max((len(l) for l in lines), default=80))
    W = int(cw*ncols) + PAD*2
    H = int(ch*len(lines)) + PAD*2 + TITLEBAR
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    # title bar
    d.rectangle([0,0,W,TITLEBAR], fill=TITLE_BG)
    d.line([(0,TITLEBAR),(W,TITLEBAR)], fill=(200,200,200))
    d.text((8, TITLEBAR//2), title, font=fu, fill=TITLE_FG, anchor="lm")
    for i,(lbl,col) in enumerate([("✕",(200,60,60)),("□",(90,90,90)),("─",(90,90,90))]):
        d.text((W-14-i*26, TITLEBAR//2), lbl, font=fu, fill=col, anchor="mm")
    # terminal text
    y = TITLEBAR + PAD
    for l in lines:
        d.text((PAD, y), l.rstrip("\n"), font=f, fill=FG)
        y += ch
    img.save(out)
    print(out, img.size)

def load(p):
    return [l.rstrip("\n") for l in open(p, encoding="utf-8")]

for name, title in [("mnist","MNIST"), ("cnn","CNN")]:
    lines = load(f"{name}_uart.txt")
    render(lines, f"putty_{name}_full.png", title="COM6 - PuTTY")
    # compact: header + first 2 digits + last digit + tally
    hdr = lines[:4]
    blk = lines[4:24]                     # digits 0, 1 (10 lines each)
    tail = lines[-13:]                    # digit 9 + tally
    render(hdr + blk + ["", "      ... (2 ~ 8 생략) ..."] + tail, f"putty_{name}_summary.png", title="COM6 - PuTTY")
