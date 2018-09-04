#include <stdio.h>
#include "MP4Encoder.h"
int main(int argc, char *argv[])
{
  if(argc < 3){
      printf("Usage: %s H264file MP4file.\n");
      return 1;
    }
  MP4Encoder mp4Encoder;
  mp4Encoder.WriteH264File(argv[1], argv[2]);

//  MP4Encoder mp4Encoder1;
//  mp4Encoder1.WriteH264File(argv[3], argv[4]);
  return 0;
}
