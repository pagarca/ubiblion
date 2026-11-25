# Screensavers

[← Back to the README](../README.md)

Long-press **BACK** while reading or in the menus to show a picture. Up to four take turns, one per
activation (the count starts over after a restart), and any button goes back to where you were
without doing anything else. The reader sleeps as usual meanwhile, and e-paper keeps the picture even
with the power off. During indexing and on the WiFi screen, BACK keeps its normal meaning.

The pictures are compiled into the firmware, so changing them means uploading it again; books and
settings are not touched. The easy way is this window (needs Python with Pillow and PlatformIO; its
labels are in Spanish):

```bash
pip install pillow
python tools/screensaver_gui.py
```

1. Press **Elegir…** to pick PNG or JPG images; several at once fill the free slots.
2. Check the preview, which is exactly what the panel will show. Order them with the arrows and drop
   any with **Quitar**.
3. Leave the board on `esp32dev` (or choose `nodemcuv2`), connect the reader over USB and press
   **Guardar y flashear**. **Solo guardar** prepares the files without uploading.

Images are cropped to a centred square, scaled to 200x200 and dithered to black and white, so a
centred subject with good contrast works best.

Your pictures are copied to `assets/screensavers/local/` and turned into `src/screensavers_local.h`.
Git ignores both, so they never end up in a commit. With that folder empty, the firmware uses the
default pictures in `assets/screensavers/`. To do it by hand, put the files in the local folder, run
`python tools/make_screensavers.py` and upload the firmware. Each picture takes 5000 bytes of program
flash.
