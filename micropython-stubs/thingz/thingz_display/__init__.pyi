"""Thingz Display"""

from __future__ import annotations

class Display:
    """Control Galaxia LCD display"""

    plot: Plot
    """
   Plot data on the LCD
   This object is an instance of `Plot`
   """

    console: Console
    """
   Show REPL output on the LCD
   This object is an instance of `Console`
   """

    raw: Raw
    """
   Display graphical elements
   This object is an instance of `Raw`
   """
