#!/bin/sh
# Using gist: https://gist.github.com/roxlu/2ac1aa06222ef788f9df235a5b2fbf7c
d=${PWD}
bd=${d}/../
sd=${bd}/src/
id=${bd}/install
ed=${d}/../
rd=${d}/../reference/
d=${PWD}
is_debug="n"
build_dir="build_unix"
cmake_build_type="Release"
cmake_config="Release"
debug_flag=""
debugger=""
os_debugger=""
parallel_builds=""
cmake_generator=""

# Detect OS.
if [ "$(uname)" == "Darwin" ]; then
    if [ "${cmake_generator}" = "" ] ; then
        cmake_generator="Unix Makefiles"
    fi
    os="mac"
    os_debugger="lldb"
    parallel_builds="-j8"
elif [ "$(expr substr $(uname -s) 1 5)" = "Linux" ]; then
    if [ "${cmake_generator}" = "" ] ; then
        cmake_generator="Unix Makefiles"
    fi
    os="linux"
    os_debugger="gdb"
    parallel_builds="-j8"
else
    if [ "${cmake_generator}" = "" ] ; then
        cmake_generator="Visual Studio 15 2017 Win64"
        build_dir="build_vs2017"
    fi
    os="win"
    os_debugger="cdb"
    parallel_builds="/verbosity:q /maxcpucount:8"
fi

# Detect Command Line Options
for var in "$@"
do
    if [ "${var}" = "debug" ] ; then
        is_debug="y"
        cmake_build_type="Debug"
        cmake_config="Debug"
        debug_flag="_debug"
        debugger="${os_debugger}"
        
    elif [ "${var}" = "xcode" ] ; then
        build_dir="build_xcode"
        cmake_generator="Xcode"
        build_dir="build_xcode"
        parallel_builds=""
    fi
done

# Create unique name for this build type.
bd="${d}/${build_dir}.${cmake_build_type}"

if [ ! -d ${bd} ] ; then 
    mkdir ${bd}
fi

# Compile the library.
cd ${bd}
cmake -DCMAKE_INSTALL_PREFIX=${id} \
      -DCMAKE_BUILD_TYPE=${cmake_build_type} \
      -G "${cmake_generator}" \
      ..

if [ $? -ne 0 ] ; then
    echo "Failed to configure"
    exit
fi

cmake --build . \
      --target install \
      --config ${cmake_build_type} \
      -- ${parallel_builds} 

if [ $? -ne 0 ] ; then
    echo "Failed to build"
    exit
fi

cd ${id}/bin
# ${debugger} ./test-nvidia-decode-v0${debug_flag}
#${debugger} ./test-nvidia-decode-v1${debug_flag}
#${debugger} ./test-nvidia-decode-v2${debug_flag}
${debugger} ./test-nvidia-decode-v3${debug_flag}

