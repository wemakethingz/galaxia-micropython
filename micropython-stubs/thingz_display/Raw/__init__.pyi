"""Thingz Display Raw"""

from __future__ import annotations

class Raw:
    """
    Use the LCD to display graphical elements
    """

    Img: Img
    """
   Class `Img` to create an image
   """

    Rect: Rect
    """
   Class `Rect` to create a rectangle
   """

    Text: Text
    """
   Class `Text` to create a text
   """
    def show(self) -> None:
        """
        Show the raw interface
        """
        ...

    def print(self, x: int, y: int, txt: str) -> None:
        """
        Print text at a given position

        :param int x: X position
        :param int y: Y position
        :param str txt: The text to print
        """
        ...

    def print_bmp(self, x: int, y: int, path: str) -> None:
        """
        Print BMP file at a given position

        :param int x: X position
        :param int y: Y position
        :param str path: The path to the BMP file
        """
        ...
