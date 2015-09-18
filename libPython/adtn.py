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


class _codeOptions(ctypes.Structure):
    _fields_ = [("fd", ctypes.c_int), ("option_name", ctypes.c_int),
                ("code", ctypes.c_char_p), ("from_file", ctypes.c_int),
                ("replace", ctypes.c_int)]

ROUTING_CODE = 0x01
PRIO_CODE = 0x02
LIFE_CODE = 0x03


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
        self.adtn.adtn_recvfrom.argtypes = [ctypes.c_int, ctypes.c_void_p,
                                            ctypes.c_size_t,
                                            ctypes.POINTER(adtnSocketAddress)]
        self.adtn.adtn_recvfrom.restype = ctypes.c_int
        self.adtn.adtn_var_setcodopt.argtypes = [_codeOptions]
        self.adtn.adtn_var_setcodopt.restype = ctypes.c_int
        self.adtn.adtn_close.argtypes = [ctypes.c_int]
        self.adtn.adtn_close.restype = ctypes.c_int
        # rit.h functions
        self.rit.rit_changePath.argtypes = [ctypes.c_char_p]
        self.rit.rit_changePath.restype = ctypes.c_int
        self.rit.rit_var_set.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        self.rit.rit_var_set.restype = ctypes.c_int
        self.rit.rit_var_unset.argtypes = [ctypes.c_char_p]
        self.rit.rit_var_unset.restype = ctypes.c_int
        self.rit.rit_var_tag.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                         ctypes.c_char_p]
        self.rit.rit_var_tag.restype = ctypes.c_int
        self.rit.rit_var_untag.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        self.rit.rit_var_untag.restype = ctypes.c_int
        self.rit.rit_getValue.argtypes = [ctypes.c_char_p]
        self.rit.rit_getValue.restype = ctypes.c_char_p
        # Class Variables
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
        buff = ctypes.create_string_buffer('\000' * 512)
        self.adtn.adtn_recv(self._sock, buff, 512)
        return buff.value

    def recvFrom(self):
        """Recive a message and the sender info.

        Returns:
            (str, adtnSocketAddress): The received message and the sender.
        """
        buff = ctypes.create_string_buffer('\000' * 512)
        node = adtnSocketAddress()
        self.adtn.adtn_recvfrom(self._sock, buff, 512, node)
        return (buff.value, node)

    def setCode(self, codeType, code, isFile=False, replace=False):
        """Set a Code to the socket.

        This code could be a routing code(ROUTING_CODE),
        a life code (LIFE_CODE) or a priority code (PRIO_CODE).

        The routing code will be executed in each hop of the network to
        determine the next hop.

        The life code will be executed in each node of the network to decide
        if the message is lapsed or not.

        The priority code will affect only to the messages in other nodes that
        pertain to same application.

        Args:
            codeType : One value from ROUTING_CODE, LIFE_CODE or PRIO_CODE
            code (str): The string containing the code or the path to the code.
            isFile (bool): If code is a path this must be set to True.
            replace (bool): If a code has been set already, and want to change
                            it this must be True.

        Returns:
            bool: True if successful, False otherwise.
        """
        options = _codeOptions(self._sock, codeType, code, isFile, replace)
        return self.adtn.adtn_var_setcodopt(options) == 0

    def ritSet(self, path, value):
        """Set a value in the RIT.

        Args:
            path (str): Path for the value.
            value (str): Value to save.

        Returns:
            bool: True if successful, False otherwise.
        """
        self.rit.rit_changePath(self.__ritPath)
        return self.rit.rit_var_set(path, value) == 0

    def ritUnset(self, path):
        """Unset a value in the RIT.

        Args:
            path (str): Path to unset.

        Returns:
            bool: True if successful, False otherwise.
        """
        self.rit.rit_changePath(self.__ritPath)
        return self.rit.rit_var_unset(path) == 0

    def ritTag(self, path, tag, value):
        """Add a tag with a value to the path in the RIT.

        Args:
            path (str): Path to add the tag.
            tag (str): tag to add.
            value (str): value for the tag.

        Returns:
            bool: True if successful, False otherwise.
        """
        self.rit.rit_changePath(self.__ritPath)
        return self.rit.rit_var_tag(path, tag, value) == 0

    def ritUntag(self, path, tag):
        """Remoev the tag to the path in the RIT.

        Args:
            path (str): Path to remove the tag.
            tag (str): Name of the tag to remove.

        Returns:
           bool: True if successful, False otherwise.
        """
        self.rit.rit_changePath(self.__ritPath)
        return self.rit.rit_var_untag(path, tag) == 0

    def ritGetValue(self, path):
        """Get the value of the given path.

        Args:
            path (str): Path to get the value.

        Returns:
            str: Value in the path, or None if not found.
        """
        self.rit.rit_changePath(self.__ritPath)
        return self.rit.rit_getValue(path)

    def __del__(self):
        """Close the open aDTN socket."""
        self.adtn.adtn_close(self._sock)
