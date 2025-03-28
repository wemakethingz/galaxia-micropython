"""Thingz button"""

from __future__ import annotations

from typing import Callable, Optional

class Button:
    """Control Galaxia's physical buttons"""

    def is_pressed(self) -> bool:
        """
        :return: True if button is pressed, False otherwise
        :rtype: bool
        """
        ...

    def was_pressed(self) -> bool:
        """
        :return: True if button has been pressed since last call, False otherwise
        :rtype: bool
        """
        ...

    def get_presses(self) -> int:
        """
        Get the number of presses since the last call

        :return: The number of presses since the last call
        :rtype: int
        """
        ...

    def on_pressed(self, callback: Callable[Optional[Button]]) -> None:
        """Register a callaback bind to press event

        :param Callable[Optional[Button]] callback: The function to call when the event occurs. When called the button will be passed as paramater
        """
        ...
