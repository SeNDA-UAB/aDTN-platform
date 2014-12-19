## aDTN platform introduction

The aDTN communication platform developed at SeNDA is a Delay and Disruption Tolerant Networking following the ActiveDTN principles that allow routing software code to be propagated by the messages all over the network.

This document covers the system requirements, installation, an installation example, usage, update and uninstallation of the platform.

## System requirements

The platform is available only for PC Machines 64 bits with debian-like Linux operating system and with at least a network interface available.

Installation instructions are only provided for Ubuntu trusty (14.04), Ubuntu utopic (14.10) and Debian wheezy (7.*). For information about how to install the platform in other unix-like systems, contact <developers@senda.uab.cat>.

## Installation

Root privileges are necessary during the whole installation. Internet connection and apt-get application are also required during installation.

Download the public key of adtn deb report and add it to your apt-key manager
```bash
sudo sh -c 'wget -O - http://tao.uab.es/adtn/conf/adtn.gpg.key | apt-key add -'
```
Update the `sources.list` files.
```bash
sudo wget -P /etc/apt/sources.list.d/ http://tao.uab.es/adtn/conf/wheezy/adtn.list
```
Update the apt-get repository.
```bash
sudo apt-get update
```

Three different installation options are provided for nodes with different needs:

* __adtn__: For basic nodes and observer nodes only.
* __adtn-lib__: For nodes running applications.
* __adtn-tools__: For nodes running applications with network connection checking tools.

Execute the line that fits better the needs of your node (ONLY ONE OF THE FOLLOWING):
```bash
sudo apt-get install adtn
sudo apt-get install adtn-lib
sudo apt-get install adtn-tools
```
During installation, you will be asked to enter the following information to properly configure your node:

1. Set the adtn identifier (default is <hostname>). The adtn identifier must be unique.
2. Set the IP (default is <available_network_interface_ip>). IP address is crucial for lower layer communication. All adtn nodes must have a unique IP address and must be in the same local network.
3. Set the adtn port (default is 4556). The default port must be ok if you are running a single adtn platform instance per machine and there is no other application using it.

## Usage

Network management requires root privileges. Therefore, sudo is required to start, get status, supervise (start stopped processes), and stop the platform.
```bash
sudo service adtn start
sudo service adtn status
sudo service adtn supervise
sudo service adtn stop
```
Network usage, even to check connectivity, does not require root privileges (sudo). The available tools are:

* adtn-ping: Sends a bundle to network nodes. For more info, execute adtn-ping -h.
* adtn-neighbours: Shows the list of neighbour nodes. For more info, execute adtn-neighbours -h.
* adtn-traceroute: Prints the route bundles take to network host. For more info, execute adtn-traceroute -h.

## aDTN library documentation

The `adtn-lib` provides a `C` and a `Java` library to develop over the platform.

[C library documentation](http://senda-uab.github.io/aDTN-platform/c_doxygen/index.html "C library documentation")

[JavaDoc](http://senda-uab.github.io/aDTN-platform/java_doc/index.html "JavaDoc")

## Example of installation and usage

This example shows how alice installs a single adtn platform in her machine using adtn-tools.
```bash
# Download the public key of adtn deb report and add it to your apt-key manager
sudo sh -c 'wget -O - http://tao.uab.es/adtn/conf/adtn.gpg.key | apt-key add -'
# Update the sources.list files.
sudo wget -P /etc/apt/sources.list.d/ http://tao.uab.es/adtn/conf/wheezy/adtn.list
# Update the apt-get repository.
sudo apt-get update
# Install platform + development library + ping + traceroute
sudo apt-get install adtn-tools
# Set platform name as alice
# Accept default IP
# Accept default node
# Run the platform
sudo service adtn start
# Run ping to your own machine
adtn-ping alice
```
## Update

Update the apt-get repository in order to look for available updates:
```bash
sudo apt-get update
```
Repeat the installation:
```bash
sudo apt-get install adtn
sudo apt-get install adtn-lib
sudo apt-get install adtn-tools
```

## Uninstall

Uninstalling the platform requires executing the following line.

```bash
sudo apt-get remove --purge adtn
```
Notice that --purge flag removes all adtn configuration files.

If you also want to remove the public key of the adtn deb repo from the apt-key manager, execute the following lines:

```bash
sudo apt-key del  `apt-key list | grep -i -1 developers@senda.uab.cat | grep pub | cut -d '/' -f2- | cut -d ' ' -f1`
sudo rm /etc/apt/sources.list.d/adtn.list
```
