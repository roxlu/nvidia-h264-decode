# NVDECODE EXPERIMENTS

Experimenting with NVDECODE. Not production ready, purely meant
as a first look into the NVDECODE API. Leaving this on Github only
for people to have a peek at the code. This was tested on Arch Linux.

www.roxlu.com

## How to build

First [download and install CUDA](https://developer.nvidia.com/cuda-download) after which you have to 
download the [nvidia Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk#Download). 

Extract the *Video Codec SDK* into:

    [repos-dir]/extern/Video_Codec_SDK/
    
So that you have something like:

    build/
    extern/Video_Codec_SDK/ 
    extern/Video_Codec_SDK/doc
    extern/Video_Codec_SDK/Samples
    ...
    src/
