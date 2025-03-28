"""Thingz Display Raw Rect"""

from __future__ import annotations

class Rect:
    """
    Create an rectangle and print it to the screen
    """

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
