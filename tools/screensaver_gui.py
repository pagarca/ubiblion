#!/usr/bin/env python3
"""Small window to choose the screensaver images and upload them.

    pip install pillow
    python tools/screensaver_gui.py

Pick up to four images, see them exactly as the e-paper will show them, put
them in order and press the upload button. It keeps copies in
assets/screensavers/local/ (ignored by git, so your pictures stay private),
runs make_screensavers.py and builds and flashes the firmware with PlatformIO.
"""

import queue
import re
import shutil
import subprocess
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from PIL import Image, ImageOps, ImageTk

import make_screensavers as converter

ENVIRONMENTS = ("esp32dev", "nodemcuv2")


def current_images():
    """Your own images or, if there are none yet, the default ones."""
    return converter.find_images(converter.LOCAL) or converter.find_images(converter.SOURCE)


def preview(path):
    """The image as the panel will show it, from the same bytes that get flashed."""
    packed = Image.frombytes("1", (converter.SIZE, converter.SIZE), converter.convert(path))
    return ImageOps.invert(packed.convert("L"))  # set bits are ink


def save_images(paths):
    """Makes assets/screensavers/local/ hold exactly `paths`, in that order, and
    regenerates the header. The default images are never touched."""
    # Read everything first: some sources are files this is about to replace.
    files = []
    for number, path in enumerate(paths, 1):
        name = re.sub(r"^\d\d-", "", path.stem)
        files.append((f"{number:02d}-{name}{path.suffix.lower()}", path.read_bytes()))
    converter.LOCAL.mkdir(parents=True, exist_ok=True)
    for old in converter.find_images(converter.LOCAL):
        old.unlink()
    saved = []
    for name, data in files:
        target = converter.LOCAL / name
        target.write_bytes(data)
        saved.append(target)
    converter.main()
    return saved


class App:
    def __init__(self, root):
        self.root = root
        self.images = []  # (path, PhotoImage)
        self.output = queue.Queue()
        self.busy = False
        self.dirty = False

        root.title("μbiblion · Salvapantallas")
        root.resizable(False, False)
        frame = ttk.Frame(root, padding=12)
        frame.pack()

        self.slots = []
        for column in range(converter.MAX_IMAGES):
            box = ttk.LabelFrame(frame, text=f"Imagen {column + 1}", padding=6)
            box.grid(row=0, column=column, padx=4)
            holder = tk.Frame(box, width=204, height=204, bg="#c8ccc8")
            holder.pack()
            holder.pack_propagate(False)
            picture = tk.Label(holder, bg="#c8ccc8")
            picture.pack(expand=True)
            name = ttk.Label(box, width=28, anchor="center")
            name.pack(pady=4)
            buttons = ttk.Frame(box)
            buttons.pack()
            left = ttk.Button(buttons, text="◀", width=3, command=lambda i=column: self.move(i, -1))
            change = ttk.Button(buttons, text="Elegir…", command=lambda i=column: self.choose(i))
            remove = ttk.Button(buttons, text="Quitar", command=lambda i=column: self.remove(i))
            right = ttk.Button(buttons, text="▶", width=3, command=lambda i=column: self.move(i, 1))
            for widget in (left, change, remove, right):
                widget.pack(side="left", padx=1)
            self.slots.append((picture, name, left, change, remove, right))

        bar = ttk.Frame(frame)
        bar.grid(row=1, column=0, columnspan=converter.MAX_IMAGES, sticky="we", pady=(12, 6))
        ttk.Label(bar, text="Placa:").pack(side="left")
        self.environment = ttk.Combobox(bar, values=ENVIRONMENTS, state="readonly", width=12)
        self.environment.set(ENVIRONMENTS[0])
        self.environment.pack(side="left", padx=6)
        self.upload_button = ttk.Button(bar, text="Guardar y flashear", command=self.upload)
        self.upload_button.pack(side="right")
        self.save_button = ttk.Button(bar, text="Solo guardar", command=self.save)
        self.save_button.pack(side="right", padx=6)

        self.status = ttk.Label(frame, text="Se muestran en este orden, una en cada activación.")
        self.status.grid(row=2, column=0, columnspan=converter.MAX_IMAGES, sticky="w")
        self.log = tk.Text(frame, height=10, state="disabled", font=("Consolas", 9))
        self.log.grid(row=3, column=0, columnspan=converter.MAX_IMAGES, sticky="we", pady=(6, 0))

        self.load(current_images()[: converter.MAX_IMAGES])
        root.protocol("WM_DELETE_WINDOW", self.close)

    # --- image list ---

    def load(self, paths):
        self.images = []
        for path in paths:
            item = self.read(path)
            if item:
                self.images.append(item)
        self.refresh()

    def read(self, path):
        try:
            return path, ImageTk.PhotoImage(preview(path), master=self.root)
        except (OSError, ValueError) as error:
            messagebox.showerror("Imagen no válida", f"{path.name}\n\n{error}")
            return None

    def choose(self, index):
        """Replaces a slot, or fills the free ones with as many files as fit."""
        replacing = index < len(self.images)
        ask = filedialog.askopenfilename if replacing else filedialog.askopenfilenames
        chosen = ask(parent=self.root, filetypes=[("Imágenes PNG o JPG", "*.png *.jpg *.jpeg")])
        if not chosen:
            return
        paths = [Path(chosen)] if replacing else [Path(p) for p in self.root.tk.splitlist(chosen)]
        items = [item for item in map(self.read, paths) if item]
        if not items:
            return
        if replacing:
            self.images[index] = items[0]
        else:
            free = converter.MAX_IMAGES - len(self.images)
            if len(items) > free:
                messagebox.showinfo("Máximo alcanzado", f"Solo caben {free} más.")
            self.images += items[:free]
        self.changed()

    def remove(self, index):
        del self.images[index]
        self.changed()

    def move(self, index, step):
        other = index + step
        self.images[index], self.images[other] = self.images[other], self.images[index]
        self.changed()

    def changed(self):
        self.dirty = True
        self.status.configure(text="Hay cambios sin guardar.")
        self.refresh()

    def refresh(self):
        def enable(widget, condition):
            widget.state(["!disabled"] if condition and not self.busy else ["disabled"])

        count = len(self.images)
        for index, (picture, name, left, change, remove, right) in enumerate(self.slots):
            used = index < count
            if used:
                path, photo = self.images[index]
                picture.configure(image=photo, text="")
                name.configure(text=path.name if len(path.name) <= 28 else path.name[:27] + "…")
            else:
                picture.configure(image="", text="Libre")
                name.configure(text="")
            enable(left, used and index > 0)
            # Slots fill from the left: only the first free one accepts images.
            enable(change, index <= count)
            enable(remove, used)
            enable(right, used and index < count - 1)
        enable(self.save_button, count > 0)
        enable(self.upload_button, count > 0)

    # --- saving and flashing ---

    def save(self):
        kept = {path.resolve() for path, _ in self.images}
        dropped = [p.name for p in converter.find_images(converter.LOCAL) if p.resolve() not in kept]
        if dropped and not messagebox.askokcancel(
            "Quitar imágenes",
            "Se borrarán de assets/screensavers/local/:\n\n" + "\n".join(dropped),
        ):
            return False
        try:
            saved = save_images([path for path, _ in self.images])
        except (OSError, ValueError, SystemExit) as error:
            messagebox.showerror("No se pudo guardar", str(error))
            return False
        self.images = [(path, photo) for path, (_, photo) in zip(saved, self.images)]
        self.dirty = False
        self.status.configure(text=f"Guardadas {len(saved)} imágenes. Falta flashear.")
        self.refresh()
        return True

    def upload(self):
        if not self.save():
            return
        pio = shutil.which("pio")
        command = [pio] if pio else [sys.executable, "-m", "platformio"]
        command += ["run", "-e", self.environment.get(), "-t", "upload"]
        self.busy = True
        self.refresh()
        self.environment.state(["disabled"])
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")
        self.status.configure(text="Compilando y flasheando… no desconectes el lector.")
        threading.Thread(target=self.run, args=(command,), daemon=True).start()
        self.root.after(100, self.poll)

    def run(self, command):
        """Worker thread: only talks to the window through the queue."""
        try:
            process = subprocess.Popen(
                command,
                cwd=converter.ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
            for line in process.stdout:
                self.output.put(line)
            self.output.put(process.wait())
        except OSError as error:
            self.output.put(f"{error}\n")
            self.output.put(1)

    def poll(self):
        while not self.output.empty():
            item = self.output.get()
            if isinstance(item, str):
                self.log.configure(state="normal")
                self.log.insert("end", item)
                self.log.see("end")
                self.log.configure(state="disabled")
                continue
            self.busy = False
            self.environment.state(["!disabled"])
            self.refresh()
            self.status.configure(
                text="Lector actualizado."
                if item == 0
                else "Ha fallado: revisa el registro y que el lector esté conectado por USB."
            )
            return
        self.root.after(100, self.poll)

    def close(self):
        if self.busy:
            messagebox.showinfo("Flasheando", "Espera a que termine antes de cerrar.")
            return
        if self.dirty and not messagebox.askokcancel("Salir", "Hay cambios sin guardar. ¿Salir?"):
            return
        self.root.destroy()


if __name__ == "__main__":
    window = tk.Tk()
    App(window)
    window.mainloop()
