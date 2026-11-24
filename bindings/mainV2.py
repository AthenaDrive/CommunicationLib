"""AthenaDrive single-screen control deck.

Everything from the original "Plots" and "Tuning" tabs now lives on one page:

  * six live telemetry plots
  * KPI cards with mini sparklines
  * torque, velocity and position setpoints
  * the full PID / limit tuning table (saved between runs)
  * link, packet, loop-time and debug diagnostics

Run it from the bindings folder (next to EthernetPython.pyd):

    ..\\venv\\Scripts\\python.exe mainV2.py
"""

import json
import math
import struct
from collections import deque
from functools import lru_cache
from pathlib import Path
from time import perf_counter

import pygame as pg

import EthernetPython
import pg_widgets as pw
from pg_widgets.utils.Primitives import roundedRect


# ---------------------------------------------------------------------------
# Palette
# ---------------------------------------------------------------------------
BG_TOP = (4, 7, 20)
BG_BOTTOM = (4, 30, 38)
WIDGET_BG = (7, 15, 27)
CARD_BG = (10, 21, 35)
CARD_EDGE = (26, 55, 76)
PLOT_BG = (8, 18, 31)
TEXT = (226, 241, 255)
MUTED = (116, 152, 174)
DIM = (72, 104, 125)
CYAN = (0, 229, 255)
GREEN = (57, 255, 136)
PINK = (255, 64, 160)
AMBER = (255, 196, 64)
ORANGE = (255, 128, 72)
PURPLE = (176, 140, 255)
RED = (255, 86, 86)
SKY = (104, 190, 255)

FONT_UI = "segoeui"
FONT_MONO = "consolas"


# ---------------------------------------------------------------------------
# Behaviour / protocol constants
# ---------------------------------------------------------------------------
MAX_VAL_TUNING_ROWS = 10
I_TERM_MULT = 0.00001

# (row name, [(label, TCP command bit, min, max, initial value)])
TUNING_ROWS = [
    ("Torque", [
        ("Torque Kp", EthernetPython.TCP_COMMAND_TORQUE_KP, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Torque Ki", EthernetPython.TCP_COMMAND_TORQUE_KI, 0.0, MAX_VAL_TUNING_ROWS * I_TERM_MULT, 0.0),
        ("Torque Kd", EthernetPython.TCP_COMMAND_TORQUE_KD, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Torque limit", EthernetPython.TCP_COMMAND_TORQUE_LIMIT, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
    ]),
    ("Velocity", [
        ("Velocity Kp", EthernetPython.TCP_COMMAND_VELOCITY_KP, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Velocity Ki", EthernetPython.TCP_COMMAND_VELOCITY_KI, 0.0, MAX_VAL_TUNING_ROWS * I_TERM_MULT, 0.0),
        ("Velocity Kd", EthernetPython.TCP_COMMAND_VELOCITY_KD, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Velocity limit", EthernetPython.TCP_COMMAND_VELOCITY_LIMIT, 0.0, 100.0, 0.0),
    ]),
    ("Position", [
        ("Position Kp", EthernetPython.TCP_COMMAND_POSITION_KP, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
        ("Position Ki", EthernetPython.TCP_COMMAND_POSITION_KI, 0.0, MAX_VAL_TUNING_ROWS * I_TERM_MULT, 0.0),
        ("Position Kd", EthernetPython.TCP_COMMAND_POSITION_KD, 0.0, MAX_VAL_TUNING_ROWS, 0.0),
    ]),
    ("Limits", [
        ("Bus current limit", EthernetPython.TCP_COMMAND_CURRENT_LIMIT_BUS, 0.0, 100.0, 0.0),
        ("Phase current limit", EthernetPython.TCP_COMMAND_CURRENT_LIMIT_PHASE, 0.0, 100.0, 0.0),
    ]),
]

GROUP_ACCENTS = {
    "Torque": PINK,
    "Velocity": GREEN,
    "Position": CYAN,
    "Limits": AMBER,
}

MAXIMA_VAL_SETPOINTS = 100
# (label, slider min, slider max, initial value, accent)
UDP_SETPOINTS = [
    ("Torque setpoint", -5, 5, 0.0, PINK),
    ("Velocity setpoint", -MAXIMA_VAL_SETPOINTS, MAXIMA_VAL_SETPOINTS, 0.0, GREEN),
    ("Position setpoint", -MAXIMA_VAL_SETPOINTS, MAXIMA_VAL_SETPOINTS, 0.0, CYAN),
]

INITIAL_DRIVING_MODE = 0b101

MAX_PLOT_POINTS = 500

TUNING_FILE = Path(__file__).with_name("tuning_parameters.json")

WINDOW_SIZE = (1500, 850)
WINDOW_TITLE = "AthenaDrive // Motor Control Deck"

# Everything the dashboard displays, requested over UDP in one config write.
UDP_DATA_CONFIG = (
    EthernetPython.UDP_DATA_TIMESTAMP
    | EthernetPython.UDP_DATA_POSITION
    | EthernetPython.UDP_DATA_VELOCITY
    | EthernetPython.UDP_DATA_ACCELERATION
    | EthernetPython.UDP_DATA_TORQUE
    | EthernetPython.UDP_DATA_PHASE_CURRENT_A
    | EthernetPython.UDP_DATA_PHASE_CURRENT_B
    | EthernetPython.UDP_DATA_PHASE_CURRENT_C
    | EthernetPython.UDP_DATA_BUS_CURRENT
    | EthernetPython.UDP_DATA_BUS_VOLTAGE
    | EthernetPython.UDP_DATA_ERROR_REGISTER
    | EthernetPython.UDP_DATA_LOOP_TIME_FOC
    | EthernetPython.UDP_DATA_LOOP_TIME_SECONDARY
    | EthernetPython.DEBUG_FLOAT_0
    | EthernetPython.DEBUG_FLOAT_1
    | EthernetPython.DEBUG_FLOAT_2
    | EthernetPython.DEBUG_FLOAT_3
)


# ---------------------------------------------------------------------------
# Small formatting helpers
# ---------------------------------------------------------------------------
def formatValue(value, unit="", digits=2):
    """Format a float for display, or an em dash when it is missing."""
    if value is None:
        return "—"
    return f"{value:.{digits}f}{unit}"


def formatCompact(value, digits=2):
    """Compact number that stays readable for both tiny and huge values."""
    if value is None:
        return "—"
    if value == 0 or not math.isfinite(value):
        return "0" if value == 0 else "—"

    magnitude = abs(value)
    if 1e-3 <= magnitude < 1e5:
        return f"{value:.{digits}f}"
    return f"{value:.{digits}e}"


def formatGain(value):
    """Readable slider text for PID gains, including tiny I-term values."""
    if not math.isfinite(value):
        return "0"
    if value == 0:
        return "0"

    magnitude = abs(value)
    if magnitude < 1e-3:
        return f"{value:.2e}"
    if magnitude < 0.01:
        return f"{value:.5f}".rstrip("0").rstrip(".")
    if magnitude < 10:
        return f"{value:.4f}".rstrip("0").rstrip(".")
    return f"{value:.2f}".rstrip("0").rstrip(".")


def formatSetpoint(value):
    return f"{value:.2f}"


def formatDuration(seconds):
    total = max(0, int(seconds))
    hours, remainder = divmod(total, 3600)
    minutes, secs = divmod(remainder, 60)
    if hours:
        return f"{hours:02d}:{minutes:02d}:{secs:02d}"
    return f"{minutes:02d}:{secs:02d}"


def clamp(value, lower, upper):
    return min(max(value, lower), upper)


def isFinite(value):
    return value is not None and math.isfinite(value)


def mixColor(first, second, amount):
    return tuple(int(a + (b - a) * amount) for a, b in zip(first, second))


def _floatToBits(value):
    """Convert a Python float to the raw uint32 sent over the wire."""
    return struct.unpack("<I", struct.pack("<f", float(value)))[0]


def _unwrap(widget):
    """Return the real widget inside a pg_widgets Border wrapper, if any."""
    children = getattr(widget, "_uiElements", None)
    if isinstance(children, dict) and "main" in children:
        return children["main"]
    return widget


@lru_cache(maxsize=512)
def _font(name, size, bold=False):
    if not pg.font.get_init():
        pg.font.init()
    return pg.font.SysFont(name, max(6, int(size)), bold=bold)


def drawText(surface, text, rect, color, *, fontName=FONT_UI, bold=False,
             align="left", valign="middle", padding=0.04):
    """Draw text scaled down until it fits inside rect."""
    if not text:
        return

    rect = pg.Rect(rect)
    padX = int(rect.width * padding)
    padY = int(rect.height * padding)
    inner = pg.Rect(rect.x + padX, rect.y + padY,
                    max(1, rect.width - 2 * padX), max(1, rect.height - 2 * padY))

    size = max(7, int(inner.height * 1.15))
    while size > 7:
        font = _font(fontName, size, bold)
        textWidth, textHeight = font.size(text)
        if textWidth <= inner.width and textHeight <= inner.height:
            break
        size -= 1

    font = _font(fontName, size, bold)
    textWidth, textHeight = font.size(text)

    if align == "right":
        x = inner.right - textWidth
    elif align == "center":
        x = inner.x + (inner.width - textWidth) // 2
    else:
        x = inner.x

    if valign == "top":
        y = inner.y
    elif valign == "bottom":
        y = inner.bottom - textHeight
    else:
        y = inner.y + (inner.height - textHeight) // 2

    surface.blit(font.render(text, True, color), (x, y))


# ---------------------------------------------------------------------------
# Custom widgets
# ---------------------------------------------------------------------------
class Label(pw.UIGroup):
    """Text only, no card behind it. Caches its rendered surface."""

    def __init__(self, pos, size, text="", color=TEXT, *, fontName=FONT_UI,
                 bold=False, align="left"):
        super().__init__(pos, size)
        self._text = text
        self._color = color
        self._fontName = fontName
        self._bold = bold
        self._align = align
        self._cacheKey = None
        self._cache = None

    def setText(self, text):
        self._text = text

    def getText(self):
        return self._text

    def setTextColor(self, color):
        self._color = color

    def render(self, bgColor, debug=False):
        width, height = (max(1, int(v)) for v in self.getSize())
        key = (self._text, self._color, width, height)
        if key == self._cacheKey:
            return self._cache

        surface = pg.Surface((width, height), pg.SRCALPHA)
        drawText(surface, self._text, surface.get_rect(), self._color,
                 fontName=self._fontName, bold=self._bold, align=self._align,
                 padding=0.0)
        self._cacheKey = key
        self._cache = surface
        return surface


class SectionTitle(pw.UIGroup):
    """Uppercase section heading with a neon marker and a thin rule."""

    def __init__(self, pos, size, text, accent=CYAN):
        super().__init__(pos, size)
        self._text = text
        self._accent = accent
        self._cacheKey = None
        self._cache = None

    def render(self, bgColor, debug=False):
        width, height = (max(1, int(v)) for v in self.getSize())
        key = (self._text, self._accent, width, height)
        if key == self._cacheKey:
            return self._cache

        surface = pg.Surface((width, height), pg.SRCALPHA)
        marker = max(3, int(height * 0.20))
        markerY = (height - marker) // 2
        pg.draw.rect(surface, self._accent, (0, markerY, marker, marker))

        font = _font(FONT_UI, max(7, int(height * 0.80)), bold=True)
        textSurface = font.render(self._text.upper(), True, self._accent)
        textX = marker + max(6, int(height * 0.42))
        textY = max(0, (height - textSurface.get_height()) // 2)
        surface.blit(textSurface, (textX, textY))

        lineX = textX + textSurface.get_width() + max(6, int(height * 0.40))
        if lineX < width - 2:
            lineColor = mixColor(WIDGET_BG, self._accent, 0.24)
            pg.draw.line(surface, lineColor, (lineX, height // 2),
                         (width - 2, height // 2), 1)

        self._cacheKey = key
        self._cache = surface
        return surface


class StatCard(pw.UIGroup):
    """KPI card with a big value and an optional live sparkline."""

    def __init__(self, pos, size, label, accent, *, sparkline=True):
        super().__init__(pos, size)
        self._label = label.upper()
        self._accent = accent
        self._value = "—"
        self._history = deque(maxlen=80)
        self._sparkline = sparkline

    def feed(self, value, text, accent=None):
        self._value = text
        if accent is not None:
            self._accent = accent
        if value is not None and math.isfinite(value):
            self._history.append(value)

    def setValue(self, text, accent=None):
        self._value = text
        if accent is not None:
            self._accent = accent

    def render(self, bgColor, debug=False):
        width, height = (max(1, int(v)) for v in self.getSize())
        surface = pg.Surface((width, height), pg.SRCALPHA)
        outer = pg.Rect(0, 0, width, height)
        radius = max(4, min(int(height * 0.30), min(width, height) // 2))

        pg.draw.rect(surface, CARD_BG, outer, border_radius=radius)
        pg.draw.rect(surface, CARD_EDGE, outer, width=1, border_radius=radius)

        barWidth = max(3, int(width * 0.016))
        barHeight = int(height * 0.54)
        pg.draw.rect(surface, self._accent,
                     (barWidth, (height - barHeight) // 2, barWidth, barHeight),
                     border_radius=max(1, barWidth // 2))

        if self._sparkline and len(self._history) >= 2:
            sparkRect = pg.Rect(int(width * 0.40), int(height * 0.54),
                                int(width * 0.52), int(height * 0.36))
            values = list(self._history)
            low = min(values)
            high = max(values)
            span = high - low
            points = []
            for index, value in enumerate(values):
                x = sparkRect.x + int(sparkRect.width * index / max(1, len(values) - 1))
                ratio = 0.5 if span == 0 else (value - low) / span
                y = sparkRect.bottom - int(ratio * sparkRect.height)
                points.append((x, y))
            if len(points) >= 2:
                pg.draw.lines(surface, mixColor(CARD_BG, self._accent, 0.60),
                              False, points, max(1, int(height * 0.035)))

        labelRect = pg.Rect(int(width * 0.07), int(height * 0.04),
                            int(width * 0.86), int(height * 0.33))
        drawText(surface, self._label, labelRect, MUTED,
                 fontName=FONT_UI, bold=True, padding=0.0)

        valueRect = pg.Rect(int(width * 0.07), int(height * 0.33),
                            int(width * 0.88), int(height * 0.62))
        drawText(surface, self._value, valueRect, self._accent,
                 fontName=FONT_MONO, bold=True, padding=0.0)

        return surface


class NeonButton(pw.Button):
    """Button with a dark fill and a neon outline. Configure after creation."""

    def __init__(self, pos, size=(1.0, 1.0)):
        super().__init__(pos, size)
        self._accent = CYAN

    def configure(self, text, accent):
        self.setText(text)
        self._accent = accent
        self._updateSurf = True

    def render(self, bgColor=(0, 0, 0)):
        width, height = (max(1, int(v)) for v in self.getSize())
        surface = pg.Surface((width, height), pg.SRCALPHA)
        rect = surface.get_rect()
        radius = max(4, min(int(height * 0.28), min(width, height) // 2))

        if self._isPress:
            pg.draw.rect(surface, self._accent, rect, border_radius=radius)
            textColor = (5, 12, 20)
        else:
            pg.draw.rect(surface, (11, 25, 42), rect, border_radius=radius)
            pg.draw.rect(surface, self._accent, rect,
                         width=max(1, int(height * 0.045)), border_radius=radius)
            textColor = self._accent

        drawText(surface, self._text, rect, textColor,
                 fontName=FONT_UI, bold=True, align="center", padding=0.06)
        return surface


class CleanPlot(pw.Plot):
    """Plot with compact, properly placed x-axis tick labels.

    The stock widget draws both x ticks near the right edge and uses raw
    str() output, so a rolling time axis shows long floats in the corner.
    """

    def render(self, bgColor=(0, 0, 0)):
        if self._updateSurfBase:
            self._updateSurfBase = False

            width = self._parentSize[0] * self._size[0]
            height = self._parentSize[1] * self._size[1]

            self._surfBase = pg.Surface((width, height))
            self._surfBase.fill(bgColor)

            roundedRect(self._surfBase, pg.Rect(0, 0, width, height), 20,
                        self._getColor("bgColor"))

            outerPoints = [
                (self._min_px, self._min_py),
                (self._max_px, self._min_py),
                (self._max_px, self._max_py),
                (self._min_px, self._max_py),
            ]
            pg.draw.aalines(self._surfBase, (0, 0, 0), True, outerPoints)

            midX = (self._min_px + self._max_px) / 2
            midY = (self._min_py + self._max_py) / 2

            # Keep the title to the right of the top-left y-axis tick so a
            # long live value cannot collide with it on narrow windows.
            titleFont = self._getFont(20)
            titleSurface = titleFont.render(
                self._title, True, self._getColor("textColor"),
                self._getColor("bgColor"))
            titleLeft = self._min_px * 2.0 + 22
            titleCenter = (titleLeft + self._max_px) / 2
            self._surfBase.blit(titleSurface, (
                int(titleCenter - titleSurface.get_width() / 2),
                max(0, (self._min_py - titleSurface.get_height()) // 2)))

            self._textItAbsolute(self._surfBase, self._x_label, midX,
                                 self._max_py + self._min_py // 2)
            self._textItAbsolute(self._surfBase, self._y_label,
                                 self._min_px // 2, midY, rotate=True)

        if self._updateSurf:
            self._updateSurf = False

            self._surf = self._surfBase.copy()
            for plotIx, points in enumerate(self._getPixelPos()):
                if len(points) > 2:
                    pg.draw.aalines(self._surf, self._getColor(f"plot{plotIx}"),
                                    False, points)

            tickY = self._max_py + self._min_py // 2 - 15
            self._textItAbsolute(self._surf, self._formatN(self._x_low),
                                 self._min_px, tickY)
            self._textItAbsolute(self._surf, self._formatN(self._x_low + self._dx),
                                 self._max_px, tickY)

            self._textItAbsolute(self._surf, self._formatN(self._y_low),
                                 self._min_px * 2.0, self._max_py)
            self._textItAbsolute(self._surf, self._formatN(self._y_low + self._dy),
                                 self._min_px * 2.0, self._min_py)

        return self._surf


class TuningRow(pw.TuningSliders):
    """TuningSliders with larger label/value boxes and nicer number text."""

    def __init__(self, pos, size=(1.0, 1.0), *, labels, lower_bounds=None,
                 upper_bounds=None, current_values=None, formatter=formatGain):
        super().__init__(pos, size, labels=labels, lower_bounds=lower_bounds,
                         upper_bounds=upper_bounds, current_values=current_values)
        self._formatter = formatter
        self._bounds = {}
        for index, label in enumerate(labels):
            lower = lower_bounds[index] if lower_bounds is not None else None
            upper = upper_bounds[index] if upper_bounds is not None else None
            self._bounds[label] = (lower, upper)

        # TuningSliders wraps every child in a Border with a 3 px inset.
        # That is fine for a tall row, but it leaves almost no room for text
        # in a compact dashboard row. Replace the wrappers with the bare
        # widgets so the full row height is available.
        for label in self._labels:
            for name in (f"text{label}", f"value{label}", f"slider{label}"):
                wrapper = self[name]
                inner = _unwrap(wrapper)
                if inner is not wrapper:
                    self._uiElements[name] = inner

        count = len(self._labels)
        for index, label in enumerate(self._labels):
            posX = index / count
            boxWidth = 1.0 / count

            self[f"text{label}"]._pos = (posX, 0.00)
            self[f"text{label}"]._size = (boxWidth, 0.28)
            self[f"value{label}"]._pos = (posX, 0.28)
            self[f"value{label}"]._size = (boxWidth, 0.28)
            self[f"slider{label}"]._pos = (posX, 0.56)
            self[f"slider{label}"]._size = (boxWidth, 0.44)

        for label in self._labels:
            value = self[f"slider{label}"].getValue()
            formatted = self._formatter(value)
            self[f"value{label}"].setText(formatted)

        self._lastText = {
            label: self[f"value{label}"].getText()
            for label in self._labels
        }

    def update(self, mousePress, mousePos):
        before = {
            label: self[f"slider{label}"].getValue()
            for label in self._labels
        }

        # Before the mouse update: apply a finished text edit. A text box is
        # only treated as user input when its content differs from the last
        # text we wrote ourselves; otherwise formatting (e.g. 3 decimals)
        # would feed back and quantise the slider value every frame.
        for label in self._labels:
            valueBox = self[f"value{label}"]
            slider = self[f"slider{label}"]
            if valueBox._isSelected:
                continue

            currentText = valueBox.getText()
            expected = self._formatter(before[label])
            lastText = self._lastText.get(label, expected)

            if currentText != lastText and currentText != expected:
                try:
                    typed = float(currentText)
                except ValueError:
                    typed = before[label]

                if not math.isfinite(typed):
                    typed = before[label]

                lower, upper = self._bounds.get(label, (None, None))
                if lower is not None:
                    typed = max(typed, lower)
                if upper is not None:
                    typed = min(typed, upper)

                if typed != before[label]:
                    slider.setValue(typed)
                expected = self._formatter(typed)

            if valueBox.getText() != expected:
                valueBox.setText(expected)
            self._lastText[label] = expected

        # Call UIGroup.update directly. TuningSliders.update treats a dragged
        # slider as typed text and snaps it back to the old value box text.
        pw.UIGroup.update(self, mousePress, mousePos)

        # After the mouse update: reflect the slider value in its text box.
        for label in self._labels:
            valueBox = self[f"value{label}"]
            if valueBox._isSelected:
                continue

            expected = self._formatter(self[f"slider{label}"].getValue())
            if valueBox.getText() != expected:
                valueBox.setText(expected)
            self._lastText[label] = expected


class Deck(pw.UIGroup):
    """Root group with a cached gradient background and custom render order."""

    def __init__(self, pos=(0.0, 0.0), size=(1.0, 1.0)):
        super().__init__(pos, size)
        self._background = None

    def changeSize(self, newSize):
        super().changeSize(newSize)
        self._background = None

    def _buildBackground(self):
        width, height = (max(1, int(v)) for v in self.getSize())
        surface = pg.Surface((width, height))

        for y in range(height):
            ratio = y / max(1, height - 1)
            surface.fill(mixColor(BG_TOP, BG_BOTTOM, ratio), (0, y, width, 1))

        grid = pg.Surface((width, height), pg.SRCALPHA)
        step = max(24, int(min(width, height) * 0.055))
        for x in range(0, width, step):
            pg.draw.line(grid, (80, 200, 220, 10), (x, 0), (x, height))
        for y in range(0, height, step):
            pg.draw.line(grid, (80, 200, 220, 8), (0, y), (width, y))
        surface.blit(grid, (0, 0))

        # Header / footer rules and the plot/control column divider.
        pg.draw.line(surface, (18, 47, 66), (0, int(height * 0.073)), (width, int(height * 0.073)))
        pg.draw.line(surface, (18, 47, 66), (0, int(height * 0.941)), (width, int(height * 0.941)))
        pg.draw.line(surface, (14, 38, 54),
                     (int(width * 0.566), int(height * 0.150)),
                     (int(width * 0.566), int(height * 0.930)))

        # Neon strip across the top.
        pg.draw.rect(surface, CYAN, (0, 0, int(width * 0.30), 2))
        pg.draw.rect(surface, PINK, (int(width * 0.30), 0, int(width * 0.06), 2))
        pg.draw.rect(surface, (0, 84, 104), (int(width * 0.36), 0, width, 2))

        self._background = surface

    def render(self, bgColor, debug=False):
        if self._background is None:
            self._buildBackground()

        output = self._background.copy()
        for element in self._uiElements.values():
            output.blit(element.render(WIDGET_BG), element.getPos())
        return output


# ---------------------------------------------------------------------------
# Persistence
# ---------------------------------------------------------------------------
def loadTuningValues():
    """Load saved tuning values as a {label: value} dict, ignoring invalid entries."""
    try:
        with TUNING_FILE.open("r", encoding="utf-8") as file:
            data = json.load(file)
    except FileNotFoundError:
        return {}
    except (OSError, json.JSONDecodeError) as error:
        print(f"Could not load tuning parameters from {TUNING_FILE}: {error}")
        return {}

    saved = data.get("tuning") if isinstance(data, dict) else None
    if not isinstance(saved, dict):
        print(f"Ignoring malformed tuning parameter file {TUNING_FILE}")
        return {}

    values = {}
    for _, params in TUNING_ROWS:
        for label, _, lower, upper, _ in params:
            value = saved.get(label)
            if isinstance(value, bool) or not isinstance(value, (int, float)):
                continue
            if not math.isfinite(value):
                continue
            values[label] = clamp(float(value), lower, upper)

    return values


def saveTuningValues(values):
    """Save tuning values to disk, replacing the previous file atomically."""
    tempFile = TUNING_FILE.with_name(TUNING_FILE.name + ".tmp")
    try:
        with tempFile.open("w", encoding="utf-8") as file:
            json.dump({"version": 1, "tuning": values}, file, indent=2, sort_keys=True)
            file.write("\n")
        tempFile.replace(TUNING_FILE)
    except OSError as error:
        print(f"Could not save tuning parameters to {TUNING_FILE}: {error}")


# ---------------------------------------------------------------------------
# UI helpers
# ---------------------------------------------------------------------------
def openWindow(controlManager, size):
    """Open a roomier initial window while keeping ControlManager in sync."""
    required = ("_ControlManager__screen", "_ControlManager__size",
                "_ControlManager__uiGroup")
    if not all(hasattr(controlManager, name) for name in required):
        return

    screen = pg.display.set_mode(size, pg.RESIZABLE)
    pg.display.set_caption(WINDOW_TITLE)

    setattr(controlManager, "_ControlManager__screen", screen)
    setattr(controlManager, "_ControlManager__size", size)
    getattr(controlManager, "_ControlManager__uiGroup").changeSize(size)


def chooseWindowSize():
    """Pick a comfortable window that still fits on the current display."""
    info = pg.display.Info()
    width = min(WINDOW_SIZE[0], max(1200, info.current_w - 80))
    height = min(WINDOW_SIZE[1], max(600, info.current_h - 120))
    return width, height


def styleRow(row, accents):
    """Apply the neon theme to one TuningRow."""
    for label in row._labels:
        accent = accents[label]
        textBox = row[f"text{label}"]
        valueBox = row[f"value{label}"]

        textBox.setColor("textBgColor", WIDGET_BG)
        textBox.setColor("textColor", (148, 184, 204))
        valueBox.setColor("textBgColor", WIDGET_BG)
        valueBox.setColor("textColor", accent)

        # Bolder text stays readable in the compact rows.
        textBox._getFont = lambda size: _font(FONT_UI, size, bold=True)
        valueBox._getFont = lambda size: _font(FONT_UI, size, bold=True)

        slider = _unwrap(row[f"slider{label}"])
        slider._bgColor = (10, 22, 36)
        slider._lineColor = (31, 64, 84)
        slider._slidyBoxColor = accent
        slider._mainRadius = 8


def createPlot(pos, size, title, yLabel, colors):
    plot = CleanPlot(pos, size)
    plot.setTitle(title)
    plot.setXLabel("t (s)")
    plot.setYLabel(yLabel)
    plot.setColor("bgColor", PLOT_BG)
    plot.setColor("textColor", MUTED)
    for name, color in colors.items():
        plot.setColor(name, color)
    return plot


def udpSampleKey(data):
    """Fallback identity for a UDP sample when the board sends no timestamp."""
    return (
        data.timestamp,
        data.position,
        data.velocity,
        data.acceleration,
        data.torque,
        data.phaseCurrentA,
        data.phaseCurrentB,
        data.phaseCurrentC,
        data.busCurrent,
        data.busVoltage,
        data.errorRegister,
        data.loopTimeFOC,
        data.loopTimeSecondary,
        data.debugFloat0,
        data.debugFloat1,
        data.debugFloat2,
        data.debugFloat3,
    )


# ---------------------------------------------------------------------------
# Main program
# ---------------------------------------------------------------------------
def main():
    print("AthenaDrive control deck starting...")

    try:
        eth = EthernetPython.Ethernet()
    except Exception as error:
        print(f"Could not start the Ethernet backend: {error}")
        return

    controlManager = pw.ControlManager()
    openWindow(controlManager, chooseWindowSize())

    t0 = perf_counter()

    # Ask the board for every telemetry field the dashboard uses.
    packet = eth.getTcpCommandPacket([
        ([EthernetPython.TCP_COMMAND_UDP_DATA_CONFIG], [UDP_DATA_CONFIG])
    ])
    eth.addTCPPacket(packet)

    packet = eth.getTcpCommandPacket([
        ([EthernetPython.TCP_COMMAND_DRIVING_MODE], [INITIAL_DRIVING_MODE])
    ])
    eth.addTCPPacket(packet)

    commandUDP = EthernetPython.CommandUDP()
    commandUDP.motorIx = 0
    commandUDP.torqueSetpoint = 0.0
    commandUDP.velocitySetpoint = 0.0
    commandUDP.positionSetpoint = 0.0
    eth.setUDPCommand([commandUDP])

    dashboard = Deck()

    # -----------------------------------------------------------------------
    # Header
    # -----------------------------------------------------------------------
    title = Label((0.012, 0.005), (0.170, 0.058), "ATHENADRIVE", CYAN,
                  bold=True)
    subtitle = Label((0.185, 0.011), (0.300, 0.044),
                     "// MOTOR CONTROL DECK", MUTED, fontName=FONT_UI,
                     bold=True)
    linkLabel = Label((0.660, 0.006), (0.328, 0.032), "● AWAITING BOARD", AMBER,
                      bold=True, align="right")
    clockLabel = Label((0.500, 0.041), (0.488, 0.023), "", DIM,
                       fontName=FONT_MONO, align="right")

    dashboard["title"] = title
    dashboard["subtitle"] = subtitle
    dashboard["linkLabel"] = linkLabel
    dashboard["clockLabel"] = clockLabel

    # -----------------------------------------------------------------------
    # KPI strip
    # -----------------------------------------------------------------------
    kpiStart = 0.012
    kpiPitch = 0.122
    kpiWidth = 0.116
    kpiY = 0.082
    kpiHeight = 0.058

    statPosition = StatCard((kpiStart + 0.0 * kpiPitch, kpiY), (kpiWidth, kpiHeight), "position", CYAN)
    statVelocity = StatCard((kpiStart + 1.0 * kpiPitch, kpiY), (kpiWidth, kpiHeight), "velocity", GREEN)
    statTorque = StatCard((kpiStart + 2.0 * kpiPitch, kpiY), (kpiWidth, kpiHeight), "torque", PINK)
    statPower = StatCard((kpiStart + 3.0 * kpiPitch, kpiY), (kpiWidth, kpiHeight), "bus power", AMBER)
    statBusVoltage = StatCard((kpiStart + 4.0 * kpiPitch, kpiY), (kpiWidth, kpiHeight), "bus voltage", PURPLE)
    statTemperature = StatCard((kpiStart + 5.0 * kpiPitch, kpiY), (kpiWidth, kpiHeight), "temperature", ORANGE)
    statBoard = StatCard((kpiStart + 6.0 * kpiPitch, kpiY), (kpiWidth, kpiHeight), "board state", SKY, sparkline=False)
    statError = StatCard((kpiStart + 7.0 * kpiPitch, kpiY), (kpiWidth, kpiHeight), "error register", GREEN, sparkline=False)

    dashboard["statPosition"] = statPosition
    dashboard["statVelocity"] = statVelocity
    dashboard["statTorque"] = statTorque
    dashboard["statPower"] = statPower
    dashboard["statBusVoltage"] = statBusVoltage
    dashboard["statTemperature"] = statTemperature
    dashboard["statBoard"] = statBoard
    dashboard["statError"] = statError

    # -----------------------------------------------------------------------
    # Plots (2 columns x 3 rows on the left)
    # -----------------------------------------------------------------------
    plotX = (0.012, 0.291)
    plotY = (0.150, 0.414, 0.678)
    plotWidth = 0.269
    plotHeight = 0.252

    plotPos = createPlot((plotX[0], plotY[0]), (plotWidth, plotHeight),
                         "POSITION", "rad", {"plot0": CYAN, "plot1": PINK})
    plotVel = createPlot((plotX[1], plotY[0]), (plotWidth, plotHeight),
                         "VELOCITY", "rad/s", {"plot0": GREEN, "plot1": SKY})
    plotAcc = createPlot((plotX[0], plotY[1]), (plotWidth, plotHeight),
                         "ACCEL", "rad/s²", {"plot0": AMBER, "plot1": ORANGE})
    plotFoc = createPlot((plotX[1], plotY[1]), (plotWidth, plotHeight),
                         "FOC LOOP", "µs", {"plot0": PURPLE, "plot1": SKY})
    plotPower = createPlot((plotX[0], plotY[2]), (plotWidth, plotHeight),
                           "BUS POWER", "W", {"plot0": ORANGE, "plot1": AMBER})
    plotCurrent = createPlot((plotX[1], plotY[2]), (plotWidth, plotHeight),
                             "PHASE CURRENTS", "A",
                             {"plot0": PINK, "plot1": CYAN, "plot2": GREEN,
                              "plot3": AMBER})

    dashboard["plotPos"] = plotPos
    dashboard["plotVel"] = plotVel
    dashboard["plotAcc"] = plotAcc
    dashboard["plotFoc"] = plotFoc
    dashboard["plotPower"] = plotPower
    dashboard["plotCurrent"] = plotCurrent

    # -----------------------------------------------------------------------
    # Right column: setpoints
    # -----------------------------------------------------------------------
    rightX = 0.575
    rightWidth = 0.413

    setpointTitle = SectionTitle((rightX, 0.150), (0.300, 0.020), "setpoints", CYAN)
    zeroButton = NeonButton((rightX + rightWidth - 0.075, 0.147), (0.075, 0.026))
    zeroButton.configure("ZERO", AMBER)

    setpointRow = TuningRow(
        (rightX, 0.172), (rightWidth, 0.090),
        labels=[item[0] for item in UDP_SETPOINTS],
        lower_bounds=[item[1] for item in UDP_SETPOINTS],
        upper_bounds=[item[2] for item in UDP_SETPOINTS],
        current_values=[item[3] for item in UDP_SETPOINTS],
        formatter=formatSetpoint)
    styleRow(setpointRow, {item[0]: item[4] for item in UDP_SETPOINTS})

    dashboard["setpointTitle"] = setpointTitle
    dashboard["zeroButton"] = zeroButton
    dashboard["setpointRow"] = setpointRow

    torqueSetpointLabel, velocitySetpointLabel, positionSetpointLabel = (
        item[0] for item in UDP_SETPOINTS
    )
    setpointSliders = {
        label: setpointRow[f"slider{label}"]
        for label, *_ in UDP_SETPOINTS
    }
    positionSlider = setpointSliders[positionSetpointLabel]

    # -----------------------------------------------------------------------
    # Right column: tuning
    # -----------------------------------------------------------------------
    tuningTitle = SectionTitle((rightX, 0.270), (rightWidth, 0.018), "PID & limits", PINK)
    dashboard["tuningTitle"] = tuningTitle

    tuningRows = {}
    tuningRowY = 0.292
    tuningRowHeight = 0.089
    tuningRowPitch = 0.094

    for index, (rowName, params) in enumerate(TUNING_ROWS):
        row = TuningRow(
            (rightX, tuningRowY + index * tuningRowPitch),
            (rightWidth, tuningRowHeight),
            labels=[param[0] for param in params],
            lower_bounds=[param[2] for param in params],
            upper_bounds=[param[3] for param in params],
            current_values=[param[4] for param in params])
        styleRow(row, {param[0]: GROUP_ACCENTS[rowName] for param in params})
        tuningRows[rowName] = (row, params)
        dashboard[f"tuning{rowName}"] = row

    # -----------------------------------------------------------------------
    # Right column: actions
    # -----------------------------------------------------------------------
    buttonY = 0.674
    buttonHeight = 0.038
    buttonGap = 0.005
    buttonWidth = (rightWidth - 2 * buttonGap) / 3

    applyButton = NeonButton((rightX, buttonY), (buttonWidth, buttonHeight))
    applyButton.configure("APPLY CHANGES", CYAN)

    sendAllButton = NeonButton(
        (rightX + buttonWidth + buttonGap, buttonY), (buttonWidth, buttonHeight))
    sendAllButton.configure("SEND ALL", GREEN)

    clearButton = NeonButton(
        (rightX + 2 * (buttonWidth + buttonGap), buttonY), (buttonWidth, buttonHeight))
    clearButton.configure("CLEAR ERRORS", RED)

    modeLabel = Label((rightX, 0.717), (0.100, 0.034), "DRIVING MODE", MUTED,
                      bold=True)
    modeInput = pw.TextBox((rightX + 0.105, 0.717), (0.095, 0.034))
    modeInput.setText(f"{INITIAL_DRIVING_MODE:#b}")
    modeInput.setColor("textBgColor", (10, 24, 38))
    modeInput.setColor("textColor", CYAN)
    modeInput.setAlignment(horizontal=pw.TextBox.AlignmentHorizontal.MIDDLE)

    modeButton = NeonButton((rightX + 0.203, 0.717), (0.210, 0.034))
    modeButton.configure("SET MODE", CYAN)

    commandStatusLabel = Label((rightX, 0.755), (rightWidth, 0.026),
                               "READY — no tuning changes sent yet", MUTED,
                               fontName=FONT_MONO)

    dashboard["applyButton"] = applyButton
    dashboard["sendAllButton"] = sendAllButton
    dashboard["clearButton"] = clearButton
    dashboard["modeLabel"] = modeLabel
    dashboard["modeInput"] = modeInput
    dashboard["modeButton"] = modeButton
    dashboard["commandStatusLabel"] = commandStatusLabel

    # -----------------------------------------------------------------------
    # Right column: diagnostic lines
    # -----------------------------------------------------------------------
    systemTitle = SectionTitle((rightX, 0.793), (rightWidth, 0.018),
                               "link & debug", SKY)
    systemLine1 = Label((rightX, 0.816), (rightWidth, 0.026), "", TEXT,
                        fontName=FONT_MONO)
    systemLine2 = Label((rightX, 0.845), (rightWidth, 0.026), "", MUTED,
                        fontName=FONT_MONO)
    systemLine3 = Label((rightX, 0.874), (rightWidth, 0.026), "", DIM,
                        fontName=FONT_MONO)
    systemLine4 = Label((rightX, 0.903), (rightWidth, 0.026), "", DIM,
                        fontName=FONT_MONO)

    dashboard["systemTitle"] = systemTitle
    dashboard["systemLine1"] = systemLine1
    dashboard["systemLine2"] = systemLine2
    dashboard["systemLine3"] = systemLine3
    dashboard["systemLine4"] = systemLine4

    # -----------------------------------------------------------------------
    # Footer
    # -----------------------------------------------------------------------
    footerLeft = Label((0.012, 0.945), (0.690, 0.026),
                       "CYAN = MEASURED  •  PINK = COMMAND  •  DRAG OR TYPE VALUES",
                       DIM, fontName=FONT_MONO)
    footerRight = Label((0.710, 0.945), (0.278, 0.026),
                        "ESC TO EXIT  •  AUTOSAVE ON", DIM, fontName=FONT_MONO,
                        align="right")

    dashboard["footerLeft"] = footerLeft
    dashboard["footerRight"] = footerRight

    controlManager["dashboard"] = dashboard

    # -----------------------------------------------------------------------
    # Runtime state
    # -----------------------------------------------------------------------
    lastMeasuredPosition = None
    lastVelocity = None
    lastAcceleration = None
    lastTorque = None
    lastLoopFOC = None
    lastLoopSecondary = None
    lastBusVoltage = None
    lastBusCurrent = None
    lastBusPower = None
    lastPhaseA = None
    lastPhaseB = None
    lastPhaseC = None
    lastDebugFloats = [None, None, None, None]

    lastBoardState = None
    lastDrivingMode = INITIAL_DRIVING_MODE
    lastErrorCode = None
    lastTemperature = None

    lastSampleKey = None
    lastTimestamp = None
    lastUdpSeen = False
    lastUdpAt = 0.0

    # -----------------------------------------------------------------------
    # Command helpers
    # -----------------------------------------------------------------------
    def setCommandStatus(text, color=MUTED):
        commandStatusLabel.setText(text)
        commandStatusLabel.setTextColor(color)

    def setRowValue(row, label, value):
        row[f"slider{label}"].setValue(value)
        row[f"value{label}"].setText(formatSetpoint(value))

    def sendTuning(onlyChanged, _=None, onlyLabels=None):
        headerBits = []
        values = []
        newValues = {}

        for row, params in tuningRows.values():
            for label, bit, lower, upper, _ in params:
                if (onlyLabels is not None) and (label not in onlyLabels):
                    continue

                value = float(row[f"slider{label}"].getValue())
                if not math.isfinite(value):
                    value = 0.0
                value = clamp(value, lower, upper)
                row[f"slider{label}"].setValue(value)

                if onlyChanged and value == lastAppliedTuning[bit]:
                    continue

                headerBits.append(bit)
                values.append(_floatToBits(value))
                newValues[bit] = value

        if not headerBits:
            setCommandStatus("No tuning changes to send", AMBER)
            return

        try:
            packet = eth.getTcpCommandPacket([(headerBits, values)])
            eth.addTCPPacket(packet)
        except Exception as error:
            setCommandStatus(f"Tuning send failed: {error}", RED)
            return

        lastAppliedTuning.update(newValues)
        if onlyChanged:
            setCommandStatus(f"Sent {len(values)} changed tuning values", GREEN)
        else:
            setCommandStatus(f"Sent all {len(values)} tuning values", GREEN)

    def sendDrivingMode(_=None):
        try:
            mode = int(modeInput.getText(), 0)
        except ValueError:
            setCommandStatus("Invalid driving mode value", RED)
            return

        if not 0 <= mode <= 0xFFFFFFFF:
            setCommandStatus("Driving mode out of range", RED)
            return

        try:
            packet = eth.getTcpCommandPacket([
                ([EthernetPython.TCP_COMMAND_DRIVING_MODE], [mode])
            ])
            eth.addTCPPacket(packet)
        except Exception as error:
            setCommandStatus(f"Mode send failed: {error}", RED)
            return

        modeInput.setText(f"{mode:#b}")
        setCommandStatus(f"Sent driving mode {mode} ({mode:#b})", GREEN)

    def clearErrors(_=None):
        if lastErrorCode is None:
            setCommandStatus("No error register received yet", AMBER)
            return

        try:
            packet = eth.getTcpCommandPacket([
                ([EthernetPython.TCP_COMMAND_CLEAR_ERROR_REGISTER], [lastErrorCode])
            ])
            eth.addTCPPacket(packet)
        except Exception as error:
            setCommandStatus(f"Clear request failed: {error}", RED)
            return

        setCommandStatus(f"Cleared error code 0x{lastErrorCode:04X}", GREEN)

    def resetSetpoints(_=None):
        for label, *_ in UDP_SETPOINTS:
            setRowValue(setpointRow, label, 0.0)

        setCommandStatus("All setpoints zeroed", GREEN)

    # -----------------------------------------------------------------------
    # Wiring, saved tuning and initial state
    # -----------------------------------------------------------------------
    applyButton.setFunction(lambda _: sendTuning(onlyChanged=True))
    sendAllButton.setFunction(lambda _: sendTuning(onlyChanged=False))
    clearButton.setFunction(clearErrors)
    modeButton.setFunction(sendDrivingMode)
    zeroButton.setFunction(resetSetpoints)

    lastAppliedTuning = {}
    for row, params in tuningRows.values():
        for label, bit, *_ in params:
            lastAppliedTuning[bit] = row[f"slider{label}"].getValue()

    loadedTuning = loadTuningValues()
    for row, params in tuningRows.values():
        for label, _, lower, upper, _ in params:
            if label not in loadedTuning:
                continue
            value = clamp(loadedTuning[label], lower, upper)
            row[f"slider{label}"].setValue(value)
            row[f"value{label}"].setText(formatGain(value))

    if loadedTuning:
        sendTuning(onlyChanged=False, onlyLabels=loadedTuning)
        setCommandStatus(f"Loaded {len(loadedTuning)} saved tuning values", GREEN)
    else:
        setCommandStatus("READY — adjust tuning, then APPLY CHANGES", MUTED)

    # -----------------------------------------------------------------------
    # Main loop
    # -----------------------------------------------------------------------
    prevTCPAsk = 0.0
    prevKpiUpdate = -1.0
    prevTitleUpdate = -1.0
    prevSystemUpdate = -1.0
    prevClockUpdate = -1.0
    loopCount = 0
    loopRateStart = 0.0
    loopHz = 0.0
    udpSamples = 0
    udpRate = 0.0
    udpRateStart = 0.0

    try:
        while controlManager.isRunning():
            t = perf_counter() - t0

            newSample = False

            # --- UDP telemetry ------------------------------------------------
            dataUDPList = eth.getUDPData()
            if dataUDPList:
                data = dataUDPList[0]
                sampleKey = udpSampleKey(data)
                timestamp = data.timestamp
                timestampKey = timestamp if timestamp not in (None, 0) else None

                if timestampKey is not None:
                    newSample = timestampKey != lastTimestamp
                    if newSample:
                        lastTimestamp = timestampKey
                    lastSampleKey = sampleKey
                else:
                    newSample = sampleKey != lastSampleKey
                    if newSample:
                        lastSampleKey = sampleKey

                if newSample:
                    lastUdpSeen = True
                    lastUdpAt = t
                    udpSamples += 1

                    if isFinite(data.position):
                        lastMeasuredPosition = data.position
                        plotPos.addValue(t, data.position, plotIx=0,
                                         maxLength=MAX_PLOT_POINTS)

                    if isFinite(data.velocity):
                        lastVelocity = data.velocity
                        plotVel.addValue(t, data.velocity, plotIx=0,
                                         maxLength=MAX_PLOT_POINTS)

                    if isFinite(data.acceleration):
                        lastAcceleration = data.acceleration
                        plotAcc.addValue(t, data.acceleration, plotIx=0,
                                         maxLength=MAX_PLOT_POINTS)

                    if isFinite(data.torque):
                        lastTorque = data.torque

                    if isFinite(data.loopTimeFOC):
                        lastLoopFOC = data.loopTimeFOC
                        plotFoc.addValue(t, data.loopTimeFOC, plotIx=0,
                                         maxLength=MAX_PLOT_POINTS)

                    if isFinite(data.loopTimeSecondary):
                        lastLoopSecondary = data.loopTimeSecondary

                    if isFinite(data.busVoltage):
                        lastBusVoltage = data.busVoltage
                    if isFinite(data.busCurrent):
                        lastBusCurrent = data.busCurrent
                    if isFinite(lastBusVoltage) and isFinite(lastBusCurrent):
                        lastBusPower = lastBusVoltage * lastBusCurrent
                        plotPower.addValue(t, lastBusPower, plotIx=0,
                                           maxLength=MAX_PLOT_POINTS)

                    if isFinite(data.phaseCurrentA):
                        lastPhaseA = data.phaseCurrentA
                        plotCurrent.addValue(t, data.phaseCurrentA, plotIx=0,
                                             maxLength=MAX_PLOT_POINTS)
                    if isFinite(data.phaseCurrentB):
                        lastPhaseB = data.phaseCurrentB
                        plotCurrent.addValue(t, data.phaseCurrentB, plotIx=1,
                                             maxLength=MAX_PLOT_POINTS)
                    if isFinite(data.phaseCurrentC):
                        lastPhaseC = data.phaseCurrentC
                        plotCurrent.addValue(t, data.phaseCurrentC, plotIx=2,
                                             maxLength=MAX_PLOT_POINTS)

                    if data.errorRegister is not None:
                        lastErrorCode = data.errorRegister

                    for index, debugValue in enumerate((
                            data.debugFloat0, data.debugFloat1,
                            data.debugFloat2, data.debugFloat3)):
                        if isFinite(debugValue):
                            lastDebugFloats[index] = debugValue

            # --- Commands -----------------------------------------------------
            torqueCommand = clamp(setpointSliders[torqueSetpointLabel].getValue(),
                                  -MAXIMA_VAL_SETPOINTS, MAXIMA_VAL_SETPOINTS)
            velocityCommand = clamp(setpointSliders[velocitySetpointLabel].getValue(),
                                    -MAXIMA_VAL_SETPOINTS, MAXIMA_VAL_SETPOINTS)
            positionCommand = clamp(positionSlider.getValue(),
                                    -MAXIMA_VAL_SETPOINTS, MAXIMA_VAL_SETPOINTS)

            if newSample:
                plotPos.addValue(t, positionCommand, plotIx=1,
                                 maxLength=MAX_PLOT_POINTS)

            commandUDP.torqueSetpoint = torqueCommand
            commandUDP.velocitySetpoint = velocityCommand
            commandUDP.positionSetpoint = positionCommand
            eth.setUDPCommand([commandUDP])

            # --- TCP status poll ---------------------------------------------
            if (t - prevTCPAsk) > 0.1:
                prevTCPAsk = t
                if eth.packetsInQueueTCP() < 8:
                    try:
                        packet = eth.getTcpDataPacket([[
                            EthernetPython.TCP_DATA_BUTTON_STATUS,
                            EthernetPython.TCP_DATA_BOARD_STATE,
                            EthernetPython.TCP_DATA_DRIVING_MODE,
                            EthernetPython.TCP_DATA_ERROR_REGISTER,
                            EthernetPython.TCP_DATA_CURRENT_LIMIT_BUS,
                            EthernetPython.TCP_DATA_CURRENT_LIMIT_PHASE,
                            EthernetPython.TCP_DATA_TEMPERATURE,
                        ]])
                        eth.addTCPPacket(packet)
                    except Exception:
                        pass

            for packet in eth.getTCPPacket():
                if packet.boardState is not None:
                    lastBoardState = packet.boardState
                if packet.drivingMode is not None:
                    lastDrivingMode = packet.drivingMode
                if packet.checkErrorCode is not None:
                    lastErrorCode = packet.checkErrorCode
                if packet.temperature is not None and math.isfinite(packet.temperature):
                    lastTemperature = packet.temperature

            # --- Loop rate / link status -------------------------------------
            loopCount += 1
            if (t - loopRateStart) >= 0.5:
                loopHz = loopCount / max(1e-6, t - loopRateStart)
                loopCount = 0
                loopRateStart = t

            age = t - lastUdpAt
            if not lastUdpSeen:
                if age < 1.0:
                    linkWord, linkColor = "AWAITING", AMBER
                else:
                    linkWord, linkColor = "NO DATA", RED
            elif age < 0.25:
                linkWord, linkColor = "ONLINE", GREEN
            elif age < 1.0:
                linkWord, linkColor = "STALE", AMBER
            else:
                linkWord, linkColor = "OFFLINE", RED

            linkLabel.setText(f"● LINK {linkWord}")
            linkLabel.setTextColor(linkColor)

            if (t - prevClockUpdate) >= 0.25:
                prevClockUpdate = t
                if (t - udpRateStart) >= 0.5:
                    udpRate = udpSamples / max(1e-6, t - udpRateStart)
                    udpSamples = 0
                    udpRateStart = t

                clockLabel.setText(
                    f"SESSION {formatDuration(t)}   •   {loopHz:.0f} FPS   •   {udpRate:.0f} SAMPLES/s")

            # --- KPI cards ----------------------------------------------------
            if (t - prevKpiUpdate) >= 0.05:
                prevKpiUpdate = t

                if lastTemperature is None:
                    tempAccent = ORANGE
                elif lastTemperature >= 70.0:
                    tempAccent = RED
                elif lastTemperature >= 55.0:
                    tempAccent = AMBER
                else:
                    tempAccent = ORANGE

                statPosition.feed(lastMeasuredPosition,
                                  formatValue(lastMeasuredPosition, " rad", 2))
                statVelocity.feed(lastVelocity,
                                  formatValue(lastVelocity, " rad/s", 2))
                statTorque.feed(lastTorque,
                                formatValue(lastTorque, " Nm", 2))
                statPower.feed(lastBusPower,
                               formatValue(lastBusPower, " W", 1))
                statBusVoltage.feed(lastBusVoltage,
                                    formatValue(lastBusVoltage, " V", 2))
                statTemperature.feed(lastTemperature,
                                     formatValue(lastTemperature, " °C", 1),
                                     tempAccent)

                statBoard.setValue("—" if lastBoardState is None else str(lastBoardState))
                if lastErrorCode is None:
                    statError.setValue("—", MUTED)
                elif lastErrorCode == 0:
                    statError.setValue("CLEAR", GREEN)
                else:
                    statError.setValue(f"0x{lastErrorCode:04X}", RED)

            # --- Plot titles show the current value ---------------------------
            if (t - prevTitleUpdate) >= 0.15:
                prevTitleUpdate = t

                plotPos.setTitle(f"POSITION  {formatValue(lastMeasuredPosition, ' rad', 3)}")
                plotVel.setTitle(f"VELOCITY  {formatValue(lastVelocity, ' rad/s', 3)}")
                plotAcc.setTitle(f"ACCEL  {formatValue(lastAcceleration, ' rad/s²', 2)}")
                plotFoc.setTitle(f"FOC LOOP  {formatValue(lastLoopFOC, ' µs', 0)}")
                plotPower.setTitle(f"BUS POWER  {formatValue(lastBusPower, ' W', 1)}")
                plotCurrent.setTitle(
                    f"PHASE  {formatValue(lastPhaseA, '', 1)}/"
                    f"{formatValue(lastPhaseB, '', 1)}/"
                    f"{formatValue(lastPhaseC, '', 1)}")

            # --- Diagnostic lines ---------------------------------------------
            if (t - prevSystemUpdate) >= 0.25:
                prevSystemUpdate = t

                boardText = "—" if lastBoardState is None else str(lastBoardState)
                modeText = "—" if lastDrivingMode is None else f"{lastDrivingMode:#b}"
                if lastErrorCode is None:
                    errorText = "—"
                    line1Color = MUTED
                elif lastErrorCode == 0:
                    errorText = "CLEAR"
                    line1Color = MUTED
                else:
                    errorText = f"0x{lastErrorCode:04X}"
                    line1Color = RED

                systemLine1.setText(
                    f"BOARD {boardText}  •  MODE {modeText}  •  ERROR {errorText}")
                systemLine1.setTextColor(line1Color)

                systemLine2.setText(
                    f"LINK {linkWord}  •  RX {udpRate:.0f}/s  •  "
                    f"AGE {age * 1000:.0f} ms  •  TCP Q {eth.packetsInQueueTCP()}")
                systemLine2.setTextColor(linkColor if linkWord in ("ONLINE", "STALE") else MUTED)

                systemLine3.setText(
                    f"DEBUG  f0 {formatCompact(lastDebugFloats[0])}  "
                    f"f1 {formatCompact(lastDebugFloats[1])}  "
                    f"f2 {formatCompact(lastDebugFloats[2])}  "
                    f"f3 {formatCompact(lastDebugFloats[3])}")

                focText = "—" if lastLoopFOC is None else str(lastLoopFOC)
                secondaryText = "—" if lastLoopSecondary is None else str(lastLoopSecondary)
                systemLine4.setText(
                    f"FOC {focText} µs  •  SEC {secondaryText} µs  •  "
                    f"UI {loopHz:.0f} FPS  •  UP {formatDuration(t)}")

            controlManager.update()

    finally:
        # Persist the tuning table so it survives between runs.
        tuningValues = {}
        for row, params in tuningRows.values():
            for label, _, lower, upper, _ in params:
                value = float(row[f"slider{label}"].getValue())
                if not math.isfinite(value):
                    value = 0.0
                tuningValues[label] = clamp(value, lower, upper)

        saveTuningValues(tuningValues)
        controlManager.close()
        print("Dashboard closed. Tuning saved.")


if __name__ == "__main__":
    main()
