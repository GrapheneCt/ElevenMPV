/*
FLAC audio decoder. Choice of public domain or MIT-0. See license statements at the end of this file.
dr_flac - v0.12.2 - 2019-10-07

David Reid - mackron@gmail.com
*/

/*
RELEASE NOTES - v0.12.0
=======================
Version 0.12.0 has breaking API changes including changes to the existing API and the removal of deprecated APIs.


Improved Client-Defined Memory Allocation
-----------------------------------------
The main change with this release is the addition of a more flexible way of implementing custom memory allocation routines. The
existing system of DRFLAC_MALLOC, DRFLAC_REALLOC and DRFLAC_FREE are still in place and will be used by default when no custom
allocation callbacks are specified.

To use the new system, you pass in a pointer to a drflac_allocation_callbacks object to drflac_open() and family, like this:

    void* my_malloc(size_t sz, void* pUserData)
    {
        return malloc(sz);
    }
    void* my_realloc(void* p, size_t sz, void* pUserData)
    {
        return realloc(p, sz);
    }
    void my_free(void* p, void* pUserData)
    {
        free(p);
    }

    ...

    drflac_allocation_callbacks allocationCallbacks;
    allocationCallbacks.pUserData = &myData;
    allocationCallbacks.onMalloc  = my_malloc;
    allocationCallbacks.onRealloc = my_realloc;
    allocationCallbacks.onFree    = my_free;
    drflac* pFlac = drflac_open_file("my_file.flac", &allocationCallbacks);

The advantage of this new system is that it allows you to specify user data which will be passed in to the allocation routines.

Passing in null for the allocation callbacks object will cause dr_flac to use defaults which is the same as DRFLAC_MALLOC,
DRFLAC_REALLOC and DRFLAC_FREE and the equivalent of how it worked in previous versions.

Every API that opens a drflac object now takes this extra parameter. These include the following:

    drflac_open()
    drflac_open_relaxed()
    drflac_open_with_metadata()
    drflac_open_with_metadata_relaxed()
    drflac_open_file()
    drflac_open_file_with_metadata()
    drflac_open_memory()
    drflac_open_memory_with_metadata()
    drflac_open_and_read_pcm_frames_s32()
    drflac_open_and_read_pcm_frames_s16()
    drflac_open_and_read_pcm_frames_f32()
    drflac_open_file_and_read_pcm_frames_s32()
    drflac_open_file_and_read_pcm_frames_s16()
    drflac_open_file_and_read_pcm_frames_f32()
    drflac_open_memory_and_read_pcm_frames_s32()
    drflac_open_memory_and_read_pcm_frames_s16()
    drflac_open_memory_and_read_pcm_frames_f32()



Optimizations
-------------
Seeking performance has been greatly improved. A new binary search based seeking algorithm has been introduced which significantly
improves performance over the brute force method which was used when no seek table was present. Seek table based seeking also takes
advantage of the new binary search seeking system to further improve performance there as well. Note that this depends on CRC which
means it will be disabled when DR_FLAC_NO_CRC is used.

The SSE4.1 pipeline has been cleaned up and optimized. You should see some improvements with decoding speed of 24-bit files in
particular. 16-bit streams should also see some improvement.

drflac_read_pcm_frames_s16() has been optimized. Previously this sat on top of drflac_read_pcm_frames_s32() and performed it's s32
to s16 conversion in a second pass. This is now all done in a single pass. This includes SSE2 and ARM NEON optimized paths.

A minor optimization has been implemented for drflac_read_pcm_frames_s32(). This will now use an SSE2 optimized pipeline for stereo
channel reconstruction which is the last part of the decoding process.

The ARM build has seen a few improvements. The CLZ (count leading zeroes) and REV (byte swap) instructions are now used when
compiling with GCC and Clang which is achieved using inline assembly. The CLZ instruction requires ARM architecture version 5 at
compile time and the REV instruction requires ARM architecture version 6.

An ARM NEON optimized pipeline has been implemented. To enable this you'll need to add -mfpu=neon to the command line when compiling.


Removed APIs
------------
The following APIs were deprecated in version 0.11.0 and have been completely removed in version 0.12.0:

    drflac_read_s32()                   -> drflac_read_pcm_frames_s32()
    drflac_read_s16()                   -> drflac_read_pcm_frames_s16()
    drflac_read_f32()                   -> drflac_read_pcm_frames_f32()
    drflac_seek_to_sample()             -> drflac_seek_to_pcm_frame()
    drflac_open_and_decode_s32()        -> drflac_open_and_read_pcm_frames_s32()
    drflac_open_and_decode_s16()        -> drflac_open_and_read_pcm_frames_s16()
    drflac_open_and_decode_f32()        -> drflac_open_and_read_pcm_frames_f32()
    drflac_open_and_decode_file_s32()   -> drflac_open_file_and_read_pcm_frames_s32()
    drflac_open_and_decode_file_s16()   -> drflac_open_file_and_read_pcm_frames_s16()
    drflac_open_and_decode_file_f32()   -> drflac_open_file_and_read_pcm_frames_f32()
    drflac_open_and_decode_memory_s32() -> drflac_open_memory_and_read_pcm_frames_s32()
    drflac_open_and_decode_memory_s16() -> drflac_open_memory_and_read_pcm_frames_s16()
    drflac_open_and_decode_memory_f32() -> drflac_open_memroy_and_read_pcm_frames_f32()

Prior versions of dr_flac operated on a per-sample basis whereas now it operates on PCM frames. The removed APIs all relate
to the old per-sample APIs. You now need to use the "pcm_frame" versions.
*/


/*
USAGE
=====
dr_flac is a single-file library. To use it, do something like the following in one .c file.

    #define DR_FLAC_IMPLEMENTATION
    #include "dr_flac.h"

You can then #include this file in other parts of the program as you would with any other header file. To decode audio data,
do something like the following:

    drflac* pFlac = drflac_open_file("MySong.flac", NULL);
    if (pFlac == NULL) {
        // Failed to open FLAC file
    }

    drflac_int32* pSamples = malloc(pFlac->totalPCMFrameCount * pFlac->channels * sizeof(drflac_int32));
    drflac_uint64 numberOfInterleavedSamplesActuallyRead = drflac_read_pcm_frames_s32(pFlac, pFlac->totalPCMFrameCount, pSamples);

The drflac object represents the decoder. It is a transparent type so all the information you need, such as the number of
channels and the bits per sample, should be directly accessible - just make sure you don't change their values. Samples are
always output as interleaved signed 32-bit PCM. In the example above a native FLAC stream was opened, however dr_flac has
seamless support for Ogg encapsulated FLAC streams as well.

You do not need to decode the entire stream in one go - you just specify how many samples you'd like at any given time and
the decoder will give you as many samples as it can, up to the amount requested. Later on when you need the next batch of
samples, just call it again. Example:

    while (drflac_read_pcm_frames_s32(pFlac, chunkSizeInPCMFrames, pChunkSamples) > 0) {
        do_something();
    }

You can seek to a specific sample with drflac_seek_to_sample(). The given sample is based on interleaving. So for example,
if you were to seek to the sample at index 0 in a stereo stream, you'll be seeking to the first sample of the left channel.
The sample at index 1 will be the first sample of the right channel. The sample at index 2 will be the second sample of the
left channel, etc.


If you just want to quickly decode an entire FLAC file in one go you can do something like this:

    unsigned int channels;
    unsigned int sampleRate;
    drflac_uint64 totalPCMFrameCount;
    drflac_int32* pSampleData = drflac_open_file_and_read_pcm_frames_s32("MySong.flac", &channels, &sampleRate, &totalPCMFrameCount, NULL);
    if (pSampleData == NULL) {
        // Failed to open and decode FLAC file.
    }

    ...

    drflac_free(pSampleData);


You can read samples as signed 16-bit integer and 32-bit floating-point PCM with the *_s16() and *_f32() family of APIs
respectively, but note that these should be considered lossy.


If you need access to metadata (album art, etc.), use drflac_open_with_metadata(), drflac_open_file_with_metdata() or
drflac_open_memory_with_metadata(). The rationale for keeping these APIs separate is that they're slightly slower than the
normal versions and also just a little bit harder to use.

dr_flac reports metadata to the application through the use of a callback, and every metadata block is reported before
drflac_open_with_metdata() returns.


The main opening APIs (drflac_open(), etc.) will fail if the header is not present. The presents a problem in certain
scenarios such as broadcast style streams or internet radio where the header may not be present because the user has
started playback mid-stream. To handle this, use the relaxed APIs: drflac_open_relaxed() and drflac_open_with_metadata_relaxed().

It is not recommended to use these APIs for file based streams because a missing header would usually indicate a
corrupt or perverse file. In addition, these APIs can take a long time to initialize because they may need to spend
a lot of time finding the first frame.



OPTIONS
=======
#define these options before including this file.

#define DR_FLAC_NO_STDIO
  Disable drflac_open_file() and family.

#define DR_FLAC_NO_OGG
  Disables support for Ogg/FLAC streams.

#define DR_FLAC_BUFFER_SIZE <number>
  Defines the size of the internal buffer to store data from onRead(). This buffer is used to reduce the number of calls
  back to the client for more data. Larger values means more memory, but better performance. My tests show diminishing
  returns after about 4KB (which is the default). Consider reducing this if you have a very efficient implementation of
  onRead(), or increase it if it's very inefficient. Must be a multiple of 8.

#define DR_FLAC_NO_CRC
  Disables CRC checks. This will offer a performance boost when CRC is unnecessary. This will disable binary search seeking.
  When seeking, the seek table will be used if available. Otherwise the seek will be performed using brute force.

#define DR_FLAC_NO_SIMD
  Disables SIMD optimizations (SSE on x86/x64 architectures, NEON on ARM architectures). Use this if you are having
  compatibility issues with your compiler.



QUICK NOTES
===========
- dr_flac does not currently support changing the sample rate nor channel count mid stream.
- This has not been tested on big-endian architectures.
- dr_flac is not thread-safe, but its APIs can be called from any thread so long as you do your own synchronization.
- When using Ogg encapsulation, a corrupted metadata block will result in drflac_open_with_metadata() and drflac_open()
  returning inconsistent samples.
*/

#include <stddef.h>
#include <stdint.h>
#include <kernel.h>
#include <paf/stdc.h>

#define malloc sce_paf_malloc
#define free sce_paf_free
#define realloc sce_paf_realloc
#define fopen sce_paf_fopen
#define fclose sce_paf_fclose
#define fread sce_paf_fread
#define fseek sce_paf_fseek
#define ftell sce_paf_ftell
#define fgetc sce_paf_fgetc
#define ferror { return 0; }

typedef int8_t           drflac_int8;
typedef uint8_t          drflac_uint8;
typedef int16_t          drflac_int16;
typedef uint16_t         drflac_uint16;
typedef int32_t          drflac_int32;
typedef uint32_t         drflac_uint32;
typedef int64_t          drflac_int64;
typedef uint64_t         drflac_uint64;
typedef drflac_uint8     drflac_bool8;
typedef drflac_uint32    drflac_bool32;
#define DRFLAC_TRUE      1
#define DRFLAC_FALSE     0

#define DRFLAC_DEPRECATED
//#define DR_FLAC_NO_STDIO

/*
As data is read from the client it is placed into an internal buffer for fast access. This controls the
size of that buffer. Larger values means more speed, but also more memory. In my testing there is diminishing
returns after about 4KB, but you can fiddle with this to suit your own needs. Must be a multiple of 8.
*/
#ifndef DR_FLAC_BUFFER_SIZE
#define DR_FLAC_BUFFER_SIZE   4096
#endif

typedef drflac_uint32 drflac_cache_t;

/* The various metadata block types. */
#define DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO       0
#define DRFLAC_METADATA_BLOCK_TYPE_PADDING          1
#define DRFLAC_METADATA_BLOCK_TYPE_APPLICATION      2
#define DRFLAC_METADATA_BLOCK_TYPE_SEEKTABLE        3
#define DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT   4
#define DRFLAC_METADATA_BLOCK_TYPE_CUESHEET         5
#define DRFLAC_METADATA_BLOCK_TYPE_PICTURE          6
#define DRFLAC_METADATA_BLOCK_TYPE_INVALID          127

/* The various picture types specified in the PICTURE block. */
#define DRFLAC_PICTURE_TYPE_OTHER                   0
#define DRFLAC_PICTURE_TYPE_FILE_ICON               1
#define DRFLAC_PICTURE_TYPE_OTHER_FILE_ICON         2
#define DRFLAC_PICTURE_TYPE_COVER_FRONT             3
#define DRFLAC_PICTURE_TYPE_COVER_BACK              4
#define DRFLAC_PICTURE_TYPE_LEAFLET_PAGE            5
#define DRFLAC_PICTURE_TYPE_MEDIA                   6
#define DRFLAC_PICTURE_TYPE_LEAD_ARTIST             7
#define DRFLAC_PICTURE_TYPE_ARTIST                  8
#define DRFLAC_PICTURE_TYPE_CONDUCTOR               9
#define DRFLAC_PICTURE_TYPE_BAND                    10
#define DRFLAC_PICTURE_TYPE_COMPOSER                11
#define DRFLAC_PICTURE_TYPE_LYRICIST                12
#define DRFLAC_PICTURE_TYPE_RECORDING_LOCATION      13
#define DRFLAC_PICTURE_TYPE_DURING_RECORDING        14
#define DRFLAC_PICTURE_TYPE_DURING_PERFORMANCE      15
#define DRFLAC_PICTURE_TYPE_SCREEN_CAPTURE          16
#define DRFLAC_PICTURE_TYPE_BRIGHT_COLORED_FISH     17
#define DRFLAC_PICTURE_TYPE_ILLUSTRATION            18
#define DRFLAC_PICTURE_TYPE_BAND_LOGOTYPE           19
#define DRFLAC_PICTURE_TYPE_PUBLISHER_LOGOTYPE      20

typedef enum
{
    drflac_container_native,
    drflac_container_ogg,
    drflac_container_unknown
} drflac_container;

typedef enum
{
    drflac_seek_origin_start,
    drflac_seek_origin_current
} drflac_seek_origin;

/* Packing is important on this structure because we map this directly to the raw data within the SEEKTABLE metadata block. */
#pragma pack(2)
typedef struct
{
    drflac_uint64 firstPCMFrame;
    drflac_uint64 flacFrameOffset;   /* The offset from the first byte of the header of the first frame. */
    drflac_uint16 pcmFrameCount;
} drflac_seekpoint;
#pragma pack()

typedef struct
{
    drflac_uint16 minBlockSizeInPCMFrames;
    drflac_uint16 maxBlockSizeInPCMFrames;
    drflac_uint32 minFrameSizeInPCMFrames;
    drflac_uint32 maxFrameSizeInPCMFrames;
    drflac_uint32 sampleRate;
    drflac_uint8  channels;
    drflac_uint8  bitsPerSample;
    drflac_uint64 totalPCMFrameCount;
    drflac_uint8  md5[16];
} drflac_streaminfo;

typedef struct
{
    /* The metadata type. Use this to know how to interpret the data below. */
    drflac_uint32 type;

    /*
    A pointer to the raw data. This points to a temporary buffer so don't hold on to it. It's best to
    not modify the contents of this buffer. Use the structures below for more meaningful and structured
    information about the metadata. It's possible for this to be null.
    */
    const void* pRawData;

    /* The size in bytes of the block and the buffer pointed to by pRawData if it's non-NULL. */
    drflac_uint32 rawDataSize;

    union
    {
        drflac_streaminfo streaminfo;

        struct
        {
            int unused;
        } padding;

        struct
        {
            drflac_uint32 id;
            const void* pData;
            drflac_uint32 dataSize;
        } application;

        struct
        {
            drflac_uint32 seekpointCount;
            const drflac_seekpoint* pSeekpoints;
        } seektable;

        struct
        {
            drflac_uint32 vendorLength;
            const char* vendor;
            drflac_uint32 commentCount;
            const void* pComments;
        } vorbis_comment;

        struct
        {
            char catalog[128];
            drflac_uint64 leadInSampleCount;
            drflac_bool32 isCD;
            drflac_uint8 trackCount;
            const void* pTrackData;
        } cuesheet;

        struct
        {
            drflac_uint32 type;
            drflac_uint32 mimeLength;
            const char* mime;
            drflac_uint32 descriptionLength;
            const char* description;
            drflac_uint32 width;
            drflac_uint32 height;
            drflac_uint32 colorDepth;
            drflac_uint32 indexColorCount;
            drflac_uint32 pictureDataSize;
            const drflac_uint8* pPictureData;
        } picture;
    } data;
} drflac_metadata;


/*
Callback for when data needs to be read from the client.

pUserData   [in]  The user data that was passed to drflac_open() and family.
pBufferOut  [out] The output buffer.
bytesToRead [in]  The number of bytes to read.

Returns the number of bytes actually read.

A return value of less than bytesToRead indicates the end of the stream. Do _not_ return from this callback until
either the entire bytesToRead is filled or you have reached the end of the stream.
*/
typedef size_t (* drflac_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

/*
Callback for when data needs to be seeked.

pUserData [in] The user data that was passed to drflac_open() and family.
offset    [in] The number of bytes to move, relative to the origin. Will never be negative.
origin    [in] The origin of the seek - the current position or the start of the stream.

Returns whether or not the seek was successful.

The offset will never be negative. Whether or not it is relative to the beginning or current position is determined
by the "origin" parameter which will be either drflac_seek_origin_start or drflac_seek_origin_current.

When seeking to a PCM frame using drflac_seek_to_pcm_frame(), dr_flac may call this with an offset beyond the end of
the FLAC stream. This needs to be detected and handled by returning DRFLAC_FALSE.
*/
typedef drflac_bool32 (* drflac_seek_proc)(void* pUserData, int offset, drflac_seek_origin origin);

/*
Callback for when a metadata block is read.

pUserData [in] The user data that was passed to drflac_open() and family.
pMetadata [in] A pointer to a structure containing the data of the metadata block.

Use pMetadata->type to determine which metadata block is being handled and how to read the data.
*/
typedef void (* drflac_meta_proc)(void* pUserData, drflac_metadata* pMetadata);


typedef struct
{
    void* pUserData;
    void* (* onMalloc)(size_t sz, void* pUserData);
    void* (* onRealloc)(void* p, size_t sz, void* pUserData);
    void  (* onFree)(void* p, void* pUserData);
} drflac_allocation_callbacks;

/* Structure for internal use. Only used for decoders opened with drflac_open_memory. */
typedef struct
{
    const drflac_uint8* data;
    size_t dataSize;
    size_t currentReadPos;
} drflac__memory_stream;

/* Structure for internal use. Used for bit streaming. */
typedef struct
{
    /* The function to call when more data needs to be read. */
    drflac_read_proc onRead;

    /* The function to call when the current read position needs to be moved. */
    drflac_seek_proc onSeek;

    /* The user data to pass around to onRead and onSeek. */
    void* pUserData;


    /*
    The number of unaligned bytes in the L2 cache. This will always be 0 until the end of the stream is hit. At the end of the
    stream there will be a number of bytes that don't cleanly fit in an L1 cache line, so we use this variable to know whether
    or not the bistreamer needs to run on a slower path to read those last bytes. This will never be more than sizeof(drflac_cache_t).
    */
    size_t unalignedByteCount;

    /* The content of the unaligned bytes. */
    drflac_cache_t unalignedCache;

    /* The index of the next valid cache line in the "L2" cache. */
    drflac_uint32 nextL2Line;

    /* The number of bits that have been consumed by the cache. This is used to determine how many valid bits are remaining. */
    drflac_uint32 consumedBits;

    /*
    The cached data which was most recently read from the client. There are two levels of cache. Data flows as such:
    Client -> L2 -> L1. The L2 -> L1 movement is aligned and runs on a fast path in just a few instructions.
    */
	drflac_cache_t* cacheL2;
    drflac_cache_t cache;

    /*
    CRC-16. This is updated whenever bits are read from the bit stream. Manually set this to 0 to reset the CRC. For FLAC, this
    is reset to 0 at the beginning of each frame.
    */
    drflac_uint16 crc16;
    drflac_cache_t crc16Cache;              /* A cache for optimizing CRC calculations. This is filled when when the L1 cache is reloaded. */
    drflac_uint32 crc16CacheIgnoredBytes;   /* The number of bytes to ignore when updating the CRC-16 from the CRC-16 cache. */
} drflac_bs;

typedef struct
{
    /* The type of the subframe: SUBFRAME_CONSTANT, SUBFRAME_VERBATIM, SUBFRAME_FIXED or SUBFRAME_LPC. */
    drflac_uint8 subframeType;

    /* The number of wasted bits per sample as specified by the sub-frame header. */
    drflac_uint8 wastedBitsPerSample;

    /* The order to use for the prediction stage for SUBFRAME_FIXED and SUBFRAME_LPC. */
    drflac_uint8 lpcOrder;

    /* A pointer to the buffer containing the decoded samples in the subframe. This pointer is an offset from drflac::pExtraData. */
    drflac_int32* pSamplesS32;
} drflac_subframe;

typedef struct
{
    /*
    If the stream uses variable block sizes, this will be set to the index of the first PCM frame. If fixed block sizes are used, this will
    always be set to 0.
    */
    drflac_uint64 pcmFrameNumber;

    /* If the stream uses fixed block sizes, this will be set to the frame number. If variable block sizes are used, this will always be 0. */
    drflac_uint32 flacFrameNumber;

    /* The sample rate of this frame. */
    drflac_uint32 sampleRate;

    /* The number of PCM frames in each sub-frame within this frame. */
    drflac_uint16 blockSizeInPCMFrames;

    /*
    The channel assignment of this frame. This is not always set to the channel count. If interchannel decorrelation is being used this
    will be set to DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE, DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE or DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE.
    */
    drflac_uint8 channelAssignment;

    /* The number of bits per sample within this frame. */
    drflac_uint8 bitsPerSample;

    /* The frame's CRC. */
    drflac_uint8 crc8;
} drflac_frame_header;

typedef struct
{
    /* The header. */
    drflac_frame_header header;

    /*
    The number of PCM frames left to be read in this FLAC frame. This is initially set to the block size. As PCM frames are read,
    this will be decremented. When it reaches 0, the decoder will see this frame as fully consumed and load the next frame.
    */
    drflac_uint32 pcmFramesRemaining;

    /* The list of sub-frames within the frame. There is one sub-frame for each channel, and there's a maximum of 8 channels. */
    drflac_subframe subframes[8];
} drflac_frame;

typedef struct
{
    /* The function to call when a metadata block is read. */
    drflac_meta_proc onMeta;

    /* The user data posted to the metadata callback function. */
    void* pUserDataMD;

    /* Memory allocation callbacks. */
    drflac_allocation_callbacks allocationCallbacks;


    /* The sample rate. Will be set to something like 44100. */
    drflac_uint32 sampleRate;

    /*
    The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc. Maximum 8. This is set based on the
    value specified in the STREAMINFO block.
    */
    drflac_uint8 channels;

    /* The bits per sample. Will be set to something like 16, 24, etc. */
    drflac_uint8 bitsPerSample;

    /* The maximum block size, in samples. This number represents the number of samples in each channel (not combined). */
    drflac_uint16 maxBlockSizeInPCMFrames;

    /*
    The total number of PCM Frames making up the stream. Can be 0 in which case it's still a valid stream, but just means
    the total PCM frame count is unknown. Likely the case with streams like internet radio.
    */
    drflac_uint64 totalPCMFrameCount;


    /* The container type. This is set based on whether or not the decoder was opened from a native or Ogg stream. */
    drflac_container container;

    /* The number of seekpoints in the seektable. */
    drflac_uint32 seekpointCount;


    /* Information about the frame the decoder is currently sitting on. */
    drflac_frame currentFLACFrame;


    /* The index of the PCM frame the decoder is currently sitting on. This is only used for seeking. */
    drflac_uint64 currentPCMFrame;

    /* The position of the first FLAC frame in the stream. This is only ever used for seeking. */
    drflac_uint64 firstFLACFramePosInBytes;


    /* A hack to avoid a malloc() when opening a decoder with drflac_open_memory(). */
    drflac__memory_stream memoryStream;


    /* A pointer to the decoded sample data. This is an offset of pExtraData. */
    drflac_int32* pDecodedSamples;

    /* A pointer to the seek table. This is an offset of pExtraData, or NULL if there is no seek table. */
    drflac_seekpoint* pSeekpoints;

    /* Internal use only. Only used with Ogg containers. Points to a drflac_oggbs object. This is an offset of pExtraData. */
    void* _oggbs;

    /* Internal use only. Used for profiling and testing different seeking modes. */
    drflac_bool32 _noSeekTableSeek    : 1;
    drflac_bool32 _noBinarySearchSeek : 1;
    drflac_bool32 _noBruteForceSeek   : 1;

    /* The bit streamer. The raw FLAC data is fed through this object. */
    drflac_bs bs;

    /* Variable length extra data. We attach this to the end of the object so we can avoid unnecessary mallocs. */
    drflac_uint8 pExtraData[1];
} drflac;

/*
Opens a FLAC decoder.

onRead               [in]           The function to call when data needs to be read from the client.
onSeek               [in]           The function to call when the read position of the client data needs to move.
pUserData            [in, optional] A pointer to application defined data that will be passed to onRead and onSeek.
pAllocationCallbacks [in, optional] A pointer to application defined callbacks for managing memory allocations.

Returns a pointer to an object representing the decoder.

Close the decoder with drflac_close().

pAllocationCallbacks can be NULL in which case it will use DRFLAC_MALLOC, DRFLAC_REALLOC and DRFLAC_FREE.

This function will automatically detect whether or not you are attempting to open a native or Ogg encapsulated
FLAC, both of which should work seamlessly without any manual intervention. Ogg encapsulation also works with
multiplexed streams which basically means it can play FLAC encoded audio tracks in videos.

This is the lowest level function for opening a FLAC stream. You can also use drflac_open_file() and drflac_open_memory()
to open the stream from a file or from a block of memory respectively.

The STREAMINFO block must be present for this to succeed. Use drflac_open_relaxed() to open a FLAC stream where
the header may not be present.

See also: drflac_open_file(), drflac_open_memory(), drflac_open_with_metadata(), drflac_close()
*/
drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks);

/*
The same as drflac_open(), except attempts to open the stream even when a header block is not present.

Because the header is not necessarily available, the caller must explicitly define the container (Native or Ogg). Do
not set this to drflac_container_unknown - that is for internal use only.

Opening in relaxed mode will continue reading data from onRead until it finds a valid frame. If a frame is never
found it will continue forever. To abort, force your onRead callback to return 0, which dr_flac will use as an
indicator that the end of the stream was found.
*/
drflac* drflac_open_relaxed(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_container container, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks);

/*
Opens a FLAC decoder and notifies the caller of the metadata chunks (album art, etc.).

onRead               [in]           The function to call when data needs to be read from the client.
onSeek               [in]           The function to call when the read position of the client data needs to move.
onMeta               [in]           The function to call for every metadata block.
pUserData            [in, optional] A pointer to application defined data that will be passed to onRead, onSeek and onMeta.
pAllocationCallbacks [in, optional] A pointer to application defined callbacks for managing memory allocations.

Returns a pointer to an object representing the decoder.

Close the decoder with drflac_close().

pAllocationCallbacks can be NULL in which case it will use DRFLAC_MALLOC, DRFLAC_REALLOC and DRFLAC_FREE.

This is slower than drflac_open(), so avoid this one if you don't need metadata. Internally, this will allocate and free
memory on the heap for every metadata block except for STREAMINFO and PADDING blocks.

The caller is notified of the metadata via the onMeta callback. All metadata blocks will be handled before the function
returns.

The STREAMINFO block must be present for this to succeed. Use drflac_open_with_metadata_relaxed() to open a FLAC
stream where the header may not be present.

Note that this will behave inconsistently with drflac_open() if the stream is an Ogg encapsulated stream and a metadata
block is corrupted. This is due to the way the Ogg stream recovers from corrupted pages. When drflac_open_with_metadata()
is being used, the open routine will try to read the contents of the metadata block, whereas drflac_open() will simply
seek past it (for the sake of efficiency). This inconsistency can result in different samples being returned depending on
whether or not the stream is being opened with metadata.

See also: drflac_open_file_with_metadata(), drflac_open_memory_with_metadata(), drflac_open(), drflac_close()
*/
drflac* drflac_open_with_metadata(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks);

/*
The same as drflac_open_with_metadata(), except attempts to open the stream even when a header block is not present.

See also: drflac_open_with_metadata(), drflac_open_relaxed()
*/
drflac* drflac_open_with_metadata_relaxed(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, drflac_container container, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks);

/*
Closes the given FLAC decoder.

pFlac [in] The decoder to close.

This will destroy the decoder object.
*/
void drflac_close(drflac* pFlac);


/*
Reads sample data from the given FLAC decoder, output as interleaved signed 32-bit PCM.

pFlac        [in]            The decoder.
framesToRead [in]            The number of PCM frames to read.
pBufferOut   [out, optional] A pointer to the buffer that will receive the decoded samples.

Returns the number of PCM frames actually read.

pBufferOut can be null, in which case the call will act as a seek, and the return value will be the number of frames
seeked.
*/
drflac_uint64 drflac_read_pcm_frames_s32(drflac* pFlac, drflac_uint64 framesToRead, drflac_int32* pBufferOut);

/*
Same as drflac_read_pcm_frames_s32(), except outputs samples as 16-bit integer PCM rather than 32-bit.

Note that this is lossy for streams where the bits per sample is larger than 16.
*/
drflac_uint64 drflac_read_pcm_frames_s16(drflac* pFlac, drflac_uint64 framesToRead, drflac_int16* pBufferOut);

/*
Same as drflac_read_pcm_frames_s32(), except outputs samples as 32-bit floating-point PCM.

Note that this should be considered lossy due to the nature of floating point numbers not being able to exactly
represent every possible number.
*/
drflac_uint64 drflac_read_pcm_frames_f32(drflac* pFlac, drflac_uint64 framesToRead, float* pBufferOut);

/*
Seeks to the PCM frame at the given index.

pFlac         [in] The decoder.
pcmFrameIndex [in] The index of the PCM frame to seek to. See notes below.

Returns DRFLAC_TRUE if successful; DRFLAC_FALSE otherwise.
*/
drflac_bool32 drflac_seek_to_pcm_frame(drflac* pFlac, drflac_uint64 pcmFrameIndex);



#ifndef DR_FLAC_NO_STDIO
/*
Opens a FLAC decoder from the file at the given path.

filename             [in]           The path of the file to open, either absolute or relative to the current directory.
pAllocationCallbacks [in, optional] A pointer to application defined callbacks for managing memory allocations.

Returns a pointer to an object representing the decoder.

Close the decoder with drflac_close().

This will hold a handle to the file until the decoder is closed with drflac_close(). Some platforms will restrict the
number of files a process can have open at any given time, so keep this mind if you have many decoders open at the
same time.

See also: drflac_open(), drflac_open_file_with_metadata(), drflac_close()
*/
drflac* drflac_open_file(const char* filename, const drflac_allocation_callbacks* pAllocationCallbacks);

/*
Opens a FLAC decoder from the file at the given path and notifies the caller of the metadata chunks (album art, etc.)

Look at the documentation for drflac_open_with_metadata() for more information on how metadata is handled.
*/
drflac* drflac_open_file_with_metadata(const char* filename, drflac_meta_proc onMeta, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks);
#endif

/*
Opens a FLAC decoder from a pre-allocated block of memory

This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
the lifetime of the decoder.
*/
drflac* drflac_open_memory(const void* data, size_t dataSize, const drflac_allocation_callbacks* pAllocationCallbacks);

/*
Opens a FLAC decoder from a pre-allocated block of memory and notifies the caller of the metadata chunks (album art, etc.)

Look at the documentation for drflac_open_with_metadata() for more information on how metadata is handled.
*/
drflac* drflac_open_memory_with_metadata(const void* data, size_t dataSize, drflac_meta_proc onMeta, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks);



/* High Level APIs */

/*
Opens a FLAC stream from the given callbacks and fully decodes it in a single operation. The return value is a
pointer to the sample data as interleaved signed 32-bit PCM. The returned data must be freed with drflac_free().

You can pass in custom memory allocation callbacks via the pAllocationCallbacks parameter. This can be NULL in which
case it will use DRFLAC_MALLOC, DRFLAC_REALLOC and DRFLAC_FREE.

Sometimes a FLAC file won't keep track of the total sample count. In this situation the function will continuously
read samples into a dynamically sized buffer on the heap until no samples are left.

Do not call this function on a broadcast type of stream (like internet radio streams and whatnot).
*/
drflac_int32* drflac_open_and_read_pcm_frames_s32(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* channels, unsigned int* sampleRate, drflac_uint64* totalPCMFrameCount, const drflac_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_and_read_pcm_frames_s32(), except returns signed 16-bit integer samples. */
drflac_int16* drflac_open_and_read_pcm_frames_s16(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* channels, unsigned int* sampleRate, drflac_uint64* totalPCMFrameCount, const drflac_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_and_read_pcm_frames_s32(), except returns 32-bit floating-point samples. */
float* drflac_open_and_read_pcm_frames_f32(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, unsigned int* channels, unsigned int* sampleRate, drflac_uint64* totalPCMFrameCount, const drflac_allocation_callbacks* pAllocationCallbacks);

#ifndef DR_FLAC_NO_STDIO
/* Same as drflac_open_and_read_pcm_frames_s32() except opens the decoder from a file. */
drflac_int32* drflac_open_file_and_read_pcm_frames_s32(const char* filename, unsigned int* channels, unsigned int* sampleRate, drflac_uint64* totalPCMFrameCount, const drflac_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_file_and_read_pcm_frames_s32(), except returns signed 16-bit integer samples. */
drflac_int16* drflac_open_file_and_read_pcm_frames_s16(const char* filename, unsigned int* channels, unsigned int* sampleRate, drflac_uint64* totalPCMFrameCount, const drflac_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_file_and_read_pcm_frames_s32(), except returns 32-bit floating-point samples. */
float* drflac_open_file_and_read_pcm_frames_f32(const char* filename, unsigned int* channels, unsigned int* sampleRate, drflac_uint64* totalPCMFrameCount, const drflac_allocation_callbacks* pAllocationCallbacks);
#endif

/* Same as drflac_open_and_read_pcm_frames_s32() except opens the decoder from a block of memory. */
drflac_int32* drflac_open_memory_and_read_pcm_frames_s32(const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate, drflac_uint64* totalPCMFrameCount, const drflac_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_memory_and_read_pcm_frames_s32(), except returns signed 16-bit integer samples. */
drflac_int16* drflac_open_memory_and_read_pcm_frames_s16(const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate, drflac_uint64* totalPCMFrameCount, const drflac_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_memory_and_read_pcm_frames_s32(), except returns 32-bit floating-point samples. */
float* drflac_open_memory_and_read_pcm_frames_f32(const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate, drflac_uint64* totalPCMFrameCount, const drflac_allocation_callbacks* pAllocationCallbacks);

/*
Frees memory that was allocated internally by dr_flac.

Set pAllocationCallbacks to the same object that was passed to drflac_open_*_and_read_pcm_frames_*(). If you originally passed in NULL, pass in NULL for this.
*/
void drflac_free(void* p, const drflac_allocation_callbacks* pAllocationCallbacks);


/* Structure representing an iterator for vorbis comments in a VORBIS_COMMENT metadata block. */
typedef struct
{
    drflac_uint32 countRemaining;
    const char* pRunningData;
} drflac_vorbis_comment_iterator;

/*
Initializes a vorbis comment iterator. This can be used for iterating over the vorbis comments in a VORBIS_COMMENT
metadata block.
*/
void drflac_init_vorbis_comment_iterator(drflac_vorbis_comment_iterator* pIter, drflac_uint32 commentCount, const void* pComments);

/*
Goes to the next vorbis comment in the given iterator. If null is returned it means there are no more comments. The
returned string is NOT null terminated.
*/
const char* drflac_next_vorbis_comment(drflac_vorbis_comment_iterator* pIter, drflac_uint32* pCommentLengthOut);


/* Structure representing an iterator for cuesheet tracks in a CUESHEET metadata block. */
typedef struct
{
    drflac_uint32 countRemaining;
    const char* pRunningData;
} drflac_cuesheet_track_iterator;

/* Packing is important on this structure because we map this directly to the raw data within the CUESHEET metadata block. */
#pragma pack(4)
typedef struct
{
    drflac_uint64 offset;
    drflac_uint8 index;
    drflac_uint8 reserved[3];
} drflac_cuesheet_track_index;
#pragma pack()

typedef struct
{
    drflac_uint64 offset;
    drflac_uint8 trackNumber;
    char ISRC[12];
    drflac_bool8 isAudio;
    drflac_bool8 preEmphasis;
    drflac_uint8 indexCount;
    const drflac_cuesheet_track_index* pIndexPoints;
} drflac_cuesheet_track;

/*
Initializes a cuesheet track iterator. This can be used for iterating over the cuesheet tracks in a CUESHEET metadata
block.
*/
void drflac_init_cuesheet_track_iterator(drflac_cuesheet_track_iterator* pIter, drflac_uint32 trackCount, const void* pTrackData);

/* Goes to the next cuesheet track in the given iterator. If DRFLAC_FALSE is returned it means there are no more comments. */
drflac_bool32 drflac_next_cuesheet_track(drflac_cuesheet_track_iterator* pIter, drflac_cuesheet_track* pCuesheetTrack);


/************************************************************************************************************************************************************
 ************************************************************************************************************************************************************

 IMPLEMENTATION

 ************************************************************************************************************************************************************
 ************************************************************************************************************************************************************/

#define DR_FLAC_IMPLEMENTATION
#ifdef DR_FLAC_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <arm_neon.h>
#include <kernel.h>

#define DRFLAC_ARM
#define DR_FLAC_NO_OGG
#define DRFLAC_SUPPORT_NEON
#define DRFLAC_NO_CPUID

/* Standard library stuff. */
#ifndef DRFLAC_MALLOC
#define DRFLAC_MALLOC(sz)                   malloc((sz))
#endif
#ifndef DRFLAC_REALLOC
#define DRFLAC_REALLOC(p, sz)               realloc((p), (sz))
#endif
#ifndef DRFLAC_FREE
#define DRFLAC_FREE(p)                      free((p))
#endif
#ifndef DRFLAC_COPY_MEMORY
#define DRFLAC_COPY_MEMORY(dst, src, sz)    memcpy((dst), (src), (sz))
#endif
#ifndef DRFLAC_ZERO_MEMORY
#define DRFLAC_ZERO_MEMORY(p, sz)           memset((p), 0, (sz))
#endif

#define DRFLAC_MAX_SIMD_VECTOR_SIZE                     64  /* 64 for AVX-512 in the future. */

typedef drflac_int32 drflac_result;
#define DRFLAC_SUCCESS                                  0
#define DRFLAC_ERROR                                    -1  /* A generic error. */
#define DRFLAC_INVALID_ARGS                             -2
#define DRFLAC_END_OF_STREAM                            -128
#define DRFLAC_CRC_MISMATCH                             -129

#define DRFLAC_SUBFRAME_CONSTANT                        0
#define DRFLAC_SUBFRAME_VERBATIM                        1
#define DRFLAC_SUBFRAME_FIXED                           8
#define DRFLAC_SUBFRAME_LPC                             32
#define DRFLAC_SUBFRAME_RESERVED                        255

#define DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE  0
#define DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2 1

#define DRFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT           0
#define DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE             8
#define DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE            9
#define DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE              10

#define drflac_align(x, a)                              ((((x) + (a) - 1) / (a)) * (a))


/* CPU caps. */
#define DRFLAC_NO_THREAD_SANITIZE


static drflac_bool32 drflac__gIsNEONSupported  = DRFLAC_TRUE;

static  drflac_bool32 drflac__has_neon()
{
return DRFLAC_TRUE;
}

DRFLAC_NO_THREAD_SANITIZE static void drflac__init_cpu_caps()
{
    drflac__gIsNEONSupported = drflac__has_neon();
}


/* Endian Management */
static  drflac_bool32 drflac__is_little_endian()
{
int n = 1;
return (*(char*)&n) == 1;
}

static  drflac_uint16 drflac__swap_endian_uint16(drflac_uint16 n)
{
return ((n & 0xFF00) >> 8) |
           ((n & 0x00FF) << 8);
}

static  drflac_uint32 drflac__swap_endian_uint32(drflac_uint32 n)
{
return ((n & 0xFF000000) >> 24) |
           ((n & 0x00FF0000) >>  8) |
           ((n & 0x0000FF00) <<  8) |
           ((n & 0x000000FF) << 24);
}

static  drflac_uint64 drflac__swap_endian_uint64(drflac_uint64 n)
{
return ((n & (drflac_uint64)0xFF00000000000000) >> 56) |
           ((n & (drflac_uint64)0x00FF000000000000) >> 40) |
           ((n & (drflac_uint64)0x0000FF0000000000) >> 24) |
           ((n & (drflac_uint64)0x000000FF00000000) >>  8) |
           ((n & (drflac_uint64)0x00000000FF000000) <<  8) |
           ((n & (drflac_uint64)0x0000000000FF0000) << 24) |
           ((n & (drflac_uint64)0x000000000000FF00) << 40) |
           ((n & (drflac_uint64)0x00000000000000FF) << 56);
}


static  drflac_uint16 drflac__be2host_16(drflac_uint16 n)
{
    if (drflac__is_little_endian()) {
        return drflac__swap_endian_uint16(n);
    }

    return n;
}

static  drflac_uint32 drflac__be2host_32(drflac_uint32 n)
{
    if (drflac__is_little_endian()) {
        return drflac__swap_endian_uint32(n);
    }

    return n;
}

static  drflac_uint64 drflac__be2host_64(drflac_uint64 n)
{
    if (drflac__is_little_endian()) {
        return drflac__swap_endian_uint64(n);
    }

    return n;
}


static  drflac_uint32 drflac__le2host_32(drflac_uint32 n)
{
    if (!drflac__is_little_endian()) {
        return drflac__swap_endian_uint32(n);
    }

    return n;
}


static  drflac_uint32 drflac__unsynchsafe_32(drflac_uint32 n)
{
    drflac_uint32 result = 0;
    result |= (n & 0x7F000000) >> 3;
    result |= (n & 0x007F0000) >> 2;
    result |= (n & 0x00007F00) >> 1;
    result |= (n & 0x0000007F) >> 0;

    return result;
}



/* The CRC code below is based on this document: http://zlib.net/crc_v3.txt */
static drflac_uint8 drflac__crc8_table[] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

static drflac_uint16 drflac__crc16_table[] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
    0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
    0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
    0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
    0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
    0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
    0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
    0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
    0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
    0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
    0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
    0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
    0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
    0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
    0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
    0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
    0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
    0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
    0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
    0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
    0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
    0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
    0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
    0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
    0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
    0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
    0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
    0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
    0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
    0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
    0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
    0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};

static  drflac_uint8 drflac_crc8_byte(drflac_uint8 crc, drflac_uint8 data)
{
    return drflac__crc8_table[crc ^ data];
}

static  drflac_uint8 drflac_crc8(drflac_uint8 crc, drflac_uint32 data, drflac_uint32 count)
{
    drflac_uint32 wholeBytes;
    drflac_uint32 leftoverBits;
    drflac_uint64 leftoverDataMask;

    static drflac_uint64 leftoverDataMaskTable[8] = {
        0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F
    };

    wholeBytes = count >> 3;
    leftoverBits = count - (wholeBytes*8);
    leftoverDataMask = leftoverDataMaskTable[leftoverBits];

    switch (wholeBytes) {
        case 4: crc = drflac_crc8_byte(crc, (drflac_uint8)((data & (0xFF000000UL << leftoverBits)) >> (24 + leftoverBits)));
        case 3: crc = drflac_crc8_byte(crc, (drflac_uint8)((data & (0x00FF0000UL << leftoverBits)) >> (16 + leftoverBits)));
        case 2: crc = drflac_crc8_byte(crc, (drflac_uint8)((data & (0x0000FF00UL << leftoverBits)) >> ( 8 + leftoverBits)));
        case 1: crc = drflac_crc8_byte(crc, (drflac_uint8)((data & (0x000000FFUL << leftoverBits)) >> ( 0 + leftoverBits)));
        case 0: if (leftoverBits > 0) crc = (crc << leftoverBits) ^ drflac__crc8_table[(crc >> (8 - leftoverBits)) ^ (data & leftoverDataMask)];
    }
    return crc;
}

static  drflac_uint16 drflac_crc16_byte(drflac_uint16 crc, drflac_uint8 data)
{
    return (crc << 8) ^ drflac__crc16_table[(drflac_uint8)(crc >> 8) ^ data];
}

static  drflac_uint16 drflac_crc16_cache(drflac_uint16 crc, drflac_cache_t data)
{
#ifdef DRFLAC_64BIT
    crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 56) & 0xFF));
    crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 48) & 0xFF));
    crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 40) & 0xFF));
    crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 32) & 0xFF));
#endif
    crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 24) & 0xFF));
    crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 16) & 0xFF));
    crc = drflac_crc16_byte(crc, (drflac_uint8)((data >>  8) & 0xFF));
    crc = drflac_crc16_byte(crc, (drflac_uint8)((data >>  0) & 0xFF));

    return crc;
}

static  drflac_uint16 drflac_crc16_bytes(drflac_uint16 crc, drflac_cache_t data, drflac_uint32 byteCount)
{
    switch (byteCount)
    {
#ifdef DRFLAC_64BIT
    case 8: crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 56) & 0xFF));
    case 7: crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 48) & 0xFF));
    case 6: crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 40) & 0xFF));
    case 5: crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 32) & 0xFF));
#endif
    case 4: crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 24) & 0xFF));
    case 3: crc = drflac_crc16_byte(crc, (drflac_uint8)((data >> 16) & 0xFF));
    case 2: crc = drflac_crc16_byte(crc, (drflac_uint8)((data >>  8) & 0xFF));
    case 1: crc = drflac_crc16_byte(crc, (drflac_uint8)((data >>  0) & 0xFF));
    }

    return crc;
}

#define drflac__be2host__cache_line drflac__be2host_32

/*
BIT READING ATTEMPT #2

This uses a 32- or 64-bit bit-shifted cache - as bits are read, the cache is shifted such that the first valid bit is sitting
on the most significant bit. It uses the notion of an L1 and L2 cache (borrowed from CPU architecture), where the L1 cache
is a 32- or 64-bit unsigned integer (depending on whether or not a 32- or 64-bit build is being compiled) and the L2 is an
array of "cache lines", with each cache line being the same size as the L1. The L2 is a buffer of about 4KB and is where data
from onRead() is read into.
*/
#define DRFLAC_CACHE_L1_SIZE_BYTES(bs)                      (sizeof((bs)->cache))
#define DRFLAC_CACHE_L1_SIZE_BITS(bs)                       (sizeof((bs)->cache)*8)
#define DRFLAC_CACHE_L1_BITS_REMAINING(bs)                  (DRFLAC_CACHE_L1_SIZE_BITS(bs) - (bs)->consumedBits)
#define DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount)           (~((~(drflac_cache_t)0) >> (_bitCount)))
#define DRFLAC_CACHE_L1_SELECTION_SHIFT(bs, _bitCount)      (DRFLAC_CACHE_L1_SIZE_BITS(bs) - (_bitCount))
#define DRFLAC_CACHE_L1_SELECT(bs, _bitCount)               (((bs)->cache) & DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount))
#define DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, _bitCount)     (DRFLAC_CACHE_L1_SELECT((bs), (_bitCount)) >>  DRFLAC_CACHE_L1_SELECTION_SHIFT((bs), (_bitCount)))
#define DRFLAC_CACHE_L1_SELECT_AND_SHIFT_SAFE(bs, _bitCount)(DRFLAC_CACHE_L1_SELECT((bs), (_bitCount)) >> (DRFLAC_CACHE_L1_SELECTION_SHIFT((bs), (_bitCount)) & (DRFLAC_CACHE_L1_SIZE_BITS(bs)-1)))
#define DRFLAC_CACHE_L2_SIZE_BYTES(bs)                      (DR_FLAC_BUFFER_SIZE)
#define DRFLAC_CACHE_L2_LINE_COUNT(bs)                      (DRFLAC_CACHE_L2_SIZE_BYTES(bs) / sizeof((bs)->cacheL2[0]))
#define DRFLAC_CACHE_L2_LINES_REMAINING(bs)                 (DRFLAC_CACHE_L2_LINE_COUNT(bs) - (bs)->nextL2Line)


#ifndef DR_FLAC_NO_CRC
static  void drflac__reset_crc16(drflac_bs* bs)
{
    bs->crc16 = 0;
    bs->crc16CacheIgnoredBytes = bs->consumedBits >> 3;
}

static  void drflac__update_crc16(drflac_bs* bs)
{
    if (bs->crc16CacheIgnoredBytes == 0) {
        bs->crc16 = drflac_crc16_cache(bs->crc16, bs->crc16Cache);
    } else {
        bs->crc16 = drflac_crc16_bytes(bs->crc16, bs->crc16Cache, DRFLAC_CACHE_L1_SIZE_BYTES(bs) - bs->crc16CacheIgnoredBytes);
        bs->crc16CacheIgnoredBytes = 0;
    }
}

static  drflac_uint16 drflac__flush_crc16(drflac_bs* bs)
{
    /*
    The bits that were read from the L1 cache need to be accumulated. The number of bytes needing to be accumulated is determined
    by the number of bits that have been consumed.
    */
    if (DRFLAC_CACHE_L1_BITS_REMAINING(bs) == 0) {
        drflac__update_crc16(bs);
    } else {
        /* We only accumulate the consumed bits. */
        bs->crc16 = drflac_crc16_bytes(bs->crc16, bs->crc16Cache >> DRFLAC_CACHE_L1_BITS_REMAINING(bs), (bs->consumedBits >> 3) - bs->crc16CacheIgnoredBytes);

        /*
        The bits that we just accumulated should never be accumulated again. We need to keep track of how many bytes were accumulated
        so we can handle that later.
        */
        bs->crc16CacheIgnoredBytes = bs->consumedBits >> 3;
    }

    return bs->crc16;
}
#endif

static  drflac_bool32 drflac__reload_l1_cache_from_l2(drflac_bs* bs)
{
    size_t bytesRead;
    size_t alignedL1LineCount;

    /* Fast path. Try loading straight from L2. */
    if (bs->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT(bs)) {
        bs->cache = bs->cacheL2[bs->nextL2Line++];
        return DRFLAC_TRUE;
    }

    /*
    If we get here it means we've run out of data in the L2 cache. We'll need to fetch more from the client, if there's
    any left.
    */
    if (bs->unalignedByteCount > 0) {
        return DRFLAC_FALSE;   /* If we have any unaligned bytes it means there's no more aligned bytes left in the client. */
    }

    bytesRead = bs->onRead(bs->pUserData, bs->cacheL2, DRFLAC_CACHE_L2_SIZE_BYTES(bs));

    bs->nextL2Line = 0;
    if (bytesRead == DRFLAC_CACHE_L2_SIZE_BYTES(bs)) {
        bs->cache = bs->cacheL2[bs->nextL2Line++];
        return DRFLAC_TRUE;
    }


    /*
    If we get here it means we were unable to retrieve enough data to fill the entire L2 cache. It probably
    means we've just reached the end of the file. We need to move the valid data down to the end of the buffer
    and adjust the index of the next line accordingly. Also keep in mind that the L2 cache must be aligned to
    the size of the L1 so we'll need to seek backwards by any misaligned bytes.
    */
    alignedL1LineCount = bytesRead / DRFLAC_CACHE_L1_SIZE_BYTES(bs);

    /* We need to keep track of any unaligned bytes for later use. */
    bs->unalignedByteCount = bytesRead - (alignedL1LineCount * DRFLAC_CACHE_L1_SIZE_BYTES(bs));
    if (bs->unalignedByteCount > 0) {
        bs->unalignedCache = bs->cacheL2[alignedL1LineCount];
    }

    if (alignedL1LineCount > 0) {
        size_t offset = DRFLAC_CACHE_L2_LINE_COUNT(bs) - alignedL1LineCount;
        size_t i;
        for (i = alignedL1LineCount; i > 0; --i) {
            bs->cacheL2[i-1 + offset] = bs->cacheL2[i-1];
        }

        bs->nextL2Line = (drflac_uint32)offset;
        bs->cache = bs->cacheL2[bs->nextL2Line++];
        return DRFLAC_TRUE;
    } else {
        /* If we get into this branch it means we weren't able to load any L1-aligned data. */
        bs->nextL2Line = DRFLAC_CACHE_L2_LINE_COUNT(bs);
        return DRFLAC_FALSE;
    }
}

static drflac_bool32 drflac__reload_cache(drflac_bs* bs)
{
    size_t bytesRead;

#ifndef DR_FLAC_NO_CRC
    drflac__update_crc16(bs);
#endif

    /* Fast path. Try just moving the next value in the L2 cache to the L1 cache. */
    if (drflac__reload_l1_cache_from_l2(bs)) {
        bs->cache = drflac__be2host__cache_line(bs->cache);
        bs->consumedBits = 0;
#ifndef DR_FLAC_NO_CRC
        bs->crc16Cache = bs->cache;
#endif
        return DRFLAC_TRUE;
    }

    /* Slow path. */

    /*
    If we get here it means we have failed to load the L1 cache from the L2. Likely we've just reached the end of the stream and the last
    few bytes did not meet the alignment requirements for the L2 cache. In this case we need to fall back to a slower path and read the
    data from the unaligned cache.
    */
    bytesRead = bs->unalignedByteCount;
    if (bytesRead == 0) {
        bs->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS(bs);   /* <-- The stream has been exhausted, so marked the bits as consumed. */
        return DRFLAC_FALSE;
    }

    bs->consumedBits = (drflac_uint32)(DRFLAC_CACHE_L1_SIZE_BYTES(bs) - bytesRead) * 8;

    bs->cache = drflac__be2host__cache_line(bs->unalignedCache);
    bs->cache &= DRFLAC_CACHE_L1_SELECTION_MASK(DRFLAC_CACHE_L1_BITS_REMAINING(bs));    /* <-- Make sure the consumed bits are always set to zero. Other parts of the library depend on this property. */
    bs->unalignedByteCount = 0;     /* <-- At this point the unaligned bytes have been moved into the cache and we thus have no more unaligned bytes. */

#ifndef DR_FLAC_NO_CRC
    bs->crc16Cache = bs->cache >> bs->consumedBits;
    bs->crc16CacheIgnoredBytes = bs->consumedBits >> 3;
#endif
    return DRFLAC_TRUE;
}

static void drflac__reset_cache(drflac_bs* bs)
{
    bs->nextL2Line   = DRFLAC_CACHE_L2_LINE_COUNT(bs);  /* <-- This clears the L2 cache. */
    bs->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS(bs);   /* <-- This clears the L1 cache. */
    bs->cache = 0;
    bs->unalignedByteCount = 0;                         /* <-- This clears the trailing unaligned bytes. */
    bs->unalignedCache = 0;

#ifndef DR_FLAC_NO_CRC
    bs->crc16Cache = 0;
    bs->crc16CacheIgnoredBytes = 0;
#endif
}


static  drflac_bool32 drflac__read_uint32(drflac_bs* bs, unsigned int bitCount, drflac_uint32* pResultOut)
{
    if (bs->consumedBits == DRFLAC_CACHE_L1_SIZE_BITS(bs)) {
        if (!drflac__reload_cache(bs)) {
            return DRFLAC_FALSE;
        }
    }

    if (bitCount <= DRFLAC_CACHE_L1_BITS_REMAINING(bs)) {
        /*
        If we want to load all 32-bits from a 32-bit cache we need to do it slightly differently because we can't do
        a 32-bit shift on a 32-bit integer. This will never be the case on 64-bit caches, so we can have a slightly
        more optimal solution for this.
        */
#ifdef DRFLAC_64BIT
        *pResultOut = (drflac_uint32)DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCount);
        bs->consumedBits += bitCount;
        bs->cache <<= bitCount;
#else
        if (bitCount < DRFLAC_CACHE_L1_SIZE_BITS(bs)) {
            *pResultOut = (drflac_uint32)DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCount);
            bs->consumedBits += bitCount;
            bs->cache <<= bitCount;
        } else {
            /* Cannot shift by 32-bits, so need to do it differently. */
            *pResultOut = (drflac_uint32)bs->cache;
            bs->consumedBits = DRFLAC_CACHE_L1_SIZE_BITS(bs);
            bs->cache = 0;
        }
#endif

        return DRFLAC_TRUE;
    } else {
        /* It straddles the cached data. It will never cover more than the next chunk. We just read the number in two parts and combine them. */
        drflac_uint32 bitCountHi = DRFLAC_CACHE_L1_BITS_REMAINING(bs);
        drflac_uint32 bitCountLo = bitCount - bitCountHi;
        drflac_uint32 resultHi = (drflac_uint32)DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCountHi);

        if (!drflac__reload_cache(bs)) {
            return DRFLAC_FALSE;
        }

        *pResultOut = (resultHi << bitCountLo) | (drflac_uint32)DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCountLo);
        bs->consumedBits += bitCountLo;
        bs->cache <<= bitCountLo;
        return DRFLAC_TRUE;
    }
}

static drflac_bool32 drflac__read_int32(drflac_bs* bs, unsigned int bitCount, drflac_int32* pResult)
{
    drflac_uint32 result = 0;
    drflac_uint32 signbit;

    if (!drflac__read_uint32(bs, bitCount, &result)) {
        return DRFLAC_FALSE;
    }

    signbit = ((result >> (bitCount-1)) & 0x01);
    result |= (~signbit + 1) << bitCount;

    *pResult = (drflac_int32)result;
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__read_uint16(drflac_bs* bs, unsigned int bitCount, drflac_uint16* pResult)
{
    drflac_uint32 result = 0;

    if (!drflac__read_uint32(bs, bitCount, &result)) {
        return DRFLAC_FALSE;
    }

    *pResult = (drflac_uint16)result;
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__read_uint8(drflac_bs* bs, unsigned int bitCount, drflac_uint8* pResult)
{
    drflac_uint32 result = 0;

    if (!drflac__read_uint32(bs, bitCount, &result)) {
        return DRFLAC_FALSE;
    }

    *pResult = (drflac_uint8)result;
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__read_int8(drflac_bs* bs, unsigned int bitCount, drflac_int8* pResult)
{
    drflac_int32 result;

    if (!drflac__read_int32(bs, bitCount, &result)) {
        return DRFLAC_FALSE;
    }

    *pResult = (drflac_int8)result;
    return DRFLAC_TRUE;
}


static drflac_bool32 drflac__seek_bits(drflac_bs* bs, size_t bitsToSeek)
{
    if (bitsToSeek <= DRFLAC_CACHE_L1_BITS_REMAINING(bs)) {
        bs->consumedBits += (drflac_uint32)bitsToSeek;
        bs->cache <<= bitsToSeek;
        return DRFLAC_TRUE;
    } else {
        /* It straddles the cached data. This function isn't called too frequently so I'm favouring simplicity here. */
        bitsToSeek       -= DRFLAC_CACHE_L1_BITS_REMAINING(bs);
        bs->consumedBits += DRFLAC_CACHE_L1_BITS_REMAINING(bs);
        bs->cache         = 0;

        /* Simple case. Seek in groups of the same number as bits that fit within a cache line. */
        while (bitsToSeek >= DRFLAC_CACHE_L1_SIZE_BITS(bs)) {
            drflac_uint32 bin;
            if (!drflac__read_uint32(bs, DRFLAC_CACHE_L1_SIZE_BITS(bs), &bin)) {
                return DRFLAC_FALSE;
            }
            bitsToSeek -= DRFLAC_CACHE_L1_SIZE_BITS(bs);
        }

        /* Whole leftover bytes. */
        while (bitsToSeek >= 8) {
            drflac_uint8 bin;
            if (!drflac__read_uint8(bs, 8, &bin)) {
                return DRFLAC_FALSE;
            }
            bitsToSeek -= 8;
        }

        /* Leftover bits. */
        if (bitsToSeek > 0) {
            drflac_uint8 bin;
            if (!drflac__read_uint8(bs, (drflac_uint32)bitsToSeek, &bin)) {
                return DRFLAC_FALSE;
            }
            bitsToSeek = 0; /* <-- Necessary for the assert below. */
        }

        return DRFLAC_TRUE;
    }
}


/* This function moves the bit streamer to the first bit after the sync code (bit 15 of the of the frame header). It will also update the CRC-16. */
static drflac_bool32 drflac__find_and_seek_to_next_sync_code(drflac_bs* bs)
{

    /*
    The sync code is always aligned to 8 bits. This is convenient for us because it means we can do byte-aligned movements. The first
    thing to do is align to the next byte.
    */
    if (!drflac__seek_bits(bs, DRFLAC_CACHE_L1_BITS_REMAINING(bs) & 7)) {
        return DRFLAC_FALSE;
    }

    for (;;) {
        drflac_uint8 hi;

#ifndef DR_FLAC_NO_CRC
        drflac__reset_crc16(bs);
#endif

        if (!drflac__read_uint8(bs, 8, &hi)) {
            return DRFLAC_FALSE;
        }

        if (hi == 0xFF) {
            drflac_uint8 lo;
            if (!drflac__read_uint8(bs, 6, &lo)) {
                return DRFLAC_FALSE;
            }

            if (lo == 0x3E) {
                return DRFLAC_TRUE;
            } else {
                if (!drflac__seek_bits(bs, DRFLAC_CACHE_L1_BITS_REMAINING(bs) & 7)) {
                    return DRFLAC_FALSE;
                }
            }
        }
    }

    /* Should never get here. */
    /*return DRFLAC_FALSE;*/
}

static  drflac_uint32 drflac__clz_software(drflac_cache_t x)
{
    drflac_uint32 n;
    static drflac_uint32 clz_table_4[] = {
        0,
        4,
        3, 3,
        2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1
    };

    if (x == 0) {
        return sizeof(x)*8;
    }

    n = clz_table_4[x >> (sizeof(x)*8 - 4)];
    if (n == 0) {

        if ((x & 0xFFFF0000) == 0) { n  = 16; x <<= 16; }
        if ((x & 0xFF000000) == 0) { n += 8;  x <<= 8;  }
        if ((x & 0xF0000000) == 0) { n += 4;  x <<= 4;  }
        n += clz_table_4[x >> (sizeof(x)*8 - 4)];
    }

    return n - 1;
}

static  drflac_uint32 drflac__clz(drflac_cache_t x)
{
return drflac__clz_software(x);
}


static  drflac_bool32 drflac__seek_past_next_set_bit(drflac_bs* bs, unsigned int* pOffsetOut)
{
    drflac_uint32 zeroCounter = 0;
    drflac_uint32 setBitOffsetPlus1;

    while (bs->cache == 0) {
        zeroCounter += (drflac_uint32)DRFLAC_CACHE_L1_BITS_REMAINING(bs);
        if (!drflac__reload_cache(bs)) {
            return DRFLAC_FALSE;
        }
    }

    setBitOffsetPlus1 = drflac__clz(bs->cache);
    setBitOffsetPlus1 += 1;

    bs->consumedBits += setBitOffsetPlus1;
    bs->cache <<= setBitOffsetPlus1;

    *pOffsetOut = zeroCounter + setBitOffsetPlus1 - 1;
    return DRFLAC_TRUE;
}



static drflac_bool32 drflac__seek_to_byte(drflac_bs* bs, drflac_uint64 offsetFromStart)
{

    /*
    Seeking from the start is not quite as trivial as it sounds because the onSeek callback takes a signed 32-bit integer (which
    is intentional because it simplifies the implementation of the onSeek callbacks), however offsetFromStart is unsigned 64-bit.
    To resolve we just need to do an initial seek from the start, and then a series of offset seeks to make up the remainder.
    */
    if (offsetFromStart > 0x7FFFFFFF) {
        drflac_uint64 bytesRemaining = offsetFromStart;
        if (!bs->onSeek(bs->pUserData, 0x7FFFFFFF, drflac_seek_origin_start)) {
            return DRFLAC_FALSE;
        }
        bytesRemaining -= 0x7FFFFFFF;

        while (bytesRemaining > 0x7FFFFFFF) {
            if (!bs->onSeek(bs->pUserData, 0x7FFFFFFF, drflac_seek_origin_current)) {
                return DRFLAC_FALSE;
            }
            bytesRemaining -= 0x7FFFFFFF;
        }

        if (bytesRemaining > 0) {
            if (!bs->onSeek(bs->pUserData, (int)bytesRemaining, drflac_seek_origin_current)) {
                return DRFLAC_FALSE;
            }
        }
    } else {
        if (!bs->onSeek(bs->pUserData, (int)offsetFromStart, drflac_seek_origin_start)) {
            return DRFLAC_FALSE;
        }
    }

    /* The cache should be reset to force a reload of fresh data from the client. */
    drflac__reset_cache(bs);
    return DRFLAC_TRUE;
}


static drflac_result drflac__read_utf8_coded_number(drflac_bs* bs, drflac_uint64* pNumberOut, drflac_uint8* pCRCOut)
{
    drflac_uint8 crc;
    drflac_uint64 result;
    unsigned char utf8[7] = {0};
    int byteCount;
    int i;

    crc = *pCRCOut;

    if (!drflac__read_uint8(bs, 8, utf8)) {
        *pNumberOut = 0;
        return DRFLAC_END_OF_STREAM;
    }
    crc = drflac_crc8(crc, utf8[0], 8);

    if ((utf8[0] & 0x80) == 0) {
        *pNumberOut = utf8[0];
        *pCRCOut = crc;
        return DRFLAC_SUCCESS;
    }

    /*byteCount = 1;*/
    if ((utf8[0] & 0xE0) == 0xC0) {
        byteCount = 2;
    } else if ((utf8[0] & 0xF0) == 0xE0) {
        byteCount = 3;
    } else if ((utf8[0] & 0xF8) == 0xF0) {
        byteCount = 4;
    } else if ((utf8[0] & 0xFC) == 0xF8) {
        byteCount = 5;
    } else if ((utf8[0] & 0xFE) == 0xFC) {
        byteCount = 6;
    } else if ((utf8[0] & 0xFF) == 0xFE) {
        byteCount = 7;
    } else {
        *pNumberOut = 0;
        return DRFLAC_CRC_MISMATCH;     /* Bad UTF-8 encoding. */
    }

    /* Read extra bytes. */

    result = (drflac_uint64)(utf8[0] & (0xFF >> (byteCount + 1)));
    for (i = 1; i < byteCount; ++i) {
        if (!drflac__read_uint8(bs, 8, utf8 + i)) {
            *pNumberOut = 0;
            return DRFLAC_END_OF_STREAM;
        }
        crc = drflac_crc8(crc, utf8[i], 8);

        result = (result << 6) | (utf8[i] & 0x3F);
    }

    *pNumberOut = result;
    *pCRCOut = crc;
    return DRFLAC_SUCCESS;
}



/*
The next two functions are responsible for calculating the prediction.

When the bits per sample is >16 we need to use 64-bit integer arithmetic because otherwise we'll run out of precision. It's
safe to assume this will be slower on 32-bit platforms so we use a more optimal solution when the bits per sample is <=16.
*/
static  drflac_int32 drflac__calculate_prediction_32(drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pDecodedSamples)
{
    drflac_int32 prediction = 0;

    /* 32-bit version. */

    /* VC++ optimizes this to a single jmp. I've not yet verified this for other compilers. */
    switch (order)
    {
    case 32: prediction += coefficients[31] * pDecodedSamples[-32];
    case 31: prediction += coefficients[30] * pDecodedSamples[-31];
    case 30: prediction += coefficients[29] * pDecodedSamples[-30];
    case 29: prediction += coefficients[28] * pDecodedSamples[-29];
    case 28: prediction += coefficients[27] * pDecodedSamples[-28];
    case 27: prediction += coefficients[26] * pDecodedSamples[-27];
    case 26: prediction += coefficients[25] * pDecodedSamples[-26];
    case 25: prediction += coefficients[24] * pDecodedSamples[-25];
    case 24: prediction += coefficients[23] * pDecodedSamples[-24];
    case 23: prediction += coefficients[22] * pDecodedSamples[-23];
    case 22: prediction += coefficients[21] * pDecodedSamples[-22];
    case 21: prediction += coefficients[20] * pDecodedSamples[-21];
    case 20: prediction += coefficients[19] * pDecodedSamples[-20];
    case 19: prediction += coefficients[18] * pDecodedSamples[-19];
    case 18: prediction += coefficients[17] * pDecodedSamples[-18];
    case 17: prediction += coefficients[16] * pDecodedSamples[-17];
    case 16: prediction += coefficients[15] * pDecodedSamples[-16];
    case 15: prediction += coefficients[14] * pDecodedSamples[-15];
    case 14: prediction += coefficients[13] * pDecodedSamples[-14];
    case 13: prediction += coefficients[12] * pDecodedSamples[-13];
    case 12: prediction += coefficients[11] * pDecodedSamples[-12];
    case 11: prediction += coefficients[10] * pDecodedSamples[-11];
    case 10: prediction += coefficients[ 9] * pDecodedSamples[-10];
    case  9: prediction += coefficients[ 8] * pDecodedSamples[- 9];
    case  8: prediction += coefficients[ 7] * pDecodedSamples[- 8];
    case  7: prediction += coefficients[ 6] * pDecodedSamples[- 7];
    case  6: prediction += coefficients[ 5] * pDecodedSamples[- 6];
    case  5: prediction += coefficients[ 4] * pDecodedSamples[- 5];
    case  4: prediction += coefficients[ 3] * pDecodedSamples[- 4];
    case  3: prediction += coefficients[ 2] * pDecodedSamples[- 3];
    case  2: prediction += coefficients[ 1] * pDecodedSamples[- 2];
    case  1: prediction += coefficients[ 0] * pDecodedSamples[- 1];
    }

    return (drflac_int32)(prediction >> shift);
}

static  drflac_int32 drflac__calculate_prediction_64(drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pDecodedSamples)
{
    drflac_int64 prediction;

    /* 64-bit version. */

    /* This method is faster on the 32-bit build when compiling with VC++. See note below. */
#ifndef DRFLAC_64BIT
    if (order == 8)
    {
        prediction  = coefficients[0] * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (drflac_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (drflac_int64)pDecodedSamples[-4];
        prediction += coefficients[4] * (drflac_int64)pDecodedSamples[-5];
        prediction += coefficients[5] * (drflac_int64)pDecodedSamples[-6];
        prediction += coefficients[6] * (drflac_int64)pDecodedSamples[-7];
        prediction += coefficients[7] * (drflac_int64)pDecodedSamples[-8];
    }
    else if (order == 7)
    {
        prediction  = coefficients[0] * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (drflac_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (drflac_int64)pDecodedSamples[-4];
        prediction += coefficients[4] * (drflac_int64)pDecodedSamples[-5];
        prediction += coefficients[5] * (drflac_int64)pDecodedSamples[-6];
        prediction += coefficients[6] * (drflac_int64)pDecodedSamples[-7];
    }
    else if (order == 3)
    {
        prediction  = coefficients[0] * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (drflac_int64)pDecodedSamples[-3];
    }
    else if (order == 6)
    {
        prediction  = coefficients[0] * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (drflac_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (drflac_int64)pDecodedSamples[-4];
        prediction += coefficients[4] * (drflac_int64)pDecodedSamples[-5];
        prediction += coefficients[5] * (drflac_int64)pDecodedSamples[-6];
    }
    else if (order == 5)
    {
        prediction  = coefficients[0] * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (drflac_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (drflac_int64)pDecodedSamples[-4];
        prediction += coefficients[4] * (drflac_int64)pDecodedSamples[-5];
    }
    else if (order == 4)
    {
        prediction  = coefficients[0] * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2] * (drflac_int64)pDecodedSamples[-3];
        prediction += coefficients[3] * (drflac_int64)pDecodedSamples[-4];
    }
    else if (order == 12)
    {
        prediction  = coefficients[0]  * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1]  * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2]  * (drflac_int64)pDecodedSamples[-3];
        prediction += coefficients[3]  * (drflac_int64)pDecodedSamples[-4];
        prediction += coefficients[4]  * (drflac_int64)pDecodedSamples[-5];
        prediction += coefficients[5]  * (drflac_int64)pDecodedSamples[-6];
        prediction += coefficients[6]  * (drflac_int64)pDecodedSamples[-7];
        prediction += coefficients[7]  * (drflac_int64)pDecodedSamples[-8];
        prediction += coefficients[8]  * (drflac_int64)pDecodedSamples[-9];
        prediction += coefficients[9]  * (drflac_int64)pDecodedSamples[-10];
        prediction += coefficients[10] * (drflac_int64)pDecodedSamples[-11];
        prediction += coefficients[11] * (drflac_int64)pDecodedSamples[-12];
    }
    else if (order == 2)
    {
        prediction  = coefficients[0] * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1] * (drflac_int64)pDecodedSamples[-2];
    }
    else if (order == 1)
    {
        prediction = coefficients[0] * (drflac_int64)pDecodedSamples[-1];
    }
    else if (order == 10)
    {
        prediction  = coefficients[0]  * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1]  * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2]  * (drflac_int64)pDecodedSamples[-3];
        prediction += coefficients[3]  * (drflac_int64)pDecodedSamples[-4];
        prediction += coefficients[4]  * (drflac_int64)pDecodedSamples[-5];
        prediction += coefficients[5]  * (drflac_int64)pDecodedSamples[-6];
        prediction += coefficients[6]  * (drflac_int64)pDecodedSamples[-7];
        prediction += coefficients[7]  * (drflac_int64)pDecodedSamples[-8];
        prediction += coefficients[8]  * (drflac_int64)pDecodedSamples[-9];
        prediction += coefficients[9]  * (drflac_int64)pDecodedSamples[-10];
    }
    else if (order == 9)
    {
        prediction  = coefficients[0]  * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1]  * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2]  * (drflac_int64)pDecodedSamples[-3];
        prediction += coefficients[3]  * (drflac_int64)pDecodedSamples[-4];
        prediction += coefficients[4]  * (drflac_int64)pDecodedSamples[-5];
        prediction += coefficients[5]  * (drflac_int64)pDecodedSamples[-6];
        prediction += coefficients[6]  * (drflac_int64)pDecodedSamples[-7];
        prediction += coefficients[7]  * (drflac_int64)pDecodedSamples[-8];
        prediction += coefficients[8]  * (drflac_int64)pDecodedSamples[-9];
    }
    else if (order == 11)
    {
        prediction  = coefficients[0]  * (drflac_int64)pDecodedSamples[-1];
        prediction += coefficients[1]  * (drflac_int64)pDecodedSamples[-2];
        prediction += coefficients[2]  * (drflac_int64)pDecodedSamples[-3];
        prediction += coefficients[3]  * (drflac_int64)pDecodedSamples[-4];
        prediction += coefficients[4]  * (drflac_int64)pDecodedSamples[-5];
        prediction += coefficients[5]  * (drflac_int64)pDecodedSamples[-6];
        prediction += coefficients[6]  * (drflac_int64)pDecodedSamples[-7];
        prediction += coefficients[7]  * (drflac_int64)pDecodedSamples[-8];
        prediction += coefficients[8]  * (drflac_int64)pDecodedSamples[-9];
        prediction += coefficients[9]  * (drflac_int64)pDecodedSamples[-10];
        prediction += coefficients[10] * (drflac_int64)pDecodedSamples[-11];
    }
    else
    {
        int j;

        prediction = 0;
        for (j = 0; j < (int)order; ++j) {
            prediction += coefficients[j] * (drflac_int64)pDecodedSamples[-j-1];
        }
    }
#endif

    /*
    VC++ optimizes this to a single jmp instruction, but only the 64-bit build. The 32-bit build generates less efficient code for some
    reason. The ugly version above is faster so we'll just switch between the two depending on the target platform.
    */

    return (drflac_int32)(prediction >> shift);
}

static  drflac_bool32 drflac__read_rice_parts_x1(drflac_bs* bs, drflac_uint8 riceParam, drflac_uint32* pZeroCounterOut, drflac_uint32* pRiceParamPartOut)
{
    drflac_uint32  riceParamPlus1 = riceParam + 1;
    /*drflac_cache_t riceParamPlus1Mask  = DRFLAC_CACHE_L1_SELECTION_MASK(riceParamPlus1);*/
    drflac_uint32  riceParamPlus1Shift = DRFLAC_CACHE_L1_SELECTION_SHIFT(bs, riceParamPlus1);
    drflac_uint32  riceParamPlus1MaxConsumedBits = DRFLAC_CACHE_L1_SIZE_BITS(bs) - riceParamPlus1;

    /*
    The idea here is to use local variables for the cache in an attempt to encourage the compiler to store them in registers. I have
    no idea how this will work in practice...
    */
    drflac_cache_t bs_cache = bs->cache;
    drflac_uint32  bs_consumedBits = bs->consumedBits;

    /* The first thing to do is find the first unset bit. Most likely a bit will be set in the current cache line. */
    drflac_uint32  lzcount = drflac__clz(bs_cache);
    if (lzcount < sizeof(bs_cache)*8) {
        pZeroCounterOut[0] = lzcount;

        /*
        It is most likely that the riceParam part (which comes after the zero counter) is also on this cache line. When extracting
        this, we include the set bit from the unary coded part because it simplifies cache management. This bit will be handled
        outside of this function at a higher level.
        */
    extract_rice_param_part:
        bs_cache       <<= lzcount;
        bs_consumedBits += lzcount;

        if (bs_consumedBits <= riceParamPlus1MaxConsumedBits) {
            /* Getting here means the rice parameter part is wholly contained within the current cache line. */
            pRiceParamPartOut[0] = (drflac_uint32)(bs_cache >> riceParamPlus1Shift);
            bs_cache       <<= riceParamPlus1;
            bs_consumedBits += riceParamPlus1;
        } else {
            drflac_uint32 riceParamPartHi;
            drflac_uint32 riceParamPartLo;
            drflac_uint32 riceParamPartLoBitCount;

            /*
            Getting here means the rice parameter part straddles the cache line. We need to read from the tail of the current cache
            line, reload the cache, and then combine it with the head of the next cache line.
            */

            /* Grab the high part of the rice parameter part. */
            riceParamPartHi = (drflac_uint32)(bs_cache >> riceParamPlus1Shift);

            /* Before reloading the cache we need to grab the size in bits of the low part. */
            riceParamPartLoBitCount = bs_consumedBits - riceParamPlus1MaxConsumedBits;

            /* Now reload the cache. */
            if (bs->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT(bs)) {
            #ifndef DR_FLAC_NO_CRC
                drflac__update_crc16(bs);
            #endif
                bs_cache = drflac__be2host__cache_line(bs->cacheL2[bs->nextL2Line++]);
                bs_consumedBits = riceParamPartLoBitCount;
            #ifndef DR_FLAC_NO_CRC
                bs->crc16Cache = bs_cache;
            #endif
            } else {
                /* Slow path. We need to fetch more data from the client. */
                if (!drflac__reload_cache(bs)) {
                    return DRFLAC_FALSE;
                }

                bs_cache = bs->cache;
                bs_consumedBits = bs->consumedBits + riceParamPartLoBitCount;
            }

            /* We should now have enough information to construct the rice parameter part. */
            riceParamPartLo = (drflac_uint32)(bs_cache >> (DRFLAC_CACHE_L1_SELECTION_SHIFT(bs, riceParamPartLoBitCount)));
            pRiceParamPartOut[0] = riceParamPartHi | riceParamPartLo;

            bs_cache <<= riceParamPartLoBitCount;
        }
    } else {
        /*
        Getting here means there are no bits set on the cache line. This is a less optimal case because we just wasted a call
        to drflac__clz() and we need to reload the cache.
        */
        drflac_uint32 zeroCounter = (drflac_uint32)(DRFLAC_CACHE_L1_SIZE_BITS(bs) - bs_consumedBits);
        for (;;) {
            if (bs->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT(bs)) {
            #ifndef DR_FLAC_NO_CRC
                drflac__update_crc16(bs);
            #endif
                bs_cache = drflac__be2host__cache_line(bs->cacheL2[bs->nextL2Line++]);
                bs_consumedBits = 0;
            #ifndef DR_FLAC_NO_CRC
                bs->crc16Cache = bs_cache;
            #endif
            } else {
                /* Slow path. We need to fetch more data from the client. */
                if (!drflac__reload_cache(bs)) {
                    return DRFLAC_FALSE;
                }

                bs_cache = bs->cache;
                bs_consumedBits = bs->consumedBits;
            }

            lzcount = drflac__clz(bs_cache);
            zeroCounter += lzcount;

            if (lzcount < sizeof(bs_cache)*8) {
                break;
            }
        }

        pZeroCounterOut[0] = zeroCounter;
        goto extract_rice_param_part;
    }

    /* Make sure the cache is restored at the end of it all. */
    bs->cache = bs_cache;
    bs->consumedBits = bs_consumedBits;

    return DRFLAC_TRUE;
}

static  drflac_bool32 drflac__seek_rice_parts(drflac_bs* bs, drflac_uint8 riceParam)
{
    drflac_uint32  riceParamPlus1 = riceParam + 1;
    drflac_uint32  riceParamPlus1MaxConsumedBits = DRFLAC_CACHE_L1_SIZE_BITS(bs) - riceParamPlus1;

    /*
    The idea here is to use local variables for the cache in an attempt to encourage the compiler to store them in registers. I have
    no idea how this will work in practice...
    */
    drflac_cache_t bs_cache = bs->cache;
    drflac_uint32  bs_consumedBits = bs->consumedBits;

    /* The first thing to do is find the first unset bit. Most likely a bit will be set in the current cache line. */
    drflac_uint32  lzcount = drflac__clz(bs_cache);
    if (lzcount < sizeof(bs_cache)*8) {
        /*
        It is most likely that the riceParam part (which comes after the zero counter) is also on this cache line. When extracting
        this, we include the set bit from the unary coded part because it simplifies cache management. This bit will be handled
        outside of this function at a higher level.
        */
    extract_rice_param_part:
        bs_cache       <<= lzcount;
        bs_consumedBits += lzcount;

        if (bs_consumedBits <= riceParamPlus1MaxConsumedBits) {
            /* Getting here means the rice parameter part is wholly contained within the current cache line. */
            bs_cache       <<= riceParamPlus1;
            bs_consumedBits += riceParamPlus1;
        } else {
            /*
            Getting here means the rice parameter part straddles the cache line. We need to read from the tail of the current cache
            line, reload the cache, and then combine it with the head of the next cache line.
            */

            /* Before reloading the cache we need to grab the size in bits of the low part. */
            drflac_uint32 riceParamPartLoBitCount = bs_consumedBits - riceParamPlus1MaxConsumedBits;

            /* Now reload the cache. */
            if (bs->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT(bs)) {
            #ifndef DR_FLAC_NO_CRC
                drflac__update_crc16(bs);
            #endif
                bs_cache = drflac__be2host__cache_line(bs->cacheL2[bs->nextL2Line++]);
                bs_consumedBits = riceParamPartLoBitCount;
            #ifndef DR_FLAC_NO_CRC
                bs->crc16Cache = bs_cache;
            #endif
            } else {
                /* Slow path. We need to fetch more data from the client. */
                if (!drflac__reload_cache(bs)) {
                    return DRFLAC_FALSE;
                }

                bs_cache = bs->cache;
                bs_consumedBits = bs->consumedBits + riceParamPartLoBitCount;
            }

            bs_cache <<= riceParamPartLoBitCount;
        }
    } else {
        /*
        Getting here means there are no bits set on the cache line. This is a less optimal case because we just wasted a call
        to drflac__clz() and we need to reload the cache.
        */
        for (;;) {
            if (bs->nextL2Line < DRFLAC_CACHE_L2_LINE_COUNT(bs)) {
            #ifndef DR_FLAC_NO_CRC
                drflac__update_crc16(bs);
            #endif
                bs_cache = drflac__be2host__cache_line(bs->cacheL2[bs->nextL2Line++]);
                bs_consumedBits = 0;
            #ifndef DR_FLAC_NO_CRC
                bs->crc16Cache = bs_cache;
            #endif
            } else {
                /* Slow path. We need to fetch more data from the client. */
                if (!drflac__reload_cache(bs)) {
                    return DRFLAC_FALSE;
                }

                bs_cache = bs->cache;
                bs_consumedBits = bs->consumedBits;
            }

            lzcount = drflac__clz(bs_cache);
            if (lzcount < sizeof(bs_cache)*8) {
                break;
            }
        }

        goto extract_rice_param_part;
    }

    /* Make sure the cache is restored at the end of it all. */
    bs->cache = bs_cache;
    bs->consumedBits = bs_consumedBits;

    return DRFLAC_TRUE;
}


static drflac_bool32 drflac__decode_samples_with_residual__rice__scalar_zeroorder(drflac_bs* bs, drflac_uint32 bitsPerSample, drflac_uint32 count, drflac_uint8 riceParam, drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pSamplesOut)
{
    drflac_uint32 t[2] = {0x00000000, 0xFFFFFFFF};
    drflac_uint32 zeroCountPart0;
    drflac_uint32 riceParamPart0;
    drflac_uint32 riceParamMask;
    drflac_uint32 i;

    (void)bitsPerSample;
    (void)order;
    (void)shift;
    (void)coefficients;

    riceParamMask  = (drflac_uint32)~((~0UL) << riceParam);

    i = 0;
    while (i < count) {
        /* Rice extraction. */
        if (!drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart0, &riceParamPart0)) {
            return DRFLAC_FALSE;
        }

        /* Rice reconstruction. */
        riceParamPart0 &= riceParamMask;
        riceParamPart0 |= (zeroCountPart0 << riceParam);
        riceParamPart0  = (riceParamPart0 >> 1) ^ t[riceParamPart0 & 0x01];

        pSamplesOut[i] = riceParamPart0;

        i += 1;
    }

    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__decode_samples_with_residual__rice__scalar(drflac_bs* bs, drflac_uint32 bitsPerSample, drflac_uint32 count, drflac_uint8 riceParam, drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pSamplesOut)
{
    drflac_uint32 t[2] = {0x00000000, 0xFFFFFFFF};
    drflac_uint32 zeroCountPart0;
    drflac_uint32 zeroCountPart1;
    drflac_uint32 zeroCountPart2;
    drflac_uint32 zeroCountPart3;
    drflac_uint32 riceParamPart0;
    drflac_uint32 riceParamPart1;
    drflac_uint32 riceParamPart2;
    drflac_uint32 riceParamPart3;
    drflac_uint32 riceParamMask;
    const drflac_int32* pSamplesOutEnd;
    drflac_uint32 i;

    if (order == 0) {
        return drflac__decode_samples_with_residual__rice__scalar_zeroorder(bs, bitsPerSample, count, riceParam, order, shift, coefficients, pSamplesOut);
    }

    riceParamMask  = (drflac_uint32)~((~0UL) << riceParam);
    pSamplesOutEnd = pSamplesOut + (count & ~3);

    if (bitsPerSample+shift > 32) {
        while (pSamplesOut < pSamplesOutEnd) {
            /*
            Rice extraction. It's faster to do this one at a time against local variables than it is to use the x4 version
            against an array. Not sure why, but perhaps it's making more efficient use of registers?
            */
            if (!drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart0, &riceParamPart0) ||
                !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart1, &riceParamPart1) ||
                !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart2, &riceParamPart2) ||
                !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart3, &riceParamPart3)) {
                return DRFLAC_FALSE;
            }

            riceParamPart0 &= riceParamMask;
            riceParamPart1 &= riceParamMask;
            riceParamPart2 &= riceParamMask;
            riceParamPart3 &= riceParamMask;

            riceParamPart0 |= (zeroCountPart0 << riceParam);
            riceParamPart1 |= (zeroCountPart1 << riceParam);
            riceParamPart2 |= (zeroCountPart2 << riceParam);
            riceParamPart3 |= (zeroCountPart3 << riceParam);

            riceParamPart0  = (riceParamPart0 >> 1) ^ t[riceParamPart0 & 0x01];
            riceParamPart1  = (riceParamPart1 >> 1) ^ t[riceParamPart1 & 0x01];
            riceParamPart2  = (riceParamPart2 >> 1) ^ t[riceParamPart2 & 0x01];
            riceParamPart3  = (riceParamPart3 >> 1) ^ t[riceParamPart3 & 0x01];

            pSamplesOut[0] = riceParamPart0 + drflac__calculate_prediction_64(order, shift, coefficients, pSamplesOut + 0);
            pSamplesOut[1] = riceParamPart1 + drflac__calculate_prediction_64(order, shift, coefficients, pSamplesOut + 1);
            pSamplesOut[2] = riceParamPart2 + drflac__calculate_prediction_64(order, shift, coefficients, pSamplesOut + 2);
            pSamplesOut[3] = riceParamPart3 + drflac__calculate_prediction_64(order, shift, coefficients, pSamplesOut + 3);

            pSamplesOut += 4;
        }
    } else {
        while (pSamplesOut < pSamplesOutEnd) {
            if (!drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart0, &riceParamPart0) ||
                !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart1, &riceParamPart1) ||
                !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart2, &riceParamPart2) ||
                !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart3, &riceParamPart3)) {
                return DRFLAC_FALSE;
            }

            riceParamPart0 &= riceParamMask;
            riceParamPart1 &= riceParamMask;
            riceParamPart2 &= riceParamMask;
            riceParamPart3 &= riceParamMask;

            riceParamPart0 |= (zeroCountPart0 << riceParam);
            riceParamPart1 |= (zeroCountPart1 << riceParam);
            riceParamPart2 |= (zeroCountPart2 << riceParam);
            riceParamPart3 |= (zeroCountPart3 << riceParam);

            riceParamPart0  = (riceParamPart0 >> 1) ^ t[riceParamPart0 & 0x01];
            riceParamPart1  = (riceParamPart1 >> 1) ^ t[riceParamPart1 & 0x01];
            riceParamPart2  = (riceParamPart2 >> 1) ^ t[riceParamPart2 & 0x01];
            riceParamPart3  = (riceParamPart3 >> 1) ^ t[riceParamPart3 & 0x01];

            pSamplesOut[0] = riceParamPart0 + drflac__calculate_prediction_32(order, shift, coefficients, pSamplesOut + 0);
            pSamplesOut[1] = riceParamPart1 + drflac__calculate_prediction_32(order, shift, coefficients, pSamplesOut + 1);
            pSamplesOut[2] = riceParamPart2 + drflac__calculate_prediction_32(order, shift, coefficients, pSamplesOut + 2);
            pSamplesOut[3] = riceParamPart3 + drflac__calculate_prediction_32(order, shift, coefficients, pSamplesOut + 3);

            pSamplesOut += 4;
        }
    }

    i = (count & ~3);
    while (i < count) {
        /* Rice extraction. */
        if (!drflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart0, &riceParamPart0)) {
            return DRFLAC_FALSE;
        }

        /* Rice reconstruction. */
        riceParamPart0 &= riceParamMask;
        riceParamPart0 |= (zeroCountPart0 << riceParam);
        riceParamPart0  = (riceParamPart0 >> 1) ^ t[riceParamPart0 & 0x01];
        /*riceParamPart0  = (riceParamPart0 >> 1) ^ (~(riceParamPart0 & 0x01) + 1);*/

        /* Sample reconstruction. */
        if (bitsPerSample+shift > 32) {
            pSamplesOut[0] = riceParamPart0 + drflac__calculate_prediction_64(order, shift, coefficients, pSamplesOut + 0);
        } else {
            pSamplesOut[0] = riceParamPart0 + drflac__calculate_prediction_32(order, shift, coefficients, pSamplesOut + 0);
        }

        i += 1;
        pSamplesOut += 1;
    }

    return DRFLAC_TRUE;
}

#if defined(DRFLAC_SUPPORT_NEON)
static  void drflac__vst2q_s16(drflac_int16* p, int16x4x2_t x)
{
    vst1q_s16(p, vcombine_s16(x.val[0], x.val[1]));
}

static  int32x4_t drflac__valignrq_s32_1(int32x4_t a, int32x4_t b)
{
    /* Equivalent to SSE's _mm_alignr_epi8(a, b, 4) */

    /* Reference */
    /*return drflac__vdupq_n_s32x4(
        vgetq_lane_s32(a, 0),
        vgetq_lane_s32(b, 3),
        vgetq_lane_s32(b, 2),
        vgetq_lane_s32(b, 1)
    );*/

    return vextq_s32(b, a, 1);
}

static  uint32x4_t drflac__valignrq_u32_1(uint32x4_t a, uint32x4_t b)
{
    /* Equivalent to SSE's _mm_alignr_epi8(a, b, 4) */

    /* Reference */
    /*return drflac__vdupq_n_s32x4(
        vgetq_lane_s32(a, 0),
        vgetq_lane_s32(b, 3),
        vgetq_lane_s32(b, 2),
        vgetq_lane_s32(b, 1)
    );*/

    return vextq_u32(b, a, 1);
}

static  int32x2_t drflac__vhaddq_s32(int32x4_t x)
{
    /* The sum must end up in position 0. */

    /* Reference */
    /*return vdupq_n_s32(
        vgetq_lane_s32(x, 3) +
        vgetq_lane_s32(x, 2) +
        vgetq_lane_s32(x, 1) +
        vgetq_lane_s32(x, 0)
    );*/

    int32x2_t r = vadd_s32(vget_high_s32(x), vget_low_s32(x));
    return vpadd_s32(r, r);
}

static  int64x1_t drflac__vhaddq_s64(int64x2_t x)
{
    return vadd_s64(vget_high_s64(x), vget_low_s64(x));
}

static  int32x4_t drflac__vrevq_s32(int32x4_t x)
{
    /* Reference */
    /*return drflac__vdupq_n_s32x4(
        vgetq_lane_s32(x, 0),
        vgetq_lane_s32(x, 1),
        vgetq_lane_s32(x, 2),
        vgetq_lane_s32(x, 3)
    );*/

    return vrev64q_s32(vcombine_s32(vget_high_s32(x), vget_low_s32(x)));
}

static  uint32x4_t drflac__vnotq_u32(uint32x4_t x)
{
    return veorq_u32(x, vdupq_n_u32(0xFFFFFFFF));
}

static drflac_bool32 drflac__decode_samples_with_residual__rice__neon_32(drflac_bs* bs, drflac_uint32 count, drflac_uint8 riceParam, drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pSamplesOut)
{
    int i;
    drflac_uint32 riceParamMask;
    drflac_int32* pDecodedSamples    = pSamplesOut;
    drflac_int32* pDecodedSamplesEnd = pSamplesOut + (count & ~3);
    drflac_uint32 zeroCountParts[4];
    drflac_uint32 riceParamParts[4];
    int32x4_t coefficients128_0;
    int32x4_t coefficients128_4;
    int32x4_t coefficients128_8;
    int32x4_t samples128_0;
    int32x4_t samples128_4;
    int32x4_t samples128_8;
    uint32x4_t riceParamMask128;
    int32x4_t riceParam128;
    int32x2_t shift64;
    uint32x4_t one128;

    const drflac_uint32 t[2] = {0x00000000, 0xFFFFFFFF};

    riceParamMask    = ~((~0UL) << riceParam);
    riceParamMask128 = vdupq_n_u32(riceParamMask);

    riceParam128 = vdupq_n_s32(riceParam);
    shift64 = vdup_n_s32(-shift); /* Negate the shift because we'll be doing a variable shift using vshlq_s32(). */
    one128 = vdupq_n_u32(1);

    /*
    Pre-loading the coefficients and prior samples is annoying because we need to ensure we don't try reading more than
    what's available in the input buffers. It would be conenient to use a fall-through switch to do this, but this results
    in strict aliasing warnings with GCC. To work around this I'm just doing something hacky. This feels a bit convoluted
    so I think there's opportunity for this to be simplified.
    */
    {
        int runningOrder = order;
        drflac_int32 tempC[4] = {0, 0, 0, 0};
        drflac_int32 tempS[4] = {0, 0, 0, 0};

        /* 0 - 3. */
        if (runningOrder >= 4) {
            coefficients128_0 = vld1q_s32(coefficients + 0);
            samples128_0      = vld1q_s32(pSamplesOut  - 4);
            runningOrder -= 4;
        } else {
            switch (runningOrder) {
                case 3: tempC[2] = coefficients[2]; tempS[1] = pSamplesOut[-3]; /* fallthrough */
                case 2: tempC[1] = coefficients[1]; tempS[2] = pSamplesOut[-2]; /* fallthrough */
                case 1: tempC[0] = coefficients[0]; tempS[3] = pSamplesOut[-1]; /* fallthrough */
            }

            coefficients128_0 = vld1q_s32(tempC);
            samples128_0      = vld1q_s32(tempS);
            runningOrder = 0;
        }

        /* 4 - 7 */
        if (runningOrder >= 4) {
            coefficients128_4 = vld1q_s32(coefficients + 4);
            samples128_4      = vld1q_s32(pSamplesOut  - 8);
            runningOrder -= 4;
        } else {
            switch (runningOrder) {
                case 3: tempC[2] = coefficients[6]; tempS[1] = pSamplesOut[-7]; /* fallthrough */
                case 2: tempC[1] = coefficients[5]; tempS[2] = pSamplesOut[-6]; /* fallthrough */
                case 1: tempC[0] = coefficients[4]; tempS[3] = pSamplesOut[-5]; /* fallthrough */
            }

            coefficients128_4 = vld1q_s32(tempC);
            samples128_4      = vld1q_s32(tempS);
            runningOrder = 0;
        }

        /* 8 - 11 */
        if (runningOrder == 4) {
            coefficients128_8 = vld1q_s32(coefficients + 8);
            samples128_8      = vld1q_s32(pSamplesOut  - 12);
            runningOrder -= 4;
        } else {
            switch (runningOrder) {
                case 3: tempC[2] = coefficients[10]; tempS[1] = pSamplesOut[-11]; /* fallthrough */
                case 2: tempC[1] = coefficients[ 9]; tempS[2] = pSamplesOut[-10]; /* fallthrough */
                case 1: tempC[0] = coefficients[ 8]; tempS[3] = pSamplesOut[- 9]; /* fallthrough */
            }

            coefficients128_8 = vld1q_s32(tempC);
            samples128_8      = vld1q_s32(tempS);
            runningOrder = 0;
        }

        /* Coefficients need to be shuffled for our streaming algorithm below to work. Samples are already in the correct order from the loading routine above. */
        coefficients128_0 = drflac__vrevq_s32(coefficients128_0);
        coefficients128_4 = drflac__vrevq_s32(coefficients128_4);
        coefficients128_8 = drflac__vrevq_s32(coefficients128_8);
    }

    /* For this version we are doing one sample at a time. */
    while (pDecodedSamples < pDecodedSamplesEnd) {
        int32x4_t prediction128;
        int32x2_t prediction64;
        uint32x4_t zeroCountPart128;
        uint32x4_t riceParamPart128;

        if (!drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[0], &riceParamParts[0]) ||
            !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[1], &riceParamParts[1]) ||
            !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[2], &riceParamParts[2]) ||
            !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[3], &riceParamParts[3])) {
            return DRFLAC_FALSE;
        }

        zeroCountPart128 = vld1q_u32(zeroCountParts);
        riceParamPart128 = vld1q_u32(riceParamParts);

        riceParamPart128 = vandq_u32(riceParamPart128, riceParamMask128);
        riceParamPart128 = vorrq_u32(riceParamPart128, vshlq_u32(zeroCountPart128, riceParam128));
        riceParamPart128 = veorq_u32(vshrq_n_u32(riceParamPart128, 1), vaddq_u32(drflac__vnotq_u32(vandq_u32(riceParamPart128, one128)), one128));

        if (order <= 4) {
            for (i = 0; i < 4; i += 1) {
                prediction128 = vmulq_s32(coefficients128_0, samples128_0);

                /* Horizontal add and shift. */
                prediction64 = drflac__vhaddq_s32(prediction128);
                prediction64 = vshl_s32(prediction64, shift64);
                prediction64 = vadd_s32(prediction64, vget_low_s32(vreinterpretq_s32_u32(riceParamPart128)));

                samples128_0 = drflac__valignrq_s32_1(vcombine_s32(prediction64, vdup_n_s32(0)), samples128_0);
                riceParamPart128 = drflac__valignrq_u32_1(vdupq_n_u32(0), riceParamPart128);
            }
        } else if (order <= 8) {
            for (i = 0; i < 4; i += 1) {
                prediction128 =                vmulq_s32(coefficients128_4, samples128_4);
                prediction128 = vmlaq_s32(prediction128, coefficients128_0, samples128_0);

                /* Horizontal add and shift. */
                prediction64 = drflac__vhaddq_s32(prediction128);
                prediction64 = vshl_s32(prediction64, shift64);
                prediction64 = vadd_s32(prediction64, vget_low_s32(vreinterpretq_s32_u32(riceParamPart128)));

                samples128_4 = drflac__valignrq_s32_1(samples128_0, samples128_4);
                samples128_0 = drflac__valignrq_s32_1(vcombine_s32(prediction64, vdup_n_s32(0)), samples128_0);
                riceParamPart128 = drflac__valignrq_u32_1(vdupq_n_u32(0), riceParamPart128);
            }
        } else {
            for (i = 0; i < 4; i += 1) {
                prediction128 =                vmulq_s32(coefficients128_8, samples128_8);
                prediction128 = vmlaq_s32(prediction128, coefficients128_4, samples128_4);
                prediction128 = vmlaq_s32(prediction128, coefficients128_0, samples128_0);

                /* Horizontal add and shift. */
                prediction64 = drflac__vhaddq_s32(prediction128);
                prediction64 = vshl_s32(prediction64, shift64);
                prediction64 = vadd_s32(prediction64, vget_low_s32(vreinterpretq_s32_u32(riceParamPart128)));

                samples128_8 = drflac__valignrq_s32_1(samples128_4, samples128_8);
                samples128_4 = drflac__valignrq_s32_1(samples128_0, samples128_4);
                samples128_0 = drflac__valignrq_s32_1(vcombine_s32(prediction64, vdup_n_s32(0)), samples128_0);
                riceParamPart128 = drflac__valignrq_u32_1(vdupq_n_u32(0), riceParamPart128);
            }
        }

        /* We store samples in groups of 4. */
        vst1q_s32(pDecodedSamples, samples128_0);
        pDecodedSamples += 4;
    }

    /* Make sure we process the last few samples. */
    i = (count & ~3);
    while (i < (int)count) {
        /* Rice extraction. */
        if (!drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[0], &riceParamParts[0])) {
            return DRFLAC_FALSE;
        }

        /* Rice reconstruction. */
        riceParamParts[0] &= riceParamMask;
        riceParamParts[0] |= (zeroCountParts[0] << riceParam);
        riceParamParts[0]  = (riceParamParts[0] >> 1) ^ t[riceParamParts[0] & 0x01];

        /* Sample reconstruction. */
        pDecodedSamples[0] = riceParamParts[0] + drflac__calculate_prediction_32(order, shift, coefficients, pDecodedSamples);

        i += 1;
        pDecodedSamples += 1;
    }

    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__decode_samples_with_residual__rice__neon_64(drflac_bs* bs, drflac_uint32 count, drflac_uint8 riceParam, drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pSamplesOut)
{
    int i;
    drflac_uint32 riceParamMask;
    drflac_int32* pDecodedSamples    = pSamplesOut;
    drflac_int32* pDecodedSamplesEnd = pSamplesOut + (count & ~3);
    drflac_uint32 zeroCountParts[4];
    drflac_uint32 riceParamParts[4];
    int32x4_t coefficients128_0;
    int32x4_t coefficients128_4;
    int32x4_t coefficients128_8;
    int32x4_t samples128_0;
    int32x4_t samples128_4;
    int32x4_t samples128_8;
    uint32x4_t riceParamMask128;
    int32x4_t riceParam128;
    int64x1_t shift64;
    uint32x4_t one128;

    const drflac_uint32 t[2] = {0x00000000, 0xFFFFFFFF};

    riceParamMask    = ~((~0UL) << riceParam);
    riceParamMask128 = vdupq_n_u32(riceParamMask);

    riceParam128 = vdupq_n_s32(riceParam);
    shift64 = vdup_n_s64(-shift); /* Negate the shift because we'll be doing a variable shift using vshlq_s32(). */
    one128 = vdupq_n_u32(1);

    /*
    Pre-loading the coefficients and prior samples is annoying because we need to ensure we don't try reading more than
    what's available in the input buffers. It would be conenient to use a fall-through switch to do this, but this results
    in strict aliasing warnings with GCC. To work around this I'm just doing something hacky. This feels a bit convoluted
    so I think there's opportunity for this to be simplified.
    */
    {
        int runningOrder = order;
        drflac_int32 tempC[4] = {0, 0, 0, 0};
        drflac_int32 tempS[4] = {0, 0, 0, 0};

        /* 0 - 3. */
        if (runningOrder >= 4) {
            coefficients128_0 = vld1q_s32(coefficients + 0);
            samples128_0      = vld1q_s32(pSamplesOut  - 4);
            runningOrder -= 4;
        } else {
            switch (runningOrder) {
                case 3: tempC[2] = coefficients[2]; tempS[1] = pSamplesOut[-3]; /* fallthrough */
                case 2: tempC[1] = coefficients[1]; tempS[2] = pSamplesOut[-2]; /* fallthrough */
                case 1: tempC[0] = coefficients[0]; tempS[3] = pSamplesOut[-1]; /* fallthrough */
            }

            coefficients128_0 = vld1q_s32(tempC);
            samples128_0      = vld1q_s32(tempS);
            runningOrder = 0;
        }

        /* 4 - 7 */
        if (runningOrder >= 4) {
            coefficients128_4 = vld1q_s32(coefficients + 4);
            samples128_4      = vld1q_s32(pSamplesOut  - 8);
            runningOrder -= 4;
        } else {
            switch (runningOrder) {
                case 3: tempC[2] = coefficients[6]; tempS[1] = pSamplesOut[-7]; /* fallthrough */
                case 2: tempC[1] = coefficients[5]; tempS[2] = pSamplesOut[-6]; /* fallthrough */
                case 1: tempC[0] = coefficients[4]; tempS[3] = pSamplesOut[-5]; /* fallthrough */
            }

            coefficients128_4 = vld1q_s32(tempC);
            samples128_4      = vld1q_s32(tempS);
            runningOrder = 0;
        }

        /* 8 - 11 */
        if (runningOrder == 4) {
            coefficients128_8 = vld1q_s32(coefficients + 8);
            samples128_8      = vld1q_s32(pSamplesOut  - 12);
            runningOrder -= 4;
        } else {
            switch (runningOrder) {
                case 3: tempC[2] = coefficients[10]; tempS[1] = pSamplesOut[-11]; /* fallthrough */
                case 2: tempC[1] = coefficients[ 9]; tempS[2] = pSamplesOut[-10]; /* fallthrough */
                case 1: tempC[0] = coefficients[ 8]; tempS[3] = pSamplesOut[- 9]; /* fallthrough */
            }

            coefficients128_8 = vld1q_s32(tempC);
            samples128_8      = vld1q_s32(tempS);
            runningOrder = 0;
        }

        /* Coefficients need to be shuffled for our streaming algorithm below to work. Samples are already in the correct order from the loading routine above. */
        coefficients128_0 = drflac__vrevq_s32(coefficients128_0);
        coefficients128_4 = drflac__vrevq_s32(coefficients128_4);
        coefficients128_8 = drflac__vrevq_s32(coefficients128_8);
    }

    /* For this version we are doing one sample at a time. */
    while (pDecodedSamples < pDecodedSamplesEnd) {
        int64x2_t prediction128;
        uint32x4_t zeroCountPart128;
        uint32x4_t riceParamPart128;

        if (!drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[0], &riceParamParts[0]) ||
            !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[1], &riceParamParts[1]) ||
            !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[2], &riceParamParts[2]) ||
            !drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[3], &riceParamParts[3])) {
            return DRFLAC_FALSE;
        }

        zeroCountPart128 = vld1q_u32(zeroCountParts);
        riceParamPart128 = vld1q_u32(riceParamParts);

        riceParamPart128 = vandq_u32(riceParamPart128, riceParamMask128);
        riceParamPart128 = vorrq_u32(riceParamPart128, vshlq_u32(zeroCountPart128, riceParam128));
        riceParamPart128 = veorq_u32(vshrq_n_u32(riceParamPart128, 1), vaddq_u32(drflac__vnotq_u32(vandq_u32(riceParamPart128, one128)), one128));

        for (i = 0; i < 4; i += 1) {
            int64x1_t prediction64;

            prediction128 = veorq_s64(prediction128, prediction128);    /* Reset to 0. */
            switch (order)
            {
            case 12:
            case 11: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_low_s32(coefficients128_8), vget_low_s32(samples128_8)));
            case 10:
            case  9: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_high_s32(coefficients128_8), vget_high_s32(samples128_8)));
            case  8:
            case  7: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_low_s32(coefficients128_4), vget_low_s32(samples128_4)));
            case  6:
            case  5: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_high_s32(coefficients128_4), vget_high_s32(samples128_4)));
            case  4:
            case  3: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_low_s32(coefficients128_0), vget_low_s32(samples128_0)));
            case  2:
            case  1: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_high_s32(coefficients128_0), vget_high_s32(samples128_0)));
            }

            /* Horizontal add and shift. */
            prediction64 = drflac__vhaddq_s64(prediction128);
            prediction64 = vshl_s64(prediction64, shift64);
            prediction64 = vadd_s64(prediction64, vdup_n_s64(vgetq_lane_u32(riceParamPart128, 0)));

            /* Our value should be sitting in prediction64[0]. We need to combine this with our SSE samples. */
            samples128_8 = drflac__valignrq_s32_1(samples128_4, samples128_8);
            samples128_4 = drflac__valignrq_s32_1(samples128_0, samples128_4);
            samples128_0 = drflac__valignrq_s32_1(vcombine_s32(vreinterpret_s32_s64(prediction64), vdup_n_s32(0)), samples128_0);

            /* Slide our rice parameter down so that the value in position 0 contains the next one to process. */
            riceParamPart128 = drflac__valignrq_u32_1(vdupq_n_u32(0), riceParamPart128);
        }

        /* We store samples in groups of 4. */
        vst1q_s32(pDecodedSamples, samples128_0);
        pDecodedSamples += 4;
    }

    /* Make sure we process the last few samples. */
    i = (count & ~3);
    while (i < (int)count) {
        /* Rice extraction. */
        if (!drflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[0], &riceParamParts[0])) {
            return DRFLAC_FALSE;
        }

        /* Rice reconstruction. */
        riceParamParts[0] &= riceParamMask;
        riceParamParts[0] |= (zeroCountParts[0] << riceParam);
        riceParamParts[0]  = (riceParamParts[0] >> 1) ^ t[riceParamParts[0] & 0x01];

        /* Sample reconstruction. */
        pDecodedSamples[0] = riceParamParts[0] + drflac__calculate_prediction_64(order, shift, coefficients, pDecodedSamples);

        i += 1;
        pDecodedSamples += 1;
    }

    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__decode_samples_with_residual__rice__neon(drflac_bs* bs, drflac_uint32 bitsPerSample, drflac_uint32 count, drflac_uint8 riceParam, drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pSamplesOut)
{

    /* In my testing the order is rarely > 12, so in this case I'm going to simplify the NEON implementation by only handling order <= 12. */
    if (order > 0 && order <= 12) {
        if (bitsPerSample+shift > 32) {
            return drflac__decode_samples_with_residual__rice__neon_64(bs, count, riceParam, order, shift, coefficients, pSamplesOut);
        } else {
            return drflac__decode_samples_with_residual__rice__neon_32(bs, count, riceParam, order, shift, coefficients, pSamplesOut);
        }
    } else {
        return drflac__decode_samples_with_residual__rice__scalar(bs, bitsPerSample, count, riceParam, order, shift, coefficients, pSamplesOut);
    }
}
#endif

static drflac_bool32 drflac__decode_samples_with_residual__rice(drflac_bs* bs, drflac_uint32 bitsPerSample, drflac_uint32 count, drflac_uint8 riceParam, drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pSamplesOut)
{
#if defined(DRFLAC_SUPPORT_SSE41)
    if (drflac__gIsSSE41Supported) {
        return drflac__decode_samples_with_residual__rice__sse41(bs, bitsPerSample, count, riceParam, order, shift, coefficients, pSamplesOut);
    } else
#elif defined(DRFLAC_SUPPORT_NEON)
    if (drflac__gIsNEONSupported) {
        return drflac__decode_samples_with_residual__rice__neon(bs, bitsPerSample, count, riceParam, order, shift, coefficients, pSamplesOut);
    } else
#endif
    {
        /* Scalar fallback. */
    #if 0
        return drflac__decode_samples_with_residual__rice__reference(bs, bitsPerSample, count, riceParam, order, shift, coefficients, pSamplesOut);
    #else
        return drflac__decode_samples_with_residual__rice__scalar(bs, bitsPerSample, count, riceParam, order, shift, coefficients, pSamplesOut);
    #endif
    }
}

/* Reads and seeks past a string of residual values as Rice codes. The decoder should be sitting on the first bit of the Rice codes. */
static drflac_bool32 drflac__read_and_seek_residual__rice(drflac_bs* bs, drflac_uint32 count, drflac_uint8 riceParam)
{
    drflac_uint32 i;

    for (i = 0; i < count; ++i) {
        if (!drflac__seek_rice_parts(bs, riceParam)) {
            return DRFLAC_FALSE;
        }
    }

    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__decode_samples_with_residual__unencoded(drflac_bs* bs, drflac_uint32 bitsPerSample, drflac_uint32 count, drflac_uint8 unencodedBitsPerSample, drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pSamplesOut)
{
    drflac_uint32 i;

    for (i = 0; i < count; ++i) {
        if (unencodedBitsPerSample > 0) {
            if (!drflac__read_int32(bs, unencodedBitsPerSample, pSamplesOut + i)) {
                return DRFLAC_FALSE;
            }
        } else {
            pSamplesOut[i] = 0;
        }

        if (bitsPerSample >= 24) {
            pSamplesOut[i] += drflac__calculate_prediction_64(order, shift, coefficients, pSamplesOut + i);
        } else {
            pSamplesOut[i] += drflac__calculate_prediction_32(order, shift, coefficients, pSamplesOut + i);
        }
    }

    return DRFLAC_TRUE;
}


/*
Reads and decodes the residual for the sub-frame the decoder is currently sitting on. This function should be called
when the decoder is sitting at the very start of the RESIDUAL block. The first <order> residuals will be ignored. The
<blockSize> and <order> parameters are used to determine how many residual values need to be decoded.
*/
static drflac_bool32 drflac__decode_samples_with_residual(drflac_bs* bs, drflac_uint32 bitsPerSample, drflac_uint32 blockSize, drflac_uint32 order, drflac_int32 shift, const drflac_int32* coefficients, drflac_int32* pDecodedSamples)
{
    drflac_uint8 residualMethod;
    drflac_uint8 partitionOrder;
    drflac_uint32 samplesInPartition;
    drflac_uint32 partitionsRemaining;

    if (!drflac__read_uint8(bs, 2, &residualMethod)) {
        return DRFLAC_FALSE;
    }

    if (residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE && residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
        return DRFLAC_FALSE;    /* Unknown or unsupported residual coding method. */
    }

    /* Ignore the first <order> values. */
    pDecodedSamples += order;

    if (!drflac__read_uint8(bs, 4, &partitionOrder)) {
        return DRFLAC_FALSE;
    }

    /*
    From the FLAC spec:
      The Rice partition order in a Rice-coded residual section must be less than or equal to 8.
    */
    if (partitionOrder > 8) {
        return DRFLAC_FALSE;
    }

    /* Validation check. */
    if ((blockSize / (1 << partitionOrder)) <= order) {
        return DRFLAC_FALSE;
    }

    samplesInPartition = (blockSize / (1 << partitionOrder)) - order;
    partitionsRemaining = (1 << partitionOrder);
    for (;;) {
        drflac_uint8 riceParam = 0;
        if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
            if (!drflac__read_uint8(bs, 4, &riceParam)) {
                return DRFLAC_FALSE;
            }
            if (riceParam == 15) {
                riceParam = 0xFF;
            }
        } else if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
            if (!drflac__read_uint8(bs, 5, &riceParam)) {
                return DRFLAC_FALSE;
            }
            if (riceParam == 31) {
                riceParam = 0xFF;
            }
        }

        if (riceParam != 0xFF) {
            if (!drflac__decode_samples_with_residual__rice(bs, bitsPerSample, samplesInPartition, riceParam, order, shift, coefficients, pDecodedSamples)) {
                return DRFLAC_FALSE;
            }
        } else {
            unsigned char unencodedBitsPerSample = 0;
            if (!drflac__read_uint8(bs, 5, &unencodedBitsPerSample)) {
                return DRFLAC_FALSE;
            }

            if (!drflac__decode_samples_with_residual__unencoded(bs, bitsPerSample, samplesInPartition, unencodedBitsPerSample, order, shift, coefficients, pDecodedSamples)) {
                return DRFLAC_FALSE;
            }
        }

        pDecodedSamples += samplesInPartition;

        if (partitionsRemaining == 1) {
            break;
        }

        partitionsRemaining -= 1;

        if (partitionOrder != 0) {
            samplesInPartition = blockSize / (1 << partitionOrder);
        }
    }

    return DRFLAC_TRUE;
}

/*
Reads and seeks past the residual for the sub-frame the decoder is currently sitting on. This function should be called
when the decoder is sitting at the very start of the RESIDUAL block. The first <order> residuals will be set to 0. The
<blockSize> and <order> parameters are used to determine how many residual values need to be decoded.
*/
static drflac_bool32 drflac__read_and_seek_residual(drflac_bs* bs, drflac_uint32 blockSize, drflac_uint32 order)
{
    drflac_uint8 residualMethod;
    drflac_uint8 partitionOrder;
    drflac_uint32 samplesInPartition;
    drflac_uint32 partitionsRemaining;

    if (!drflac__read_uint8(bs, 2, &residualMethod)) {
        return DRFLAC_FALSE;
    }

    if (residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE && residualMethod != DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
        return DRFLAC_FALSE;    /* Unknown or unsupported residual coding method. */
    }

    if (!drflac__read_uint8(bs, 4, &partitionOrder)) {
        return DRFLAC_FALSE;
    }

    /*
    From the FLAC spec:
      The Rice partition order in a Rice-coded residual section must be less than or equal to 8.
    */
    if (partitionOrder > 8) {
        return DRFLAC_FALSE;
    }

    /* Validation check. */
    if ((blockSize / (1 << partitionOrder)) <= order) {
        return DRFLAC_FALSE;
    }

    samplesInPartition = (blockSize / (1 << partitionOrder)) - order;
    partitionsRemaining = (1 << partitionOrder);
    for (;;)
    {
        drflac_uint8 riceParam = 0;
        if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
            if (!drflac__read_uint8(bs, 4, &riceParam)) {
                return DRFLAC_FALSE;
            }
            if (riceParam == 15) {
                riceParam = 0xFF;
            }
        } else if (residualMethod == DRFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
            if (!drflac__read_uint8(bs, 5, &riceParam)) {
                return DRFLAC_FALSE;
            }
            if (riceParam == 31) {
                riceParam = 0xFF;
            }
        }

        if (riceParam != 0xFF) {
            if (!drflac__read_and_seek_residual__rice(bs, samplesInPartition, riceParam)) {
                return DRFLAC_FALSE;
            }
        } else {
            unsigned char unencodedBitsPerSample = 0;
            if (!drflac__read_uint8(bs, 5, &unencodedBitsPerSample)) {
                return DRFLAC_FALSE;
            }

            if (!drflac__seek_bits(bs, unencodedBitsPerSample * samplesInPartition)) {
                return DRFLAC_FALSE;
            }
        }


        if (partitionsRemaining == 1) {
            break;
        }

        partitionsRemaining -= 1;
        samplesInPartition = blockSize / (1 << partitionOrder);
    }

    return DRFLAC_TRUE;
}


static drflac_bool32 drflac__decode_samples__constant(drflac_bs* bs, drflac_uint32 blockSize, drflac_uint32 subframeBitsPerSample, drflac_int32* pDecodedSamples)
{
    drflac_uint32 i;

    /* Only a single sample needs to be decoded here. */
    drflac_int32 sample;
    if (!drflac__read_int32(bs, subframeBitsPerSample, &sample)) {
        return DRFLAC_FALSE;
    }

    /*
    We don't really need to expand this, but it does simplify the process of reading samples. If this becomes a performance issue (unlikely)
    we'll want to look at a more efficient way.
    */
    for (i = 0; i < blockSize; ++i) {
        pDecodedSamples[i] = sample;
    }

    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__decode_samples__verbatim(drflac_bs* bs, drflac_uint32 blockSize, drflac_uint32 subframeBitsPerSample, drflac_int32* pDecodedSamples)
{
    drflac_uint32 i;

    for (i = 0; i < blockSize; ++i) {
        drflac_int32 sample;
        if (!drflac__read_int32(bs, subframeBitsPerSample, &sample)) {
            return DRFLAC_FALSE;
        }

        pDecodedSamples[i] = sample;
    }

    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__decode_samples__fixed(drflac_bs* bs, drflac_uint32 blockSize, drflac_uint32 subframeBitsPerSample, drflac_uint8 lpcOrder, drflac_int32* pDecodedSamples)
{
    drflac_uint32 i;

    static drflac_int32 lpcCoefficientsTable[5][4] = {
        {0,  0, 0,  0},
        {1,  0, 0,  0},
        {2, -1, 0,  0},
        {3, -3, 1,  0},
        {4, -6, 4, -1}
    };

    /* Warm up samples and coefficients. */
    for (i = 0; i < lpcOrder; ++i) {
        drflac_int32 sample;
        if (!drflac__read_int32(bs, subframeBitsPerSample, &sample)) {
            return DRFLAC_FALSE;
        }

        pDecodedSamples[i] = sample;
    }

    if (!drflac__decode_samples_with_residual(bs, subframeBitsPerSample, blockSize, lpcOrder, 0, lpcCoefficientsTable[lpcOrder], pDecodedSamples)) {
        return DRFLAC_FALSE;
    }

    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__decode_samples__lpc(drflac_bs* bs, drflac_uint32 blockSize, drflac_uint32 bitsPerSample, drflac_uint8 lpcOrder, drflac_int32* pDecodedSamples)
{
    drflac_uint8 i;
    drflac_uint8 lpcPrecision;
    drflac_int8 lpcShift = 0;
    drflac_int32 coefficients[32];

    /* Warm up samples. */
    for (i = 0; i < lpcOrder; ++i) {
        drflac_int32 sample;
        if (!drflac__read_int32(bs, bitsPerSample, &sample)) {
            return DRFLAC_FALSE;
        }

        pDecodedSamples[i] = sample;
    }

    if (!drflac__read_uint8(bs, 4, &lpcPrecision)) {
        return DRFLAC_FALSE;
    }
    if (lpcPrecision == 15) {
        return DRFLAC_FALSE;    /* Invalid. */
    }
    lpcPrecision += 1;

    if (!drflac__read_int8(bs, 5, &lpcShift)) {
        return DRFLAC_FALSE;
    }

    DRFLAC_ZERO_MEMORY(coefficients, sizeof(coefficients));
    for (i = 0; i < lpcOrder; ++i) {
        if (!drflac__read_int32(bs, lpcPrecision, coefficients + i)) {
            return DRFLAC_FALSE;
        }
    }

    if (!drflac__decode_samples_with_residual(bs, bitsPerSample, blockSize, lpcOrder, lpcShift, coefficients, pDecodedSamples)) {
        return DRFLAC_FALSE;
    }

    return DRFLAC_TRUE;
}


static drflac_bool32 drflac__read_next_flac_frame_header(drflac_bs* bs, drflac_uint8 streaminfoBitsPerSample, drflac_frame_header* header)
{
    const drflac_uint32 sampleRateTable[12]  = {0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
    const drflac_uint8 bitsPerSampleTable[8] = {0, 8, 12, (drflac_uint8)-1, 16, 20, 24, (drflac_uint8)-1};   /* -1 = reserved. */

    /* Keep looping until we find a valid sync code. */
    for (;;) {
        drflac_uint8 crc8 = 0xCE; /* 0xCE = drflac_crc8(0, 0x3FFE, 14); */
        drflac_uint8 reserved = 0;
        drflac_uint8 blockingStrategy = 0;
        drflac_uint8 blockSize = 0;
        drflac_uint8 sampleRate = 0;
        drflac_uint8 channelAssignment = 0;
        drflac_uint8 bitsPerSample = 0;
        drflac_bool32 isVariableBlockSize;

        if (!drflac__find_and_seek_to_next_sync_code(bs)) {
            return DRFLAC_FALSE;
        }

        if (!drflac__read_uint8(bs, 1, &reserved)) {
            return DRFLAC_FALSE;
        }
        if (reserved == 1) {
            continue;
        }
        crc8 = drflac_crc8(crc8, reserved, 1);

        if (!drflac__read_uint8(bs, 1, &blockingStrategy)) {
            return DRFLAC_FALSE;
        }
        crc8 = drflac_crc8(crc8, blockingStrategy, 1);

        if (!drflac__read_uint8(bs, 4, &blockSize)) {
            return DRFLAC_FALSE;
        }
        if (blockSize == 0) {
            continue;
        }
        crc8 = drflac_crc8(crc8, blockSize, 4);

        if (!drflac__read_uint8(bs, 4, &sampleRate)) {
            return DRFLAC_FALSE;
        }
        crc8 = drflac_crc8(crc8, sampleRate, 4);

        if (!drflac__read_uint8(bs, 4, &channelAssignment)) {
            return DRFLAC_FALSE;
        }
        if (channelAssignment > 10) {
            continue;
        }
        crc8 = drflac_crc8(crc8, channelAssignment, 4);

        if (!drflac__read_uint8(bs, 3, &bitsPerSample)) {
            return DRFLAC_FALSE;
        }
        if (bitsPerSample == 3 || bitsPerSample == 7) {
            continue;
        }
        crc8 = drflac_crc8(crc8, bitsPerSample, 3);


        if (!drflac__read_uint8(bs, 1, &reserved)) {
            return DRFLAC_FALSE;
        }
        if (reserved == 1) {
            continue;
        }
        crc8 = drflac_crc8(crc8, reserved, 1);


        isVariableBlockSize = blockingStrategy == 1;
        if (isVariableBlockSize) {
            drflac_uint64 pcmFrameNumber;
            drflac_result result = drflac__read_utf8_coded_number(bs, &pcmFrameNumber, &crc8);
            if (result != DRFLAC_SUCCESS) {
                if (result == DRFLAC_END_OF_STREAM) {
                    return DRFLAC_FALSE;
                } else {
                    continue;
                }
            }
            header->flacFrameNumber  = 0;
            header->pcmFrameNumber = pcmFrameNumber;
        } else {
            drflac_uint64 flacFrameNumber = 0;
            drflac_result result = drflac__read_utf8_coded_number(bs, &flacFrameNumber, &crc8);
            if (result != DRFLAC_SUCCESS) {
                if (result == DRFLAC_END_OF_STREAM) {
                    return DRFLAC_FALSE;
                } else {
                    continue;
                }
            }
            header->flacFrameNumber  = (drflac_uint32)flacFrameNumber;   /* <-- Safe cast. */
            header->pcmFrameNumber = 0;
        }


        if (blockSize == 1) {
            header->blockSizeInPCMFrames = 192;
        } else if (blockSize >= 2 && blockSize <= 5) {
            header->blockSizeInPCMFrames = 576 * (1 << (blockSize - 2));
        } else if (blockSize == 6) {
            if (!drflac__read_uint16(bs, 8, &header->blockSizeInPCMFrames)) {
                return DRFLAC_FALSE;
            }
            crc8 = drflac_crc8(crc8, header->blockSizeInPCMFrames, 8);
            header->blockSizeInPCMFrames += 1;
        } else if (blockSize == 7) {
            if (!drflac__read_uint16(bs, 16, &header->blockSizeInPCMFrames)) {
                return DRFLAC_FALSE;
            }
            crc8 = drflac_crc8(crc8, header->blockSizeInPCMFrames, 16);
            header->blockSizeInPCMFrames += 1;
        } else {
            header->blockSizeInPCMFrames = 256 * (1 << (blockSize - 8));
        }


        if (sampleRate <= 11) {
            header->sampleRate = sampleRateTable[sampleRate];
        } else if (sampleRate == 12) {
            if (!drflac__read_uint32(bs, 8, &header->sampleRate)) {
                return DRFLAC_FALSE;
            }
            crc8 = drflac_crc8(crc8, header->sampleRate, 8);
            header->sampleRate *= 1000;
        } else if (sampleRate == 13) {
            if (!drflac__read_uint32(bs, 16, &header->sampleRate)) {
                return DRFLAC_FALSE;
            }
            crc8 = drflac_crc8(crc8, header->sampleRate, 16);
        } else if (sampleRate == 14) {
            if (!drflac__read_uint32(bs, 16, &header->sampleRate)) {
                return DRFLAC_FALSE;
            }
            crc8 = drflac_crc8(crc8, header->sampleRate, 16);
            header->sampleRate *= 10;
        } else {
            continue;  /* Invalid. Assume an invalid block. */
        }


        header->channelAssignment = channelAssignment;

        header->bitsPerSample = bitsPerSampleTable[bitsPerSample];
        if (header->bitsPerSample == 0) {
            header->bitsPerSample = streaminfoBitsPerSample;
        }

        if (!drflac__read_uint8(bs, 8, &header->crc8)) {
            return DRFLAC_FALSE;
        }

#ifndef DR_FLAC_NO_CRC
        if (header->crc8 != crc8) {
            continue;    /* CRC mismatch. Loop back to the top and find the next sync code. */
        }
#endif
        return DRFLAC_TRUE;
    }
}

static drflac_bool32 drflac__read_subframe_header(drflac_bs* bs, drflac_subframe* pSubframe)
{
    drflac_uint8 header;
    int type;

    if (!drflac__read_uint8(bs, 8, &header)) {
        return DRFLAC_FALSE;
    }

    /* First bit should always be 0. */
    if ((header & 0x80) != 0) {
        return DRFLAC_FALSE;
    }

    type = (header & 0x7E) >> 1;
    if (type == 0) {
        pSubframe->subframeType = DRFLAC_SUBFRAME_CONSTANT;
    } else if (type == 1) {
        pSubframe->subframeType = DRFLAC_SUBFRAME_VERBATIM;
    } else {
        if ((type & 0x20) != 0) {
            pSubframe->subframeType = DRFLAC_SUBFRAME_LPC;
            pSubframe->lpcOrder = (type & 0x1F) + 1;
        } else if ((type & 0x08) != 0) {
            pSubframe->subframeType = DRFLAC_SUBFRAME_FIXED;
            pSubframe->lpcOrder = (type & 0x07);
            if (pSubframe->lpcOrder > 4) {
                pSubframe->subframeType = DRFLAC_SUBFRAME_RESERVED;
                pSubframe->lpcOrder = 0;
            }
        } else {
            pSubframe->subframeType = DRFLAC_SUBFRAME_RESERVED;
        }
    }

    if (pSubframe->subframeType == DRFLAC_SUBFRAME_RESERVED) {
        return DRFLAC_FALSE;
    }

    /* Wasted bits per sample. */
    pSubframe->wastedBitsPerSample = 0;
    if ((header & 0x01) == 1) {
        unsigned int wastedBitsPerSample = 0;
        if (!drflac__seek_past_next_set_bit(bs, &wastedBitsPerSample)) {
            return DRFLAC_FALSE;
        }
        pSubframe->wastedBitsPerSample = (unsigned char)wastedBitsPerSample + 1;
    }

    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__decode_subframe(drflac_bs* bs, drflac_frame* frame, int subframeIndex, drflac_int32* pDecodedSamplesOut)
{
    drflac_subframe* pSubframe;
    drflac_uint32 subframeBitsPerSample;

    pSubframe = frame->subframes + subframeIndex;
    if (!drflac__read_subframe_header(bs, pSubframe)) {
        return DRFLAC_FALSE;
    }

    /* Side channels require an extra bit per sample. Took a while to figure that one out... */
    subframeBitsPerSample = frame->header.bitsPerSample;
    if ((frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE || frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE) && subframeIndex == 1) {
        subframeBitsPerSample += 1;
    } else if (frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE && subframeIndex == 0) {
        subframeBitsPerSample += 1;
    }

    /* Need to handle wasted bits per sample. */
    if (pSubframe->wastedBitsPerSample >= subframeBitsPerSample) {
        return DRFLAC_FALSE;
    }
    subframeBitsPerSample -= pSubframe->wastedBitsPerSample;

    pSubframe->pSamplesS32 = pDecodedSamplesOut;

    switch (pSubframe->subframeType)
    {
        case DRFLAC_SUBFRAME_CONSTANT:
        {
            drflac__decode_samples__constant(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->pSamplesS32);
        } break;

        case DRFLAC_SUBFRAME_VERBATIM:
        {
            drflac__decode_samples__verbatim(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->pSamplesS32);
        } break;

        case DRFLAC_SUBFRAME_FIXED:
        {
            drflac__decode_samples__fixed(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->lpcOrder, pSubframe->pSamplesS32);
        } break;

        case DRFLAC_SUBFRAME_LPC:
        {
            drflac__decode_samples__lpc(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->lpcOrder, pSubframe->pSamplesS32);
        } break;

        default: return DRFLAC_FALSE;
    }

    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__seek_subframe(drflac_bs* bs, drflac_frame* frame, int subframeIndex)
{
    drflac_subframe* pSubframe;
    drflac_uint32 subframeBitsPerSample;

    pSubframe = frame->subframes + subframeIndex;
    if (!drflac__read_subframe_header(bs, pSubframe)) {
        return DRFLAC_FALSE;
    }

    /* Side channels require an extra bit per sample. Took a while to figure that one out... */
    subframeBitsPerSample = frame->header.bitsPerSample;
    if ((frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE || frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE) && subframeIndex == 1) {
        subframeBitsPerSample += 1;
    } else if (frame->header.channelAssignment == DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE && subframeIndex == 0) {
        subframeBitsPerSample += 1;
    }

    /* Need to handle wasted bits per sample. */
    if (pSubframe->wastedBitsPerSample >= subframeBitsPerSample) {
        return DRFLAC_FALSE;
    }
    subframeBitsPerSample -= pSubframe->wastedBitsPerSample;

    pSubframe->pSamplesS32 = NULL;

    switch (pSubframe->subframeType)
    {
        case DRFLAC_SUBFRAME_CONSTANT:
        {
            if (!drflac__seek_bits(bs, subframeBitsPerSample)) {
                return DRFLAC_FALSE;
            }
        } break;

        case DRFLAC_SUBFRAME_VERBATIM:
        {
            unsigned int bitsToSeek = frame->header.blockSizeInPCMFrames * subframeBitsPerSample;
            if (!drflac__seek_bits(bs, bitsToSeek)) {
                return DRFLAC_FALSE;
            }
        } break;

        case DRFLAC_SUBFRAME_FIXED:
        {
            unsigned int bitsToSeek = pSubframe->lpcOrder * subframeBitsPerSample;
            if (!drflac__seek_bits(bs, bitsToSeek)) {
                return DRFLAC_FALSE;
            }

            if (!drflac__read_and_seek_residual(bs, frame->header.blockSizeInPCMFrames, pSubframe->lpcOrder)) {
                return DRFLAC_FALSE;
            }
        } break;

        case DRFLAC_SUBFRAME_LPC:
        {
            unsigned char lpcPrecision;

            unsigned int bitsToSeek = pSubframe->lpcOrder * subframeBitsPerSample;
            if (!drflac__seek_bits(bs, bitsToSeek)) {
                return DRFLAC_FALSE;
            }

            if (!drflac__read_uint8(bs, 4, &lpcPrecision)) {
                return DRFLAC_FALSE;
            }
            if (lpcPrecision == 15) {
                return DRFLAC_FALSE;    /* Invalid. */
            }
            lpcPrecision += 1;


            bitsToSeek = (pSubframe->lpcOrder * lpcPrecision) + 5;    /* +5 for shift. */
            if (!drflac__seek_bits(bs, bitsToSeek)) {
                return DRFLAC_FALSE;
            }

            if (!drflac__read_and_seek_residual(bs, frame->header.blockSizeInPCMFrames, pSubframe->lpcOrder)) {
                return DRFLAC_FALSE;
            }
        } break;

        default: return DRFLAC_FALSE;
    }

    return DRFLAC_TRUE;
}


static  drflac_uint8 drflac__get_channel_count_from_channel_assignment(drflac_int8 channelAssignment)
{
    drflac_uint8 lookup[] = {1, 2, 3, 4, 5, 6, 7, 8, 2, 2, 2};

    return lookup[channelAssignment];
}

static drflac_result drflac__decode_flac_frame(drflac* pFlac)
{
    int channelCount;
    int i;
    drflac_uint8 paddingSizeInBits;
    drflac_uint16 desiredCRC16;
#ifndef DR_FLAC_NO_CRC
    drflac_uint16 actualCRC16;
#endif

    /* This function should be called while the stream is sitting on the first byte after the frame header. */
    DRFLAC_ZERO_MEMORY(pFlac->currentFLACFrame.subframes, sizeof(pFlac->currentFLACFrame.subframes));

    /* The frame block size must never be larger than the maximum block size defined by the FLAC stream. */
    if (pFlac->currentFLACFrame.header.blockSizeInPCMFrames > pFlac->maxBlockSizeInPCMFrames) {
        return DRFLAC_ERROR;
    }

    /* The number of channels in the frame must match the channel count from the STREAMINFO block. */
    channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFLACFrame.header.channelAssignment);
    if (channelCount != (int)pFlac->channels) {
        return DRFLAC_ERROR;
    }

    for (i = 0; i < channelCount; ++i) {
        if (!drflac__decode_subframe(&pFlac->bs, &pFlac->currentFLACFrame, i, pFlac->pDecodedSamples + (pFlac->currentFLACFrame.header.blockSizeInPCMFrames * i))) {
            return DRFLAC_ERROR;
        }
    }

    paddingSizeInBits = DRFLAC_CACHE_L1_BITS_REMAINING(&pFlac->bs) & 7;
    if (paddingSizeInBits > 0) {
        drflac_uint8 padding = 0;
        if (!drflac__read_uint8(&pFlac->bs, paddingSizeInBits, &padding)) {
            return DRFLAC_END_OF_STREAM;
        }
    }

#ifndef DR_FLAC_NO_CRC
    actualCRC16 = drflac__flush_crc16(&pFlac->bs);
#endif
    if (!drflac__read_uint16(&pFlac->bs, 16, &desiredCRC16)) {
        return DRFLAC_END_OF_STREAM;
    }

#ifndef DR_FLAC_NO_CRC
    if (actualCRC16 != desiredCRC16) {
        return DRFLAC_CRC_MISMATCH;    /* CRC mismatch. */
    }
#endif

    pFlac->currentFLACFrame.pcmFramesRemaining = pFlac->currentFLACFrame.header.blockSizeInPCMFrames;

    return DRFLAC_SUCCESS;
}

static drflac_result drflac__seek_flac_frame(drflac* pFlac)
{
    int channelCount;
    int i;
    drflac_uint16 desiredCRC16;
#ifndef DR_FLAC_NO_CRC
    drflac_uint16 actualCRC16;
#endif

    channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFLACFrame.header.channelAssignment);
    for (i = 0; i < channelCount; ++i) {
        if (!drflac__seek_subframe(&pFlac->bs, &pFlac->currentFLACFrame, i)) {
            return DRFLAC_ERROR;
        }
    }

    /* Padding. */
    if (!drflac__seek_bits(&pFlac->bs, DRFLAC_CACHE_L1_BITS_REMAINING(&pFlac->bs) & 7)) {
        return DRFLAC_ERROR;
    }

    /* CRC. */
#ifndef DR_FLAC_NO_CRC
    actualCRC16 = drflac__flush_crc16(&pFlac->bs);
#endif
    if (!drflac__read_uint16(&pFlac->bs, 16, &desiredCRC16)) {
        return DRFLAC_END_OF_STREAM;
    }

#ifndef DR_FLAC_NO_CRC
    if (actualCRC16 != desiredCRC16) {
        return DRFLAC_CRC_MISMATCH;    /* CRC mismatch. */
    }
#endif

    return DRFLAC_SUCCESS;
}

static drflac_bool32 drflac__read_and_decode_next_flac_frame(drflac* pFlac)
{
    for (;;) {
        drflac_result result;

        if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
            return DRFLAC_FALSE;
        }

        result = drflac__decode_flac_frame(pFlac);
        if (result != DRFLAC_SUCCESS) {
            if (result == DRFLAC_CRC_MISMATCH) {
                continue;   /* CRC mismatch. Skip to the next frame. */
            } else {
                return DRFLAC_FALSE;
            }
        }

        return DRFLAC_TRUE;
    }
}

static void drflac__get_pcm_frame_range_of_current_flac_frame(drflac* pFlac, drflac_uint64* pFirstPCMFrame, drflac_uint64* pLastPCMFrame)
{
    drflac_uint64 firstPCMFrame;
    drflac_uint64 lastPCMFrame;

    firstPCMFrame = pFlac->currentFLACFrame.header.pcmFrameNumber;
    if (firstPCMFrame == 0) {
        firstPCMFrame = pFlac->currentFLACFrame.header.flacFrameNumber * pFlac->maxBlockSizeInPCMFrames;
    }

    lastPCMFrame = firstPCMFrame + (pFlac->currentFLACFrame.header.blockSizeInPCMFrames);
    if (lastPCMFrame > 0) {
        lastPCMFrame -= 1; /* Needs to be zero based. */
    }

    if (pFirstPCMFrame) {
        *pFirstPCMFrame = firstPCMFrame;
    }
    if (pLastPCMFrame) {
        *pLastPCMFrame = lastPCMFrame;
    }
}

static drflac_bool32 drflac__seek_to_first_frame(drflac* pFlac)
{
    drflac_bool32 result;

    result = drflac__seek_to_byte(&pFlac->bs, pFlac->firstFLACFramePosInBytes);

    DRFLAC_ZERO_MEMORY(&pFlac->currentFLACFrame, sizeof(pFlac->currentFLACFrame));
    pFlac->currentPCMFrame = 0;

    return result;
}

static  drflac_result drflac__seek_to_next_flac_frame(drflac* pFlac)
{
    /* This function should only ever be called while the decoder is sitting on the first byte past the FRAME_HEADER section. */
    return drflac__seek_flac_frame(pFlac);
}


drflac_uint64 drflac__seek_forward_by_pcm_frames(drflac* pFlac, drflac_uint64 pcmFramesToSeek)
{
    drflac_uint64 pcmFramesRead = 0;
    while (pcmFramesToSeek > 0) {
        if (pFlac->currentFLACFrame.pcmFramesRemaining == 0) {
            if (!drflac__read_and_decode_next_flac_frame(pFlac)) {
                break;  /* Couldn't read the next frame, so just break from the loop and return. */
            }
        } else {
            if (pFlac->currentFLACFrame.pcmFramesRemaining > pcmFramesToSeek) {
                pcmFramesRead   += pcmFramesToSeek;
                pFlac->currentFLACFrame.pcmFramesRemaining -= (drflac_uint32)pcmFramesToSeek;   /* <-- Safe cast. Will always be < currentFrame.pcmFramesRemaining < 65536. */
                pcmFramesToSeek  = 0;
            } else {
                pcmFramesRead   += pFlac->currentFLACFrame.pcmFramesRemaining;
                pcmFramesToSeek -= pFlac->currentFLACFrame.pcmFramesRemaining;
                pFlac->currentFLACFrame.pcmFramesRemaining = 0;
            }
        }
    }

    pFlac->currentPCMFrame += pcmFramesRead;
    return pcmFramesRead;
}


static drflac_bool32 drflac__seek_to_pcm_frame__brute_force(drflac* pFlac, drflac_uint64 pcmFrameIndex)
{
    drflac_bool32 isMidFrame = DRFLAC_FALSE;
    drflac_uint64 runningPCMFrameCount;

    /* If we are seeking forward we start from the current position. Otherwise we need to start all the way from the start of the file. */
    if (pcmFrameIndex >= pFlac->currentPCMFrame) {
        /* Seeking forward. Need to seek from the current position. */
        runningPCMFrameCount = pFlac->currentPCMFrame;

        /* The frame header for the first frame may not yet have been read. We need to do that if necessary. */
        if (pFlac->currentPCMFrame == 0 && pFlac->currentFLACFrame.pcmFramesRemaining == 0) {
            if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
                return DRFLAC_FALSE;
            }
        } else {
            isMidFrame = DRFLAC_TRUE;
        }
    } else {
        /* Seeking backwards. Need to seek from the start of the file. */
        runningPCMFrameCount = 0;

        /* Move back to the start. */
        if (!drflac__seek_to_first_frame(pFlac)) {
            return DRFLAC_FALSE;
        }

        /* Decode the first frame in preparation for sample-exact seeking below. */
        if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
            return DRFLAC_FALSE;
        }
    }

    /*
    We need to as quickly as possible find the frame that contains the target sample. To do this, we iterate over each frame and inspect its
    header. If based on the header we can determine that the frame contains the sample, we do a full decode of that frame.
    */
    for (;;) {
        drflac_uint64 pcmFrameCountInThisFLACFrame;
        drflac_uint64 firstPCMFrameInFLACFrame = 0;
        drflac_uint64 lastPCMFrameInFLACFrame = 0;

        drflac__get_pcm_frame_range_of_current_flac_frame(pFlac, &firstPCMFrameInFLACFrame, &lastPCMFrameInFLACFrame);

        pcmFrameCountInThisFLACFrame = (lastPCMFrameInFLACFrame - firstPCMFrameInFLACFrame) + 1;
        if (pcmFrameIndex < (runningPCMFrameCount + pcmFrameCountInThisFLACFrame)) {
            /*
            The sample should be in this frame. We need to fully decode it, however if it's an invalid frame (a CRC mismatch), we need to pretend
            it never existed and keep iterating.
            */
            drflac_uint64 pcmFramesToDecode = pcmFrameIndex - runningPCMFrameCount;

            if (!isMidFrame) {
                drflac_result result = drflac__decode_flac_frame(pFlac);
                if (result == DRFLAC_SUCCESS) {
                    /* The frame is valid. We just need to skip over some samples to ensure it's sample-exact. */
                    return drflac__seek_forward_by_pcm_frames(pFlac, pcmFramesToDecode) == pcmFramesToDecode;  /* <-- If this fails, something bad has happened (it should never fail). */
                } else {
                    if (result == DRFLAC_CRC_MISMATCH) {
                        goto next_iteration;   /* CRC mismatch. Pretend this frame never existed. */
                    } else {
                        return DRFLAC_FALSE;
                    }
                }
            } else {
                /* We started seeking mid-frame which means we need to skip the frame decoding part. */
                return drflac__seek_forward_by_pcm_frames(pFlac, pcmFramesToDecode) == pcmFramesToDecode;
            }
        } else {
            /*
            It's not in this frame. We need to seek past the frame, but check if there was a CRC mismatch. If so, we pretend this
            frame never existed and leave the running sample count untouched.
            */
            if (!isMidFrame) {
                drflac_result result = drflac__seek_to_next_flac_frame(pFlac);
                if (result == DRFLAC_SUCCESS) {
                    runningPCMFrameCount += pcmFrameCountInThisFLACFrame;
                } else {
                    if (result == DRFLAC_CRC_MISMATCH) {
                        goto next_iteration;   /* CRC mismatch. Pretend this frame never existed. */
                    } else {
                        return DRFLAC_FALSE;
                    }
                }
            } else {
                /*
                We started seeking mid-frame which means we need to seek by reading to the end of the frame instead of with
                drflac__seek_to_next_flac_frame() which only works if the decoder is sitting on the byte just after the frame header.
                */
                runningPCMFrameCount += pFlac->currentFLACFrame.pcmFramesRemaining;
                pFlac->currentFLACFrame.pcmFramesRemaining = 0;
                isMidFrame = DRFLAC_FALSE;
            }

            /* If we are seeking to the end of the file and we've just hit it, we're done. */
            if (pcmFrameIndex == pFlac->totalPCMFrameCount && runningPCMFrameCount == pFlac->totalPCMFrameCount) {
                return DRFLAC_TRUE;
            }
        }

    next_iteration:
        /* Grab the next frame in preparation for the next iteration. */
        if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
            return DRFLAC_FALSE;
        }
    }
}


#if !defined(DR_FLAC_NO_CRC)
/*
We use an average compression ratio to determine our approximate start location. FLAC files are generally about 50%-70% the size of their
uncompressed counterparts so we'll use this as a basis. I'm going to split the middle and use a factor of 0.6 to determine the starting
location.
*/
#define DRFLAC_BINARY_SEARCH_APPROX_COMPRESSION_RATIO 0.6f

static drflac_bool32 drflac__seek_to_approximate_flac_frame_to_byte(drflac* pFlac, drflac_uint64 targetByte, drflac_uint64 rangeLo, drflac_uint64 rangeHi, drflac_uint64* pLastSuccessfulSeekOffset)
{
    *pLastSuccessfulSeekOffset = pFlac->firstFLACFramePosInBytes;

    for (;;) {
        /* When seeking to a byte, failure probably means we've attempted to seek beyond the end of the stream. To counter this we just halve it each attempt. */
        if (!drflac__seek_to_byte(&pFlac->bs, targetByte)) {
            /* If we couldn't even seek to the first byte in the stream we have a problem. Just abandon the whole thing. */
            if (targetByte == 0) {
                drflac__seek_to_first_frame(pFlac); /* Try to recover. */
                return DRFLAC_FALSE;
            }

            /* Halve the byte location and continue. */
            targetByte = rangeLo + ((rangeHi - rangeLo)/2);
            rangeHi = targetByte;
        } else {
            /* Getting here should mean that we have seeked to an appropriate byte. */

            /* Clear the details of the FLAC frame so we don't misreport data. */
            DRFLAC_ZERO_MEMORY(&pFlac->currentFLACFrame, sizeof(pFlac->currentFLACFrame));

            /*
            Now seek to the next FLAC frame. We need to decode the entire frame (not just the header) because it's possible for the header to incorrectly pass the
            CRC check and return bad data. We need to decode the entire frame to be more certain. Although this seems unlikely, this has happened to me in testing
            to it needs to stay this way for now.
            */
#if 1
            if (!drflac__read_and_decode_next_flac_frame(pFlac)) {
                /* Halve the byte location and continue. */
                targetByte = rangeLo + ((rangeHi - rangeLo)/2);
                rangeHi = targetByte;
            } else {
                break;
            }
#else
            if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
                /* Halve the byte location and continue. */
                targetByte = rangeLo + ((rangeHi - rangeLo)/2);
                rangeHi = targetByte;
            } else {
                break;
            }
#endif
        }
    }

    /* The current PCM frame needs to be updated based on the frame we just seeked to. */
    drflac__get_pcm_frame_range_of_current_flac_frame(pFlac, &pFlac->currentPCMFrame, NULL);

    *pLastSuccessfulSeekOffset = targetByte;
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__decode_flac_frame_and_seek_forward_by_pcm_frames(drflac* pFlac, drflac_uint64 offset)
{
    /* This section of code would be used if we were only decoding the FLAC frame header when calling drflac__seek_to_approximate_flac_frame_to_byte(). */
#if 0
    if (drflac__decode_flac_frame(pFlac) != DRFLAC_SUCCESS) {
        /* We failed to decode this frame which may be due to it being corrupt. We'll just use the next valid FLAC frame. */
        if (drflac__read_and_decode_next_flac_frame(pFlac) == DRFLAC_FALSE) {
            return DRFLAC_FALSE;
        }
    }
#endif

    return drflac__seek_forward_by_pcm_frames(pFlac, offset) == offset;
}


static drflac_bool32 drflac__seek_to_pcm_frame__binary_search_internal(drflac* pFlac, drflac_uint64 pcmFrameIndex, drflac_uint64 byteRangeLo, drflac_uint64 byteRangeHi)
{
    /* This assumes pFlac->currentPCMFrame is sitting on byteRangeLo upon entry. */

    drflac_uint64 targetByte;
    drflac_uint64 pcmRangeLo = pFlac->totalPCMFrameCount;
    drflac_uint64 pcmRangeHi = 0;
    drflac_uint64 lastSuccessfulSeekOffset = (drflac_uint64)-1;
    drflac_uint64 closestSeekOffsetBeforeTargetPCMFrame = byteRangeLo;
    drflac_uint32 seekForwardThreshold = (pFlac->maxBlockSizeInPCMFrames != 0) ? pFlac->maxBlockSizeInPCMFrames*2 : 4096;

    targetByte = byteRangeLo + (drflac_uint64)((pcmFrameIndex - pFlac->currentPCMFrame) * pFlac->channels * pFlac->bitsPerSample/8 * DRFLAC_BINARY_SEARCH_APPROX_COMPRESSION_RATIO);
    if (targetByte > byteRangeHi) {
        targetByte = byteRangeHi;
    }

    for (;;) {
        if (drflac__seek_to_approximate_flac_frame_to_byte(pFlac, targetByte, byteRangeLo, byteRangeHi, &lastSuccessfulSeekOffset)) {
            /* We found a FLAC frame. We need to check if it contains the sample we're looking for. */
            drflac_uint64 newPCMRangeLo;
            drflac_uint64 newPCMRangeHi;
            drflac__get_pcm_frame_range_of_current_flac_frame(pFlac, &newPCMRangeLo, &newPCMRangeHi);

            /* If we selected the same frame, it means we should be pretty close. Just decode the rest. */
            if (pcmRangeLo == newPCMRangeLo) {
                if (!drflac__seek_to_approximate_flac_frame_to_byte(pFlac, closestSeekOffsetBeforeTargetPCMFrame, closestSeekOffsetBeforeTargetPCMFrame, byteRangeHi, &lastSuccessfulSeekOffset)) {
                    break;  /* Failed to seek to closest frame. */
                }

                if (drflac__decode_flac_frame_and_seek_forward_by_pcm_frames(pFlac, pcmFrameIndex - pFlac->currentPCMFrame)) {
                    return DRFLAC_TRUE;
                } else {
                    break;  /* Failed to seek forward. */
                }
            }

            pcmRangeLo = newPCMRangeLo;
            pcmRangeHi = newPCMRangeHi;

            if (pcmRangeLo <= pcmFrameIndex && pcmRangeHi >= pcmFrameIndex) {
                /* The target PCM frame is in this FLAC frame. */
                if (drflac__decode_flac_frame_and_seek_forward_by_pcm_frames(pFlac, pcmFrameIndex - pFlac->currentPCMFrame) ) {
                    return DRFLAC_TRUE;
                } else {
                    break;  /* Failed to seek to FLAC frame. */
                }
            } else {
                const float approxCompressionRatio = (lastSuccessfulSeekOffset - pFlac->firstFLACFramePosInBytes) / (pcmRangeLo * pFlac->channels * pFlac->bitsPerSample/8.0f);

                if (pcmRangeLo > pcmFrameIndex) {
                    /* We seeked too far forward. We need to move our target byte backward and try again. */
                    byteRangeHi = lastSuccessfulSeekOffset;
                    if (byteRangeLo > byteRangeHi) {
                        byteRangeLo = byteRangeHi;
                    }

                    /*targetByte = lastSuccessfulSeekOffset - (drflac_uint64)((pcmRangeLo-pcmFrameIndex) * pFlac->channels * pFlac->bitsPerSample/8 * approxCompressionRatio);*/
                    targetByte = byteRangeLo + ((byteRangeHi - byteRangeLo) / 2);
                    if (targetByte < byteRangeLo) {
                        targetByte = byteRangeLo;
                    }
                } else /*if (pcmRangeHi < pcmFrameIndex)*/ {
                    /* We didn't seek far enough. We need to move our target byte forward and try again. */

                    /* If we're close enough we can just seek forward. */
                    if ((pcmFrameIndex - pcmRangeLo) < seekForwardThreshold) {
                        if (drflac__decode_flac_frame_and_seek_forward_by_pcm_frames(pFlac, pcmFrameIndex - pFlac->currentPCMFrame)) {
                            return DRFLAC_TRUE;
                        } else {
                            break;  /* Failed to seek to FLAC frame. */
                        }
                    } else {
                        byteRangeLo = lastSuccessfulSeekOffset;
                        if (byteRangeHi < byteRangeLo) {
                            byteRangeHi = byteRangeLo;
                        }

                        /*targetByte = byteRangeLo + (drflac_uint64)((pcmFrameIndex-pcmRangeLo) * pFlac->channels * pFlac->bitsPerSample/8 * approxCompressionRatio);*/
                        targetByte = lastSuccessfulSeekOffset + (drflac_uint64)((pcmFrameIndex-pcmRangeLo) * pFlac->channels * pFlac->bitsPerSample/8 * approxCompressionRatio);
                        /*targetByte = byteRangeLo + ((byteRangeHi - byteRangeLo) / 2);*/

                        if (targetByte > byteRangeHi) {
                            targetByte = byteRangeHi;
                        }

                        if (closestSeekOffsetBeforeTargetPCMFrame < lastSuccessfulSeekOffset) {
                            closestSeekOffsetBeforeTargetPCMFrame = lastSuccessfulSeekOffset;
                        }
                    }
                }
            }
        } else {
            /* Getting here is really bad. We just recover as best we can, but moving to the first frame in the stream, and then abort. */
            break;
        }
    }

    drflac__seek_to_first_frame(pFlac); /* <-- Try to recover. */
    return DRFLAC_FALSE;
}

static drflac_bool32 drflac__seek_to_pcm_frame__binary_search(drflac* pFlac, drflac_uint64 pcmFrameIndex)
{
    drflac_uint64 byteRangeLo;
    drflac_uint64 byteRangeHi;
    drflac_uint32 seekForwardThreshold = (pFlac->maxBlockSizeInPCMFrames != 0) ? pFlac->maxBlockSizeInPCMFrames*2 : 4096;

    /* Our algorithm currently assumes the PCM frame */
    if (drflac__seek_to_first_frame(pFlac) == DRFLAC_FALSE) {
        return DRFLAC_FALSE;
    }

    /* If we're close enough to the start, just move to the start and seek forward. */
    if (pcmFrameIndex < seekForwardThreshold) {
        return drflac__seek_forward_by_pcm_frames(pFlac, pcmFrameIndex) == pcmFrameIndex;
    }

    /*
    Our starting byte range is the byte position of the first FLAC frame and the approximate end of the file as if it were completely uncompressed. This ensures
    the entire file is included, even though most of the time it'll exceed the end of the actual stream. This is OK as the frame searching logic will handle it.
    */
    byteRangeLo = pFlac->firstFLACFramePosInBytes;
    byteRangeHi = pFlac->firstFLACFramePosInBytes + (drflac_uint64)(pFlac->totalPCMFrameCount * pFlac->channels * pFlac->bitsPerSample/8);

    return drflac__seek_to_pcm_frame__binary_search_internal(pFlac, pcmFrameIndex, byteRangeLo, byteRangeHi);
}
#endif  /* !DR_FLAC_NO_CRC */

static drflac_bool32 drflac__seek_to_pcm_frame__seek_table(drflac* pFlac, drflac_uint64 pcmFrameIndex)
{
    drflac_uint32 iClosestSeekpoint = 0;
    drflac_bool32 isMidFrame = DRFLAC_FALSE;
    drflac_uint64 runningPCMFrameCount;
    drflac_uint32 iSeekpoint;

    if (pFlac->pSeekpoints == NULL || pFlac->seekpointCount == 0) {
        return DRFLAC_FALSE;
    }

    for (iSeekpoint = 0; iSeekpoint < pFlac->seekpointCount; ++iSeekpoint) {
        if (pFlac->pSeekpoints[iSeekpoint].firstPCMFrame >= pcmFrameIndex) {
            break;
        }

        iClosestSeekpoint = iSeekpoint;
    }

#if !defined(DR_FLAC_NO_CRC)
    /* At this point we should know the closest seek point. We can use a binary search for this. We need to know the total sample count for this. */
    if (pFlac->totalPCMFrameCount > 0) {
        drflac_uint64 byteRangeLo;
        drflac_uint64 byteRangeHi;

        byteRangeHi = pFlac->firstFLACFramePosInBytes + (drflac_uint64)(pFlac->totalPCMFrameCount * pFlac->channels * pFlac->bitsPerSample/8);
        byteRangeLo = pFlac->firstFLACFramePosInBytes + pFlac->pSeekpoints[iClosestSeekpoint].flacFrameOffset;

        if (iClosestSeekpoint < pFlac->seekpointCount-1) {
            if (pFlac->pSeekpoints[iClosestSeekpoint+1].firstPCMFrame != (((drflac_uint64)0xFFFFFFFF << 32) | 0xFFFFFFFF)) {   /* Is it a placeholder seekpoint. */
                byteRangeHi = pFlac->firstFLACFramePosInBytes + pFlac->pSeekpoints[iClosestSeekpoint+1].flacFrameOffset-1; /* Must be zero based. */
            }
        }

        if (drflac__seek_to_byte(&pFlac->bs, pFlac->firstFLACFramePosInBytes + pFlac->pSeekpoints[iClosestSeekpoint].flacFrameOffset)) {
            if (drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
                drflac__get_pcm_frame_range_of_current_flac_frame(pFlac, &pFlac->currentPCMFrame, NULL);

                if (drflac__seek_to_pcm_frame__binary_search_internal(pFlac, pcmFrameIndex, byteRangeLo, byteRangeHi)) {
                    return DRFLAC_TRUE;
                }
            }
        }
    }
#endif  /* !DR_FLAC_NO_CRC */

    /* Getting here means we need to use a slower algorithm because the binary search method failed or cannot be used. */

    /*
    If we are seeking forward and the closest seekpoint is _before_ the current sample, we just seek forward from where we are. Otherwise we start seeking
    from the seekpoint's first sample.
    */
    if (pcmFrameIndex >= pFlac->currentPCMFrame && pFlac->pSeekpoints[iClosestSeekpoint].firstPCMFrame <= pFlac->currentPCMFrame) {
        /* Optimized case. Just seek forward from where we are. */
        runningPCMFrameCount = pFlac->currentPCMFrame;

        /* The frame header for the first frame may not yet have been read. We need to do that if necessary. */
        if (pFlac->currentPCMFrame == 0 && pFlac->currentFLACFrame.pcmFramesRemaining == 0) {
            if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
                return DRFLAC_FALSE;
            }
        } else {
            isMidFrame = DRFLAC_TRUE;
        }
    } else {
        /* Slower case. Seek to the start of the seekpoint and then seek forward from there. */
        runningPCMFrameCount = pFlac->pSeekpoints[iClosestSeekpoint].firstPCMFrame;

        if (!drflac__seek_to_byte(&pFlac->bs, pFlac->firstFLACFramePosInBytes + pFlac->pSeekpoints[iClosestSeekpoint].flacFrameOffset)) {
            return DRFLAC_FALSE;
        }

        /* Grab the frame the seekpoint is sitting on in preparation for the sample-exact seeking below. */
        if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
            return DRFLAC_FALSE;
        }
    }

    for (;;) {
        drflac_uint64 pcmFrameCountInThisFLACFrame;
        drflac_uint64 firstPCMFrameInFLACFrame = 0;
        drflac_uint64 lastPCMFrameInFLACFrame = 0;

        drflac__get_pcm_frame_range_of_current_flac_frame(pFlac, &firstPCMFrameInFLACFrame, &lastPCMFrameInFLACFrame);

        pcmFrameCountInThisFLACFrame = (lastPCMFrameInFLACFrame - firstPCMFrameInFLACFrame) + 1;
        if (pcmFrameIndex < (runningPCMFrameCount + pcmFrameCountInThisFLACFrame)) {
            /*
            The sample should be in this frame. We need to fully decode it, but if it's an invalid frame (a CRC mismatch) we need to pretend
            it never existed and keep iterating.
            */
            drflac_uint64 pcmFramesToDecode = pcmFrameIndex - runningPCMFrameCount;

            if (!isMidFrame) {
                drflac_result result = drflac__decode_flac_frame(pFlac);
                if (result == DRFLAC_SUCCESS) {
                    /* The frame is valid. We just need to skip over some samples to ensure it's sample-exact. */
                    return drflac__seek_forward_by_pcm_frames(pFlac, pcmFramesToDecode) == pcmFramesToDecode;  /* <-- If this fails, something bad has happened (it should never fail). */
                } else {
                    if (result == DRFLAC_CRC_MISMATCH) {
                        goto next_iteration;   /* CRC mismatch. Pretend this frame never existed. */
                    } else {
                        return DRFLAC_FALSE;
                    }
                }
            } else {
                /* We started seeking mid-frame which means we need to skip the frame decoding part. */
                return drflac__seek_forward_by_pcm_frames(pFlac, pcmFramesToDecode) == pcmFramesToDecode;
            }
        } else {
            /*
            It's not in this frame. We need to seek past the frame, but check if there was a CRC mismatch. If so, we pretend this
            frame never existed and leave the running sample count untouched.
            */
            if (!isMidFrame) {
                drflac_result result = drflac__seek_to_next_flac_frame(pFlac);
                if (result == DRFLAC_SUCCESS) {
                    runningPCMFrameCount += pcmFrameCountInThisFLACFrame;
                } else {
                    if (result == DRFLAC_CRC_MISMATCH) {
                        goto next_iteration;   /* CRC mismatch. Pretend this frame never existed. */
                    } else {
                        return DRFLAC_FALSE;
                    }
                }
            } else {
                /*
                We started seeking mid-frame which means we need to seek by reading to the end of the frame instead of with
                drflac__seek_to_next_flac_frame() which only works if the decoder is sitting on the byte just after the frame header.
                */
                runningPCMFrameCount += pFlac->currentFLACFrame.pcmFramesRemaining;
                pFlac->currentFLACFrame.pcmFramesRemaining = 0;
                isMidFrame = DRFLAC_FALSE;
            }

            /* If we are seeking to the end of the file and we've just hit it, we're done. */
            if (pcmFrameIndex == pFlac->totalPCMFrameCount && runningPCMFrameCount == pFlac->totalPCMFrameCount) {
                return DRFLAC_TRUE;
            }
        }

    next_iteration:
        /* Grab the next frame in preparation for the next iteration. */
        if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
            return DRFLAC_FALSE;
        }
    }
}


#ifndef DR_FLAC_NO_OGG
typedef struct
{
    drflac_uint8 capturePattern[4];  /* Should be "OggS" */
    drflac_uint8 structureVersion;   /* Always 0. */
    drflac_uint8 headerType;
    drflac_uint64 granulePosition;
    drflac_uint32 serialNumber;
    drflac_uint32 sequenceNumber;
    drflac_uint32 checksum;
    drflac_uint8 segmentCount;
    drflac_uint8 segmentTable[255];
} drflac_ogg_page_header;
#endif

typedef struct
{
    drflac_read_proc onRead;
    drflac_seek_proc onSeek;
    drflac_meta_proc onMeta;
    drflac_container container;
    void* pUserData;
    void* pUserDataMD;
    drflac_uint32 sampleRate;
    drflac_uint8  channels;
    drflac_uint8  bitsPerSample;
    drflac_uint64 totalPCMFrameCount;
    drflac_uint16 maxBlockSizeInPCMFrames;
    drflac_uint64 runningFilePos;
    drflac_bool32 hasStreamInfoBlock;
    drflac_bool32 hasMetadataBlocks;
    drflac_bs bs;                           /* <-- A bit streamer is required for loading data during initialization. */
    drflac_frame_header firstFrameHeader;   /* <-- The header of the first frame that was read during relaxed initalization. Only set if there is no STREAMINFO block. */

#ifndef DR_FLAC_NO_OGG
    drflac_uint32 oggSerial;
    drflac_uint64 oggFirstBytePos;
    drflac_ogg_page_header oggBosHeader;
#endif
} drflac_init_info;

static  void drflac__decode_block_header(drflac_uint32 blockHeader, drflac_uint8* isLastBlock, drflac_uint8* blockType, drflac_uint32* blockSize)
{
    blockHeader = drflac__be2host_32(blockHeader);
    *isLastBlock = (blockHeader & 0x80000000UL) >> 31;
    *blockType   = (blockHeader & 0x7F000000UL) >> 24;
    *blockSize   = (blockHeader & 0x00FFFFFFUL);
}

static  drflac_bool32 drflac__read_and_decode_block_header(drflac_read_proc onRead, void* pUserData, drflac_uint8* isLastBlock, drflac_uint8* blockType, drflac_uint32* blockSize)
{
    drflac_uint32 blockHeader;
    if (onRead(pUserData, &blockHeader, 4) != 4) {
        return DRFLAC_FALSE;
    }

    drflac__decode_block_header(blockHeader, isLastBlock, blockType, blockSize);
    return DRFLAC_TRUE;
}

drflac_bool32 drflac__read_streaminfo(drflac_read_proc onRead, void* pUserData, drflac_streaminfo* pStreamInfo)
{
    drflac_uint32 blockSizes;
    drflac_uint64 frameSizes = 0;
    drflac_uint64 importantProps;
    drflac_uint8 md5[16];

    /* min/max block size. */
    if (onRead(pUserData, &blockSizes, 4) != 4) {
        return DRFLAC_FALSE;
    }

    /* min/max frame size. */
    if (onRead(pUserData, &frameSizes, 6) != 6) {
        return DRFLAC_FALSE;
    }

    /* Sample rate, channels, bits per sample and total sample count. */
    if (onRead(pUserData, &importantProps, 8) != 8) {
        return DRFLAC_FALSE;
    }

    /* MD5 */
    if (onRead(pUserData, md5, sizeof(md5)) != sizeof(md5)) {
        return DRFLAC_FALSE;
    }

    blockSizes     = drflac__be2host_32(blockSizes);
    frameSizes     = drflac__be2host_64(frameSizes);
    importantProps = drflac__be2host_64(importantProps);

    pStreamInfo->minBlockSizeInPCMFrames = (blockSizes & 0xFFFF0000) >> 16;
    pStreamInfo->maxBlockSizeInPCMFrames = (blockSizes & 0x0000FFFF);
    pStreamInfo->minFrameSizeInPCMFrames = (drflac_uint32)((frameSizes     &  (((drflac_uint64)0x00FFFFFF << 16) << 24)) >> 40);
    pStreamInfo->maxFrameSizeInPCMFrames = (drflac_uint32)((frameSizes     &  (((drflac_uint64)0x00FFFFFF << 16) <<  0)) >> 16);
    pStreamInfo->sampleRate              = (drflac_uint32)((importantProps &  (((drflac_uint64)0x000FFFFF << 16) << 28)) >> 44);
    pStreamInfo->channels                = (drflac_uint8 )((importantProps &  (((drflac_uint64)0x0000000E << 16) << 24)) >> 41) + 1;
    pStreamInfo->bitsPerSample           = (drflac_uint8 )((importantProps &  (((drflac_uint64)0x0000001F << 16) << 20)) >> 36) + 1;
    pStreamInfo->totalPCMFrameCount      =                ((importantProps & ((((drflac_uint64)0x0000000F << 16) << 16) | 0xFFFFFFFF)));
    DRFLAC_COPY_MEMORY(pStreamInfo->md5, md5, sizeof(md5));

    return DRFLAC_TRUE;
}

static void* drflac__malloc_default(size_t sz, void* pUserData)
{
	(void)pUserData;
	return DRFLAC_MALLOC(sz);
}

static void* drflac__realloc_default(void* p, size_t sz, void* pUserData)
{
    (void)pUserData;
    return DRFLAC_REALLOC(p, sz);
}

static void drflac__free_default(void* p, void* pUserData)
{
	(void)pUserData;
	DRFLAC_FREE(p);
}


static void* drflac__malloc_from_callbacks(size_t sz, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    if (pAllocationCallbacks == NULL) {
        return NULL;
    }

    if (pAllocationCallbacks->onMalloc != NULL) {
        return pAllocationCallbacks->onMalloc(sz, pAllocationCallbacks->pUserData);
    }

    /* Try using realloc(). */
    if (pAllocationCallbacks->onRealloc != NULL) {
        return pAllocationCallbacks->onRealloc(NULL, sz, pAllocationCallbacks->pUserData);
    }

    return NULL;
}

static void drflac__free_from_callbacks(void* p, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    if (p == NULL || pAllocationCallbacks == NULL) {
        return;
    }

    if (pAllocationCallbacks->onFree != NULL) {
        pAllocationCallbacks->onFree(p, pAllocationCallbacks->pUserData);
    }
}


drflac_bool32 drflac__read_and_decode_metadata(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD, drflac_uint64* pFirstFramePos, drflac_uint64* pSeektablePos, drflac_uint32* pSeektableSize, drflac_allocation_callbacks* pAllocationCallbacks)
{
    /*
    We want to keep track of the byte position in the stream of the seektable. At the time of calling this function we know that
    we'll be sitting on byte 42.
    */
    drflac_uint64 runningFilePos = 42;
    drflac_uint64 seektablePos   = 0;
    drflac_uint32 seektableSize  = 0;

    for (;;) {
        drflac_metadata metadata;
        drflac_uint8 isLastBlock = 0;
        drflac_uint8 blockType = 0;
        drflac_uint32 blockSize;
        if (!drflac__read_and_decode_block_header(onRead, pUserData, &isLastBlock, &blockType, &blockSize)) {
            return DRFLAC_FALSE;
        }
        runningFilePos += 4;

        metadata.type = blockType;
        metadata.pRawData = NULL;
        metadata.rawDataSize = 0;

        switch (blockType)
        {
            case DRFLAC_METADATA_BLOCK_TYPE_APPLICATION:
            {
                if (blockSize < 4) {
                    return DRFLAC_FALSE;
                }

                if (onMeta) {
                    void* pRawData = drflac__malloc_from_callbacks(blockSize, pAllocationCallbacks);
                    if (pRawData == NULL) {
                        return DRFLAC_FALSE;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;
                    metadata.data.application.id       = drflac__be2host_32(*(drflac_uint32*)pRawData);
                    metadata.data.application.pData    = (const void*)((drflac_uint8*)pRawData + sizeof(drflac_uint32));
                    metadata.data.application.dataSize = blockSize - sizeof(drflac_uint32);
                    onMeta(pUserDataMD, &metadata);

                    drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_SEEKTABLE:
            {
                seektablePos  = runningFilePos;
                seektableSize = blockSize;

                if (onMeta) {
                    drflac_uint32 iSeekpoint;
                    void* pRawData;

                    pRawData = drflac__malloc_from_callbacks(blockSize, pAllocationCallbacks);
                    if (pRawData == NULL) {
                        return DRFLAC_FALSE;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;
                    metadata.data.seektable.seekpointCount = blockSize/sizeof(drflac_seekpoint);
                    metadata.data.seektable.pSeekpoints = (const drflac_seekpoint*)pRawData;

                    /* Endian swap. */
                    for (iSeekpoint = 0; iSeekpoint < metadata.data.seektable.seekpointCount; ++iSeekpoint) {
                        drflac_seekpoint* pSeekpoint = (drflac_seekpoint*)pRawData + iSeekpoint;
                        pSeekpoint->firstPCMFrame   = drflac__be2host_64(pSeekpoint->firstPCMFrame);
                        pSeekpoint->flacFrameOffset = drflac__be2host_64(pSeekpoint->flacFrameOffset);
                        pSeekpoint->pcmFrameCount   = drflac__be2host_16(pSeekpoint->pcmFrameCount);
                    }

                    onMeta(pUserDataMD, &metadata);

                    drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT:
            {
                if (blockSize < 8) {
                    return DRFLAC_FALSE;
                }

                if (onMeta) {
                    void* pRawData;
                    const char* pRunningData;
                    const char* pRunningDataEnd;
                    drflac_uint32 i;

                    pRawData = drflac__malloc_from_callbacks(blockSize, pAllocationCallbacks);
                    if (pRawData == NULL) {
                        return DRFLAC_FALSE;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;

                    pRunningData    = (const char*)pRawData;
                    pRunningDataEnd = (const char*)pRawData + blockSize;

                    metadata.data.vorbis_comment.vendorLength = drflac__le2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;

                    /* Need space for the rest of the block */
                    if ((pRunningDataEnd - pRunningData) - 4 < (drflac_int64)metadata.data.vorbis_comment.vendorLength) { /* <-- Note the order of operations to avoid overflow to a valid value */
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }
                    metadata.data.vorbis_comment.vendor       = pRunningData;                                            pRunningData += metadata.data.vorbis_comment.vendorLength;
                    metadata.data.vorbis_comment.commentCount = drflac__le2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;

                    /* Need space for 'commentCount' comments after the block, which at minimum is a drflac_uint32 per comment */
                    if ((pRunningDataEnd - pRunningData) / sizeof(drflac_uint32) < metadata.data.vorbis_comment.commentCount) { /* <-- Note the order of operations to avoid overflow to a valid value */
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }
                    metadata.data.vorbis_comment.pComments    = pRunningData;

                    /* Check that the comments section is valid before passing it to the callback */
                    for (i = 0; i < metadata.data.vorbis_comment.commentCount; ++i) {
                        drflac_uint32 commentLength;

                        if (pRunningDataEnd - pRunningData < 4) {
                            drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                            return DRFLAC_FALSE;
                        }

                        commentLength = drflac__le2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;
                        if (pRunningDataEnd - pRunningData < (drflac_int64)commentLength) { /* <-- Note the order of operations to avoid overflow to a valid value */
                            drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                            return DRFLAC_FALSE;
                        }
                        pRunningData += commentLength;
                    }

                    onMeta(pUserDataMD, &metadata);

                    drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_CUESHEET:
            {
                if (blockSize < 396) {
                    return DRFLAC_FALSE;
                }

                if (onMeta) {
                    void* pRawData;
                    const char* pRunningData;
                    const char* pRunningDataEnd;
                    drflac_uint8 iTrack;
                    drflac_uint8 iIndex;

                    pRawData = drflac__malloc_from_callbacks(blockSize, pAllocationCallbacks);
                    if (pRawData == NULL) {
                        return DRFLAC_FALSE;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;

                    pRunningData    = (const char*)pRawData;
                    pRunningDataEnd = (const char*)pRawData + blockSize;

                    DRFLAC_COPY_MEMORY(metadata.data.cuesheet.catalog, pRunningData, 128);                              pRunningData += 128;
                    metadata.data.cuesheet.leadInSampleCount = drflac__be2host_64(*(const drflac_uint64*)pRunningData); pRunningData += 8;
                    metadata.data.cuesheet.isCD              = (pRunningData[0] & 0x80) != 0;                           pRunningData += 259;
                    metadata.data.cuesheet.trackCount        = pRunningData[0];                                         pRunningData += 1;
                    metadata.data.cuesheet.pTrackData        = pRunningData;

                    /* Check that the cuesheet tracks are valid before passing it to the callback */
                    for (iTrack = 0; iTrack < metadata.data.cuesheet.trackCount; ++iTrack) {
                        drflac_uint8 indexCount;
                        drflac_uint32 indexPointSize;

                        if (pRunningDataEnd - pRunningData < 36) {
                            drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                            return DRFLAC_FALSE;
                        }

                        /* Skip to the index point count */
                        pRunningData += 35;
                        indexCount = pRunningData[0]; pRunningData += 1;
                        indexPointSize = indexCount * sizeof(drflac_cuesheet_track_index);
                        if (pRunningDataEnd - pRunningData < (drflac_int64)indexPointSize) {
                            drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                            return DRFLAC_FALSE;
                        }

                        /* Endian swap. */
                        for (iIndex = 0; iIndex < indexCount; ++iIndex) {
                            drflac_cuesheet_track_index* pTrack = (drflac_cuesheet_track_index*)pRunningData;
                            pRunningData += sizeof(drflac_cuesheet_track_index);
                            pTrack->offset = drflac__be2host_64(pTrack->offset);
                        }
                    }

                    onMeta(pUserDataMD, &metadata);

                    drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_PICTURE:
            {
                if (blockSize < 32) {
                    return DRFLAC_FALSE;
                }

                if (onMeta) {
                    void* pRawData;
                    const char* pRunningData;
                    const char* pRunningDataEnd;

                    pRawData = drflac__malloc_from_callbacks(blockSize, pAllocationCallbacks);
                    if (pRawData == NULL) {
                        return DRFLAC_FALSE;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;

                    pRunningData    = (const char*)pRawData;
                    pRunningDataEnd = (const char*)pRawData + blockSize;

                    metadata.data.picture.type       = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.mimeLength = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;

                    /* Need space for the rest of the block */
                    if ((pRunningDataEnd - pRunningData) - 24 < (drflac_int64)metadata.data.picture.mimeLength) { /* <-- Note the order of operations to avoid overflow to a valid value */
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }
                    metadata.data.picture.mime              = pRunningData;                                            pRunningData += metadata.data.picture.mimeLength;
                    metadata.data.picture.descriptionLength = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;

                    /* Need space for the rest of the block */
                    if ((pRunningDataEnd - pRunningData) - 20 < (drflac_int64)metadata.data.picture.descriptionLength) { /* <-- Note the order of operations to avoid overflow to a valid value */
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }
                    metadata.data.picture.description     = pRunningData;                                            pRunningData += metadata.data.picture.descriptionLength;
                    metadata.data.picture.width           = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.height          = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.colorDepth      = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.indexColorCount = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.pictureDataSize = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;
                    metadata.data.picture.pPictureData    = (const drflac_uint8*)pRunningData;

                    /* Need space for the picture after the block */
                    if (pRunningDataEnd - pRunningData < (drflac_int64)metadata.data.picture.pictureDataSize) { /* <-- Note the order of operations to avoid overflow to a valid value */
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }

                    onMeta(pUserDataMD, &metadata);

                    drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_PADDING:
            {
                if (onMeta) {
                    metadata.data.padding.unused = 0;

                    /* Padding doesn't have anything meaningful in it, so just skip over it, but make sure the caller is aware of it by firing the callback. */
                    if (!onSeek(pUserData, blockSize, drflac_seek_origin_current)) {
                        isLastBlock = DRFLAC_TRUE;  /* An error occurred while seeking. Attempt to recover by treating this as the last block which will in turn terminate the loop. */
                    } else {
                        onMeta(pUserDataMD, &metadata);
                    }
                }
            } break;

            case DRFLAC_METADATA_BLOCK_TYPE_INVALID:
            {
                /* Invalid chunk. Just skip over this one. */
                if (onMeta) {
                    if (!onSeek(pUserData, blockSize, drflac_seek_origin_current)) {
                        isLastBlock = DRFLAC_TRUE;  /* An error occurred while seeking. Attempt to recover by treating this as the last block which will in turn terminate the loop. */
                    }
                }
            } break;

            default:
            {
                /*
                It's an unknown chunk, but not necessarily invalid. There's a chance more metadata blocks might be defined later on, so we
                can at the very least report the chunk to the application and let it look at the raw data.
                */
                if (onMeta) {
                    void* pRawData = drflac__malloc_from_callbacks(blockSize, pAllocationCallbacks);
                    if (pRawData == NULL) {
                        return DRFLAC_FALSE;
                    }

                    if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                        drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                        return DRFLAC_FALSE;
                    }

                    metadata.pRawData = pRawData;
                    metadata.rawDataSize = blockSize;
                    onMeta(pUserDataMD, &metadata);

                    drflac__free_from_callbacks(pRawData, pAllocationCallbacks);
                }
            } break;
        }

        /* If we're not handling metadata, just skip over the block. If we are, it will have been handled earlier in the switch statement above. */
        if (onMeta == NULL && blockSize > 0) {
            if (!onSeek(pUserData, blockSize, drflac_seek_origin_current)) {
                isLastBlock = DRFLAC_TRUE;
            }
        }

        runningFilePos += blockSize;
        if (isLastBlock) {
            break;
        }
    }

    *pSeektablePos = seektablePos;
    *pSeektableSize = seektableSize;
    *pFirstFramePos = runningFilePos;

    return DRFLAC_TRUE;
}

drflac_bool32 drflac__init_private__native(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD, drflac_bool32 relaxed)
{
    /* Pre Condition: The bit stream should be sitting just past the 4-byte id header. */

    drflac_uint8 isLastBlock = 0;
    drflac_uint8 blockType = 0;
    drflac_uint32 blockSize;

    (void)onSeek;

    pInit->container = drflac_container_native;

    /* The first metadata block should be the STREAMINFO block. */
    if (!drflac__read_and_decode_block_header(onRead, pUserData, &isLastBlock, &blockType, &blockSize)) {
        return DRFLAC_FALSE;
    }

    if (blockType != DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO || blockSize != 34) {
        if (!relaxed) {
            /* We're opening in strict mode and the first block is not the STREAMINFO block. Error. */
            return DRFLAC_FALSE;
        } else {
            /*
            Relaxed mode. To open from here we need to just find the first frame and set the sample rate, etc. to whatever is defined
            for that frame.
            */
            pInit->hasStreamInfoBlock = DRFLAC_FALSE;
            pInit->hasMetadataBlocks  = DRFLAC_FALSE;

            if (!drflac__read_next_flac_frame_header(&pInit->bs, 0, &pInit->firstFrameHeader)) {
                return DRFLAC_FALSE;    /* Couldn't find a frame. */
            }

            if (pInit->firstFrameHeader.bitsPerSample == 0) {
                return DRFLAC_FALSE;    /* Failed to initialize because the first frame depends on the STREAMINFO block, which does not exist. */
            }

            pInit->sampleRate              = pInit->firstFrameHeader.sampleRate;
            pInit->channels                = drflac__get_channel_count_from_channel_assignment(pInit->firstFrameHeader.channelAssignment);
            pInit->bitsPerSample           = pInit->firstFrameHeader.bitsPerSample;
            pInit->maxBlockSizeInPCMFrames = 65535;   /* <-- See notes here: https://xiph.org/flac/format.html#metadata_block_streaminfo */
            return DRFLAC_TRUE;
        }
    } else {
        drflac_streaminfo streaminfo;
        if (!drflac__read_streaminfo(onRead, pUserData, &streaminfo)) {
            return DRFLAC_FALSE;
        }

        pInit->hasStreamInfoBlock      = DRFLAC_TRUE;
        pInit->sampleRate              = streaminfo.sampleRate;
        pInit->channels                = streaminfo.channels;
        pInit->bitsPerSample           = streaminfo.bitsPerSample;
        pInit->totalPCMFrameCount      = streaminfo.totalPCMFrameCount;
        pInit->maxBlockSizeInPCMFrames = streaminfo.maxBlockSizeInPCMFrames;    /* Don't care about the min block size - only the max (used for determining the size of the memory allocation). */
        pInit->hasMetadataBlocks       = !isLastBlock;

        if (onMeta) {
            drflac_metadata metadata;
            metadata.type = DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO;
            metadata.pRawData = NULL;
            metadata.rawDataSize = 0;
            metadata.data.streaminfo = streaminfo;
            onMeta(pUserDataMD, &metadata);
        }

        return DRFLAC_TRUE;
    }
}

drflac_bool32 drflac__init_private(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, drflac_container container, void* pUserData, void* pUserDataMD)
{
    drflac_bool32 relaxed;
    drflac_uint8 id[4];

    if (pInit == NULL || onRead == NULL || onSeek == NULL) {
        return DRFLAC_FALSE;
    }

    DRFLAC_ZERO_MEMORY(pInit, sizeof(*pInit));
	pInit->bs.cacheL2 = (drflac_cache_t *)malloc(DR_FLAC_BUFFER_SIZE);
    pInit->onRead       = onRead;
    pInit->onSeek       = onSeek;
    pInit->onMeta       = onMeta;
    pInit->container    = container;
    pInit->pUserData    = pUserData;
    pInit->pUserDataMD  = pUserDataMD;

    pInit->bs.onRead    = onRead;
    pInit->bs.onSeek    = onSeek;
    pInit->bs.pUserData = pUserData;
    drflac__reset_cache(&pInit->bs);


    /* If the container is explicitly defined then we can try opening in relaxed mode. */
    relaxed = container != drflac_container_unknown;

    /* Skip over any ID3 tags. */
    for (;;) {
        if (onRead(pUserData, id, 4) != 4) {
            return DRFLAC_FALSE;    /* Ran out of data. */
        }
        pInit->runningFilePos += 4;

        if (id[0] == 'I' && id[1] == 'D' && id[2] == '3') {
            drflac_uint8 header[6];
            drflac_uint8 flags;
            drflac_uint32 headerSize;

            if (onRead(pUserData, header, 6) != 6) {
                return DRFLAC_FALSE;    /* Ran out of data. */
            }
            pInit->runningFilePos += 6;

            flags = header[1];

            DRFLAC_COPY_MEMORY(&headerSize, header+2, 4);
            headerSize = drflac__unsynchsafe_32(drflac__be2host_32(headerSize));
            if (flags & 0x10) {
                headerSize += 10;
            }

            if (!onSeek(pUserData, headerSize, drflac_seek_origin_current)) {
                return DRFLAC_FALSE;    /* Failed to seek past the tag. */
            }
            pInit->runningFilePos += headerSize;
        } else {
            break;
        }
    }

    if (id[0] == 'f' && id[1] == 'L' && id[2] == 'a' && id[3] == 'C') {
        return drflac__init_private__native(pInit, onRead, onSeek, onMeta, pUserData, pUserDataMD, relaxed);
    }
#ifndef DR_FLAC_NO_OGG
    if (id[0] == 'O' && id[1] == 'g' && id[2] == 'g' && id[3] == 'S') {
        return drflac__init_private__ogg(pInit, onRead, onSeek, onMeta, pUserData, pUserDataMD, relaxed);
    }
#endif

    /* If we get here it means we likely don't have a header. Try opening in relaxed mode, if applicable. */
    if (relaxed) {
        if (container == drflac_container_native) {
            return drflac__init_private__native(pInit, onRead, onSeek, onMeta, pUserData, pUserDataMD, relaxed);
        }
#ifndef DR_FLAC_NO_OGG
        if (container == drflac_container_ogg) {
            return drflac__init_private__ogg(pInit, onRead, onSeek, onMeta, pUserData, pUserDataMD, relaxed);
        }
#endif
    }

    /* Unsupported container. */
    return DRFLAC_FALSE;
}

void drflac__init_from_info(drflac* pFlac, drflac_init_info* pInit)
{
    DRFLAC_ZERO_MEMORY(pFlac, sizeof(*pFlac));
    pFlac->bs                      = pInit->bs;
    pFlac->onMeta                  = pInit->onMeta;
    pFlac->pUserDataMD             = pInit->pUserDataMD;
    pFlac->maxBlockSizeInPCMFrames = pInit->maxBlockSizeInPCMFrames;
    pFlac->sampleRate              = pInit->sampleRate;
    pFlac->channels                = (drflac_uint8)pInit->channels;
    pFlac->bitsPerSample           = (drflac_uint8)pInit->bitsPerSample;
    pFlac->totalPCMFrameCount      = pInit->totalPCMFrameCount;
    pFlac->container               = pInit->container;
}


drflac* drflac_open_with_metadata_private(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, drflac_container container, void* pUserData, void* pUserDataMD, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    drflac_init_info init;
    drflac_uint32 allocationSize;
    drflac_uint32 wholeSIMDVectorCountPerChannel;
    drflac_uint32 decodedSamplesAllocationSize;
#ifndef DR_FLAC_NO_OGG
    drflac_oggbs oggbs;
#endif
    drflac_uint64 firstFramePos;
    drflac_uint64 seektablePos;
    drflac_uint32 seektableSize;
    drflac_allocation_callbacks allocationCallbacks;
    drflac* pFlac;

    /* CPU support first. */
    drflac__init_cpu_caps();

    if (!drflac__init_private(&init, onRead, onSeek, onMeta, container, pUserData, pUserDataMD)) {
        return NULL;
    }

    if (pAllocationCallbacks != NULL) {
        allocationCallbacks = *pAllocationCallbacks;
        if (allocationCallbacks.onFree == NULL || (allocationCallbacks.onMalloc == NULL && allocationCallbacks.onRealloc == NULL)) {
            return NULL;    /* Invalid allocation callbacks. */
        }
    } else {
        allocationCallbacks.pUserData = NULL;
        allocationCallbacks.onMalloc  = drflac__malloc_default;
        allocationCallbacks.onRealloc = drflac__realloc_default;
        allocationCallbacks.onFree    = drflac__free_default;
    }


    /*
    The size of the allocation for the drflac object needs to be large enough to fit the following:
      1) The main members of the drflac structure
      2) A block of memory large enough to store the decoded samples of the largest frame in the stream
      3) If the container is Ogg, a drflac_oggbs object

    The complicated part of the allocation is making sure there's enough room the decoded samples, taking into consideration
    the different SIMD instruction sets.
    */
    allocationSize = sizeof(drflac);

    /*
    The allocation size for decoded frames depends on the number of 32-bit integers that fit inside the largest SIMD vector
    we are supporting.
    */
    if ((init.maxBlockSizeInPCMFrames % (DRFLAC_MAX_SIMD_VECTOR_SIZE / sizeof(drflac_int32))) == 0) {
        wholeSIMDVectorCountPerChannel = (init.maxBlockSizeInPCMFrames / (DRFLAC_MAX_SIMD_VECTOR_SIZE / sizeof(drflac_int32)));
    } else {
        wholeSIMDVectorCountPerChannel = (init.maxBlockSizeInPCMFrames / (DRFLAC_MAX_SIMD_VECTOR_SIZE / sizeof(drflac_int32))) + 1;
    }

    decodedSamplesAllocationSize = wholeSIMDVectorCountPerChannel * DRFLAC_MAX_SIMD_VECTOR_SIZE * init.channels;

    allocationSize += decodedSamplesAllocationSize;
    allocationSize += DRFLAC_MAX_SIMD_VECTOR_SIZE;  /* Allocate extra bytes to ensure we have enough for alignment. */

#ifndef DR_FLAC_NO_OGG
    /* There's additional data required for Ogg streams. */
    if (init.container == drflac_container_ogg) {
        allocationSize += sizeof(drflac_oggbs);
    }

    DRFLAC_ZERO_MEMORY(&oggbs, sizeof(oggbs));
    if (init.container == drflac_container_ogg) {
        oggbs.onRead = onRead;
        oggbs.onSeek = onSeek;
        oggbs.pUserData = pUserData;
        oggbs.currentBytePos = init.oggFirstBytePos;
        oggbs.firstBytePos = init.oggFirstBytePos;
        oggbs.serialNumber = init.oggSerial;
        oggbs.bosPageHeader = init.oggBosHeader;
        oggbs.bytesRemainingInPage = 0;
    }
#endif

    /*
    This part is a bit awkward. We need to load the seektable so that it can be referenced in-memory, but I want the drflac object to
    consist of only a single heap allocation. To this, the size of the seek table needs to be known, which we determine when reading
    and decoding the metadata.
    */
    firstFramePos = 42;   /* <-- We know we are at byte 42 at this point. */
    seektablePos  = 0;
    seektableSize = 0;
    if (init.hasMetadataBlocks) {
        drflac_read_proc onReadOverride = onRead;
        drflac_seek_proc onSeekOverride = onSeek;
        void* pUserDataOverride = pUserData;

#ifndef DR_FLAC_NO_OGG
        if (init.container == drflac_container_ogg) {
            onReadOverride = drflac__on_read_ogg;
            onSeekOverride = drflac__on_seek_ogg;
            pUserDataOverride = (void*)&oggbs;
        }
#endif

        if (!drflac__read_and_decode_metadata(onReadOverride, onSeekOverride, onMeta, pUserDataOverride, pUserDataMD, &firstFramePos, &seektablePos, &seektableSize, &allocationCallbacks)) {
            return NULL;
        }

        allocationSize += seektableSize;
    }


    pFlac = (drflac*)drflac__malloc_from_callbacks(allocationSize, &allocationCallbacks);
    drflac__init_from_info(pFlac, &init);
    pFlac->allocationCallbacks = allocationCallbacks;
    pFlac->pDecodedSamples = (drflac_int32*)drflac_align((size_t)pFlac->pExtraData, DRFLAC_MAX_SIMD_VECTOR_SIZE);

#ifndef DR_FLAC_NO_OGG
    if (init.container == drflac_container_ogg) {
        drflac_oggbs* pInternalOggbs = (drflac_oggbs*)((drflac_uint8*)pFlac->pDecodedSamples + decodedSamplesAllocationSize + seektableSize);
        *pInternalOggbs = oggbs;

        /* The Ogg bistream needs to be layered on top of the original bitstream. */
        pFlac->bs.onRead = drflac__on_read_ogg;
        pFlac->bs.onSeek = drflac__on_seek_ogg;
        pFlac->bs.pUserData = (void*)pInternalOggbs;
        pFlac->_oggbs = (void*)pInternalOggbs;
    }
#endif

    pFlac->firstFLACFramePosInBytes = firstFramePos;

    /* NOTE: Seektables are not currently compatible with Ogg encapsulation (Ogg has its own accelerated seeking system). I may change this later, so I'm leaving this here for now. */
#ifndef DR_FLAC_NO_OGG
    if (init.container == drflac_container_ogg)
    {
        pFlac->pSeekpoints = NULL;
        pFlac->seekpointCount = 0;
    }
    else
#endif
    {
        /* If we have a seektable we need to load it now, making sure we move back to where we were previously. */
        if (seektablePos != 0) {
            pFlac->seekpointCount = seektableSize / sizeof(*pFlac->pSeekpoints);
            pFlac->pSeekpoints = (drflac_seekpoint*)((drflac_uint8*)pFlac->pDecodedSamples + decodedSamplesAllocationSize);

            /* Seek to the seektable, then just read directly into our seektable buffer. */
            if (pFlac->bs.onSeek(pFlac->bs.pUserData, (int)seektablePos, drflac_seek_origin_start)) {
                if (pFlac->bs.onRead(pFlac->bs.pUserData, pFlac->pSeekpoints, seektableSize) == seektableSize) {
                    /* Endian swap. */
                    drflac_uint32 iSeekpoint;
                    for (iSeekpoint = 0; iSeekpoint < pFlac->seekpointCount; ++iSeekpoint) {
                        pFlac->pSeekpoints[iSeekpoint].firstPCMFrame   = drflac__be2host_64(pFlac->pSeekpoints[iSeekpoint].firstPCMFrame);
                        pFlac->pSeekpoints[iSeekpoint].flacFrameOffset = drflac__be2host_64(pFlac->pSeekpoints[iSeekpoint].flacFrameOffset);
                        pFlac->pSeekpoints[iSeekpoint].pcmFrameCount   = drflac__be2host_16(pFlac->pSeekpoints[iSeekpoint].pcmFrameCount);
                    }
                } else {
                    /* Failed to read the seektable. Pretend we don't have one. */
                    pFlac->pSeekpoints = NULL;
                    pFlac->seekpointCount = 0;
                }

                /* We need to seek back to where we were. If this fails it's a critical error. */
                if (!pFlac->bs.onSeek(pFlac->bs.pUserData, (int)pFlac->firstFLACFramePosInBytes, drflac_seek_origin_start)) {
                    drflac__free_from_callbacks(pFlac, &allocationCallbacks);
                    return NULL;
                }
            } else {
                /* Failed to seek to the seektable. Ominous sign, but for now we can just pretend we don't have one. */
                pFlac->pSeekpoints = NULL;
                pFlac->seekpointCount = 0;
            }
        }
    }


    /*
    If we get here, but don't have a STREAMINFO block, it means we've opened the stream in relaxed mode and need to decode
    the first frame.
    */
    if (!init.hasStreamInfoBlock) {
        pFlac->currentFLACFrame.header = init.firstFrameHeader;
        do
        {
            drflac_result result = drflac__decode_flac_frame(pFlac);
            if (result == DRFLAC_SUCCESS) {
                break;
            } else {
                if (result == DRFLAC_CRC_MISMATCH) {
                    if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
                        drflac__free_from_callbacks(pFlac, &allocationCallbacks);
                        return NULL;
                    }
                    continue;
                } else {
                    drflac__free_from_callbacks(pFlac, &allocationCallbacks);
                    return NULL;
                }
            }
        } while (1);
    }

    return pFlac;
}



#ifndef DR_FLAC_NO_STDIO
#include <stdio.h>

static size_t drflac__on_read_stdio(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)pUserData);
}

static drflac_bool32 drflac__on_seek_stdio(void* pUserData, int offset, drflac_seek_origin origin)
{
    return fseek((FILE*)pUserData, offset, (origin == drflac_seek_origin_current) ? SEEK_CUR : SEEK_SET) == 0;
}

static FILE* drflac__fopen(const char* filename)
{
    FILE* pFile;
    pFile = fopen(filename, "rb");
    if (pFile == NULL) {
        return NULL;
    }

    return pFile;
}


drflac* drflac_open_file(const char* filename, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    drflac* pFlac;
    FILE* pFile;

    pFile = drflac__fopen(filename);
    if (pFile == NULL) {
        return NULL;
    }

    pFlac = drflac_open(drflac__on_read_stdio, drflac__on_seek_stdio, (void*)pFile, pAllocationCallbacks);
    if (pFlac == NULL) {
        fclose(pFile);
        return NULL;
    }

    return pFlac;
}

drflac* drflac_open_file_with_metadata(const char* filename, drflac_meta_proc onMeta, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks)
{
	drflac* pFlac;
	FILE* pFile;

	pFile = drflac__fopen(filename);
	if (pFile == NULL) {
		return NULL;
	}

	pFlac = drflac_open_with_metadata_private(drflac__on_read_stdio, drflac__on_seek_stdio, onMeta, drflac_container_unknown, (void*)pFile, pUserData, pAllocationCallbacks);
	if (pFlac == NULL) {
		fclose(pFile);
		return pFlac;
	}

	return pFlac;
}
#endif  /* DR_FLAC_NO_STDIO */

drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    return drflac_open_with_metadata_private(onRead, onSeek, NULL, drflac_container_unknown, pUserData, pUserData, pAllocationCallbacks);
}
drflac* drflac_open_relaxed(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_container container, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    return drflac_open_with_metadata_private(onRead, onSeek, NULL, container, pUserData, pUserData, pAllocationCallbacks);
}

drflac* drflac_open_with_metadata(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    return drflac_open_with_metadata_private(onRead, onSeek, onMeta, drflac_container_unknown, pUserData, pUserData, pAllocationCallbacks);
}
drflac* drflac_open_with_metadata_relaxed(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, drflac_container container, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    return drflac_open_with_metadata_private(onRead, onSeek, onMeta, container, pUserData, pUserData, pAllocationCallbacks);
}

void drflac_close(drflac* pFlac)
{
    if (pFlac == NULL) {
        return;
    }

#ifndef DR_FLAC_NO_STDIO
    /*
    If we opened the file with drflac_open_file() we will want to close the file handle. We can know whether or not drflac_open_file()
    was used by looking at the callbacks.
    */
    if (pFlac->bs.onRead == drflac__on_read_stdio) {
        fclose((FILE*)pFlac->bs.pUserData);
    }

#ifndef DR_FLAC_NO_OGG
    /* Need to clean up Ogg streams a bit differently due to the way the bit streaming is chained. */
    if (pFlac->container == drflac_container_ogg) {
        drflac_oggbs* oggbs = (drflac_oggbs*)pFlac->_oggbs;

        if (oggbs->onRead == drflac__on_read_stdio) {
            fclose((FILE*)oggbs->pUserData);
        }
    }
#endif
#endif

	free(pFlac->bs.cacheL2);
    drflac__free_from_callbacks(pFlac, &pFlac->allocationCallbacks);
}

static  void drflac_read_pcm_frames_s16__decode_left_side__scalar(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    drflac_uint64 i;
    drflac_uint64 frameCount4 = frameCount >> 2;

    drflac_int32 shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
    drflac_int32 shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
    for (i = 0; i < frameCount4; ++i) {
        drflac_int32 left0 = pInputSamples0[i*4+0] << shift0;
        drflac_int32 left1 = pInputSamples0[i*4+1] << shift0;
        drflac_int32 left2 = pInputSamples0[i*4+2] << shift0;
        drflac_int32 left3 = pInputSamples0[i*4+3] << shift0;

        drflac_int32 side0 = pInputSamples1[i*4+0] << shift1;
        drflac_int32 side1 = pInputSamples1[i*4+1] << shift1;
        drflac_int32 side2 = pInputSamples1[i*4+2] << shift1;
        drflac_int32 side3 = pInputSamples1[i*4+3] << shift1;

        drflac_int32 right0 = left0 - side0;
        drflac_int32 right1 = left1 - side1;
        drflac_int32 right2 = left2 - side2;
        drflac_int32 right3 = left3 - side3;

        left0  >>= 16;
        left1  >>= 16;
        left2  >>= 16;
        left3  >>= 16;

        right0 >>= 16;
        right1 >>= 16;
        right2 >>= 16;
        right3 >>= 16;

        pOutputSamples[i*8+0] = (drflac_int16)left0;
        pOutputSamples[i*8+1] = (drflac_int16)right0;
        pOutputSamples[i*8+2] = (drflac_int16)left1;
        pOutputSamples[i*8+3] = (drflac_int16)right1;
        pOutputSamples[i*8+4] = (drflac_int16)left2;
        pOutputSamples[i*8+5] = (drflac_int16)right2;
        pOutputSamples[i*8+6] = (drflac_int16)left3;
        pOutputSamples[i*8+7] = (drflac_int16)right3;
    }

    for (i = (frameCount4 << 2); i < frameCount; ++i) {
        drflac_int32 left  = pInputSamples0[i] << shift0;
        drflac_int32 side  = pInputSamples1[i] << shift1;
        drflac_int32 right = left - side;

        left  >>= 16;
        right >>= 16;

        pOutputSamples[i*2+0] = (drflac_int16)left;
        pOutputSamples[i*2+1] = (drflac_int16)right;
    }
}

#if defined(DRFLAC_SUPPORT_NEON)
static  void drflac_read_pcm_frames_s16__decode_left_side__neon(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    drflac_uint64 frameCount4;
    drflac_int32 shift0;
    drflac_int32 shift1;
    drflac_uint64 i;
    int32x4_t shift0_4;
    int32x4_t shift1_4;

    frameCount4 = frameCount >> 2;

    shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
    shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

    shift0_4 = vdupq_n_s32(shift0);
    shift1_4 = vdupq_n_s32(shift1);

    for (i = 0; i < frameCount4; ++i) {
        int32x4_t left;
        int32x4_t side;
        int32x4_t right;

        left  = vshlq_s32(vld1q_s32(pInputSamples0 + i*4), shift0_4);
        side  = vshlq_s32(vld1q_s32(pInputSamples1 + i*4), shift1_4);
        right = vsubq_s32(left, side);

        left  = vshrq_n_s32(left,  16);
        right = vshrq_n_s32(right, 16);

        drflac__vst2q_s16(pOutputSamples + i*8, vzip_s16(vmovn_s32(left), vmovn_s32(right)));
    }

    for (i = (frameCount4 << 2); i < frameCount; ++i) {
        drflac_int32 left  = pInputSamples0[i] << shift0;
        drflac_int32 side  = pInputSamples1[i] << shift1;
        drflac_int32 right = left - side;

        left  >>= 16;
        right >>= 16;

        pOutputSamples[i*2+0] = (drflac_int16)left;
        pOutputSamples[i*2+1] = (drflac_int16)right;
    }
}
#endif

static  void drflac_read_pcm_frames_s16__decode_left_side(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    if (drflac__gIsNEONSupported && pFlac->bitsPerSample <= 24) {
        drflac_read_pcm_frames_s16__decode_left_side__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
    } else
    {
        /* Scalar fallback. */
        drflac_read_pcm_frames_s16__decode_left_side__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
    }
}

static  void drflac_read_pcm_frames_s16__decode_right_side__scalar(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    drflac_uint64 i;
    drflac_uint64 frameCount4 = frameCount >> 2;

    drflac_int32 shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
    drflac_int32 shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
    for (i = 0; i < frameCount4; ++i) {
        drflac_int32 side0  = pInputSamples0[i*4+0] << shift0;
        drflac_int32 side1  = pInputSamples0[i*4+1] << shift0;
        drflac_int32 side2  = pInputSamples0[i*4+2] << shift0;
        drflac_int32 side3  = pInputSamples0[i*4+3] << shift0;

        drflac_int32 right0 = pInputSamples1[i*4+0] << shift1;
        drflac_int32 right1 = pInputSamples1[i*4+1] << shift1;
        drflac_int32 right2 = pInputSamples1[i*4+2] << shift1;
        drflac_int32 right3 = pInputSamples1[i*4+3] << shift1;

        drflac_int32 left0 = right0 + side0;
        drflac_int32 left1 = right1 + side1;
        drflac_int32 left2 = right2 + side2;
        drflac_int32 left3 = right3 + side3;

        left0  >>= 16;
        left1  >>= 16;
        left2  >>= 16;
        left3  >>= 16;

        right0 >>= 16;
        right1 >>= 16;
        right2 >>= 16;
        right3 >>= 16;

        pOutputSamples[i*8+0] = (drflac_int16)left0;
        pOutputSamples[i*8+1] = (drflac_int16)right0;
        pOutputSamples[i*8+2] = (drflac_int16)left1;
        pOutputSamples[i*8+3] = (drflac_int16)right1;
        pOutputSamples[i*8+4] = (drflac_int16)left2;
        pOutputSamples[i*8+5] = (drflac_int16)right2;
        pOutputSamples[i*8+6] = (drflac_int16)left3;
        pOutputSamples[i*8+7] = (drflac_int16)right3;
    }

    for (i = (frameCount4 << 2); i < frameCount; ++i) {
        drflac_int32 side  = pInputSamples0[i] << shift0;
        drflac_int32 right = pInputSamples1[i] << shift1;
        drflac_int32 left  = right + side;

        left  >>= 16;
        right >>= 16;

        pOutputSamples[i*2+0] = (drflac_int16)left;
        pOutputSamples[i*2+1] = (drflac_int16)right;
    }
}

#if defined(DRFLAC_SUPPORT_NEON)
static  void drflac_read_pcm_frames_s16__decode_right_side__neon(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    drflac_uint64 frameCount4;
    drflac_int32 shift0;
    drflac_int32 shift1;
    drflac_uint64 i;
    int32x4_t shift0_4;
    int32x4_t shift1_4;

    frameCount4 = frameCount >> 2;

    shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
    shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

    shift0_4 = vdupq_n_s32(shift0);
    shift1_4 = vdupq_n_s32(shift1);

    for (i = 0; i < frameCount4; ++i) {
        int32x4_t side;
        int32x4_t right;
        int32x4_t left;

        side  = vshlq_s32(vld1q_s32(pInputSamples0 + i*4), shift0_4);
        right = vshlq_s32(vld1q_s32(pInputSamples1 + i*4), shift1_4);
        left  = vaddq_s32(right, side);

        left  = vshrq_n_s32(left,  16);
        right = vshrq_n_s32(right, 16);

        drflac__vst2q_s16(pOutputSamples + i*8, vzip_s16(vmovn_s32(left), vmovn_s32(right)));
    }

    for (i = (frameCount4 << 2); i < frameCount; ++i) {
        drflac_int32 side  = pInputSamples0[i] << shift0;
        drflac_int32 right = pInputSamples1[i] << shift1;
        drflac_int32 left  = right + side;

        left  >>= 16;
        right >>= 16;

        pOutputSamples[i*2+0] = (drflac_int16)left;
        pOutputSamples[i*2+1] = (drflac_int16)right;
    }
}
#endif

static  void drflac_read_pcm_frames_s16__decode_right_side(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    if (drflac__gIsNEONSupported && pFlac->bitsPerSample <= 24) {
        drflac_read_pcm_frames_s16__decode_right_side__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
    } else
    {
        /* Scalar fallback. */
        drflac_read_pcm_frames_s16__decode_right_side__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
    }
}

static  void drflac_read_pcm_frames_s16__decode_mid_side__scalar(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    drflac_uint64 i;
    drflac_uint64 frameCount4 = frameCount >> 2;

    int shift = unusedBitsPerSample;
    if (shift > 0) {
        shift -= 1;
        for (i = 0; i < frameCount4; ++i) {
            drflac_int32 temp0L;
            drflac_int32 temp1L;
            drflac_int32 temp2L;
            drflac_int32 temp3L;
            drflac_int32 temp0R;
            drflac_int32 temp1R;
            drflac_int32 temp2R;
            drflac_int32 temp3R;

            drflac_int32 mid0  = pInputSamples0[i*4+0] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
            drflac_int32 mid1  = pInputSamples0[i*4+1] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
            drflac_int32 mid2  = pInputSamples0[i*4+2] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
            drflac_int32 mid3  = pInputSamples0[i*4+3] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;

            drflac_int32 side0 = pInputSamples1[i*4+0] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
            drflac_int32 side1 = pInputSamples1[i*4+1] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
            drflac_int32 side2 = pInputSamples1[i*4+2] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
            drflac_int32 side3 = pInputSamples1[i*4+3] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

            mid0 = (((drflac_uint32)mid0) << 1) | (side0 & 0x01);
            mid1 = (((drflac_uint32)mid1) << 1) | (side1 & 0x01);
            mid2 = (((drflac_uint32)mid2) << 1) | (side2 & 0x01);
            mid3 = (((drflac_uint32)mid3) << 1) | (side3 & 0x01);

            temp0L = ((mid0 + side0) << shift);
            temp1L = ((mid1 + side1) << shift);
            temp2L = ((mid2 + side2) << shift);
            temp3L = ((mid3 + side3) << shift);

            temp0R = ((mid0 - side0) << shift);
            temp1R = ((mid1 - side1) << shift);
            temp2R = ((mid2 - side2) << shift);
            temp3R = ((mid3 - side3) << shift);

            temp0L >>= 16;
            temp1L >>= 16;
            temp2L >>= 16;
            temp3L >>= 16;

            temp0R >>= 16;
            temp1R >>= 16;
            temp2R >>= 16;
            temp3R >>= 16;

            pOutputSamples[i*8+0] = (drflac_int16)temp0L;
            pOutputSamples[i*8+1] = (drflac_int16)temp0R;
            pOutputSamples[i*8+2] = (drflac_int16)temp1L;
            pOutputSamples[i*8+3] = (drflac_int16)temp1R;
            pOutputSamples[i*8+4] = (drflac_int16)temp2L;
            pOutputSamples[i*8+5] = (drflac_int16)temp2R;
            pOutputSamples[i*8+6] = (drflac_int16)temp3L;
            pOutputSamples[i*8+7] = (drflac_int16)temp3R;
        }
    } else {
        for (i = 0; i < frameCount4; ++i) {
            drflac_int32 temp0L;
            drflac_int32 temp1L;
            drflac_int32 temp2L;
            drflac_int32 temp3L;
            drflac_int32 temp0R;
            drflac_int32 temp1R;
            drflac_int32 temp2R;
            drflac_int32 temp3R;

            drflac_int32 mid0  = pInputSamples0[i*4+0] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
            drflac_int32 mid1  = pInputSamples0[i*4+1] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
            drflac_int32 mid2  = pInputSamples0[i*4+2] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
            drflac_int32 mid3  = pInputSamples0[i*4+3] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;

            drflac_int32 side0 = pInputSamples1[i*4+0] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
            drflac_int32 side1 = pInputSamples1[i*4+1] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
            drflac_int32 side2 = pInputSamples1[i*4+2] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
            drflac_int32 side3 = pInputSamples1[i*4+3] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

            mid0 = (((drflac_uint32)mid0) << 1) | (side0 & 0x01);
            mid1 = (((drflac_uint32)mid1) << 1) | (side1 & 0x01);
            mid2 = (((drflac_uint32)mid2) << 1) | (side2 & 0x01);
            mid3 = (((drflac_uint32)mid3) << 1) | (side3 & 0x01);

            temp0L = ((mid0 + side0) >> 1);
            temp1L = ((mid1 + side1) >> 1);
            temp2L = ((mid2 + side2) >> 1);
            temp3L = ((mid3 + side3) >> 1);

            temp0R = ((mid0 - side0) >> 1);
            temp1R = ((mid1 - side1) >> 1);
            temp2R = ((mid2 - side2) >> 1);
            temp3R = ((mid3 - side3) >> 1);

            temp0L >>= 16;
            temp1L >>= 16;
            temp2L >>= 16;
            temp3L >>= 16;

            temp0R >>= 16;
            temp1R >>= 16;
            temp2R >>= 16;
            temp3R >>= 16;

            pOutputSamples[i*8+0] = (drflac_int16)temp0L;
            pOutputSamples[i*8+1] = (drflac_int16)temp0R;
            pOutputSamples[i*8+2] = (drflac_int16)temp1L;
            pOutputSamples[i*8+3] = (drflac_int16)temp1R;
            pOutputSamples[i*8+4] = (drflac_int16)temp2L;
            pOutputSamples[i*8+5] = (drflac_int16)temp2R;
            pOutputSamples[i*8+6] = (drflac_int16)temp3L;
            pOutputSamples[i*8+7] = (drflac_int16)temp3R;
        }
    }

    for (i = (frameCount4 << 2); i < frameCount; ++i) {
        drflac_int32 mid  = pInputSamples0[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
        drflac_int32 side = pInputSamples1[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

        mid = (((drflac_uint32)mid) << 1) | (side & 0x01);

        pOutputSamples[i*2+0] = (drflac_int16)((((mid + side) >> 1) << unusedBitsPerSample) >> 16);
        pOutputSamples[i*2+1] = (drflac_int16)((((mid - side) >> 1) << unusedBitsPerSample) >> 16);
    }
}

#if defined(DRFLAC_SUPPORT_NEON)
static  void drflac_read_pcm_frames_s16__decode_mid_side__neon(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    drflac_uint64 i;
    drflac_uint64 frameCount4;
    int shift;
    int32x4_t wbpsShift0_4; /* wbps = Wasted Bits Per Sample */
    int32x4_t wbpsShift1_4; /* wbps = Wasted Bits Per Sample */

    frameCount4 = frameCount >> 2;

    wbpsShift0_4 = vdupq_n_s32(pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
    wbpsShift1_4 = vdupq_n_s32(pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

    shift = unusedBitsPerSample;
    if (shift == 0) {
        for (i = 0; i < frameCount4; ++i) {
            int32x4_t mid;
            int32x4_t side;
            int32x4_t left;
            int32x4_t right;

            mid   = vshlq_s32(vld1q_s32(pInputSamples0 + i*4), wbpsShift0_4);
            side  = vshlq_s32(vld1q_s32(pInputSamples1 + i*4), wbpsShift1_4);

            mid   = vorrq_s32(vshlq_n_s32(mid, 1), vandq_s32(side, vdupq_n_s32(1)));

            left  = vshrq_n_s32(vaddq_s32(mid, side), 1);
            right = vshrq_n_s32(vsubq_s32(mid, side), 1);

            left  = vshrq_n_s32(left,  16);
            right = vshrq_n_s32(right, 16);

            drflac__vst2q_s16(pOutputSamples + i*8, vzip_s16(vmovn_s32(left), vmovn_s32(right)));
        }

        for (i = (frameCount4 << 2); i < frameCount; ++i) {
            drflac_int32 mid  = pInputSamples0[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
            drflac_int32 side = pInputSamples1[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

            mid = (((drflac_uint32)mid) << 1) | (side & 0x01);

            pOutputSamples[i*2+0] = (drflac_int16)(((mid + side) >> 1) >> 16);
            pOutputSamples[i*2+1] = (drflac_int16)(((mid - side) >> 1) >> 16);
        }
    } else {
        int32x4_t shift4;

        shift -= 1;
        shift4 = vdupq_n_s32(shift);

        for (i = 0; i < frameCount4; ++i) {
            int32x4_t mid;
            int32x4_t side;
            int32x4_t left;
            int32x4_t right;

            mid   = vshlq_s32(vld1q_s32(pInputSamples0 + i*4), wbpsShift0_4);
            side  = vshlq_s32(vld1q_s32(pInputSamples1 + i*4), wbpsShift1_4);

            mid   = vorrq_s32(vshlq_n_s32(mid, 1), vandq_s32(side, vdupq_n_s32(1)));

            left  = vshlq_s32(vaddq_s32(mid, side), shift4);
            right = vshlq_s32(vsubq_s32(mid, side), shift4);

            left  = vshrq_n_s32(left,  16);
            right = vshrq_n_s32(right, 16);

            drflac__vst2q_s16(pOutputSamples + i*8, vzip_s16(vmovn_s32(left), vmovn_s32(right)));
        }

        for (i = (frameCount4 << 2); i < frameCount; ++i) {
            drflac_int32 mid  = pInputSamples0[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
            drflac_int32 side = pInputSamples1[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

            mid = (((drflac_uint32)mid) << 1) | (side & 0x01);

            pOutputSamples[i*2+0] = (drflac_int16)(((mid + side) << shift) >> 16);
            pOutputSamples[i*2+1] = (drflac_int16)(((mid - side) << shift) >> 16);
        }
    }
}
#endif

static  void drflac_read_pcm_frames_s16__decode_mid_side(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    if (drflac__gIsNEONSupported && pFlac->bitsPerSample <= 24) {
        drflac_read_pcm_frames_s16__decode_mid_side__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
    } else
    {
        /* Scalar fallback. */
        drflac_read_pcm_frames_s16__decode_mid_side__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
    }
}

static  void drflac_read_pcm_frames_s16__decode_independent_stereo__scalar(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    drflac_uint64 i;
    drflac_uint64 frameCount4 = frameCount >> 2;

    int shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
    int shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

    for (i = 0; i < frameCount4; ++i) {
        drflac_int32 tempL0 = pInputSamples0[i*4+0] << shift0;
        drflac_int32 tempL1 = pInputSamples0[i*4+1] << shift0;
        drflac_int32 tempL2 = pInputSamples0[i*4+2] << shift0;
        drflac_int32 tempL3 = pInputSamples0[i*4+3] << shift0;

        drflac_int32 tempR0 = pInputSamples1[i*4+0] << shift1;
        drflac_int32 tempR1 = pInputSamples1[i*4+1] << shift1;
        drflac_int32 tempR2 = pInputSamples1[i*4+2] << shift1;
        drflac_int32 tempR3 = pInputSamples1[i*4+3] << shift1;

        tempL0 >>= 16;
        tempL1 >>= 16;
        tempL2 >>= 16;
        tempL3 >>= 16;

        tempR0 >>= 16;
        tempR1 >>= 16;
        tempR2 >>= 16;
        tempR3 >>= 16;

        pOutputSamples[i*8+0] = (drflac_int16)tempL0;
        pOutputSamples[i*8+1] = (drflac_int16)tempR0;
        pOutputSamples[i*8+2] = (drflac_int16)tempL1;
        pOutputSamples[i*8+3] = (drflac_int16)tempR1;
        pOutputSamples[i*8+4] = (drflac_int16)tempL2;
        pOutputSamples[i*8+5] = (drflac_int16)tempR2;
        pOutputSamples[i*8+6] = (drflac_int16)tempL3;
        pOutputSamples[i*8+7] = (drflac_int16)tempR3;
    }

    for (i = (frameCount4 << 2); i < frameCount; ++i) {
        pOutputSamples[i*2+0] = (drflac_int16)((pInputSamples0[i] << shift0) >> 16);
        pOutputSamples[i*2+1] = (drflac_int16)((pInputSamples1[i] << shift1) >> 16);
    }
}

#if defined(DRFLAC_SUPPORT_NEON)
static  void drflac_read_pcm_frames_s16__decode_independent_stereo__neon(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    drflac_uint64 i;
    drflac_uint64 frameCount4 = frameCount >> 2;

    drflac_int32 shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
    drflac_int32 shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

    int32x4_t shift0_4 = vdupq_n_s32(shift0);
    int32x4_t shift1_4 = vdupq_n_s32(shift1);

    for (i = 0; i < frameCount4; ++i) {
        int32x4_t left;
        int32x4_t right;

        left  = vshlq_s32(vld1q_s32(pInputSamples0 + i*4), shift0_4);
        right = vshlq_s32(vld1q_s32(pInputSamples1 + i*4), shift1_4);

        left  = vshrq_n_s32(left,  16);
        right = vshrq_n_s32(right, 16);

        drflac__vst2q_s16(pOutputSamples + i*8, vzip_s16(vmovn_s32(left), vmovn_s32(right)));
    }

    for (i = (frameCount4 << 2); i < frameCount; ++i) {
        pOutputSamples[i*2+0] = (drflac_int16)((pInputSamples0[i] << shift0) >> 16);
        pOutputSamples[i*2+1] = (drflac_int16)((pInputSamples1[i] << shift1) >> 16);
    }
}
#endif

static  void drflac_read_pcm_frames_s16__decode_independent_stereo(drflac* pFlac, drflac_uint64 frameCount, drflac_int32 unusedBitsPerSample, const drflac_int32* pInputSamples0, const drflac_int32* pInputSamples1, drflac_int16* pOutputSamples)
{
    if (drflac__gIsNEONSupported && pFlac->bitsPerSample <= 24) {
        drflac_read_pcm_frames_s16__decode_independent_stereo__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
    } else
    {
        /* Scalar fallback. */
        drflac_read_pcm_frames_s16__decode_independent_stereo__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
    }
}

drflac_uint64 drflac_read_pcm_frames_s16(drflac* pFlac, drflac_uint64 framesToRead, drflac_int16* pBufferOut)
{
    drflac_uint64 framesRead;
    drflac_int32 unusedBitsPerSample;

    if (pFlac == NULL || framesToRead == 0) {
        return 0;
    }

    if (pBufferOut == NULL) {
        return drflac__seek_forward_by_pcm_frames(pFlac, framesToRead);
    }

    unusedBitsPerSample = 32 - pFlac->bitsPerSample;

    framesRead = 0;
    while (framesToRead > 0) {
        /* If we've run out of samples in this frame, go to the next. */
        if (pFlac->currentFLACFrame.pcmFramesRemaining == 0) {
            if (!drflac__read_and_decode_next_flac_frame(pFlac)) {
                break;  /* Couldn't read the next frame, so just break from the loop and return. */
            }
        } else {
            unsigned int channelCount = drflac__get_channel_count_from_channel_assignment(pFlac->currentFLACFrame.header.channelAssignment);
            drflac_uint64 iFirstPCMFrame = pFlac->currentFLACFrame.header.blockSizeInPCMFrames - pFlac->currentFLACFrame.pcmFramesRemaining;
            drflac_uint64 frameCountThisIteration = framesToRead;

            if (frameCountThisIteration > pFlac->currentFLACFrame.pcmFramesRemaining) {
                frameCountThisIteration = pFlac->currentFLACFrame.pcmFramesRemaining;
            }

            if (channelCount == 2) {
                const drflac_int32* pDecodedSamples0 = pFlac->currentFLACFrame.subframes[0].pSamplesS32 + iFirstPCMFrame;
                const drflac_int32* pDecodedSamples1 = pFlac->currentFLACFrame.subframes[1].pSamplesS32 + iFirstPCMFrame;

                switch (pFlac->currentFLACFrame.header.channelAssignment)
                {
                    case DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE:
                    {
                        drflac_read_pcm_frames_s16__decode_left_side(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
                    } break;

                    case DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                    {
                        drflac_read_pcm_frames_s16__decode_right_side(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
                    } break;

                    case DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE:
                    {
                        drflac_read_pcm_frames_s16__decode_mid_side(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
                    } break;

                    case DRFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT:
                    default:
                    {
                        drflac_read_pcm_frames_s16__decode_independent_stereo(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
                    } break;
                }
            } else {
                /* Generic interleaving. */
                drflac_uint64 i;
                for (i = 0; i < frameCountThisIteration; ++i) {
                    unsigned int j;
                    for (j = 0; j < channelCount; ++j) {
                        drflac_int32 sampleS32 = (drflac_int32)((drflac_uint32)(pFlac->currentFLACFrame.subframes[j].pSamplesS32[iFirstPCMFrame + i]) << (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[j].wastedBitsPerSample));
                        pBufferOut[(i*channelCount)+j] = (drflac_int16)(sampleS32 >> 16);
                    }
                }
            }

            framesRead                += frameCountThisIteration;
            pBufferOut                += frameCountThisIteration * channelCount;
            framesToRead              -= frameCountThisIteration;
            pFlac->currentPCMFrame    += frameCountThisIteration;
            pFlac->currentFLACFrame.pcmFramesRemaining -= (drflac_uint32)frameCountThisIteration;
        }
    }

    return framesRead;
}

drflac_bool32 drflac_seek_to_pcm_frame(drflac* pFlac, drflac_uint64 pcmFrameIndex)
{
    if (pFlac == NULL) {
        return DRFLAC_FALSE;
    }

    /* Don't do anything if we're already on the seek point. */
    if (pFlac->currentPCMFrame == pcmFrameIndex) {
        return DRFLAC_TRUE;
    }

    /*
    If we don't know where the first frame begins then we can't seek. This will happen when the STREAMINFO block was not present
    when the decoder was opened.
    */
    if (pFlac->firstFLACFramePosInBytes == 0) {
        return DRFLAC_FALSE;
    }

    if (pcmFrameIndex == 0) {
        pFlac->currentPCMFrame = 0;
        return drflac__seek_to_first_frame(pFlac);
    } else {
        drflac_bool32 wasSuccessful = DRFLAC_FALSE;

        /* Clamp the sample to the end. */
        if (pcmFrameIndex > pFlac->totalPCMFrameCount) {
            pcmFrameIndex = pFlac->totalPCMFrameCount;
        }

        /* If the target sample and the current sample are in the same frame we just move the position forward. */
        if (pcmFrameIndex > pFlac->currentPCMFrame) {
            /* Forward. */
            drflac_uint32 offset = (drflac_uint32)(pcmFrameIndex - pFlac->currentPCMFrame);
            if (pFlac->currentFLACFrame.pcmFramesRemaining >  offset) {
                pFlac->currentFLACFrame.pcmFramesRemaining -= offset;
                pFlac->currentPCMFrame = pcmFrameIndex;
                return DRFLAC_TRUE;
            }
        } else {
            /* Backward. */
            drflac_uint32 offsetAbs = (drflac_uint32)(pFlac->currentPCMFrame - pcmFrameIndex);
            drflac_uint32 currentFLACFramePCMFrameCount = pFlac->currentFLACFrame.header.blockSizeInPCMFrames;
            drflac_uint32 currentFLACFramePCMFramesConsumed = currentFLACFramePCMFrameCount - pFlac->currentFLACFrame.pcmFramesRemaining;
            if (currentFLACFramePCMFramesConsumed > offsetAbs) {
                pFlac->currentFLACFrame.pcmFramesRemaining += offsetAbs;
                pFlac->currentPCMFrame = pcmFrameIndex;
                return DRFLAC_TRUE;
            }
        }

        /*
        Different techniques depending on encapsulation. Using the native FLAC seektable with Ogg encapsulation is a bit awkward so
        we'll instead use Ogg's natural seeking facility.
        */
#ifndef DR_FLAC_NO_OGG
        if (pFlac->container == drflac_container_ogg)
        {
            wasSuccessful = drflac_ogg__seek_to_pcm_frame(pFlac, pcmFrameIndex);
        }
        else
#endif
        {
            /* First try seeking via the seek table. If this fails, fall back to a brute force seek which is much slower. */
            if (!wasSuccessful && !pFlac->_noSeekTableSeek) {
                wasSuccessful = drflac__seek_to_pcm_frame__seek_table(pFlac, pcmFrameIndex);
            }

#if !defined(DR_FLAC_NO_CRC)
            /* Fall back to binary search if seek table seeking fails. This requires the length of the stream to be known. */
            if (!wasSuccessful && !pFlac->_noBinarySearchSeek && pFlac->totalPCMFrameCount > 0) {
                wasSuccessful = drflac__seek_to_pcm_frame__binary_search(pFlac, pcmFrameIndex);
            }
#endif

            /* Fall back to brute force if all else fails. */
            if (!wasSuccessful && !pFlac->_noBruteForceSeek) {
                wasSuccessful = drflac__seek_to_pcm_frame__brute_force(pFlac, pcmFrameIndex);
            }
        }

        pFlac->currentPCMFrame = pcmFrameIndex;
        return wasSuccessful;
    }
}

/* High Level APIs */

#if defined(SIZE_MAX)
    #define DRFLAC_SIZE_MAX  SIZE_MAX
#else
    #if defined(DRFLAC_64BIT)
        #define DRFLAC_SIZE_MAX  ((drflac_uint64)0xFFFFFFFFFFFFFFFF)
    #else
        #define DRFLAC_SIZE_MAX  0xFFFFFFFF
    #endif
#endif

void drflac_free(void* p, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    if (pAllocationCallbacks != NULL) {
        drflac__free_from_callbacks(p, pAllocationCallbacks);
    } else {
        drflac__free_default(p, NULL);
    }
}

void drflac_init_vorbis_comment_iterator(drflac_vorbis_comment_iterator* pIter, drflac_uint32 commentCount, const void* pComments)
{
    if (pIter == NULL) {
        return;
    }

    pIter->countRemaining = commentCount;
    pIter->pRunningData   = (const char*)pComments;
}

const char* drflac_next_vorbis_comment(drflac_vorbis_comment_iterator* pIter, drflac_uint32* pCommentLengthOut)
{
    drflac_int32 length;
    const char* pComment;

    /* Safety. */
    if (pCommentLengthOut) {
        *pCommentLengthOut = 0;
    }

    if (pIter == NULL || pIter->countRemaining == 0 || pIter->pRunningData == NULL) {
        return NULL;
    }

    length = drflac__le2host_32(*(const drflac_uint32*)pIter->pRunningData);
    pIter->pRunningData += 4;

    pComment = pIter->pRunningData;
    pIter->pRunningData += length;
    pIter->countRemaining -= 1;

    if (pCommentLengthOut) {
        *pCommentLengthOut = length;
    }

    return pComment;
}


void drflac_init_cuesheet_track_iterator(drflac_cuesheet_track_iterator* pIter, drflac_uint32 trackCount, const void* pTrackData)
{
    if (pIter == NULL) {
        return;
    }

    pIter->countRemaining = trackCount;
    pIter->pRunningData   = (const char*)pTrackData;
}

drflac_bool32 drflac_next_cuesheet_track(drflac_cuesheet_track_iterator* pIter, drflac_cuesheet_track* pCuesheetTrack)
{
    drflac_cuesheet_track cuesheetTrack;
    const char* pRunningData;
    drflac_uint64 offsetHi;
    drflac_uint64 offsetLo;

    if (pIter == NULL || pIter->countRemaining == 0 || pIter->pRunningData == NULL) {
        return DRFLAC_FALSE;
    }

    pRunningData = pIter->pRunningData;

    offsetHi                   = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;
    offsetLo                   = drflac__be2host_32(*(const drflac_uint32*)pRunningData); pRunningData += 4;
    cuesheetTrack.offset       = offsetLo | (offsetHi << 32);
    cuesheetTrack.trackNumber  = pRunningData[0];                                         pRunningData += 1;
    DRFLAC_COPY_MEMORY(cuesheetTrack.ISRC, pRunningData, sizeof(cuesheetTrack.ISRC));     pRunningData += 12;
    cuesheetTrack.isAudio      = (pRunningData[0] & 0x80) != 0;
    cuesheetTrack.preEmphasis  = (pRunningData[0] & 0x40) != 0;                           pRunningData += 14;
    cuesheetTrack.indexCount   = pRunningData[0];                                         pRunningData += 1;
    cuesheetTrack.pIndexPoints = (const drflac_cuesheet_track_index*)pRunningData;        pRunningData += cuesheetTrack.indexCount * sizeof(drflac_cuesheet_track_index);

    pIter->pRunningData = pRunningData;
    pIter->countRemaining -= 1;

    if (pCuesheetTrack) {
        *pCuesheetTrack = cuesheetTrack;
    }

    return DRFLAC_TRUE;
}

#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
#endif  /* DR_FLAC_IMPLEMENTATION */


/*
REVISION HISTORY
================
v0.12.2 - 2019-10-07
  - Internal code clean up.

v0.12.1 - 2019-09-29
  - Fix some Clang Static Analyzer warnings.
  - Fix an unused variable warning.

v0.12.0 - 2019-09-23
  - API CHANGE: Add support for user defined memory allocation routines. This system allows the program to specify their own memory allocation
    routines with a user data pointer for client-specific contextual data. This adds an extra parameter to the end of the following APIs:
    - drflac_open()
    - drflac_open_relaxed()
    - drflac_open_with_metadata()
    - drflac_open_with_metadata_relaxed()
    - drflac_open_file()
    - drflac_open_file_with_metadata()
    - drflac_open_memory()
    - drflac_open_memory_with_metadata()
    - drflac_open_and_read_pcm_frames_s32()
    - drflac_open_and_read_pcm_frames_s16()
    - drflac_open_and_read_pcm_frames_f32()
    - drflac_open_file_and_read_pcm_frames_s32()
    - drflac_open_file_and_read_pcm_frames_s16()
    - drflac_open_file_and_read_pcm_frames_f32()
    - drflac_open_memory_and_read_pcm_frames_s32()
    - drflac_open_memory_and_read_pcm_frames_s16()
    - drflac_open_memory_and_read_pcm_frames_f32()
    Set this extra parameter to NULL to use defaults which is the same as the previous behaviour. Setting this NULL will use
    DRFLAC_MALLOC, DRFLAC_REALLOC and DRFLAC_FREE.
  - Remove deprecated APIs:
    - drflac_read_s32()
    - drflac_read_s16()
    - drflac_read_f32()
    - drflac_seek_to_sample()
    - drflac_open_and_decode_s32()
    - drflac_open_and_decode_s16()
    - drflac_open_and_decode_f32()
    - drflac_open_and_decode_file_s32()
    - drflac_open_and_decode_file_s16()
    - drflac_open_and_decode_file_f32()
    - drflac_open_and_decode_memory_s32()
    - drflac_open_and_decode_memory_s16()
    - drflac_open_and_decode_memory_f32()
  - Remove drflac.totalSampleCount which is now replaced with drflac.totalPCMFrameCount. You can emulate drflac.totalSampleCount
    by doing pFlac->totalPCMFrameCount*pFlac->channels.
  - Rename drflac.currentFrame to drflac.currentFLACFrame to remove ambiguity with PCM frames.
  - Fix errors when seeking to the end of a stream.
  - Optimizations to seeking.
  - SSE improvements and optimizations.
  - ARM NEON optimizations.
  - Optimizations to drflac_read_pcm_frames_s16().
  - Optimizations to drflac_read_pcm_frames_s32().

v0.11.10 - 2019-06-26
  - Fix a compiler error.

v0.11.9 - 2019-06-16
  - Silence some ThreadSanitizer warnings.

v0.11.8 - 2019-05-21
  - Fix warnings.

v0.11.7 - 2019-05-06
  - C89 fixes.

v0.11.6 - 2019-05-05
  - Add support for C89.
  - Fix a compiler warning when CRC is disabled.
  - Change license to choice of public domain or MIT-0.

v0.11.5 - 2019-04-19
  - Fix a compiler error with GCC.

v0.11.4 - 2019-04-17
  - Fix some warnings with GCC when compiling with -std=c99.

v0.11.3 - 2019-04-07
  - Silence warnings with GCC.

v0.11.2 - 2019-03-10
  - Fix a warning.

v0.11.1 - 2019-02-17
  - Fix a potential bug with seeking.

v0.11.0 - 2018-12-16
  - API CHANGE: Deprecated drflac_read_s32(), drflac_read_s16() and drflac_read_f32() and replaced them with
    drflac_read_pcm_frames_s32(), drflac_read_pcm_frames_s16() and drflac_read_pcm_frames_f32(). The new APIs take
    and return PCM frame counts instead of sample counts. To upgrade you will need to change the input count by
    dividing it by the channel count, and then do the same with the return value.
  - API_CHANGE: Deprecated drflac_seek_to_sample() and replaced with drflac_seek_to_pcm_frame(). Same rules as
    the changes to drflac_read_*() apply.
  - API CHANGE: Deprecated drflac_open_and_decode_*() and replaced with drflac_open_*_and_read_*(). Same rules as
    the changes to drflac_read_*() apply.
  - Optimizations.

v0.10.0 - 2018-09-11
  - Remove the DR_FLAC_NO_WIN32_IO option and the Win32 file IO functionality. If you need to use Win32 file IO you
    need to do it yourself via the callback API.
  - Fix the clang build.
  - Fix undefined behavior.
  - Fix errors with CUESHEET metdata blocks.
  - Add an API for iterating over each cuesheet track in the CUESHEET metadata block. This works the same way as the
    Vorbis comment API.
  - Other miscellaneous bug fixes, mostly relating to invalid FLAC streams.
  - Minor optimizations.

v0.9.11 - 2018-08-29
  - Fix a bug with sample reconstruction.

v0.9.10 - 2018-08-07
  - Improve 64-bit detection.

v0.9.9 - 2018-08-05
  - Fix C++ build on older versions of GCC.

v0.9.8 - 2018-07-24
  - Fix compilation errors.

v0.9.7 - 2018-07-05
  - Fix a warning.

v0.9.6 - 2018-06-29
  - Fix some typos.

v0.9.5 - 2018-06-23
  - Fix some warnings.

v0.9.4 - 2018-06-14
  - Optimizations to seeking.
  - Clean up.

v0.9.3 - 2018-05-22
  - Bug fix.

v0.9.2 - 2018-05-12
  - Fix a compilation error due to a missing break statement.

v0.9.1 - 2018-04-29
  - Fix compilation error with Clang.

v0.9 - 2018-04-24
  - Fix Clang build.
  - Start using major.minor.revision versioning.

v0.8g - 2018-04-19
  - Fix build on non-x86/x64 architectures.

v0.8f - 2018-02-02
  - Stop pretending to support changing rate/channels mid stream.

v0.8e - 2018-02-01
  - Fix a crash when the block size of a frame is larger than the maximum block size defined by the FLAC stream.
  - Fix a crash the the Rice partition order is invalid.

v0.8d - 2017-09-22
  - Add support for decoding streams with ID3 tags. ID3 tags are just skipped.

v0.8c - 2017-09-07
  - Fix warning on non-x86/x64 architectures.

v0.8b - 2017-08-19
  - Fix build on non-x86/x64 architectures.

v0.8a - 2017-08-13
  - A small optimization for the Clang build.

v0.8 - 2017-08-12
  - API CHANGE: Rename dr_* types to drflac_*.
  - Optimizations. This brings dr_flac back to about the same class of efficiency as the reference implementation.
  - Add support for custom implementations of malloc(), realloc(), etc.
  - Add CRC checking to Ogg encapsulated streams.
  - Fix VC++ 6 build. This is only for the C++ compiler. The C compiler is not currently supported.
  - Bug fixes.

v0.7 - 2017-07-23
  - Add support for opening a stream without a header block. To do this, use drflac_open_relaxed() / drflac_open_with_metadata_relaxed().

v0.6 - 2017-07-22
  - Add support for recovering from invalid frames. With this change, dr_flac will simply skip over invalid frames as if they
    never existed. Frames are checked against their sync code, the CRC-8 of the frame header and the CRC-16 of the whole frame.

v0.5 - 2017-07-16
  - Fix typos.
  - Change drflac_bool* types to unsigned.
  - Add CRC checking. This makes dr_flac slower, but can be disabled with #define DR_FLAC_NO_CRC.

v0.4f - 2017-03-10
  - Fix a couple of bugs with the bitstreaming code.

v0.4e - 2017-02-17
  - Fix some warnings.

v0.4d - 2016-12-26
  - Add support for 32-bit floating-point PCM decoding.
  - Use drflac_int* and drflac_uint* sized types to improve compiler support.
  - Minor improvements to documentation.

v0.4c - 2016-12-26
  - Add support for signed 16-bit integer PCM decoding.

v0.4b - 2016-10-23
  - A minor change to drflac_bool8 and drflac_bool32 types.

v0.4a - 2016-10-11
  - Rename drBool32 to drflac_bool32 for styling consistency.

v0.4 - 2016-09-29
  - API/ABI CHANGE: Use fixed size 32-bit booleans instead of the built-in bool type.
  - API CHANGE: Rename drflac_open_and_decode*() to drflac_open_and_decode*_s32().
  - API CHANGE: Swap the order of "channels" and "sampleRate" parameters in drflac_open_and_decode*(). Rationale for this is to
    keep it consistent with drflac_audio.

v0.3f - 2016-09-21
  - Fix a warning with GCC.

v0.3e - 2016-09-18
  - Fixed a bug where GCC 4.3+ was not getting properly identified.
  - Fixed a few typos.
  - Changed date formats to ISO 8601 (YYYY-MM-DD).

v0.3d - 2016-06-11
  - Minor clean up.

v0.3c - 2016-05-28
  - Fixed compilation error.

v0.3b - 2016-05-16
  - Fixed Linux/GCC build.
  - Updated documentation.

v0.3a - 2016-05-15
  - Minor fixes to documentation.

v0.3 - 2016-05-11
  - Optimizations. Now at about parity with the reference implementation on 32-bit builds.
  - Lots of clean up.

v0.2b - 2016-05-10
  - Bug fixes.

v0.2a - 2016-05-10
  - Made drflac_open_and_decode() more robust.
  - Removed an unused debugging variable

v0.2 - 2016-05-09
  - Added support for Ogg encapsulation.
  - API CHANGE. Have the onSeek callback take a third argument which specifies whether or not the seek
    should be relative to the start or the current position. Also changes the seeking rules such that
    seeking offsets will never be negative.
  - Have drflac_open_and_decode() fail gracefully if the stream has an unknown total sample count.

v0.1b - 2016-05-07
  - Properly close the file handle in drflac_open_file() and family when the decoder fails to initialize.
  - Removed a stale comment.

v0.1a - 2016-05-05
  - Minor formatting changes.
  - Fixed a warning on the GCC build.

v0.1 - 2016-05-03
  - Initial versioned release.
*/

/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2018 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
