"""Thingz Display"""

from __future__ import annotations

from typing import Callable

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

""" Thingz Display Console
"""

class Console:
    """
    Show the REPL output on the screen
    """

    def show(self) -> None:
        """
        Show the REPL
        """
        ...

""" Thingz Display Plot
"""

class Plot:
    """
    Use the LCD as a plot
    """

    def show(self) -> None:
        """
        Show the plot
        """
        ...

    def add_point(self, value: int | float) -> None:
        """
        Add a new point to the plot.

        :param int|float value: The position on the Y axis of new point
        """
        ...

    def set_y_scale(self, min: int, max: int) -> None:
        """
        Set the scale of the plot

        :param int min: The min value of the Y axis
        :param int max: The max value of the Y axis
        """
        ...

    def set_animate_function(self, func: Callable, interval: int) -> None:
        """
        Configure a function that will be called once every interval to add a point to the plot. The function must return the value of the new point

        :param Callable func: The function to call
        :param int interval: Time to wait between each function call, in seconds
        """
        ...

""" Thingz Display Raw
"""

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

    def print(self, x, y, txt) -> None:
        """
        Print text at a given position

        :param int x: X position
        :param int y: Y position
        :param str txt: The text to print
        """
        ...

    def print_bmp(self, x, y, path) -> None:
        """
        Print BMP file at a given position

        :param int x: X position
        :param int y: Y position
        :param str path: The path to the BMP file
        """
        ...

""" Thingz Display Raw Img
"""

class Img(x, y, path, white_replacement):
    """
    Create an image and print it to the screen
    :param int x: X position
    :param int y: Y position
    :param str path: Path to the BMP file
    :param int white_replacement: Color used to replace white pixels
    """

    def show(self, show) -> None:
        """
        Show/Hide the image

        :param bool show: Show the image if True hide if False
        """
        ...

    def x(self, pos) -> None:
        """
        Set x position

        :param int pos: x position
        """
        ...

    def y(self, pos) -> None:
        """
        Set y position

        :param int pos: y position
        """
        ...

    def white_replacement_color(self, color) -> None:
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

class Rect(x, y, width, height, color):
    """
    Create an rectangle and print it to the screen
    :param int x: X position
    :param int y: Y position
    :param int width: Width of the rectangle
    :param int height: Height of the rectangle
    """

    def show(self, show) -> None:
        """
        Show/Hide the rectangle

        :param bool show: Show the rectangle if True hide if False
        """
        ...

    def x(self, pos) -> None:
        """
        Set x position

        :param int pos: x position
        """
        ...

    def y(self, pos) -> None:
        """
        Set y position

        :param int pos: y position
        """
        ...

    def color(self, color) -> None:
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
