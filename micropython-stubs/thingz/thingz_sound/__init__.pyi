"""Thingz Sound"""

from __future__ import annotations

class Sound:
    """Output sound using Galaxia's Jack connector"""

    def play(self, on: bool, freq: int) -> None:
        """
        :param bool on: Enable frequency generation onto the jack
        :param int freq: The frequency to generate in Hz
        """
        ...

    def set_frequency(self, freq: int) -> None:
        """
        :param int freq: The frequency to generate in Hz
        """
        ...

    def set_volume(self, volume: int) -> None:
        """
        Set the volume of the sound

        :param int volume: The volume of the sound between 0 and 100
        """
        ...

    def play_sample(self, filename: str) -> None:
        """
        Play a sound sample. Sample must be in wav format

        :param str filename: The path to file
        """
        ...
