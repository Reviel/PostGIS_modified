#!/bin/bash
###############################################################################
# yangzhegnlong 2023/12/08 compile postgis shared library for dev6_opt_lib 
#   branches. needs build spatial previously.
# 
# usage:
# <path to this file>/build.sh <DEBUG or RELEASE> <make parallelism> [noclean]
# eg: ./build.sh RELEASE 2 noclean
###############################################################################

########################### Preparation #######################################
# get current script dir path (build path)
PRG="$0"
PRGDIR=`dirname "$PRG"`
cd "$PRGDIR"
POSTGIS_HOME=`pwd`
cd "$POSTGIS_HOME"



# check if compiler support c++11
# echo "main(){}" > test_cpp_11.cpp
# g++ --std=c++11 -o test_cpp_11 test_cpp_11.cpp
# RC=$?
# if [ $RC == 0 ]; then
#     rm -f test_cpp_11
#     rm -f test_cpp_11.cpp
#     echo "C++11(GCC) check pass"
# else
#     rm -f test_cpp_11
#     rm -f test_cpp_11.cpp
#     echo "error:No support C++11(GCC) for GEO2"
#     exit 1
# fi


CC_HIGH=gcc
CXX_HIGH=g++
HIGH_BASHRC=""
ORIGIN_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
ORIGIN_CPATH=$CPATH
use_high_compiler(){
    if [ "$HIGH_BASHRC" != "" ]; then
        source $HIGH_BASHRC
    fi
    export CC=$CC_HIGH
    export CXX=$CXX_HIGH
}
use_low_compiler(){
    export LD_LIBRARY_PATH=$ORIGIN_LD_LIBRARY_PATH
    export CPATH=$ORIGIN_CPATH
    export CC=gcc
    export CXX=g++
}


# check if system has static libstdc++ is no need any more



# get build type
if [[ -n $1 ]]; then
    if [ $1 = "DEBUG" ]; then
        POSTGIS_BUILD_TYPE=Debug
    elif [ $1 = "RELEASE" ]; then
        POSTGIS_BUILD_TYPE=Release
    elif [ $1 = "STRIPDEBUG" ]; then
        POSTGIS_BUILD_TYPE=RelWithDebInfo
    else
        echo "error:unknown build type $1"
        exit 1
    fi
else
    echo "error:needs build type"
    exit 1
fi



# get building parallelism
# TODO check if $2 is legal
if [[ -n $2 ]]; then
    if [ $2 = "0" ]; then
        MAKE_PARALLELISM="-j"
    else
        MAKE_PARALLELISM="-j $2"
    fi
else
    MAKE_PARALLELISM="-j"
fi



# get clean flag
if [[ -n $3 ]]; then
    if [ $3 = "noclean" ]; then
        CLEAN_FLG=0
    else
        echo "error:unknown clean flag $3"
        exit 1
    fi
else
    CLEAN_FLG=1
fi



# check if needs shared library exist in ../thirdparty
# check gmp
SHARED_LIB_ENOUGH="1"
if [ ! -f "$POSTGIS_HOME/../thirdparty/libgmp.so" ]; then
    SHARED_LIB_ENOUGH="0"
fi
# check mpfr
if [ ! -f "$POSTGIS_HOME/../thirdparty/libmpfr.so" ]; then
    SHARED_LIB_ENOUGH="0"
fi
# check sqlite3
#if [ ! -f "$POSTGIS_HOME/../thirdparty/libsqlite3.so" ]; then
#    SHARED_LIB_ENOUGH="0"
#fi
# use static library
# check PROJ
if [ ! -f "$POSTGIS_HOME/../thirdparty/libproj.so" ]; then
    SHARED_LIB_ENOUGH="0"
fi
# check JSON-C
if [ ! -f "$POSTGIS_HOME/../thirdparty/libjson-c.so" ]; then
    SHARED_LIB_ENOUGH="0"
fi
# check GEOS
if [ ! -f "$POSTGIS_HOME/../thirdparty/libgeos.so" ]; then
    SHARED_LIB_ENOUGH="0"
fi
# check GEOS_C
if [ ! -f "$POSTGIS_HOME/../thirdparty/libgeos_c.so" ]; then
    SHARED_LIB_ENOUGH="0"
fi
# check SFCGAL
if [ $POSTGIS_BUILD_TYPE == "Debug" ]; then
    if [ ! -f "$POSTGIS_HOME/../thirdparty/libSFCGALd.so" ]; then
        SHARED_LIB_ENOUGH="0"
    fi
elif [ $POSTGIS_BUILD_TYPE == "Release" -o $POSTGIS_BUILD_TYPE == "RelWithDebInfo" ]; then
    if [ ! -f "$POSTGIS_HOME/../thirdparty/libSFCGAL.so" ]; then
        SHARED_LIB_ENOUGH="0"
    fi
else
    echo "error:unknown build type"
    exit 1
fi
# check GDAL
if [ ! -f "$POSTGIS_HOME/../thirdparty/libgdal.so" ]; then
    SHARED_LIB_ENOUGH="0"
fi
# check PROTOBUF-C
if [ ! -f "$POSTGIS_HOME/../thirdparty/libprotobuf-c.so" ]; then
    SHARED_LIB_ENOUGH="0"
fi


# build spatial shared library
if [ $SHARED_LIB_ENOUGH == "0" ]; then
    echo "spatial shared library check fail"
    echo "build spatial shared library now."
    cd ..
    chmod +x build_others.sh
    if [[ -n $2 ]]; then
        SPATIAL_BUILD_ARG2=$2
    else
        SPATIAL_BUILD_ARG2=""
    fi
    if [[ -n $3 ]]; then
        SPATIAL_BUILD_ARG3=$3
    else
        SPATIAL_BUILD_ARG3=""
    fi
    ./build_others.sh $1 $SPATIAL_BUILD_ARG2 $SPATIAL_BUILD_ARG3
fi
RC=$?
if [ $RC != 0 ]
then
    echo "build(SPATIAL)"
    exit $RC
fi
###############################################################################


########################### Building ##########################################
if [ $CXX_HIGH != "g++" ]; then
    use_high_compiler
fi

cd "$POSTGIS_HOME"
if [ $CLEAN_FLG == "1" ]; then
    rm -rf build
    rm -rf tmp_install
fi
mkdir -p build
mkdir -p tmp_install
cd build
if [ $CXX_HIGH != "g++" ]; then
    POSTGIS_CFLAGS="-static-libgcc"
    POSTGIS_CXXFLAGS="-static-libstdc++"
else
    POSTGIS_CFLAGS=""
    POSTGIS_CXXFLAGS=""
fi
cmake -DCMAKE_BUILD_TYPE=$POSTGIS_BUILD_TYPE -DCMAKE_INSTALL_RPATH="\$ORIGIN/" \
    -DCMAKE_INSTALL_PREFIX=$POSTGIS_HOME/tmp_install -DCMAKE_CXX_FLAGS="$POSTGIS_CXXFLAGS -fPIC" \
    -DCMAKE_C_FLAGS="$POSTGIS_CFLAGS" -DCMAKE_C_COMPILER=$CC_HIGH -DCMAKE_CXX_COMPILER=$CXX_HIGH \
    -DJSON_C_DIR=$POSTGIS_HOME/../json-c-json-c-0.17-20230812/tmp_install \
    -DGEOS_DIR=$POSTGIS_HOME/../geos-3.12.1/tmp_install \
    -DPROJ_DIR=$POSTGIS_HOME/../proj-9.3.0/tmp_install \
    -DGDAL_DIR=$POSTGIS_HOME/../gdal-3.7.2/tmp_install \
    -DPROTOBUF_C_DIR=$POSTGIS_HOME/../protobuf-c-1.4.1/tmp_install \
    -DSFCGAL_DIR=$POSTGIS_HOME/../SFCGAL-v1.5.0/tmp_install ..
RC=$?
if [ $RC != 0 ]
then
    echo "cmake(POSTGIS)"
    exit $RC
fi
cmake --build ./ --target install --config $POSTGIS_BUILD_TYPE $MAKE_PARALLELISM
RC=$?
if [ $RC != 0 ]
then
    echo "cmake build(POSTGIS)"
    exit $RC
fi

if [ $CXX_HIGH != "g++" ]; then
    use_low_compiler
fi
###############################################################################



########################### Copy library And Set rpath ########################
strip_debug_file(){
    if [ $POSTGIS_BUILD_TYPE == "RelWithDebInfo" ]; then
        for i in $(ls $POSTGIS_HOME/$THIRDPARTY_DIR_NAME/${1})
        do
            objcopy --only-keep-debug "$i" "$POSTGIS_HOME/$SYMBOLS_DIR_NAME/$(basename $i).debug"
            objcopy --strip-debug "$i"
            objcopy --add-gnu-debuglink "$POSTGIS_HOME/$SYMBOLS_DIR_NAME/$(basename $i).debug" "$i"
        done
    fi
}

# clean dir
THIRDPARTY_DIR_NAME=thirdparty
SYMBOLS_DIR_NAME=symbols
rm -rf "$POSTGIS_HOME/$THIRDPARTY_DIR_NAME"
rm -rf "$POSTGIS_HOME/$SYMBOLS_DIR_NAME"
mkdir -p $POSTGIS_HOME/$THIRDPARTY_DIR_NAME
mkdir -p $POSTGIS_HOME/$SYMBOLS_DIR_NAME

# Copy POSTGIS
cp -a $POSTGIS_HOME/tmp_install/lib/*.so* $POSTGIS_HOME/$THIRDPARTY_DIR_NAME
strip_debug_file '*.so*'

# Change shared library's rpath
chrpath=$POSTGIS_HOME/../chrpath-trunk/tmp_install/bin/chrpath
for i in $(ls $POSTGIS_HOME/$THIRDPARTY_DIR_NAME/*.so*)
do
    $chrpath -r '$ORIGIN/' $i
done

echo "Build Postgis library SUCCESSFULLY!"
echo "You can find shared library at $POSTGIS_HOME/$THIRDPARTY_DIR_NAME"
###############################################################################
