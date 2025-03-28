"""Thingz Log"""

from __future__ import annotations

class Log:
    """Save data inside Galaxia internal memory. The data is saved as CSV, at each boot the Galaxia transfer the internal file to USB mass storage as data.csv"""

    def add(self, data: list) -> None:
        """Add data to log. The data must be in a list, each element is a tuple composed of the name of the column and the data associeted.
        OS EFBIG exception can be thrown if no space left in memory.

        :param list data: The data to add"""
        ...

    def delete(self) -> None:
        """Erase all the data saved in log."""
        ...

    def set_columns(self, data: list) -> None:
        """Create the columns of the CSV. The log reset every time the columns are redefined.

        :param list data: The columns to create"""
        ...
