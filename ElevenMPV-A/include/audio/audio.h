#ifndef _ELEVENMPV_AUDIO_H_
#define _ELEVENMPV_AUDIO_H_

#include <paf.h>

#include <opus/opus.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <shellaudio.h>
#include <sceavplayer.h>

#include "nestegg.h"
#include "netmedia.h"
#include "localmedia.h"
#include "xmp.h"
#include "audiocodec.h"

using namespace paf;

typedef SceUInt64 drflac_uint64;
typedef SceInt64 ogg_int64_t;

namespace audio {

	class PlayerCoverLoaderJob : public job::JobItem
	{
	public:

		using job::JobItem::JobItem;

		~PlayerCoverLoaderJob()
		{

		}

		SceVoid Run();

		SceVoid Finish();

		ScePVoid workptr;
		SceSize size;
		SceBool isExtMem;
		graph::Surface *coverTex;
	};

	class YoutubePlayerCoverLoaderJob : public job::JobItem
	{
	public:

		using job::JobItem::JobItem;

		~YoutubePlayerCoverLoaderJob()
		{

		}

		SceVoid Run();

		SceVoid Finish();

		string url;

		graph::Surface *coverTex;
	};

	class Utils
	{
	public:

		static SceVoid ResetBgmMode();
	};

	class DualIo
	{
	public:

		SceAvPlayerFileReplacement fio;
		LocalFile *mio;
	};

	class GenericDecoder
	{
	public:

		class Metadata
		{
		public:

			SceBool hasMeta;
			SceBool hasCover;
			wstring title;
			wstring album;
			wstring artist;
		};

		GenericDecoder(const char *path, SceBool isSwDecoderUsed);

		virtual ~GenericDecoder();

		virtual SceUInt64 Seek(SceFloat32 percent);

		virtual SceVoid Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata);

		virtual SceUInt32 GetSampleRate();

		virtual SceUInt8 GetChannels();

		virtual SceUInt64 GetPosition();

		virtual SceUInt64 GetLength();

		virtual SceBool IsPaused();

		virtual SceVoid Pause();

		virtual SceVoid Stop();

		virtual Metadata *GetMetadataLocation();

		SceBool IsValid();

		SceBool isPlaying;
		SceBool isPaused;
		SceBool isValid;
		Metadata *metadata;
		string dataPath;
	};

	class FlacDecoder : public GenericDecoder
	{
	public:

		FlacDecoder(const char *path, SceBool isSwDecoderUsed);

		~FlacDecoder();

		static SceVoid MetadataCbEntry(ScePVoid pUserData, ScePVoid pMetadata);

		static SceSize drflacReadCB(ScePVoid pUserData, ScePVoid pBufferOut, SceSize bytesToRead);

		static SceUInt32 drflacSeekCB(ScePVoid pUserData, SceInt32 offset, SceInt32 origin);

		SceUInt64 Seek(SceFloat32 percent);

		SceVoid Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata);

		SceUInt32 GetSampleRate();

		SceUInt8 GetChannels();

		SceUInt64 GetPosition();

		SceUInt64 GetLength();

		DualIo io;

	private:

		SceNmHandle nmHandle;
		ScePVoid flac;
		drflac_uint64 framesRead;
	};

	class OpDecoder : public GenericDecoder
	{
	public:

		OpDecoder(const char *path, SceBool isSwDecoderUsed);

		~OpDecoder();

		SceUInt64 Seek(SceFloat32 percent);

		SceVoid Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata);

		SceUInt32 GetSampleRate();

		SceUInt8 GetChannels();

		SceUInt64 GetPosition();

		SceUInt64 GetLength();

		DualIo io;

	private:

		static SceInt32 OpRead(ScePVoid _stream, SceUChar8 *_ptr, SceInt32 _nbytes);

		static SceInt32 OpSeek(ScePVoid _stream, SceInt64 _offset, SceInt32 _whence);

		static SceInt64 OpTell(ScePVoid _stream);

		static SceInt32 OpClose(ScePVoid _stream);

		SceNmHandle nmHandle;
		ScePVoid opus;
		ogg_int64_t samplesRead;
		ogg_int64_t	maxSamples;
	};

	class WebmOpusDecoder : public GenericDecoder
	{
	public:

		WebmOpusDecoder(const char *path, SceBool isSwDecoderUsed);

		~WebmOpusDecoder();

		SceUInt64 Seek(SceFloat32 percent);

		SceVoid Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata);

		SceUInt32 GetSampleRate();

		SceUInt8 GetChannels();

		SceUInt64 GetPosition();

		SceUInt64 GetLength();

		SceAvPlayerFileReplacement fio;

	private:

		static SceInt32 NeRead(ScePVoid buffer, SceSize length, ScePVoid userdata);

		static SceInt32 NeSeek(SceInt64 offset, SceInt32 whence, ScePVoid userdata);

		static SceInt64 NeTell(ScePVoid userdata);

		nestegg *ne;
		OpusDecoder *opusDec;
		SceNmHandle nmHandle;
		SceUInt64 totalTime;
		SceUInt64 samplesRead;
		SceUInt64 seekTargetSamples;
		SceUInt32 sampleRate;
		SceInt32 maxSamples;
		SceUInt8 channelNum;
		SceBool isSeeking;
	};

	class OggDecoder : public GenericDecoder
	{
	public:

		OggDecoder(const char *path, SceBool isSwDecoderUsed);

		~OggDecoder();

		SceUInt64 Seek(SceFloat32 percent);

		SceVoid Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata);

		SceUInt32 GetSampleRate();

		SceUInt8 GetChannels();

		SceUInt64 GetPosition();

		SceUInt64 GetLength();

		SceAvPlayerFileReplacement fio;

	private:

		static SceUInt32 OggRead(ScePVoid ptr, SceUInt32 size, SceUInt32 nmemb, ScePVoid datasource);

		static SceInt32 OggSeek(ScePVoid datasource, ogg_int64_t offset, SceInt32 whence);

		static long OggTell(ScePVoid datasource);

		static SceInt32 OggClose(ScePVoid datasource);

		SceUInt64 FillBuffer(char *out);

		SceNmHandle nmHandle;
		OggVorbis_File ogg;
		vorbis_info *oggInfo;
		ogg_int64_t samplesRead;
		ogg_int64_t oldSamplesRead;
		ogg_int64_t	maxLength;
	};

	class XmDecoder : public GenericDecoder
	{
	public:

		XmDecoder(const char *path, SceBool isSwDecoderUsed);

		~XmDecoder();

		SceUInt64 Seek(SceFloat32 percent);

		SceVoid Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata);

		SceUInt32 GetSampleRate();

		SceUInt8 GetChannels();

		SceUInt64 GetPosition();

		SceUInt64 GetLength();

	private:

		SceUInt64 samplesRead;
		SceUInt64 totalSamples;
		xmp_context xmp;
		xmp_frame_info frame_info;
		xmp_module_info module_info;
	};

	class At3Decoder : public GenericDecoder
	{
	public:

		enum Type
		{
			Type_AT3,
			Type_AT3P
		};

		At3Decoder(const char *path, SceBool isSwDecoderUsed);

		~At3Decoder();

		SceUInt64 Seek(SceFloat32 percent);

		SceVoid Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata);

		SceUInt32 GetSampleRate();

		SceUInt8 GetChannels();

		SceUInt64 GetPosition();

		SceUInt64 GetLength();

		SceAvPlayerFileReplacement fio;

	private:

		static SceInt32 sceAudiocodecGetAt3ConfigPSP2(SceUInt32 cmode, SceUInt32 nbytes);

		SceVoid InitOMA(const char *path);

		SceVoid InitRIFF(const char *path);

		SceVoid InitCommon(const char *path, SceUInt8 type, SceUInt8 param1, SceUInt8 param2, SceSize dataOffset);

		SceNmHandle nmHandle;
		SceAudiocodecCtrl codecCtrl;
		ScePVoid esBuffer;
		SceSize dataBeginOffset;
		SceUInt64 totalEsSamples;
		SceUInt64 totalEsPlayed;
		SceUInt32 codecType;
	};

	class ShellCommonDecoder : public GenericDecoder
	{
	public:

		ShellCommonDecoder(const char *path, SceBool isSwDecoderUsed);

		~ShellCommonDecoder();

		SceUInt64 Seek(SceFloat32 percent);

		SceUInt32 GetSampleRate();

		SceUInt64 GetPosition();

		SceUInt64 GetLength();

	private:

		SceBool isSeeking;
		SceUInt32 timeRead;
		SceUInt32 totalTime;
		SceUInt32 seekFrame;
		SceMusicPlayerServicePlayStatusExtension pbStat;
	};

	class Mp3Decoder : public ShellCommonDecoder
	{
	public:

		Mp3Decoder(const char *path, SceBool isSwDecoderUsed);

	};

	class YoutubeDecoder : public GenericDecoder
	{
	public:

		YoutubeDecoder(const char *path, SceBool isSwDecoderUsed);

		~YoutubeDecoder();

		SceUInt64 Seek(SceFloat32 percent);

		SceVoid Decode(ScePVoid stream, SceUInt32 length, ScePVoid userdata);

		SceUInt32 GetSampleRate();

		SceUInt8 GetChannels();

		SceUInt64 GetPosition();

		SceUInt64 GetLength();

		SceAvPlayerFileReplacement fio;

	private:

		static SceInt32 NeRead(ScePVoid buffer, SceSize length, ScePVoid userdata);

		static SceInt32 NeSeek(SceInt64 offset, SceInt32 whence, ScePVoid userdata);

		static SceInt64 NeTell(ScePVoid userdata);

		nestegg *ne;
		OpusDecoder *opusDec;
		SceNmHandle nmHandle;
		SceUInt64 totalTime;
		SceUInt64 samplesRead;
		SceUInt64 seekTargetSamples;
		SceUInt32 sampleRate;
		SceInt32 maxSamples;
		SceUInt8 channelNum;
		SceBool isSeeking;
	};
}

#endif
