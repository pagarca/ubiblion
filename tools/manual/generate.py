# Spanish source of the user manuals. Don't run it directly: build.py does,
# once per language, and writes the PDFs to docs/manuals/.
from reportlab.graphics import renderPDF
from reportlab.graphics.barcode.qr import QrCodeWidget
from reportlab.graphics.shapes import Drawing
from svglib.svglib import svg2rlg
from reportlab.pdfgen.canvas import Canvas
from reportlab.lib.pagesizes import A4
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.utils import simpleSplit

pdfmetrics.registerFont(TTFont("Body", "C:/Windows/Fonts/arial.ttf"))
pdfmetrics.registerFont(TTFont("Bold", "C:/Windows/Fonts/arialbd.ttf"))
c = Canvas("docs/manuals/manual-ubiblion-es.pdf", pagesize=A4)
c.setTitle("Manual de usuario de μbiblion")
c.setSubject("Guía ilustrada de lectura y carga de libros")
W, H = A4
INK = HexColor("#171c19")
MUTED = HexColor("#454c47")
LINE = HexColor("#aab2ac")
PALE = HexColor("#f0f3f0")

def text(x, y, value, size=12, font="Body", color=INK):
    c.setFont(font, size)
    c.setFillColor(color)
    c.drawString(x, H-y, value)

def paragraph(x, y, value, width, size=12, lead=17):
    for line in simpleSplit(value, "Body", size, width):
        text(x, y, line, size)
        y += lead
    return y

def box(x, y, w, h, fill=white, stroke=LINE, radius=8):
    c.setStrokeColor(stroke)
    c.setFillColor(fill)
    c.roundRect(x, H-y-h, w, h, radius, fill=1, stroke=1)

def rule(x1, y, x2):
    c.setStrokeColor(LINE)
    c.line(x1, H-y, x2, H-y)

def header(page, title, subtitle):
    text(42, 53, "μbiblion  /  MANUAL DE USUARIO", 10, "Bold", MUTED)
    text(42, 104, title, 34, "Bold")
    text(43, 131, subtitle, 12, color=MUTED)
    rule(42, 781, 553)
    text(42, 801, "μbiblion  ·  Firmware 1.0.0", 9, color=MUTED)
    text(519, 801, f"{page} / 2", 9, "Bold", MUTED)

def key(x, y, label, width=51):
    box(x, y, width, 24, PALE, INK, 5)
    c.setFillColor(INK)
    c.setFont("Bold", 10)
    c.drawCentredString(x+width/2, H-y-16, label)

def step(y, number, title, content):
    c.setFillColor(INK)
    c.circle(55, H-y+4, 12, fill=1, stroke=0)
    c.setFillColor(white)
    c.setFont("Bold", 11)
    c.drawCentredString(55, H-y, str(number))
    text(77, y, title, 14, "Bold")
    return paragraph(77, y+20, content, 469)

def cover():
    text(43, 74, "μbiblion  /  LECTOR DE TINTA ELECTRÓNICA", 10, "Bold", MUTED)
    rule(43, 91, 552)
    text(39, 173, "μbiblion", 66, "Bold")
    text(44, 220, "Manual de usuario", 28, "Bold")
    text(45, 254, "Lee, navega y añade tus libros por WiFi.", 14, color=MUTED)

    box(108, 306, 379, 383, PALE, LINE, 20)
    box(150, 330, 295, 333, white, INK, 21)
    box(179, 355, 237, 235, PALE, INK, 8)
    text(196, 383, "LA ISLA DEL TESORO", 11, "Bold")
    rule(196, 396, 399)
    text(196, 432, "Capítulo I", 19, "Bold")
    for i, width in enumerate((184, 202, 174, 195, 201, 153)):
        y = 457 + i*15
        c.setFillColor(LINE)
        c.roundRect(196, H-y, width, 3, 1.5, fill=1, stroke=0)
    rule(196, 557, 399)
    text(196, 575, "1 / 128", 9, color=MUTED)
    for i, name in enumerate(("PREV", "NEXT", "OK", "BACK")):
        key(179+i*62, 615, name)

    rule(42, 758, 553)
    text(43, 783, "GUÍA ILUSTRADA", 10, "Bold", MUTED)
    text(442, 783, "Firmware 1.0.0", 10, color=MUTED)
    c.showPage()

def back_cover():
    text(43, 74, "μbiblion  /  CONSULTA RÁPIDA", 10, "Bold", MUTED)
    rule(43, 91, 552)
    text(42, 160, "A mano mientras lees", 36, "Bold")
    text(43, 190, "Los cuatro gestos que más usarás.", 14, color=MUTED)

    shortcuts = (
        ("PREV", "Página anterior"),
        ("NEXT", "Página siguiente"),
        ("OK", "Biblioteca"),
        ("BACK", "WiFi si lo mantienes pulsado"),
    )
    for i, (button, action) in enumerate(shortcuts):
        x = 43 + (i % 2)*261
        y = 227 + (i // 2)*104
        box(x, y, 249, 91, PALE)
        key(x+15, y+15, button)
        text(x+15, y+65, action, 13, "Bold")

    box(43, 467, 510, 151, white, INK)
    text(61, 497, "Para añadir un libro", 17, "Bold")
    paragraph(61, 525, "Mantén BACK para encender el WiFi. Abre en el navegador la dirección que muestra el lector y elige el archivo EPUB o TXT.", 465, 13, 20)
    rule(61, 565, 535)
    text(61, 593, "Si aparece la red ubiblion, su clave es ubiblion.", 11, color=MUTED)

    url = "https://github.com/pagarca/ubiblion"
    box(43, 635, 510, 109, PALE)
    github = svg2rlg("tools/manual/github.svg")
    github.scale(2, 2)
    renderPDF.draw(github, c, 61, H-694)
    text(104, 676, "Proyecto en GitHub", 15, "Bold")
    text(104, 698, "github.com/pagarca/ubiblion", 12)
    text(104, 721, "Escanea el QR para abrir el proyecto.", 10, color=MUTED)
    c.linkURL(url, (59, H-703, 362, H-656), relative=0, thickness=0)

    qr = QrCodeWidget(url, barWidth=98, barHeight=98, barBorder=4, barLevel="M")
    c.setFillColor(white)
    c.rect(441, H-739, 98, 98, fill=1, stroke=0)
    drawing = Drawing(98, 98)
    drawing.add(qr)
    renderPDF.draw(drawing, c, 441, H-739)
    c.linkURL(url, (441, H-739, 539, H-641), relative=0, thickness=0)

    rule(42, 758, 553)
    text(43, 783, "μbiblion", 11, "Bold")
    text(436, 783, "Manual de usuario", 10, color=MUTED)
    c.showPage()

cover()

header(1, "Leer un libro", "Enciende el lector y usa los cuatro botones para navegar.")
box(42, 155, 256, 218, PALE)
text(56, 177, "Pantalla y botones", 11, "Bold")
box(91, 189, 159, 133, white, INK, 8)
text(105, 209, "Biblioteca", 11, "Bold")
rule(103, 216, 237)
box(103, 222, 135, 23, INK, INK, 3)
text(109, 238, "La isla del tesoro", 10, "Bold", white)
text(109, 262, "Viaje al centro de la Tierra", 9)
text(109, 285, "Ajustes...", 10)
rule(103, 296, 237)
text(105, 308, "OK abre, largo borra", 7)
for i, name in enumerate(("PREV", "NEXT", "OK", "BACK")):
    key(56+i*58, 331, name)
text(56, 365, "Esquema. La posición física puede variar.", 8, color=MUTED)

text(318, 180, "Desde la Biblioteca", 15, "Bold")
paragraph(318, 203, "Mueve la selección con PREV o NEXT. Da un toque a OK para abrir el libro resaltado.", 229)
text(318, 280, "Mientras lees", 15, "Bold")
paragraph(318, 303, "Pasa páginas con PREV y NEXT. Para volver a la Biblioteca, da un toque a OK o BACK.", 229)

text(42, 416, "Botones durante la lectura", 19, "Bold")
text(42, 443, "Un toque se registra al soltar. Para una pulsación larga, mantén un segundo.", 11)
box(42, 462, 511, 164)
text(56, 484, "Botón", 10, "Bold")
text(132, 484, "Toque corto", 10, "Bold")
text(316, 484, "Pulsación larga", 10, "Bold")
for i, (button, short, long) in enumerate((
    ("PREV", "Página anterior", "Retrocede 10 páginas"),
    ("NEXT", "Página siguiente", "Avanza 10 páginas"),
    ("OK", "Abre la Biblioteca", "Abre Ajustes"),
    ("BACK", "Abre la Biblioteca", "Enciende o apaga el WiFi"),
)):
    y = 487 + i*32
    rule(54, y+5, 541)
    key(56, y+8, button)
    text(132, y+24, short, 11)
    text(316, y+24, long, 11)

box(42, 651, 511, 96, PALE)
text(56, 673, "Ajustes y borrado", 12, "Bold")
paragraph(56, 693, "En la Biblioteca, selecciona Ajustes... para cambiar la fuente o el idioma. Para borrar un libro, resáltalo y mantén OK. Confirma con OK o cancela con BACK.", 477, 11, 17)
c.showPage()

header(2, "Añadir libros", "Envía archivos EPUB o TXT desde el navegador del móvil o del ordenador.")
box(42, 155, 511, 179, PALE)
box(59, 176, 152, 111, white, INK)
text(73, 196, "WiFi", 12, "Bold")
rule(70, 204, 200)
text(73, 224, "AP: ubiblion", 10)
text(73, 244, "Clave: ubiblion", 10)
text(73, 265, "http://192.168.4.1", 10, "Bold")
c.setStrokeColor(INK)
c.setLineWidth(1.5)
c.line(224, H-230, 260, H-230)
c.line(260, H-230, 253, H-224)
c.line(260, H-230, 253, H-236)
c.setLineWidth(1)
box(275, 176, 261, 111, white, INK)
rule(275, 201, 536)
text(288, 193, "http://192.168.4.1", 10, "Bold")
c.setDash(3, 3)
box(288, 213, 234, 59, white, LINE, 5)
c.setDash()
text(306, 238, "Arrastra un libro aquí", 12, "Bold")
text(306, 257, "o pulsa para elegirlo (.epub o .txt)", 10)
text(59, 312, "Pantalla del lector", 10, "Bold", MUTED)
text(275, 312, "Navegador del móvil u ordenador", 10, "Bold", MUTED)

step(373, 1, "Enciende el WiFi del lector",
     "Desde la Biblioteca o mientras lees, mantén pulsado BACK hasta que aparezca la pantalla WiFi.")
step(449, 2, "Conéctate y abre la dirección",
     "Si el lector muestra AP: ubiblion, conéctate a esa red con la clave ubiblion. Abre en el navegador la dirección que aparece en la pantalla del lector.")
step(525, 3, "Elige el archivo y espera",
     "Arrastra el EPUB o TXT a la página, o pulsa en el recuadro para elegirlo. Espera a que el libro aparezca como listo para leer.")
step(601, 4, "Vuelve a la Biblioteca",
     "En la pantalla WiFi del lector, pulsa BACK para apagar el WiFi. Selecciona el libro con PREV o NEXT y ábrelo con OK.")

box(42, 672, 511, 81, PALE)
text(56, 693, "Si el lector ya está en tu red WiFi", 12, "Bold")
paragraph(56, 714, "Conecta el móvil u ordenador a la misma red y abre la dirección que muestra el lector. Si la página no carga, revisa que sigues en esa red.", 477, 11)
c.showPage()
back_cover()
c.save()
