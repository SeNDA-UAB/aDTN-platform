#! /bin/bash

cd `dirname $0`
CURRPWD=`pwd`

CONFIG_FILE=configs/create_deb.conf
LIBRARY_PATH_TMP=$LIBRARY_PATH
C_INCLUDE_PATH_TMP=$C_INCLUDE_PATH

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
cp $ADTNPATH/aDTNConfig.cmake.in $ADTNAUX/.


#-------- LIBJ PATCH ----------
#Patch in order to include libj into adtn-lib package
mkdir -p $ADTNAUX/tmp-lib/build
cp -r $ADTNAUX/lib/* $ADTNAUX/tmp-lib 
cd $CURRPWD/$ADTNAUX/tmp-lib/build
cmake ../ -DCMAKE_INSTALL_PREFIX=$CURRPWD/$ADTNAUX/inst-lib -DCMAKE_BUILD_TYPE=$DBTYPE -DARCH=$ARCH
make install
export LIBRARY_PATH=$LIBRARY_PATH:$CURRPWD/$ADTNAUX/inst-lib/lib
export C_INCLUDE_PATH=$C_INCLUDE_PATH:$CURRPWD/$ADTNAUX/inst-lib/include
cp -r $CURRPWD/$ADTNPATH/libj $CURRPWD/$ADTNAUX/
cd $CURRPWD/$ADTNAUX/libj
ant dist -Dver="$VERSLIB"
cp dist/libNativeAdtnApi.so ../lib/api
mkdir $CURRPWD/$RELEASE/v$VERSLIB-$RELELIB/$JAR > /dev/null 2>&1
cp dist/*.jar $CURRPWD/$RELEASE/v$VERSLIB-$RELELIB/$JAR
cd ../lib
echo 'install(FILES \${PROJECT_SOURCE_DIR}/api/libNativeAdtnApi.so PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE DESTINATION lib)' >> CMakeLists.txt
export LIBRARY_PATH=$LIBRARY_PATH_TMP
export C_INCLUDE_PATH=$C_INCLUDE_PATH_TMP

#------------ LIB -------------
cd $CURRPWD/$ADTNAUX/lib
rm -r build > /dev/null 2>&1
mkdir -p build/doc-pak
cp $DOCSLIB build/doc-pak
cd build
echo $DESCLIB > description-pak
cp $CURRPWD/install_scripts/lib/* .

cmake ../ -DCMAKE_INSTALL_PREFIX=/usr -DSYSCONFDIR=/etc -DCMAKE_BUILD_TYPE=$DBTYPE -DARCH=$ARCH
sudo checkinstall -y --pkgname=$NAMELIB --pkgversion=$VERSLIB --pkgarch=$ARCH  --maintainer=$MAINTAIN --install=$INST --requires=$REQPLIB --pkgrelease=$RELELIB --backup=no --fstrans=yes #--strip=no --stripso=no 
mkdir -p $CURRPWD/$RELEASE/v$VERSLIB-$RELELIB/$DEB > /dev/null 2>&1
cp *.deb $CURRPWD/$RELEASE/v$VERSLIB-$RELELIB/$DEB
#------------------------------
echo "Deb packages have been copied to $CURRPWD/$DEB"
exit 0
