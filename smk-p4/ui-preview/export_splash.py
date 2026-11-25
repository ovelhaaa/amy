"""Export real LVGL framebuffer captures; Pillow is preview-only, not firmware."""
import sys
from pathlib import Path
from PIL import Image
source = Path(sys.argv[1])
for path in source.glob("*.ppm"):
    with Image.open(path) as image:
        image.save(path.with_suffix(".png"))
frames = [Image.open(path).convert("RGB") for path in sorted(source.glob("frame_*.png"))]
frames[0].save(source / "p4my_preview.gif", save_all=True, append_images=frames[1:], duration=33, loop=0)
for frame in frames:
    frame.close()
(source / "replay.html").write_text("""<!doctype html><meta charset="utf-8"><title>P4my — LVGL manual replay</title>
<style>body{background:#080f17;color:#e6edf2;font:16px sans-serif;text-align:center}img{width:800px;height:480px}input{width:700px}</style>
<p>P4my · LVGL 9.3 · 800 × 480</p><img id="frame" src="frame_000.png"><p>
<button id="play">Play / pause</button><input id="time" type="range" min="0" max="90" value="0"><output id="ms">0 ms</output></p>
<script>const s=document.querySelector('#time'),f=document.querySelector('#frame'),o=document.querySelector('#ms');let timer;
function show(){f.src='frame_'+String(s.value).padStart(3,'0')+'.png';o.value=(s.value*33)+' ms'}
s.oninput=show;document.querySelector('#play').onclick=()=>{if(timer){clearInterval(timer);timer=null}else timer=setInterval(()=>{s.value=(+s.value+1)%91;show()},33)}</script>
""", encoding="utf-8")
print(f"Exported four milestones, 91 frames, GIF and manual scrubber: {source}")
