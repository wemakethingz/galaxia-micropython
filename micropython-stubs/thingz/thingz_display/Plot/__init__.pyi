"""Thingz Display Plot"""

from __future__ import annotations

from typing import Callable

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
