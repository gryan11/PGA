#!/bin/bash

# Description:
#    Use this script to configure the LLVM build environment.
# Typically, this needs to be run only once while setting up
# the repo and the build environment. For re-building LLVM in 
# an already configured setup, one needs only to run
# 'make && make install' in the build directory.
#
# Author: Koustubha Bhat (Vrije Universiteit, Amsterdam)
# Columbia University, New York.
#
# Sourced from older install script on Oct 4th 2018.
#
# Modified by Gabriel Ryan Feb 16th 2021 to be standalone

LOGFILE="`pwd`/configure-llvm.log"
: ${BIN_DIR="`pwd`/bin"}
: ${SRC_DIR_LLVM=`pwd`/../llvm-7.0.0.src}
: ${OBJ_DIR_LLVM=`pwd`}
: ${INSTALL_BINUTILS=1}
: ${USE_NINJA=1}
: ${LLVM_BUILD_TYPE=RelWithDebInfo}
: ${BINUTILS_NAME="binutils-2.27.90"}
: ${BINUTILS_PATH=`pwd`/binutils}


set -e 

cmake_configure()
{
  SRCDIR=$1
  BUILDDIR=$2
  CMAKE_ADDL_OPTIONS=$3
  INSTALLDIR=${BIN_DIR}

  echo "Build dir: $BUILDDIR"
  echo "Install dir: ${INSTALLDIR}"
  cd $BUILDDIR
  cmake ${CMAKE_ADDL_OPTIONS} $SRCDIR
  cmake -DCMAKE_INSTALL_PREFIX:PATH=${INSTALLDIR} \
    -DLLVM_ENABLE_ASSERTIONS:BOOL=ON              \
    -DCMAKE_BUILD_TYPE:STRING=${LLVM_BUILD_TYPE}  \
    ${CMAKE_ADDL_OPTIONS} ${BUILDDIR}
}

install_binutils()
{
  BINUTILS_TARBALL="${BINUTILS_NAME}.tar.bz2"

  # *** Download and extract
  if [ ! -d ${BINUTILS_PATH} ]; then
    mkdir -p ${BINUTILS_PATH}
  fi
  # Skip if already installed.
  if [ "${FORCE_INSTALL_BINUTILS}" == "0" ] \
    && [ -e ${BINUTILS_PATH}/bin/include/plugin-api.h ] \
    && [ -e ${BINUTILS_PATH}/bin/lib/libbfd.so ] \
    && [ -e ${BINUTILS_PATH}/bin/lib/libopcodes.so ]; then
    echo -n "${BINUTILS_NAME} already found here: ${BINUTILS_PATH}/bin"
    echo "  [ Skipping ahead ]"
    return
  fi

  # *** Install binutils with gold support
  if [ "${INSTALL_BINUTILS}" != "0" ]; then
    echo "Installing binutils (${BINUTILS_NAME})... "
    cd ${BINUTILS_PATH}
    if [ ! -e ${BINUTILS_TARBALL} ]; then
      wget ftp://sourceware.org/pub/binutils/snapshots/${BINUTILS_TARBALL} 
    fi
    if [ ! -d ${BINUTILS_NAME} ]; then tar -xf ${BINUTILS_TARBALL}; fi
      cd ${BINUTILS_NAME}
      if [ ! -d ${BINUTILS_PATH}/bin ]; then
         mkdir -p ${BINUTILS_PATH}/bin
      fi
      ./configure --prefix=${BINUTILS_PATH}/bin --enable-shared --enable-gold --enable-plugins --disable-werror
      make -j16 all-gold
      make install || true
      if [ ! -e ${BINUTILS_PATH}/bin/include/plugin-api.h ] || [ ! -e ${BINUTILS_PATH}/bin/lib/libbfd.so ] || [ ! -e ${BINUTILS_PATH}/bin/lib/libopcodes.so ]; then
      echo "Failure during installation of binutils."
      #exit 1
    fi
    echo "      [done]"
    echo
  fi
}

main()
{
  if [ "${INSTALL_BINUTILS}" != "0" ]; then
    install_binutils
  fi

  # Ensure build and install directories are sane
  if [ -e ${BIN_DIR} ]; then
    read -p "Are you sure you want to delete: $BIN_DIR ? [y|n]" confirm
    if [ "y" != "$confirm" ];
    then
      rm -rf ${BIN_DIR} ${OBJ_DIR_LLVM} 2>/dev/null || true
    fi
  fi
  if [ ! -d ${OBJ_DIR_LLVM} ]; then  mkdir -p $OBJ_DIR_LLVM; fi
  if [ ! -d ${BIN_DIR} ]; then  mkdir -p ${BIN_DIR} ; fi

  CMAKE_NINJA_OPTS=
  if [ ${USE_NINJA} -eq 1 ]; then
    CMAKE_NINJA_OPTS="-DCMAKE_C_COMPILER=`which gcc` -DCMAKE_CXX_COMPILER=`which g++` -G Ninja "
  fi
  cmake_configure ${SRC_DIR_LLVM} ${OBJ_DIR_LLVM}   \
    "-DLLVM_CONFIG:PATH=${OBJ_DIR_LLVM}/bin/llvm-config \
    -DLLVM_BINUTILS_INCDIR=${BINUTILS_PATH}/${BINUTILS_NAME}/include \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DLLVM_BUILD_LLVM_DYLIB=ON -DLLVM_LINK_LLVM_DYLIB=ON \
    ${CMAKE_NINJA_OPTS}"


    #-DCMAKE_ASM_COMPILER=clang \
}

main | tee -a ${LOGFILE}
