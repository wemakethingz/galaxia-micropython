"""Thingz Display Raw Img"""

from __future__ import annotations

class Img(x, y, path, white_replacement):
    """
    Create an image and print it to the screen
    """

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
