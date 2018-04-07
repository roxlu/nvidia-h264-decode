/*
  NVIDIA DECODE EXPERIMENTS
  =========================
  
  GENERAL INFO:

    This repository contains a collection of experiments using
    the NVDECODE SDK to decode H264 using hardware
    acceleration. These tests are meant to be minimal and should
    not be used in production environments. The code was written 
    while diving into the APIs so things might be incorrect.

    This particular test writes the decoded YUV into a file which
    can be played back with ffplay. Though atm only videos with
    IDR only frames are working correctly.

  QUESTIONS:
  
    Q1: Should I use the CUVIDDECODECREATEINFO.vidLock .. and when? 
    A1: ...

    Q2: What are the video parser callbacks supposed to return?
    A2: ....

    Q3: When calling a cuvidCreateVideoParser(), do I need to provide `pExtVideoInfo` ? 
    A3: I tested this by setting the pExtVideoInfo member to nullptr in the cudaDecodeGL example
        and things were working fine w/o.

  REFERENCES:

    [0]: http://docs.nvidia.com/cuda/pdf/CUDA_C_Programming_Guide.pdf "Cuda C Programming Guide"
    [1]: https://github.com/gpac/gpac/blob/9bf9d23283553bf8214d13b286ce759ddd216be0/modules/nvdec/nvdec.c "GPAC implementation of NVDECODE"

 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <NvDecoder/nvcuvid.h>
#include <NvDecoder/cuviddec.h>

/* ------------------------------------------------ */

static void print_cuvid_decode_caps(CUVIDDECODECAPS* caps);
static void print_cuvid_parser_disp_info(CUVIDPARSERDISPINFO* info);
static void print_cuvid_pic_params(CUVIDPICPARAMS* pic);
static int parser_sequence_callback(void* user, CUVIDEOFORMAT* fmt);
static int parser_decode_picture_callback(void* user, CUVIDPICPARAMS* pic);
static int parser_display_picture_callback(void* user, CUVIDPARSERDISPINFO* info);

/* ------------------------------------------------ */

CUcontext context = { 0 };
CUvideodecoder decoder = nullptr;
CUdevice device = { 0 };
std::ofstream ofs;

char* yuv_buffer = nullptr;
int yuv_nbytes_needed = 0;
int coded_width = 0;
int coded_height = 0;

/* ------------------------------------------------ */

int main() {
 
  printf("\n\nnvidia decode test v1.\n\n");
  
  CUresult r = CUDA_SUCCESS;
  const char* err_str = nullptr;

  ofs.open("out.nv12", std::ios::out | std::ios::binary);
  if (!ofs.is_open()) {
    printf("Failed to open output file. (exiting).\n");
    exit(EXIT_FAILURE);
  }

  /* Initialize cuda, must be done before anything else. */
  r = cuInit(0);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to initialize cuda: %s. (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  int device_count = 0;
  r = cuDeviceGetCount(&device_count);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to get the cuda device count: %s. (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  printf("We have %d cuda device(s).\n", device_count);

  r = cuDeviceGet(&device, 0);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to get a handle to the cuda device: %s. (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  char name[80] = { 0 };
  r = cuDeviceGetName(name, sizeof(name), device);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to get the cuda device name: %s. (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  printf("Cuda device: %s.\n", name);

  r = cuCtxCreate(&context, 0, device);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to create a cuda context: %s. (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  /* Create a video parser that gives us the CUVIDPICPARAMS structures. */
  CUVIDPARSERPARAMS parser_params;
  memset((void*)&parser_params, 0x00, sizeof(parser_params));
  parser_params.CodecType = cudaVideoCodec_H264;
  parser_params.ulMaxNumDecodeSurfaces = 4;
  parser_params.ulClockRate = 0;
  parser_params.ulErrorThreshold = 0;
  parser_params.ulMaxDisplayDelay = 1;
  parser_params.pUserData = nullptr;
  parser_params.pfnSequenceCallback = parser_sequence_callback;
  parser_params.pfnDecodePicture = parser_decode_picture_callback;
  parser_params.pfnDisplayPicture = parser_display_picture_callback;

  CUvideoparser parser = nullptr;
  r = cuvidCreateVideoParser(&parser, &parser_params);
  
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to create a video parser: %s (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  /* Load our h264 nal parser. */
  std::string filename = "";
  filename = "./moonlight.264";

  /* Instead of reading the file one nal at a time, we just read a huge chunk and feed that into the decoder. */
  std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::binary);
  if (!ifs.is_open()) {
    printf("Failed to open the file: %s. (exiting).\n", filename.c_str());
    exit(EXIT_FAILURE);
  }
         
  ifs.seekg(0, std::ifstream::end);
  size_t ifs_size = ifs.tellg();
  ifs.seekg(0, std::ifstream::beg);
  printf("Loaded %s which holds %zu bytes.\n", filename.c_str(), ifs_size);
  
  char* ifs_buf = (char*)malloc(ifs_size);
  ifs.read(ifs_buf, ifs_size);

  CUVIDSOURCEDATAPACKET pkt;
  pkt.flags = 0;
  pkt.payload_size = ifs_size;
  pkt.payload = (uint8_t*)ifs_buf;
  pkt.timestamp = 0;
 
  r = cuvidParseVideoData(parser, &pkt);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to parse h264 packet: %s (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }
  
  /* Cleanup */
  /* ------------------------------------------------------ */

  printf("Cleaning up.\n");
  
  if (nullptr != parser) {
    printf("Destroying video parser.\n");
    r = cuvidDestroyVideoParser(parser);
    if (CUDA_SUCCESS != r) {
      cuGetErrorString(r, &err_str);
      printf("Failed to the video parser context: %s. (exiting).\n", err_str);
      exit(EXIT_FAILURE);
    }
  }

  if (nullptr != decoder) {
    printf("Destroying decoder.\n");
    r = cuvidDestroyDecoder(decoder);
    if (CUDA_SUCCESS != r) {
      cuGetErrorString(r, &err_str);
      printf("Failed to cleanly destroy the decoder context: %s. (exiting).\n", err_str);
      exit(EXIT_FAILURE);
    }
  }

  if (nullptr != context) {
    printf("Destroying context.\n");
    r = cuCtxDestroy(context);
    if (CUDA_SUCCESS != r) {
      cuGetErrorString(r, &err_str);
      printf("Failed to cleanly destroy the cuda context: %s (exiting).\n", err_str);
      exit(EXIT_FAILURE);
    }
    printf("Context destroyed.\n");
  }
  
  if (nullptr != yuv_buffer) {
    /* Segfaults on Win (?) */
    /*
    printf("Freeing yuv buffer.\n");
    free(yuv_buffer);
    yuv_buffer = nullptr;
    yuv_nbytes_needed = 0;
    */
  }
  
  printf("Playback with: ");
  printf("ffplay -f rawvideo -pix_fmt nv12 -s %dx%d -i out.nv12\n", coded_width, coded_height);
  
  printf("Resetting state.\n");
  context = nullptr;
  decoder = nullptr;
  parser = nullptr;
  coded_width = 0;
  coded_height = 0;

  if (ofs.is_open()) {
    ofs.close();
  }

  return 0;
}

/* ------------------------------------------------ */

static int parser_sequence_callback(void* user, CUVIDEOFORMAT* fmt) {

  if (nullptr == context) {
    printf("The CUcontext is nullptr, you should initialize it before kicking off the decoder.\n");
    exit(EXIT_FAILURE);
  }

  coded_width = fmt->coded_width;
  coded_height = fmt->coded_height;

  const char* err_str = nullptr;

  printf("CUVIDEOFORMAT.Coded size: %d x %d\n", fmt->coded_width, fmt->coded_height);
  printf("CUVIDEOFORMAT.Display area: %d %d %d %d\n", fmt->display_area.left, fmt->display_area.top, fmt->display_area.right, fmt->display_area.bottom);
  printf("CUVIDEOFORMAT.Bitrate: %u\n", fmt->bitrate);

  CUVIDDECODECAPS decode_caps;
  memset((char*)&decode_caps, 0x00, sizeof(decode_caps));
  decode_caps.eCodecType = fmt->codec;
  decode_caps.eChromaFormat = fmt->chroma_format;
  decode_caps.nBitDepthMinus8 = fmt->bit_depth_luma_minus8;

  CUresult r = cuvidGetDecoderCaps(&decode_caps);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to get decoder caps: %s (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  if (!decode_caps.bIsSupported) {
    printf("The video file format is not supported by NVDECODE. (exiting).\n");
    exit(EXIT_FAILURE);
  }

  /* Create decoder context. */
  CUVIDDECODECREATEINFO create_info = { 0 };
  create_info.CodecType = fmt->codec;
  create_info.ChromaFormat = fmt->chroma_format;
  create_info.OutputFormat = (fmt->bit_depth_luma_minus8) ? cudaVideoSurfaceFormat_P016 : cudaVideoSurfaceFormat_NV12;
  create_info.bitDepthMinus8 = fmt->bit_depth_luma_minus8;
  create_info.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
  create_info.ulNumOutputSurfaces = 1;
  create_info.ulNumDecodeSurfaces = 20;   
  create_info.ulCreationFlags = cudaVideoCreate_PreferCUVID;
  create_info.vidLock = nullptr;
  create_info.ulIntraDecodeOnly = 0; /* Set to 1 when the source only has intra frames; memory will be optimized. */
  create_info.ulTargetWidth = fmt->coded_width;
  create_info.ulTargetHeight = fmt->coded_height;
  create_info.ulWidth = fmt->coded_width;
  create_info.ulHeight = fmt->coded_height;

  cuCtxPushCurrent(context);
  {
    r = cuvidCreateDecoder(&decoder, &create_info);
    if (CUDA_SUCCESS != r) {
      cuGetErrorString(r, &err_str);
      printf("Failed to create the decoder: %s. (exiting).\n", err_str);
      exit(EXIT_FAILURE);
    }
  }
  cuCtxPopCurrent(nullptr);

  printf("Created the decoder.\n");
  
  return 0;
}

static int parser_decode_picture_callback(void* user, CUVIDPICPARAMS* pic) {
  
  CUresult r = CUDA_SUCCESS;
 
  if (nullptr == decoder) {
    printf("decoder is nullptr. (exiting).");
    exit(EXIT_FAILURE);
  }

  r = cuvidDecodePicture(decoder, pic);
  if (CUDA_SUCCESS != r) {
    printf("Failed to decode the picture.");
  }
  
  return 1;
}

static int parser_display_picture_callback(void* user, CUVIDPARSERDISPINFO* info) {

  const char* err_str = nullptr;
  CUresult r = CUDA_SUCCESS;
  CUVIDPROCPARAMS vpp = { 0 };
  unsigned int pitch = 0;
  int to_map = info->picture_index;
  CUdeviceptr device_ptr = 0;

  vpp.progressive_frame = info->progressive_frame;
  vpp.top_field_first = info->top_field_first;
  vpp.unpaired_field = (info->repeat_first_field < 0);
  vpp.second_field = 0;

  r = cuvidMapVideoFrame(decoder, to_map, &device_ptr, &pitch, &vpp);

  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("- mapping: %u failed: %s\n", to_map, err_str);
    return 0;
  }

  if (nullptr == yuv_buffer) {
    printf("Allocating yuv buffer.\n");
    yuv_nbytes_needed = (pitch * coded_height) + (pitch * coded_height * 0.5);
    r = cuMemAllocHost((void**)&yuv_buffer, yuv_nbytes_needed);
    if (CUDA_SUCCESS != r) {
      printf("Failed to allocate the buffer for the decoded yuv frames. (exiting).\n");
      exit(EXIT_FAILURE);
    }
  }

  if (nullptr == yuv_buffer
      || 0 == yuv_nbytes_needed)
    {
      printf("No valid yuf buffer. (exiting).\n");
      exit(EXIT_FAILURE);
    }
  
  r = cuMemcpyDtoH(yuv_buffer, device_ptr, yuv_nbytes_needed);
  if (CUDA_SUCCESS != r) {
    printf("Failed to copy the decode frame into our (cpu) buffer. (exiting).\n");
    exit(EXIT_FAILURE);
  }

  if (false == ofs.is_open()) {
    printf("The output file is not opened. (exiting).\n");
    exit(EXIT_FAILURE);
  }

  for (int j = 0; j < coded_height; ++j) {
    ofs.write(yuv_buffer + j * pitch, coded_width);
  }
  
  int half_height = coded_height * 0.5;
  for (int j = 0; j < half_height; ++j) {
    ofs.write(yuv_buffer + (coded_height * pitch) + j * pitch, coded_width);
  }

  ofs.flush();

  printf("+ mapping: %u succeeded, device_ptr: %d\n", to_map, device_ptr);
  
  r = cuvidUnmapVideoFrame(decoder, device_ptr);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("- failed to unmap the video frame: %s, %d\n", err_str, to_map);
    return 0;
  }
  
  return 1;
}

/* ------------------------------------------------ */

static void print_cuvid_decode_caps(CUVIDDECODECAPS* caps) {
  
  if (nullptr == caps) {
    printf("Cannot print the cuvid decode caps as the given pointer is a nullptr.");
    return;
  }

  printf("CUVIDDECODECAPS.nBitDepthMinus8: %u\n", caps->nBitDepthMinus8);
  printf("CUVIDDECODECAPS.bIsSupported: %u\n", caps->bIsSupported);
  printf("CUVIDDECODECAPS.nMaxWidth: %u\n", caps->nMaxWidth);
  printf("CUVIDDECODECAPS.nMaxHeight: %u\n", caps->nMaxHeight);
  printf("CUVIDDECODECAPS.nMaxMBCount: %u\n", caps->nMaxMBCount);
  printf("CUVIDDECODECAPS.nMinWidth: %u\n", caps->nMinWidth);
  printf("CUVIDDECODECAPS.nMinHeight: %u\n", caps->nMinHeight);
}

static void print_cuvid_parser_disp_info(CUVIDPARSERDISPINFO* info) {

  if (nullptr == info) {
    printf("Cannot print the cuvid parser disp info, nullptr given.");
    return;
  }

  printf("CUVIDPARSERDISPINFO.picture_index: %d\n", info->picture_index);
  printf("CUVIDPARSERDISPINFO.progressive_frame: %d\n", info->progressive_frame);
  printf("CUVIDPARSERDISPINFO.top_field_first: %d\n", info->top_field_first);
  printf("CUVIDPARSERDISPINFO.repeat_first_field: %d\n", info->repeat_first_field);
  printf("CUVIDPARSERDISPINFO.timestamp: %lld\n", info->timestamp);
}

static void print_cuvid_pic_params(CUVIDPICPARAMS* pic) {

  if (nullptr == pic) {
    printf("Cannot print the cuvid pic params, nullptr given.");
    return;
  }

  printf("CUVIDPICPARAMS.PicWithInMbs: %d\n", pic->PicWidthInMbs);
  printf("CUVIDPICPARAMS.FrameHeightInMbs: %d\n", pic->FrameHeightInMbs);
  printf("CUVIDPICPARAMS.CurrPicIdx: %d\n", pic->CurrPicIdx);
  printf("CUVIDPICPARAMS.field_pic_flag: %d\n", pic->field_pic_flag);
  printf("CUVIDPICPARAMS.bottom_field_flag: %d\n", pic->bottom_field_flag);
  printf("CUVIDPICPARAMS.second_field: %d\n", pic->second_field);
  printf("CUVIDPICPARAMS.nBitstreamDataLen: %u\n", pic->nBitstreamDataLen);
  printf("CUVIDPICPARAMS.nNumSlices: %u\n", pic->nNumSlices);
  printf("CUVIDPICPARAMS.ref_pic_flag: %d\n", pic->ref_pic_flag);
  printf("CUVIDPICPARAMS.intra_pic_flag: %d\n", pic->intra_pic_flag);
}

/* ------------------------------------------------ */

