#! /bin/bash

cd `dirname $0`
CURRPWD=`pwd`

CONFIG_FILE=configs/create_deb.conf

if [[ -f $CONFIG_FILE ]]; then
    . $CONFIG_FILE
else
    echo "Config file missing: $CONFIG_FILE"
    exit 1
fi

if [[ -z $ADTNPATH ]]; then
    echo "ADTNPATH variable must be set in $CONFIG_FILE config file."
    exit 2
fi

rm -rf $ADTNAUX > /dev/null 2>&1
mkdir $ADTNAUX > /dev/null 2>&1
cp -r $ADTNPATH/bundleAgent $ADTNAUX/ > /dev/null 2>&1
cp $ADTNPATH/aDTNConfig.cmake.in $ADTNAUX/.
cd $ADTNAUX

#---------- PLATFORM ----------
cd bundleAgent 
rm -r build > /dev/null 2>&1
mkdir -p build/doc-pak
cp $DOCSPLAT build/doc-pak
cd build
echo $DESCPLAT > description-pak

#Build scripts for before and after installation process with the predefined constants
cd $CURRPWD/$INSTSCRIPTSPATH/adtn/
for script in `ls $INSTSCRIPTS`; do
  cat constants $script > $CURRPWD/$ADTNAUX/bundleAgent/build/$script-pak
done
cd - > /dev/null 2>&1

#Modify CMakeList with the required instructions to build the deb correctly
sed -i "s/DESTINATION $INIPATHOLD)/DESTINATION $INIPATHNEW)/g" ../CMakeLists.txt 
echo $INITPLATFORM >> ../CMakeLists.txt

#Copy init.d scripts to the right place in order to let CMake get them
mkdir -p $CURRPWD/$ADTNAUX/bundleAgent/config/init
cp $CURRPWD/configs/init/* $CURRPWD/$ADTNAUX/bundleAgent/config/init/ > /dev/null 2>&1

#Modify adtn.ini path
sed -i "s/\/var\/local\//\/var\//g" $CURRPWD/$ADTNAUX/bundleAgent/config/adtn.ini

cmake ../ -DCMAKE_INSTALL_PREFIX=/usr -DSYSCONFDIR=/etc -DCMAKE_BUILD_TYPE=debug
sudo checkinstall -y --pkgname=$NAMEPLAT --pkgversion=$VERSPLAT --pkgarch=$ARCH --install=$INST --maintainer=$MAINTAIN --requires=$REQPPLAT --pkgrelease=$RELEPLAT --backup=no --stripso=no --strip=no

mkdir $CURRPWD/$DEB > /dev/null 2>&1
cp *.deb $CURRPWD/$DEB
#------------------------------

echo "Deb packages have been copied to $CURRPWD/$DEB"
exit 0
