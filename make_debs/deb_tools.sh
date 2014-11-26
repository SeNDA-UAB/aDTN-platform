#! /bin/bash

cd `dirname $0`
CURRPWD=`pwd`

CONFIG_FILE=configs/create_deb.conf

if [[ -f $CONFIG_FILE ]]; then
    . $CONFIG_FILE
else
    CONFIG_FILE=configs/create_deb.conf
    exit 1
fi

if [[ -z $ADTNPATH ]]; then
    echo "ADTNPATH variable must be set in $CONFIG_FILE config file."
    exit 2
fi

mkdir $ADTNAUX > /dev/null 2>&1
sudo rm -rf $ADTNAUX/tools > /dev/null 2>&1
cp -r $ADTNPATH/tools $ADTNAUX/ > /dev/null 2>&1
cp $ADTNPATH/aDTNConfig.cmake.in $ADTNAUX/.

#------------ EXM -------------
cd $CURRPWD/$ADTNAUX/tools
rm -r build > /dev/null 2>&1
mkdir -p build/doc-pak
cp $DOCSEXM build/doc-pak
cd build
echo $DESCEXM > description-pak
cp $CURRPWD/install_scripts/tools/* .

cmake ../ -DCMAKE_INSTALL_PREFIX=/usr -DSYSCONFDIR=/etc -DCMAKE_BUILD_TYPE=debug
sudo checkinstall -y --pkgname=$NAMEEXM --pkgversion=$VERSEXM --pkgarch=$ARCH --maintainer=$MAINTAIN --install=$INST --requires=$REQPEXM --pkgrelease=$RELEEXM --backup=no --strip=no --stripso=no 
mkdir $DEB > /dev/null 2>&1
cp *.deb $CURRPWD/$DEB
#------------------------------

echo "Deb packages have been copied to $CURRPWD/$DEB"
exit 0
