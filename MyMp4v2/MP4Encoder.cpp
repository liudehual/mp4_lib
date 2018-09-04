#include "MP4Encoder.h"

#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN32_WCE)
#include<winsock2.h>
#pragma comment(lib,"ws2_32.lib")//这句关键;
#else
#endif

MP4Encoder::MP4Encoder():
    fMp4FileHandler(NULL),
    fVideoId(MP4_INVALID_TRACK_ID),
    fAudioId(MP4_INVALID_TRACK_ID)
{

}

MP4Encoder::~MP4Encoder()
{
    /// 一定要关闭,不关闭会写入失败
    closeMP4File();
}
bool MP4Encoder::CreateMP4File(const char *fileName,int width,int height,int timeScale ,int frameRate)
{
    if(fileName == NULL){
        return false;
    }

    /// 创建mp4文件，并获取句柄
    fMp4FileHandler= MP4Create(fileName,0);
    if (fMp4FileHandler == MP4_INVALID_FILE_HANDLE){
        fprintf(stderr,"ERROR:Open file failed.\n");
        return false;
    }

    /// 配置时间尺度
    fWidth = width;
    fHeight = height;
    fTimeScale = timeScale;
    fFrameRate = frameRate;
    if(!MP4SetTimeScale(fMp4FileHandler, fTimeScale)){
        return false;
    }
    return true;
}
bool MP4Encoder::write264Sps(char *buf,int size)
{
    /// 添加视频轨道
    fVideoId = MP4AddH264VideoTrack
            (fMp4FileHandler,
             fTimeScale,
             fTimeScale / fFrameRate,
             fWidth,    /// width
             fHeight,   /// height
             buf[1],    /// sps[1] AVCProfileIndication
            buf[2],    /// sps[2] profile_compat
            buf[3],    /// sps[3] AVCLevelIndication
            3);        /// 4 bytes length before each NAL unit
    if (fVideoId == MP4_INVALID_TRACK_ID){
        fprintf(stderr,"add video track failed.\n");
        return false;
    }
    MP4SetVideoProfileLevel(fMp4FileHandler, 0x01); //  Simple Profile @ Level 3

    /// 配置sps
    MP4AddH264SequenceParameterSet(fMp4FileHandler,fVideoId,(const unsigned char *)buf,size);
    return true;
}
bool MP4Encoder::write264Pps(char *buf,int size)
{
    /// 配置pps
    MP4AddH264PictureParameterSet(fMp4FileHandler,fVideoId,(const unsigned char *)buf,size);
    return true;
}
int MP4Encoder::writeH264Data(unsigned char* buf,int size)
{
    /// 计算视频帧类型
    unsigned char type=buf[4] & 0x0F;

    /// 计算nalu数据长度
    int naluSize=size-4;

    /// 用nalu长度替换00 00 00 01 头
    buf[0] = naluSize >> 24;
    buf[1] = naluSize >> 16;
    buf[2] = naluSize >> 8;
    buf[3] = naluSize & 0xff;

    if(type==0x05){ /// 关键帧
        if(!MP4WriteSample(fMp4FileHandler, fVideoId, buf, size,MP4_INVALID_DURATION)){
            return 0;
        }
    }else{ /// 非关键帧 sps/pps/b/p帧
        if(!MP4WriteSample(fMp4FileHandler, fVideoId, buf, size,MP4_INVALID_DURATION,0,0)){
            return 0;
        }
    }
    return 1;
}

int MP4Encoder::writeAACTrack(unsigned char *buf,int size)
{
    /// 添加 AAC track
    fAudioId = MP4AddAudioTrack(fMp4FileHandler,
                                44100,/// aac 采样率
                                /*AUDIO_TIME_SCALE / 8*/1024, /// aac 每秒采集1024次
                                MP4_MPEG4_AUDIO_TYPE /// 音频格式
                                );

    if (fAudioId == MP4_INVALID_TRACK_ID){
        fprintf(stderr,"Add audio track failed!\n");
        return 0;
    }

    /// 该值为音频的编码方式、采样率
    /// AAC(2字节)
    /// 编码方式: LOW(LC)
    /// 采样率：44100 hz
    /// 声道数: Stereo
    /// 第一栏：AAC Object Type
    /// 第二栏：Sample Rate Index
    /// 第三栏：Channel Number
    /// 第四栏：Don't care，設 0
    ///
    /// 第一栏：00010(5位)
    /// 第二栏：0100(4位)
    /// 第三栏：0010(4位)
    /// 第四栏：000(3位，默认为0)
    /// 合起來： 00010010 00010000 ＝＞ 0x12 0x10
    /// @see:https://blog.csdn.net/xiaota00/article/details/76445501

    ///unsigned char aac_conf_buf[2]={0x12,0x10}; /// LOW(LC)/44100/立体声

    /// 设置音频属性
    MP4SetAudioProfileLevel(fMp4FileHandler, 0x01);

    /// 写入音频配置信息
    bool theInitState=MP4SetTrackESConfiguration(fMp4FileHandler, fAudioId,buf, size);
    if(!theInitState){
        fprintf(stderr,"ES configuration error \n");
    }
    return 1;
}
int MP4Encoder::writeAACData(unsigned char* buf,int size)
{
    if(size<ADTS_HEADER_LEN){
        return 0;
    }
    /// 传入的数据必须是一个完整的ADTS数据包(或整数个音频包(ADTS+AAC))
    /// ADTS格式的音频数据需要将ADTS头去掉，只使用纯AAC数据
    unsigned char *pos=buf;
    unsigned char *end=&buf[size];
    while(1){
        if((unsigned char)pos[0]==0xFF && ((unsigned char)pos[1] & 0xF0)==0xF0){ /// 检查ADTS头(0xFFF)

            /// 解析ADTS头，共7字节长度
            //            int id=pos[1]>>3 & 0x01;
            //            int layer=pos[1]>>1 & 0x03;
            //            int protection_absent=pos[1] & 0x01;
            //            int profile=pos[2]>>6 & 0x03;
            //            int sampling_frequency_index=pos[2]>>2 & 0x0F;
            //            int private_bit=pos[2]>>1 & 0x01;
            //            int channel_configuration=(pos[2] & 0x01)<<2 | pos[3]>>6 & 0x03;
            //            int original_copy=pos[3]>>5 & 0x01;
            //            int home=pos[3]>>4 & 0x01;
            //            int copyright_identification_bit=pos[3]>>3 & 0x01;
            //            int copyright_identification_start=pos[3]>>2 & 0x01;
            int aac_frame_length=(int)((pos[3] & 0x03)<<13 | pos[4]<<3 | (pos[5]>>5) & 0x07);
            //            int adts_buffer_fullness=((unsigned short)(pos[5] & 0x1F))<<6 | ((unsigned short)(pos[6]>>2) & 0x7F);
            //            unsigned char number_of_raw_data_blocks_in_frame=(pos[6] & 0x03);

            //  fprintf(stderr,"aac frame len %d "
            //                 "audio object type %d "
            //                 "sampling_frequency_index %d "
            //                 "channel numbers %d \n",
            //                 aac_frame_length,
            //                 profile+1,
            //                 sampling_frequency_index,
            //                 channel_configuration);

            /// 写入数据时，需要去掉ADTS头
            if(!MP4WriteSample(fMp4FileHandler,
                               fAudioId,
                               &pos[ADTS_HEADER_LEN],
                               aac_frame_length-ADTS_HEADER_LEN,
                               MP4_INVALID_DURATION,
                               0,
                               1)){
                return 0;
            }


            pos+=aac_frame_length;

            if(pos>=end){ /// 遍历完成或出错,跳出循环，终止查找
                break;
            }else{ /// 未完成，接着查找
                continue;
            }

        }else{ /// 不是标准格式，遍历查找
            fprintf(stderr,"[%s] [%d] \n",__FUNCTION__,__LINE__);
            pos++;
        }
    }

    return 1;
}

int MP4Encoder::getAACConfig(unsigned short &config,unsigned char *buf,int size)
{
    unsigned short conf_buf=0;

    int audio_object_type= (buf[2]>>6 & 0x03)+1;
    int sampling_frequency_index=buf[2]>>2 & 0x0F;
    int channel_configuration=(buf[2] & 0x01)<<2 | buf[3]>>6 & 0x03;

    conf_buf=conf_buf | (audio_object_type & 0x0000001F)<<11;
    conf_buf=conf_buf | (sampling_frequency_index & 0x0000000F)<<7;
    conf_buf=conf_buf | (channel_configuration & 0x0000000F)<<3;

    config=htons(conf_buf);
    return 1;
}
int MP4Encoder::closeMP4File()
{
    /// 数据转换完成后，必须关闭，mp4v2库会写入一些必要信息
    if(fMp4FileHandler){
        MP4Close(fMp4FileHandler);
        fMp4FileHandler = NULL;
    }
    return 1;
}
