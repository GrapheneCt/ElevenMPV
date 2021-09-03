#include <kernel.h>
#include <appmgr.h>
#include <shellaudio.h>
#include <stdlib.h>

#include "audio.h"
#include "config.h"
#include "utils.h"
#include "vitaaudiolib.h"
#include "menu_audioplayer.h"

audio::ShellCommonDecoder::ShellCommonDecoder(const char *path, SceBool isSwDecoderUsed) : GenericDecoder::GenericDecoder(path, isSwDecoderUsed)
{
	isSeeking = SCE_FALSE;
	timeRead = 0;
	totalTime = 0;
	seekFrame = 0;

	SceInt32 error = 0;
	SceUInt32 timeoutIter = 0;

	sce_paf_memset(&pbStat, 0, sizeof(SceMusicPlayerServicePlayStatusExtension));

	error = sceMusicPlayerServiceInitialize(0);
	if (error < 0)
		return;

	error = sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_STOP, 0);
	if (error < 0)
		return;

	error = sceMusicPlayerServiceOpen(path, NULL);
	if (error < 0)
		return;

	sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_PLAY, 0);

	//Wait until SceShell is ready
	pbStat.currentState = SCE_MUSIC_EVENTID_STOP;
	while (pbStat.currentState == SCE_MUSIC_EVENTID_STOP && timeoutIter < 1000) {
		timeoutIter++;
		sceMusicPlayerServiceGetPlayStatusExtension(&pbStat);
		thread::Thread::Sleep(10);
	}

	SceMusicPlayerServiceTrackInfo data;
	sceMusicPlayerServiceGetTrackInfo(&data);
	totalTime = data.duration;
}

SceUInt32 audio::ShellCommonDecoder::GetSampleRate()
{
	return 1000;
}

SceUInt64 audio::ShellCommonDecoder::GetPosition()
{
	sceMusicPlayerServiceGetPlayStatusExtension(&pbStat);

	if (!isSeeking)
		timeRead = pbStat.currentTime;
	else {
		if (seekFrame != pbStat.currentTime)
			timeRead = seekFrame;
		else {
			timeRead = pbStat.currentTime;
			isSeeking = SCE_FALSE;
		}
	}

	if (pbStat.currentState == SCE_MUSIC_EVENTID_STOP && pbStat.currentTime == 0)
		isPlaying = SCE_FALSE;
	//This is to prevent audio from stopping when power button is pressed
	if (pbStat.currentState == SCE_MUSIC_EVENTID_STOP && isPlaying && !isPaused)
		sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_PLAY, 0);

	return timeRead;
}

SceUInt64 audio::ShellCommonDecoder::GetLength()
{
	if (totalTime == 0) {
		SceMusicPlayerServiceTrackInfo data;
		sceMusicPlayerServiceGetTrackInfo(&data);
		totalTime = data.duration;
	}

	return totalTime;
}

SceUInt64 audio::ShellCommonDecoder::Seek(SceFloat32 percent)
{
	isSeeking = SCE_TRUE;
	seekFrame = (SceUInt32)((SceFloat32)totalTime * percent / 100.0f);

	sceMusicPlayerServiceSetSeekTime(seekFrame);
	sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_SEEK, 0);

	return -1;
}

audio::ShellCommonDecoder::~ShellCommonDecoder()
{
	isPlaying = SCE_TRUE;
	isPaused = SCE_FALSE;

	sceMusicPlayerServiceSendEvent(SCE_MUSIC_EVENTID_STOP, 0);
	sceMusicPlayerServiceTerminate();

	audio::DecoderCore::EndPre();
	thread::Thread::Sleep(100);
	audio::DecoderCore::End();
}