Building the adtn platform
============================

The aDTN platform has the following dependences. These are ubuntu packages:

	make
	cmake
	libssl-dev
	libsqlite3-dev
	gcc

Uthash is a header file that can be found at http://http://troydhanson.github.io/uthash/. It is provided with the code so it doesn't need to be installed. It and can be found in common/include. The license of this file is BSD and can be found in the file uthash.h.

Building process:
============================

It is recommended to build the sources in a different directory than where the source files are located. From the source root dir:

mkdir build
cd build

The following options are supported:

CMAKE_INSTALL_PREFIX: Sets the prefix where the platform will be installed. Default /usr/local
CMAKE_BUILD_TYPE: Supported options debug and release. Default release
SYSCONFDIR: Sets where the configuration files will be installed. Default ${CMAKE_INSTALL_PREFIX}/etc

To use an option -D${OPTION}=VALUE. For example to install in the default path and build as debug:

cmake .. -DCMAKE_BUILD_TYPE=debug
make 
# make install

Then check ${SYSCONFDIR}/etc/adtn/adtn.ini to configure the platform.

Notes:
============================
If the library is installed in a non standard path, the library path has to be added to the LD_LIBRARY_PATH environment variable or included in the ldconfig config files. Once configured you need to update update the dynamic libraries cache:

# ldconfig


