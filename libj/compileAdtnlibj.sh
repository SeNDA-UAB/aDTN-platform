#!/bin/bash

cd `dirname $0`
#
# This file compiles the adtnlibj for a given architecture (the current one)
#

default_inc="/usr/include"
default_lib="/usr/lib"

echo -e "|---------------------------------|"
echo -e "|   adtnlib java compile script   |"
echo -e "|                                 |"
echo -e "|  Default include: \e[91m/usr/include\e[39m  |"
echo -e "|  Default lib: \e[91m/usr/lib\e[39m          |"
echo -e "|---------------------------------|"

read -p "Do you want to change default values? (y/N)" var
var=${var,,} #To lower case
if [[ $var =~ (yes|y)$ ]]; then
	read -p "Set include path (/usr/include): " i_include
	read -p "Set lib path (/usr/lib): " i_lib
	if [ -n "$i_include" ]; then
		default_inc=$i_include
	fi
	if [ -n "$i_lib" ];then
		default_lib=$i_lib
	fi
fi
echo "Values Changed Correctly";



#Compilem tot menys examples, deixant els .class a bin, llavors executem el javah a bin
ant compile

cd c
#Compilem el C
mv ../bin/cat_uab_senda_adtn_comm_Comm.h .
gcc -shared -fPIC cat_uab_senda_adtn_comm_Comm.c -o ../bin/libNativeAdtnApi.so -I $default_inc -I $JAVA_HOME/include -I $JAVA_HOME/include/linux -L $default_lib -ladtnAPI

cd ../
#Construim el jar, generam javadoc i zip de tot.
ant dist

echo -e "\e[92mRequesting sudo rights in order to install native library\e[39m"
sudo cp bin/libNativeAdtnApi.so /usr/lib/
