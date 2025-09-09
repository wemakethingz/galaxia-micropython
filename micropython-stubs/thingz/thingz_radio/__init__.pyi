"""Thingz Radio"""

from __future__ import annotations

class Radio:
    """Send and receive messages between boards"""

    def send(self, data: str) -> None:
        """
        Send a message. The message is broadcasted, the board around, if on the same channel, will receive it

        :param str data: The data to send
        """
        ...

    def receive(self) -> str:
        """
        Wait for data to be received

        :return: The data received
        :rtype: str
        """
        ...

    def set_chanel(self, channel: int) -> None:
        """
        Change the channel used by the radio module

        :param int channel: The channel between 1 and 10
        """
        ...

    def get_channel(self) -> int:
        """
        Get the channel used by the radio module

        :return: The channel used by the radio module
        :rtype: int
        """
        ...

    def get_mac(self) -> bytes:
        """
        Get the mac address used by the radio module

        :return: The mac address used by the radio module
        :rtype: bytes
        """
        ...
