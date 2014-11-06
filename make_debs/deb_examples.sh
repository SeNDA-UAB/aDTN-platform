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

mkdir $ADTNAUX > /dev/null 2>&1
sudo rm -rf $ADTNAUX/examples > /dev/null 2>&1
cp -r $ADTNPATH/examples $ADTNAUX/ > /dev/null 2>&1
mkdir $DEB > /dev/null 2>&1

#------------ EXM -------------
cd $CURRPWD/$ADTNAUX/examples
rm -r build > /dev/null 2>&1
mkdir -p build/doc-pak
cp $DOCSEXM build/doc-pak
cd build
echo $DESCEXM > description-pak
cp $CURRPWD/install_scripts/examples/* .

cmake ../ -DCMAKE_INSTALL_PREFIX=/usr -DBTYPE=$DBTYPE
sudo checkinstall -y --pkgname=$NAMEEXM --pkgversion=$VERSEXM --pkgarch=$ARCH --maintainer=$MAINTAIN --install=$INST --requires=$REQPEXM --pkgrelease=$RELEEXM --backup=no --strip=no --stripso=no 
cp *.deb $CURRPWD/$DEB
#------------------------------

echo "Deb packages have been copied to $CURRPWD/$DEB"
exit 0
