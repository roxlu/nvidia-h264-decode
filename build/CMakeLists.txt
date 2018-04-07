cmake_minimum_required(VERSION 3.4)
project(nvidia-h264-decode C CXX)

set(CMAKE_CXX_STANDARD 11)
set(bd ${CMAKE_CURRENT_LIST_DIR}/../)
set(sd ${bd}/src)

# Find CUDA which sets:
#   - CUDA_INCLUDE_DIRS
#   - CUDA_LIBRARIES
find_package(CUDA)

if (WIN32)
  list(APPEND libs
    $ENV{CUDA_PATH}/lib/x64/cuda.lib
    ${bd}/extern/Video_Codec_SDK/Samples/NvCodec/Lib/x64/nvcuvid.lib
    )
elseif(UNIX)
  list(APPEND libs
    nvcuvid
    cuda
    )
endif()

include_directories(
  ${sd}
  ${CUDA_INCLUDE_DIRS}
  ${bd}/extern/Video_Codec_SDK/Samples/NvCodec/
  )

list(APPEND libs
  ${CUDA_LIBRARIES}
  )

if (NOT EXISTS ${CMAKE_INSTALL_PREFIX}/bin/moonlight.264)
  file(DOWNLOAD http://samples.mplayerhq.hu/V-codecs/h264/moonlight.264 ${CMAKE_INSTALL_PREFIX}/bin/moonlight.264)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(debug_flag "_debug")
endif()

macro(create_test name)
  set(test_name "test-${name}${debug_flag}")
  add_executable(${test_name} ${sd}/test-${name}.cpp)
  target_link_libraries(${test_name} ${libs} )
  install(TARGETS ${test_name} DESTINATION bin/)
endmacro()

create_test("nvidia-decode-v0")
create_test("nvidia-decode-v1")
create_test("nvidia-decode-v2")
      

