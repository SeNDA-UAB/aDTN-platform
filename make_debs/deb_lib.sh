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
sudo rm -rf $ADTNAUX/lib > /dev/null 2>&1
cp -r $ADTNPATH/lib $ADTNAUX/ > /dev/null 2>&1
mkdir $DEB > /dev/null 2>&1

#------------ LIB -------------
cd $CURRPWD/$ADTNAUX/lib
rm -r build > /dev/null 2>&1
mkdir -p build/doc-pak
cp $DOCSLIB build/doc-pak
cd build
echo $DESCLIB > description-pak
cp $CURRPWD/install_scripts/lib/* .

cmake ../ -DCMAKE_INSTALL_PREFIX=/usr -DBTYPE=$DBTYPE
sudo checkinstall -y --pkgname=$NAMELIB --pkgversion=$VERSLIB --pkgarch=$ARCH  --maintainer=$MAINTAIN --install=$INST --requires=$REQPLIB --pkgrelease=$RELELIB --backup=no --strip=no --stripso=no 
cp *.deb $CURRPWD/$DEB
#------------------------------
echo "Deb packages have been copied to $CURRPWD/$DEB"
#Remove created auxiliar path
#sudo rm -fr $CURRPWD/$ADTNAUX > /dev/null 2>&1
exit 0
