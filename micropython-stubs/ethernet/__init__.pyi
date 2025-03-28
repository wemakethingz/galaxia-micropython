"""Ethernet module

Add ethernet connectivity using Thingz ethernet extension board


"""

from __future__ import annotations

def active(self, state: bool) -> True:
    """If called with no args, return the current state of the ethernet driver.
    Else start or stop the ethernet driver

    :param bool state: activate the ethernet driver if True else stop it"""
    ...

def status(self) -> int:
    """Return the current status of the driver can be:
    * INITIALIZED: 0, intial status
    * STARTED: 1, the driver is started and the module was found
    * STOPPED: 2, the driver is stopped
    * CONNECTED: 3, ethernet connection established
    * DISCONNECTED: 4, ethernet connection lost
    * GOT_IP: 5, ethernet connection established and IP address set
    """

    ...

def isconnected(self) -> bool:
    """Return True if IP is set"""

    ...

def ifconfig(self, ip_config: list / tuple / str) -> int:
    """If called with no arg, return the current IP address.
    Else set the interface configuration

    :param list/tuple/str ip_config: Can be "dhcp" to configure the interface using DHCP or a list/tuple with the following args:
    * ip address: str
    * netmask: str
    * gateway: str
    * DNS: str
    """

    ...
