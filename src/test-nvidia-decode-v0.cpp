/*

  NVIDIA DECODE EXPERIMENTS
  =========================
  
  GENERAL INFO:

    This repository contains a collection of experiments using
    the NVDECODE SDK to decode H264 using hardware
    acceleration. These tests are meant to be minimal and should
    not be used in production environments. The code was written 
    while diving into the APIs so things might be incorrect.
  
  QUESTIONS:
  
    Q1: Should I use the CUVIDDECODECREATEINFO.vidLock .. and when? 
    A1: ...

    Q2: What are the video parser callbacks supposed to return?
    A2: ....

    Q3: When calling a cuvidCreateVideoParser(), do I need to provide `pExtVideoInfo` ? 
    A3: I tested this by setting the pExtVideoInfo member to
        nullptr in the cudaDecodeGL example and things were
        working fine w/o.

  REFERENCES:

    [0]: http://docs.nvidia.com/cuda/pdf/CUDA_C_Programming_Guide.pdf "Cuda C Programming Guide"
    [1]: https://github.com/gpac/gpac/blob/9bf9d23283553bf8214d13b286ce759ddd216be0/modules/nvdec/nvdec.c "GPAC implementation of NVDECODE"

 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

/* ------------------------------------------------ */

int main() {

  printf("\n\nNvDecoder Test v0.\n\n");
  CUresult r = CUDA_SUCCESS;
  const char* err_str = nullptr;

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

  /* Query capabilities. */
  CUVIDDECODECAPS decode_caps = {};
  decode_caps.eCodecType = cudaVideoCodec_H264;
  decode_caps.eChromaFormat = cudaVideoChromaFormat_420;
  decode_caps.nBitDepthMinus8 = 0;

  r = cuvidGetDecoderCaps(&decode_caps);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to get decoder caps: %s (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }
  
  /* Create decoder context. */
  CUVIDDECODECREATEINFO create_info = { 0 };
  create_info.CodecType = decode_caps.eCodecType;                    /* cudaVideoCodex_XXX */
  create_info.ChromaFormat = decode_caps.eChromaFormat;              /* cudaVideoChromaFormat_XXX */
  create_info.OutputFormat = cudaVideoSurfaceFormat_NV12;            /* cudaVideoSurfaceFormat_XXX */
  create_info.ulCreationFlags = cudaVideoCreate_PreferCUVID;         /* cudaVideoCreate_XXX */
  create_info.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;      /* cudaVideoDeinterlaceMode_XXX */
  create_info.bitDepthMinus8 = decode_caps.nBitDepthMinus8;;
  create_info.ulNumOutputSurfaces = 2;                               /* Maximum number of internal decode surfaces. */
  create_info.ulNumDecodeSurfaces = 4;                               /* @todo from NvDecoder.cpp, assuming worst case here ... Maximum number of internal decode surfaces. */
  create_info.ulIntraDecodeOnly = 0;                                 /* @todo this seems like an interesting flag. */

  /* Size is specific for the moonlight.264 file. */
  create_info.ulWidth = 512;                                        /* Coded sequence width in pixels. */
  create_info.ulHeight = 384;                                       /* Coded sequence height in pixels. */
  create_info.ulTargetWidth = create_info.ulWidth;                   /* Post-processed output width (should be aligned to 2). */
  create_info.ulTargetHeight = create_info.ulHeight;                 /* Post-processed output height (should be aligned to 2). */

  
  /* @todo do we need this? */
  /* create_info.vidLock = ...*/

  r = cuvidCreateDecoder(&decoder, &create_info);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to create the decoder: %s. (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  /* Create a video parser that gives us the CUVIDPICPARAMS structures. */
  CUVIDPARSERPARAMS parser_params;
  memset((void*)&parser_params, 0x00, sizeof(parser_params));
  parser_params.CodecType = create_info.CodecType;
  parser_params.ulMaxNumDecodeSurfaces = create_info.ulNumDecodeSurfaces;
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
    printf("Failed to open the input .h264 file: %s. (exiting).\n", filename.c_str());
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

  r = cuCtxDestroy(context);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to cleanly destroy the cuda context: %s (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  r = cuvidDestroyDecoder(decoder);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("Failed to cleanly destroy the decoder context: %s. (exiting).\n", err_str);
    exit(EXIT_FAILURE);
  }

  if (nullptr != parser) {
    r = cuvidDestroyVideoParser(parser);
    if (CUDA_SUCCESS != r) {
      cuGetErrorString(r, &err_str);
      printf("Failed to the video parser context: %s. (exiting).\n", err_str);
      exit(EXIT_FAILURE);
    }
  }

  context = nullptr;
  decoder = nullptr;
  parser = nullptr;

  return 0;
}

/* ------------------------------------------------ */

static int parser_sequence_callback(void* user, CUVIDEOFORMAT* fmt) {
  printf("CUVIDEOFORMAT.Coded size: %d x %d\n", fmt->coded_width, fmt->coded_height);
  printf("CUVIDEOFORMAT.Display area: %d %d %d %d\n", fmt->display_area.left, fmt->display_area.top, fmt->display_area.right, fmt->display_area.bottom);
  printf("CUVIDEOFORMAT.Bitrate: %u\n", fmt->bitrate);
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

  vpp.progressive_frame = info->progressive_frame;
  vpp.top_field_first = info->top_field_first;
  vpp.unpaired_field = (info->repeat_first_field < 0);
  vpp.second_field = 0;

  r = cuvidMapVideoFrame(decoder, to_map, (unsigned long long*)&device, &pitch, &vpp);

  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("- mapping: %u failed: %s\n", to_map, err_str);
    return 0;
  }
  
  printf("+ mapping: %u succeeded\n", to_map);
  
  r = cuvidUnmapVideoFrame(decoder, (unsigned long long)device);
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

