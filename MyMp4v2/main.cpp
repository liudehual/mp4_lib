#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "MP4Encoder.h"
#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN32_WCE)
#include<winsock2.h>
#pragma comment(lib,"ws2_32.lib")//这句关键;
#else
#endif
long long filesize(FILE *fp)
{
    fseek(fp, SEEK_SET, 0);
    fseek(fp, 0L, SEEK_END);
    int length=ftell(fp);
    fseek(fp, SEEK_SET, 0);
    return length;
}
int main(int argc,char *argv[])
{

#if 0
    /// 视频封装测试
    FILE *f_video=fopen("test.h264","rb");
    if(!f_video){
        return 0;
    }
    int size=filesize(f_video);
    char *buf=new char[size];

#endif

#if 1
    /// 音频封装测试 ok
    FILE *f_audio=NULL;
    f_audio=fopen("test.aac","rb");
    if(!f_audio){
        return 0;
    }
    int size=filesize(f_audio);
    char *buf=new char[size];

    int nread;

    if ((nread = fread(buf,size,sizeof(char),f_audio)) < 0) {
        return 0;
    }

    MP4Encoder encoder;
    int theState=encoder.CreateMP4File("test2.mp4",320,240);
    if(!theState){
        fprintf(stderr,"create mp4 file error \n");
        return 0;
    }

    unsigned short conf_buf=0;

    encoder.getAACConfig(conf_buf,(unsigned char *)buf);

    theState=encoder.writeAACTrack((unsigned char *)&conf_buf,2);
    if(!theState){
        fprintf(stderr,"write aac track error\n");
        return 0;
    }
    encoder.writeAACData((unsigned char *)buf,size);

#endif

    getchar();

    return 1;
}
