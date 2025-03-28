"""Thingz button touch"""

from __future__ import annotations

from typing import Callable, Optional

class ButtonTouch:
    """Control Galaxia's touch buttons"""

    def is_touched(self) -> bool:
        """
        :return: True if button is touched, False otherwise
        :rtype: bool
        """
        ...

    def was_touched(self) -> bool:
        """
        :return: True if button has been touched since last call, False otherwise
        :rtype: bool
        """
        ...

    def get_touches(self) -> int:
        """
        Get the number of touches since the last call

        :return: The number of touches since the last call
        :rtype: int
        """
        ...

    def on_touched(self, callback: Callable[Optional[Button]]) -> None:
        """Register a callaback bind to touch event

        :param Callable[Optional[ButtonTouch]] callback: The function to call when the event occurs. When called the button will be passed as paramater
        """
        ...
