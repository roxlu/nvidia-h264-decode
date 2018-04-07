# NVDECODE EXPERIMENTS

Experimenting with NVDECODE. Not production ready, purely meant
as a first look into the NVDECODE API. Leaving this on Github only
for people to have a peek at the code. This was tested on Arch Linux.

www.roxlu.com

## How to build on Windows

- Tested this with Cuda 9.1
- Make sure the *CUDA_PATH* environment variable is set.
- Make sure that _ncuvid.dll_ is stored in C:\Windows\system32
- Install cmake 
- [download and install CUDA](https://developer.nvidia.com/cuda-download) 
- [download nvidia Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk#Download). Extract the *Video Codec SDK* into:

        [repos-dir]/extern/Video_Codec_SDK/

- Open a terminal and:

        cd build
        release.bat release
         

## How to build on Linux

- Install CUDA using your package manager
- Open a terminal and:

        cd build
        ./release.sh

