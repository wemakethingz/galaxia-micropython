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

""" Thingz Display Raw Img
"""

class Img:
    def __init__(self, x: int, y: int, path: str, white_replacement: int) -> None:
        """
        Create an image and print it to the screen

        :param int x: X position
        :param int y: Y position
        :param str path: Path to the BMP file
        :param int white_replacement: Color used to replace white pixels
        """
        ...

    def show(self, show: bool) -> None:
        """
        Show/Hide the image

        :param bool show: Show the image if True hide if False
        """
        ...

    def x(self, pos: int) -> None:
        """
        Set x position

        :param int pos: x position
        """
        ...

    def y(self, pos: int) -> None:
        """
        Set y position

        :param int pos: y position
        """
        ...

    def white_replacement_color(self, color: bool) -> None:
        """
        Change all white pixels to another color

        :param int color: color to use as replacement
        """
        ...

    def get_show(self) -> bool:
        """
        Return True if the image is shown

        """
        ...

    def get_x(self) -> int:
        """
        Get x position
        """
        ...

    def get_y(self) -> int:
        """
        Get y position
        """
        ...

    def get_width(self) -> int:
        """
        Get image width
        """
        ...

    def get_height(self) -> int:
        """
        Get image height
        """
        ...

    def get_white_replacement(self) -> int:
        """
        Get color used as white replacement
        """
        ...

""" Thingz Display Raw Rect
"""

class Rect:
    def __init__(self, x: int, y: int, width: int, height: int, color: int) -> None:
        """
        Create a rectangle and print it to the screen

        :param int x: X position
        :param int y: Y position
        :param int width: width
        :param int height: height
        :param int color: Color
        """
        ...

    def show(self, show: bool) -> None:
        """
        Show/Hide the rectangle

        :param bool show: Show the rectangle if True hide if False
        """
        ...

    def x(self, pos: int) -> None:
        """
        Set x position

        :param int pos: x position
        """
        ...

    def y(self, pos: int) -> None:
        """
        Set y position

        :param int pos: y position
        """
        ...

    def color(self, color: int) -> None:
        """
        Set rectangle color

        :param int color: color
        """
        ...

    def get_show(self) -> bool:
        """
        Return True if the rectangle is shown

        """
        ...

    def get_x(self) -> int:
        """
        Get x position
        """
        ...

    def get_y(self) -> int:
        """
        Get y position
        """
        ...

    def get_width(self) -> int:
        """
        Get image width
        """
        ...

    def get_height(self) -> int:
        """
        Get image height
        """
        ...

    def get_color(self) -> int:
        """
        Get color
        """
        ...

""" Thingz Display Raw Text
"""

class Text:
    def __init__(self, x: int, y: int, text: str, color: int) -> None:
        """
        Create an image and print it to the screen

        :param int x: X position
        :param int y: Y position
        :param str text: text
        :param int color: color
        """

    def show(self, show: bool) -> None:
        """
        Show/Hide the text

        :param bool show: Show the text if True hide if False
        """
        ...

    def x(self, pos: int) -> None:
        """
        Set x position

        :param int pos: x position
        """
        ...

    def y(self, pos: int) -> None:
        """
        Set y position

        :param int pos: y position
        """
        ...

    def set_text(self, text: str) -> None:
        """
        Set text

        :param str text: text
        """
        ...

    def get_show(self) -> bool:
        """
        Return True is text is shown else False
        """
        ...

    def get_x(self) -> int:
        """
        Get x position
        """
        ...

    def get_y(self) -> int:
        """
        Get y position
        """
        ...

    def get_width(self) -> int:
        """
        Get text width
        """
        ...

    def get_height(self) -> int:
        """
        Get text height
        """
        ...

    def get_color(self) -> int:
        """
        Get text color
        """
        ...

    def get_text(self) -> int:
        """
        Get text
        """
        ...
