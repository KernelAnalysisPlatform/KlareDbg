#!/bin/bash

python="python"
# if you don't have ubuntu you are on your own here
if [ $(which apt-get) ]; then
  echo "fetching qemu build-deps, enter your password"
  sudo apt-get update -qq
  sudo apt-get --no-install-recommends -qq -y build-dep qemu
  sudo apt-get install -qq -y wget flex bison libtool automake autoconf autotools-dev pkg-config libglib2.0-dev
elif [ $(which pacman) ]; then
  python="python2"
  echo "WARNING: you are using pacman, you are awesome but are going to need to fetch the build deps of QEMU on your own"
else
  echo "WARNING: you don't have apt-get, you are required to fetch the build deps of QEMU on your own"
fi

cd qemu/decaf
./configure --target-list=x86_64-softmmu
make -j $(grep processor < /proc/cpuinfo | wc -l)
