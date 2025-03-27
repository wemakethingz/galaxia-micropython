"""Thingz module

Used to access Galaxia's internal component


"""

from __future__ import annotations

button_a: Button
"""
Galaxia's button A
This object is an instance of `Button`
"""

button_b: Button
"""
Galaxia's button B
This object is an instance of `Button`
"""

touch_n: ButtonTouch
"""
Galaxia's touch button North
This object is an instance of `ButtonTouch`
"""

touch_s: ButtonTouch
"""
Galaxia's touch button South
This object is an instance of `ButtonTouch`
"""

touch_e: ButtonTouch
"""
Galaxia's touch button Est
This object is an instance of `ButtonTouch`
"""

touch_w: ButtonTouch
"""
Galaxia's touch button West
This object is an instance of `ButtonTouch`
"""

led: Led
"""
Galaxia's RGB LED
This object is an instance of `Led`
"""

accelerometer: Accel
"""
Galaxia's accelerometer
This object is an instance of `Accel`
"""

compass: Compass
"""
Galaxia's magnetometer
This object is an instance of `Compass`
"""

sound: Sound
"""
Galaxia's jack connector
This object is an instance of `Sound`
"""

radio: Radio
"""
Galaxia's wireless communication
This object is an instance of `Radio`
"""

def temperature() -> int:
    """
    :return: the current temperature by reading internal sensor.
    :rtype: int
    """
    ...

def set_temperature_offset(offset: int) -> None:
    """Calibrate internal temperature sensor by applying an offset"""
    ...
