"""Thingz accelerometer"""

from __future__ import annotations

from typing import Callable, Optional

class Accel:
    """Control Galaxia's accelerometer"""

    def get_x(self) -> float:
        """
        :return: the acceleration value of x axis in mG
        :rtype: float
        """
        ...

    def get_y(self) -> float:
        """
        :return: the acceleration value of y axis in mG
        :rtype: float
        """
        ...

    def get_z(self) -> float:
        """
        :return: the acceleration value of z axis in mG
        :rtype: float
        """
        ...

    def get_values(self) -> list:
        """
        :return: the acceleration values of the 3 axis in a list. Index 0 is X, 1 is Y, 2 is Z
        :rtype: list
        """
        ...

    def current_gesture(self) -> str:
        """
        Get the current gesture. Can be on of the following value:

        * up

        * down

        * left

        * right

        * face up

        * face down

        * freefall

        * 3g

        * 6g

        * 8g

        * shake

        * none

        :return: the current gesture
        :rtype: str
        """
        ...

    def is_gesture(self, gesture: str) -> bool:
        """
        :param str gesture: the gesture to test
        :return: True if the current gesture is the gesture receive as parameter
        :rtype: bool
        """
        ...

    def was_gesture(self, gesture: str) -> bool:
        """
        :param str gesture: the gesture to test
        :return: True if the gesture has been active since the last call to this function
        :rtype: bool
        """
        ...

    def get_gestures(self) -> list:
        """
        :return: The history of gesture. The most recent is listed last
        :rtype: list
        """
        ...

    def on_gesture(self, gesture: str, callback: Callable[Optional[str]]) -> None:
        """Register a callaback bind to press event

        :param str gesture: The gesture to bind the callback to
        :param Callable[Optional[str]] callback: The function to call when the event occurs. When called the gesture will be passed as paramater
        """
        ...

""" Thingz Magnetometer
"""

class Compass:
    """Control Galaxia's magnetometer"""

    def get_x(self) -> float:
        """
        :return: the magnetic field value of x axis in uT
        :rtype: float
        """
        ...

    def get_y(self) -> float:
        """
        :return: the magnetic field value of y axis in uT
        :rtype: float
        """
        ...

    def get_x(self) -> float:
        """
        :return: the magnetic field value of z axis in uT
        :rtype: float
        """
        ...

    def get_values(self) -> list:
        """
        :return: the magnetic field values of the 3 axis in a list. Index 0 is X, 1 is Y, 2 is Z
        :rtype: list
        """
        ...

    def heading(self) -> float:
        """
        :return: the current heading
        :rtype: float
        """
        ...

    def calibrate(self, calibration_time: int, nb_of_samples: int) -> None:
        """
        Calibrate the magnetometer. During the calibration the board has to be rotated along the Z axis

        :param int calibration_time: Time to spend on the calibration, value is in seconds
        :param int nb_of_samples: Number of sample to take
        """
        ...
