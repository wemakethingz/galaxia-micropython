"""Thingz LED"""

from __future__ import annotations

class Led:
    """Control Galaxia's RGB LED"""

    def set_colors(self, red: int, green: int, blue: int) -> None:
        """Set red, green and blue values

        :param int red: The red value between 0 and 255
        :param int green: The green value between 0 and 255
        :param int blue: The blue value between 0 and 255"""
        ...

    def set_red(self, red: int) -> None:
        """Set red value

        :param int red: The red value between 0 and 255"""
        ...

    def set_green(self, green: int) -> None:
        """Set green value

        :param int green: The green value between 0 and 255"""
        ...

    def set_blue(self, blue: int) -> None:
        """Set blue value

        :param int blue: The blue value between 0 and 255"""
        ...

    def get_red(self) -> int:
        """
        Get red value

        :return: The red value between 0 and 255
        :rtype: int
        """
        ...

    def get_green(self) -> int:
        """
        Get green value

        :return: The green value between 0 and 255
        :rtype: int
        """
        ...

    def get_blue(self) -> int:
        """
        Get blue value

        :return: The blue value between 0 and 255
        :rtype: int
        """
        ...

    def read_light_level(self) -> int:
        """
        Get the current light level

        :return: The light level between 0 (dark) and 100 (luminous)
        :rtype: int
        """
        ...
