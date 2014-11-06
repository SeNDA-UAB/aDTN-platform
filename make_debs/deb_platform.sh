#! /bin/bash

cd `dirname $0`
CURRPWD=`pwd`

CONFIG_FILE=configs/create_deb.conf

if [[ -f $CONFIG_FILE ]]; then
    . $CONFIG_FILE
else
    exit 1
fi

if [[ -z $ADTNPATH ]]; then
    echo "ADTNPATH variable must be set in $CONFIG_FILE config file."
    exit 2
fi

rm -rf $ADTNAUX > /dev/null 2>&1
mkdir $ADTNAUX > /dev/null 2>&1
cp -r $ADTNPATH/platform $ADTNAUX/ > /dev/null 2>&1
mkdir $DEB > /dev/null 2>&1
cd $ADTNAUX

#---------- PLATFORM ----------
cd platform
rm -r build > /dev/null 2>&1
mkdir -p build/doc-pak
cp $DOCSPLAT build/doc-pak
cd build
echo $DESCPLAT > description-pak

#Build scripts for before and after installation process with the predefined constants
cd $CURRPWD/$INSTSCRIPTSPATH/platform/
for script in `ls $INSTSCRIPTS`; do
  cat constants $script > $CURRPWD/$ADTNAUX/platform/build/$script-pak
done
cd - > /dev/null 2>&1

#Modify CMakeList with the required instructions to build the deb correctly
sed -i "s/DESTINATION $INIPATHOLD)/DESTINATION $INIPATHNEW)/g" ../CMakeLists.txt 
echo $INITPLATFORM >> ../CMakeLists.txt

#Copy init.d scripts to the right place in order to let CMake get them
mkdir -p $CURRPWD/$ADTNAUX/platform/config/init
cp $CURRPWD/configs/init/* $CURRPWD/$ADTNAUX/platform/config/init/ > /dev/null 2>&1

#Modify adtn.ini path
sed -i "s/\/var\/local\//\/var\//g" $CURRPWD/$ADTNAUX/platform/config/adtn.ini

cmake ../ -DCMAKE_INSTALL_PREFIX=/usr -DBTYPE=$DBTYPE
sudo checkinstall -y --pkgname=$NAMEPLAT --pkgversion=$VERSPLAT --pkgarch=$ARCH --install=$INST --maintainer=$MAINTAIN --requires=$REQPPLAT --pkgrelease=$RELEPLAT --backup=no --stripso=no --strip=no
cp *.deb $CURRPWD/$DEB
#------------------------------

echo "Deb packages have been copied to $CURRPWD/$DEB"
exit 0
