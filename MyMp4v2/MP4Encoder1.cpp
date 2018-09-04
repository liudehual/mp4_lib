// MP4Encoder.cpp
#pragma once

#include "MP4Encoder1.h"

#include "OS.h"


#define MIN_FRAME_SIZE 32
#define VIDEO_TIME_SCALE 90000
#define AUDIO_TIME_SCALE 8000
#define MOVIE_TIME_SCALE VIDEO_TIME_SCALE
#define PTS2TIME_SCALE(CurPTS, PrevPTS, timeScale) \
  ((MP4Duration)((CurPTS - PrevPTS) * 1.0 / (double)(1e+6) * timeScale))
#define INVALID_PTS 0xFFFFFFFFFFFFFFFF
/* Warning: Followings are magic data originally */
#define DEFAULT_VIDEO_TRACK_NUM 3
#define DEFAULT_VIDEO_PROFILE_LEVEL 1
#define DEFAULT_AUDIO_PROFILE_LEVEL 2

MP4Encoder::MP4Encoder(void)
  : m_hFile(MP4_INVALID_FILE_HANDLE)
  , m_bFirstVideo(true)
  , m_bFirstAudio(true)
  , m_uSecond(DEFAULT_RECORD_TIME)
  , m_videoTrack(MP4_INVALID_TRACK_ID)
  , m_audioTrack(MP4_INVALID_TRACK_ID)
  , m_u64VideoPTS(0)
  , m_u64AudioPTS(0)
  , m_u64FirstPTS(INVALID_PTS)
  , m_u64LastPTS(INVALID_PTS)
{
}


MP4Encoder::~MP4Encoder(void)
{
}

MP4EncoderResult MP4Encoder::MP4CreateFile(const char *sFileName,
                                           unsigned uRecordTime /* = DEFAULT_RECORD_TIME */)
{
  m_hFile = MP4Create(sFileName);
  if (m_hFile == MP4_INVALID_FILE_HANDLE)
    return MP4ENCODER_ERROR(MP4ENCODER_E_CREATE_FAIL);
  if (!MP4SetTimeScale(m_hFile, MOVIE_TIME_SCALE))
    return MP4ENCODER_ERROR(MP4ENCODER_E_CREATE_FAIL);
  m_uSecond = uRecordTime;
  return MP4ENCODER_ENONE;
}

MP4EncoderResult MP4Encoder::MP4AddH264Track(const uint8_t *sData, int nSize,
                                             int nWidth, int nHeight, int nFrameRate/* = 25 */)
{
  int sps, pps;
  for (sps = 0; sps < nSize;)
    if (sData[sps++] == 0x00 && sData[sps++] == 0x00 && sData[sps++] == 0x00
        && sData[sps++] == 0x01)
      break;
  for (pps = sps; pps < nSize;)
    if (sData[pps++] == 0x00 && sData[pps++] == 0x00 && sData[pps++] == 0x00
        && sData[pps++] == 0x01)
      break;
  if (sps >= nSize || pps >= nSize)
    return MP4ENCODER_ERROR(MP4ENCODER_E_ADD_VIDEO_TRACK);

  m_videoTrack = MP4AddH264VideoTrack(m_hFile, VIDEO_TIME_SCALE,
                                      VIDEO_TIME_SCALE / nFrameRate, nWidth, nHeight,
                                      sData[sps + 1], sData[sps + 2], sData[sps + 3], DEFAULT_VIDEO_TRACK_NUM);
  if (MP4_INVALID_TRACK_ID == m_videoTrack)
    return MP4ENCODER_ERROR(MP4ENCODER_E_ADD_VIDEO_TRACK);

  MP4SetVideoProfileLevel(m_hFile, DEFAULT_VIDEO_PROFILE_LEVEL);
  MP4AddH264SequenceParameterSet(m_hFile, m_videoTrack, sData + sps,
                                 pps - sps - 4);
  MP4AddH264PictureParameterSet(m_hFile, m_videoTrack, sData + pps,
                                nSize - pps);

  return MP4ENCODER_ENONE;
}

MP4EncoderResult MP4Encoder::MP4AddAACTrack(const uint8_t *sData, int nSize)
{
  m_audioTrack = MP4AddAudioTrack(m_hFile, AUDIO_TIME_SCALE,
                                  /**
                                                 * In fact, this is not a magic number. A formula might be:
                                                 * SampleRate * ChannelNum * 2 / SampleFormat
                                                 * 8000 * 1 * 2 / 16 (字节对齐，这里是 AV_SAMPLE_FMT_S16)
                                                 */
                                  AUDIO_TIME_SCALE / 8, MP4_MPEG4_AUDIO_TYPE);
  if (MP4_INVALID_TRACK_ID == m_audioTrack)
    return MP4ENCODER_ERROR(MP4ENCODER_E_ADD_AUDIO_TRACK);
  MP4SetAudioProfileLevel(m_hFile, DEFAULT_AUDIO_PROFILE_LEVEL);
  if (!MP4SetTrackESConfiguration(m_hFile, m_audioTrack, sData, nSize))
    return MP4ENCODER_ERROR(MP4ENCODER_E_ADD_AUDIO_TRACK);

  return MP4ENCODER_ENONE;
}

MP4EncoderResult MP4Encoder::MP4WriteH264Data(uint8_t *sData, int nSize, uint64_t u64PTS)
{
  if (nSize < MIN_FRAME_SIZE)
    return MP4ENCODER_ENONE;
  bool result = false;
  sData[0] = (nSize - 4) >> 24;
  sData[1] = (nSize - 4) >> 16;
  sData[2] = (nSize - 4) >> 8;
  sData[3] = nSize - 4;
  if (m_bFirstVideo)
    {
      if (m_u64FirstPTS > u64PTS)
        m_u64FirstPTS = u64PTS;
      m_u64VideoPTS = u64PTS;
      m_bFirstVideo = false;
    }
  if ((sData[4] & 0x0F) == 5)
    result = MP4WriteSample(m_hFile, m_videoTrack, sData, nSize,
                            PTS2TIME_SCALE(u64PTS, m_u64VideoPTS, VIDEO_TIME_SCALE));
  else
    result = MP4WriteSample(m_hFile, m_videoTrack, sData, nSize,
                            PTS2TIME_SCALE(u64PTS, m_u64VideoPTS, VIDEO_TIME_SCALE), 0, false);
  if (!result)
    return MP4ENCODER_ERROR(MP4ENCODER_E_WRITE_VIDEO_DATA);
  m_u64LastPTS = m_u64VideoPTS = u64PTS;
  if (m_uSecond && (m_u64LastPTS - m_u64FirstPTS) / (1e+6) >= m_uSecond)
    return MP4ENCODER_ERROR(MP4ENCODER_WARN_RECORD_OVER);
  return MP4ENCODER_ENONE;
}

MP4EncoderResult MP4Encoder::MP4WriteAACData(const uint8_t *sData, int nSize,
                                             uint64_t u64PTS)
{
  if (nSize < MIN_FRAME_SIZE)
    return MP4ENCODER_ENONE;
  bool result = false;
  if (m_bFirstAudio)
    {
      if (m_u64FirstPTS > u64PTS)
        m_u64FirstPTS = u64PTS;
      m_u64AudioPTS = u64PTS;
      m_bFirstAudio = false;
    }
  result = MP4WriteSample(m_hFile, m_audioTrack, sData, nSize,
                          PTS2TIME_SCALE(u64PTS, m_u64AudioPTS, AUDIO_TIME_SCALE));
  if (!result)
    return MP4ENCODER_ERROR(MP4ENCODER_E_WRITE_AUDIO_DATA);
  m_u64LastPTS = m_u64AudioPTS = u64PTS;
  if (m_uSecond && (m_u64LastPTS - m_u64FirstPTS) / (1e+6) >= m_uSecond)
    return MP4ENCODER_ERROR(MP4ENCODER_WARN_RECORD_OVER);
  return MP4ENCODER_ENONE;
}
int MP4Encoder::writeAACData(unsigned char* buf,int size)
{
  /// 传入的数据必须是一个完整的ADTS数据包(或整数个音频包(ADTS+AAC))
  /// ADTS格式的音频数据需要将ADTS头去掉，只使用纯AAC数据
  unsigned char *pos=buf;
  unsigned char *end=&buf[size];
  while(1){
      if((unsigned char)pos[0]==0xFF && ((unsigned char)pos[1] & 0xF0)==0xF0){ /// 检查ADTS头(0xFFF)

          /// 解析ADTS头，共7字节长度
          int id=pos[1]>>3 & 0x01;
          int layer=pos[1]>>1 & 0x03;
          int protection_absent=pos[1] & 0x01;
          int profile=pos[2]>>6 & 0x03;
          int sampling_frequency_index=pos[2]>>2 & 0x0F;
          int private_bit=pos[2]>>1 & 0x01;
          int channel_configuration=(pos[2] & 0x01)<<2 | pos[3]>>6 & 0x03;
          int original_copy=pos[3]>>5 & 0x01;
          int home=pos[3]>>4 & 0x01;
          int copyright_identification_bit=pos[3]>>3 & 0x01;
          int copyright_identification_start=pos[3]>>2 & 0x01;
          int aac_frame_length=(int)((pos[3] & 0x03)<<13 | pos[4]<<3 | (pos[5]>>5) & 0x07);
          int adts_buffer_fullness=((unsigned short)(pos[5] & 0x1F))<<6 | ((unsigned short)(pos[6]>>2) & 0x7F);
          unsigned char number_of_raw_data_blocks_in_frame=(pos[6] & 0x03);

          fprintf(stderr,"aac frame len %d "
                         "id %u "
                         "profile %u "
                         "sampling_frequency_index %u "
                         "channel_configuration %u "
                         "adts_buffer_fullness %u \n",
                  aac_frame_length,
                  id,
                  profile,
                  sampling_frequency_index,
                  channel_configuration,
                  adts_buffer_fullness);


          long long time=OS::Milliseconds();
          this->MP4WriteAACData(&pos[0],aac_frame_length,time);


          pos+=aac_frame_length;



          if(pos>=end){ /// 遍历完成或出错,跳出循环，终止查找
              break;
            }else{ /// 未完成，接着查找
              continue;
            }

        }else{ /// 不是标准格式，遍历查找
          pos++;
        }
    }

  return 1;
}
void MP4Encoder::MP4ReleaseFile()
{
  if (m_hFile != MP4_INVALID_FILE_HANDLE)
    {
      MP4Close(m_hFile);
      m_hFile = MP4_INVALID_FILE_HANDLE;
    }
}


