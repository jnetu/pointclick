#!/usr/bin/env python3
"""Gera a arte sintetica dos playgrounds (assets/sprites/playground/*.png).

Os PNGs sao quadrados coloridos e faixas assimetricas, so para exercitar clips, spritesheets,
`flip_x` e `set_animation` sem depender de arte de verdade. Nada aqui usa bibliotecas externas
e a saida e deterministica: o PNG usa blocos deflate sem compressao (nenhuma versao do zlib muda
os bytes), entao rodar o script duas vezes, em qualquer maquina, grava arquivos identicos.

    python3 scripts/make_playground_art.py          grava os arquivos
    python3 scripts/make_playground_art.py --check  confere que os arquivos do disco sao os que o
                                                    script geraria (sai com 1 se algum difere)

O --check roda no ctest (pointclick_playground_art) quando ha um Python 3.
"""

import struct
import sys
import zlib
from pathlib import Path

OUTPUT = Path(__file__).resolve().parent.parent / "assets" / "sprites" / "playground"

Color = tuple[int, int, int, int]

CLEAR: Color = (0, 0, 0, 0)
WOOD: Color = (150, 95, 45, 255)
FRAME: Color = (90, 55, 25, 255)
HINGE: Color = (40, 60, 200, 255)      # post on the hinge side: the asymmetry flip_x mirrors
HANDLE: Color = (250, 225, 60, 255)
DOORWAY: Color = (25, 25, 40, 255)
GOLD: Color = (200, 170, 50, 255)
ORANGE: Color = (250, 120, 40, 255)
LIGHT: Color = (255, 240, 120, 255)
BORDER: Color = (60, 45, 10, 255)


class Canvas:
    def __init__(self, width: int, height: int, fill: Color = CLEAR) -> None:
        self.width = width
        self.height = height
        self.pixels = [fill] * (width * height)

    def rect(self, x: int, y: int, w: int, h: int, color: Color) -> None:
        for row in range(max(y, 0), min(y + h, self.height)):
            for column in range(max(x, 0), min(x + w, self.width)):
                self.pixels[row * self.width + column] = color


def chunk(kind: bytes, data: bytes) -> bytes:
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def stored_zlib(raw: bytes) -> bytes:
    """A zlib stream of stored (uncompressed) blocks: identical bytes on every zlib."""
    out = bytearray(b"\x78\x01")
    position = 0
    while True:
        block = raw[position:position + 65535]
        position += len(block)
        last = position >= len(raw)
        out += struct.pack("<BHH", 1 if last else 0, len(block), len(block) ^ 0xFFFF)
        out += block
        if last:
            break
    out += struct.pack(">I", zlib.adler32(raw) & 0xFFFFFFFF)
    return bytes(out)


def encode(canvas: Canvas) -> bytes:
    raw = bytearray()
    for row in range(canvas.height):
        raw.append(0)  # filter: none
        for column in range(canvas.width):
            raw += bytes(canvas.pixels[row * canvas.width + column])
    header = struct.pack(">IIBBBBB", canvas.width, canvas.height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", stored_zlib(bytes(raw))) + chunk(b"IEND", b""))


def door_frame(canvas: Canvas, x: int, leaf_width: int) -> None:
    """One 32x64 frame at column x: the doorway, the hinge post and a leaf that swings away."""
    canvas.rect(x, 0, 32, 64, DOORWAY)
    canvas.rect(x, 0, leaf_width, 64, FRAME)
    canvas.rect(x + 2, 2, max(leaf_width - 4, 0), 60, WOOD)
    canvas.rect(x, 0, 6, 64, HINGE)  # the post stays put; flip_x moves it to the other side
    if leaf_width >= 24:
        canvas.rect(x + leaf_width - 8, 30, 4, 6, HANDLE)


def door_closed() -> Canvas:
    canvas = Canvas(32, 64)
    door_frame(canvas, 0, 32)
    return canvas


def door_open() -> Canvas:
    canvas = Canvas(128, 64)
    for index, leaf_width in enumerate((32, 22, 14, 8)):  # the last frame leaves the right open
        door_frame(canvas, index * 32, leaf_width)
    return canvas


def knocker_idle() -> Canvas:
    canvas = Canvas(16, 16)
    canvas.rect(0, 0, 16, 16, BORDER)
    canvas.rect(2, 2, 12, 12, GOLD)
    return canvas


def knocker_pulse() -> Canvas:
    canvas = Canvas(48, 16)
    for index, color in enumerate((GOLD, ORANGE, LIGHT)):
        canvas.rect(index * 16, 0, 16, 16, BORDER)
        canvas.rect(index * 16 + 2, 2, 12, 12, color)
    return canvas


FILES = {
    "door_closed.png": door_closed,
    "door_open.png": door_open,
    "knocker_idle.png": knocker_idle,
    "knocker_pulse.png": knocker_pulse,
}


def main(arguments: list[str]) -> int:
    check = arguments == ["--check"]
    if arguments and not check:
        print(__doc__)
        return 2
    differing = []
    for name, make in FILES.items():
        data = encode(make())
        path = OUTPUT / name
        if check:
            if not path.is_file() or path.read_bytes() != data:
                differing.append(name)
        else:
            OUTPUT.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            print(f"{path.relative_to(OUTPUT.parent.parent.parent)}: {len(data)} bytes")
    if differing:
        print("arte dos playgrounds fora do script: " + ", ".join(differing)
              + " (rode scripts/make_playground_art.py)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
