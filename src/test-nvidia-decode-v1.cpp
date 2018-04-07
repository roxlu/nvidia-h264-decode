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

CUcontext context = nullptr;
CUvideodecoder decoder = nullptr;
CUdevice device = 0;

/* ------------------------------------------------ */

int main() {

  printf("\n\nnvidia decode test v1.\n\n");
  
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

  /* Create a video parser that gives us the CUVIDPICPARAMS structures. */
  CUVIDPARSERPARAMS parser_params;
  memset((void*)&parser_params, 0x00, sizeof(parser_params));
  parser_params.CodecType = cudaVideoCodec_H264;
  parser_params.ulMaxNumDecodeSurfaces = 1;
  parser_params.ulMaxDisplayDelay = 0; 
  parser_params.ulClockRate = 0;
  parser_params.ulErrorThreshold = 0;
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

  if (nullptr == decoder) {
    printf("Error: no decoder created yet, should have been done inside the sequence callback. (exiting).\n");
    exit(EXIT_FAILURE);
  }
  
  /* Cleanup */
  /* ------------------------------------------------------ */

  if (nullptr != context) {
    r = cuCtxDestroy(context);
    if (CUDA_SUCCESS != r) {
      cuGetErrorString(r, &err_str);
      printf("Failed to cleanly destroy the cuda context: %s (exiting).\n", err_str);
      exit(EXIT_FAILURE);
    }
  }

  if (nullptr != decoder) {
    r = cuvidDestroyDecoder(decoder);
    if (CUDA_SUCCESS != r) {
      cuGetErrorString(r, &err_str);
      printf("Failed to cleanly destroy the decoder context: %s. (exiting).\n", err_str);
      exit(EXIT_FAILURE);
    }
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

  const char* err_str = nullptr;
  
  if (nullptr == context) {
    printf("The CUcontext is nullptr, you should initialize it before kicking off the decoder.\n");
    exit(EXIT_FAILURE);
  }

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
  create_info.ulNumOutputSurfaces = 2;
  create_info.ulNumDecodeSurfaces = 20;   
  create_info.ulCreationFlags = cudaVideoCreate_PreferCUVID;
  create_info.vidLock = nullptr;
  create_info.ulIntraDecodeOnly = 0;
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
  
  return create_info.ulNumDecodeSurfaces;
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
  unsigned int pitch = 0;
  CUdeviceptr src_frame = 0;

  CUVIDPROCPARAMS vpp = { 0 };
  vpp.progressive_frame = info->progressive_frame;
  vpp.second_field = info->repeat_first_field + 1;
  vpp.top_field_first = info->top_field_first;
  vpp.unpaired_field = (info->repeat_first_field < 0);
  vpp.output_stream = nullptr; /* @todo do we need to set this to something? */

  r = cuvidMapVideoFrame(decoder, info->picture_index, &src_frame, &pitch, &vpp);

  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("- mapping: %u failed: %s\n", info->picture_index, err_str);
    return 0;
  }
  
  printf("+ mapping: %u succeeded\n", info->picture_index);
  
  r = cuvidUnmapVideoFrame(decoder, src_frame);
  if (CUDA_SUCCESS != r) {
    cuGetErrorString(r, &err_str);
    printf("- failed to unmap the video frame: %s, %d\n", err_str, info->picture_index);
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

