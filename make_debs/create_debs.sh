#! /bin/bash

cd `dirname $0`
CURRPWD=`pwd`

CONFIG_FILE=configs/create_deb.conf

if [[ -f $CONFIG_FILE ]]; then
    . $CONFIG_FILE
else
    exit 1
fi

echo -e "\e[92m"
read -p "Do you want to increment the old version platform (${RELEPLAT})? (y/N)" releplat
releplat=${releplat,,}	#var to lower case
if [[ $releplat =~ (yes|y)$ ]]; then
  sed -i "s/RELEPLAT=\"${RELEPLAT}\"/RELEPLAT=\"$((${RELEPLAT}+1))\"/g" $CONFIG_FILE
fi

read -p "Do you want to increment the old version library (${RELELIB})? (y/N)" relelib
relelib=${relelib,,}	#var to lower case
if [[ $relelib =~ (yes|y)$ ]]; then
  sed -i "s/RELELIB=\"${RELELIB}\"/RELELIB=\"$((${RELELIB}+1))\"/g" $CONFIG_FILE
fi

read -p "Do you want to increment the old version platform (${RELEEXM})? (y/N)" releexm
releplat=${releexm,,}	#var to lower case
if [[ $releexm =~ (yes|y)$ ]]; then
  sed -i "s/RELEEXM=\"${RELEEXM}\"/RELEEXM=\"$((${RELEEXM}+1))\"/g" $CONFIG_FILE
fi

for arch in $ARCHLIST; do
  if [[ $ARCH == $ARCH32 ]]; then
    sed -i "s/ARCH=\$ARCH32/ARCH=$arch/g" $CONFIG_FILE
  elif [[ $ARCH == $ARCH64 ]]; then
    sed -i "s/ARCH=\$ARCH64/ARCH=$arch/g" $CONFIG_FILE
  fi
  echo -e "\e[92mRemoving old debs if any...\e[39m"
  sudo dpkg --purge $NAMEEXM $NAMELIB $NAMEPLAT
  echo -e "\e[92mRemoving previous auxiliar installation folder...\e[39m"
  sudo rm -fr $ADTNAUX
  echo -e "\e[92mGenerating deb platform...\e[39m"
  bash deb_platform.sh
  echo -e "\e[92mInstalling platform...\e[39m"
  sudo dpkg -i $DEB/${NAMEPLAT}_${VERSPLAT}-${RELEPLAT}_${ARCH}.deb
  
  echo -e "\e[92mGenerating lib platform...\e[39m"
  bash deb_lib.sh
  echo -e "\e[92mInstalling lib...\e[39m"
  sudo dpkg -i $DEB/${NAMELIB}_${VERSLIB}-${RELELIB}_${ARCH}.deb
  echo -e "\e[92mGenerating examples...\e[39m"
  bash deb_examples.sh
  echo -e "\e[92mInstalling examples...\e[39m"
  sudo dpkg -i $DEB/${NAMEEXM}_${VERSEXM}-${RELEEXM}_${ARCH}.deb
  echo -e "\e[92m"
  read -p "Do you want to remove the debs installed? (Y/n)" var
  echo -e "\e[39m"
  var=${var,,}	#var to lower case
  if [[ $var =~ (yes|y)$ ]] || [ -z $var ]; then
    echo -e "\e[92mRemoving installed debs...\e[39m"
    sudo dpkg --purge $NAMEEXM $NAMELIB $NAMEPLAT
  fi
  ARCH=`eval echo -e "$arch"`
done;

echo -e "\e[92mEverything went as expected. Have a nice day =)~\e[39m"
exit 0;
