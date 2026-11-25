"""Builds the six user manuals in docs/manuals/.

    pip install reportlab svglib
    python tools/manual/build.py

generate.py holds the layout and the Spanish text; translations.txt has one
line per string (es|en|fr|de|it|ca). Uses the Arial fonts shipped with Windows.
"""
import ast
import os
from pathlib import Path

root = Path(__file__).resolve().parent
os.chdir(root.parents[1])  # paths in generate.py are relative to the repo
base = (root / "generate.py").read_text(encoding="utf-8-sig")
base = base.replace(
    'text(306, 257, "o pulsa para elegirlo (.epub o .txt)", 10)',
    'paragraph(306, 253, "o pulsa para elegirlo (.epub o .txt)", 201, 9, 10)',
).replace(
    'text(109, 262, "Viaje al centro de la Tierra", 9)',
    'text(109, 262, "Viaje al centro de la Tierra", 8)',
)
rows = [line.split("|") for line in (root / "translations.txt").read_text(encoding="utf-8-sig").splitlines() if line]
assert all(len(row) == 6 for row in rows)
exec(compile((root / "generate.py").read_text(encoding="utf-8-sig"), "manual-es", "exec"), {})
for index, lang in enumerate(("en", "fr", "de", "it", "ca"), 1):
    words = {row[0]: row[index] for row in rows}
    words["docs/manuals/manual-ubiblion-es.pdf"] = f"docs/manuals/manual-ubiblion-{lang}.pdf"

    class Translate(ast.NodeTransformer):
        def visit_Constant(self, node):
            if isinstance(node.value, str) and node.value in words:
                node.value = words[node.value]
            return node

    tree = Translate().visit(ast.parse(base))
    exec(compile(tree, f"manual-{lang}", "exec"), {})

