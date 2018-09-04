#ifndef MP4ENCODER_H
#define MP4ENCODER_H
#include "mp4v2\mp4v2.h"


#define AUDIO_TIME_SCALE 44100
#define VIDEO_TIME_SCALE 90000
#define ADTS_HEADER_LEN 0x07
class MP4Encoder
{
public:
  MP4Encoder();
  ~MP4Encoder();
  bool CreateMP4File(const char *fileName,int width,int height,int timeScale = 90000,int frameRate = 25);

  /// video
  bool write264Sps(char *buf,int size);
  bool write264Pps(char *buf,int size);
  int writeH264Data(unsigned char* buf,int size);

  /// audio
  int writeAACTrack(unsigned char *buf,int size);
  int writeAACData(unsigned char* buf,int size);
  int getAACConfig(unsigned short &config,unsigned char *buf,int size=7);

  int closeMP4File();
private:
  MP4FileHandle fMp4FileHandler;

  int fWidth;
  int fHeight;
  int fFrameRate;
  int fTimeScale;

  MP4TrackId fVideoId;
  MP4TrackId fAudioId;
};

#endif // MP4ENCODER_H
