"""aDTN python wrapper.

This module is a wrapper of the aDTN API.
"""
import ctypes
import ConfigParser


class adtnSocketAddress(ctypes.Structure):

    """This class contains the structure used to identify a node.

    Attributes:
        adtn_port (int): Port of the application.
        id (str): Identifier of the node.
    """

    _fields_ = [("adtn_port", ctypes.c_int), ("id", ctypes.c_char_p)]


class adtnSocket(object):

    """This class holds an aDTN socket and functions to interact with it."""

    def __init__(self, config):
        """aDTN socket.

        This will fill the node name, and the RIT path from the configuration
        file.

        Args:
            config (str): Path to the configuration file.
        """
        self.adtn = ctypes.CDLL('libadtnAPI.so')
        self.rit = ctypes.CDLL('libadtn.so')
        # adtn.h functions
        self.adtn.adtn_var_socket.argtypes = [ctypes.c_char_p]
        self.adtn.adtn_var_socket.restype = ctypes.c_int
        self.adtn.adtn_bind.argtypes = [ctypes.c_int,
                                        ctypes.POINTER(adtnSocketAddress)]
        self.adtn.adtn_bind.restype = ctypes.c_int
        self.adtn.adtn_sendto.argtypes = [ctypes.c_int, ctypes.c_void_p,
                                          ctypes.c_size_t, adtnSocketAddress]
        self.adtn.adtn_sendto.restype = ctypes.c_int
        self.adtn.adtn_recv.argtypes = [ctypes.c_int, ctypes.c_void_p,
                                        ctypes.c_size_t]
        self.adtn.adtn_recv.restype = ctypes.c_int
        self.adtn.adtn_close.argtypes = [ctypes.c_int]
        self.adtn.adtn_close.restype = ctypes.c_int
        # rit.h functions
        self.rit.rit_changePath.argtypes = [ctypes.c_char_p]
        self.rit.rit_changePath.restype = ctypes.c_int
        self.rit.rit_var_set.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        self.rit.rit_var_set.restype = ctypes.c_int
        self.__sockInfo = adtnSocketAddress()
        self.__config = config
        configP = ConfigParser.ConfigParser()
        configP.readfp(open(self.__config))
        self.__id = configP.get("global", "id")
        self.__sockInfo.id = self.__id
        self.__ritPath = configP.get("global", "data") + "/RIT"
        self._sock = self.adtn.adtn_var_socket(self.__config)

    def bind(self, port):
        """Bind the socket to the given port.

        Args:
            port (int): Port to bind the aDTN socket.

        Returns:
            bool: True if successful, False otherwise.
        """
        self.__sockInfo.adtn_port = port
        return self.adtn.adtn_bind(self._sock, self.__sockInfo) == 0

    def send(self, message, who):
        """Send a message to a node.

        Args:
            message (str): Message to send.
            who (adtnSocketAddress): aDTNSocketAddress with the destination.

        Returns:
            bool: True if successful, False otherwise.
        """
        r = self.adtn.adtn_sendto(self._sock, message, len(message), who) != -1
        return r

    def recv(self):
        """Recive a message.

        Returns:
            str: The received message.
        """
        aux = ctypes.create_string_buffer('\000' * 512)
        self.adtn.adtn_recv(self._sock, aux, 512)
        return aux.value

    def ritSet(self, path, value):
        """Set a value in the RIT.

        Args:
            path (str): Path for the value.
            value (str): Value to save.

        Returns:
            bool: True if successful, False otherwise
        """
        self.rit.rit_changePath(self.__ritPath)
        return self.rit.rit_var_set(path, value) == 0

    def __del__(self):
        """Close the open aDTN socket."""
        self.adtn.adtn_close(self._sock)
