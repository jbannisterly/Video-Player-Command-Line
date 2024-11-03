#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavformat/avio.h>
#include <libavcodec/codec_par.h>
#include <windows.h>
#include <time.h>

void Parse(AVFormatContext *pFormatContext, int videoStream, int audioStream, int displayWidth, int displayHeight);
void GetFrameData(AVFrame *pFrame, uint8_t *result, uint8_t *Cb, uint8_t *Cr);
WORD ConvertColour(uint8_t cb, uint8_t cr, uint8_t y);

int main(int argc, char** argv){
  

  HWND consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO *pConsoleInfo = malloc(sizeof(CONSOLE_SCREEN_BUFFER_INFO));
  GetConsoleScreenBufferInfo(consoleHandle, pConsoleInfo);
  int displayWidth = pConsoleInfo->dwMaximumWindowSize.X;
  int displayHeight = pConsoleInfo->dwMaximumWindowSize.Y;
  DWORD mode;
  GetConsoleMode(consoleHandle, &mode);
  SetConsoleMode(consoleHandle, mode + 4);
  const char *filename = argv[1];
  AVFormatContext *pFormatContext = avformat_alloc_context();
  avformat_open_input(&pFormatContext, filename, NULL, NULL);
  avformat_find_stream_info(pFormatContext, NULL);

  int videoStreamIndex = -1;
  int audioStreamIndex = -1;
  
  for(int i = 0; i < pFormatContext->nb_streams; i++){
    switch(pFormatContext->streams[i]->codec->codec_type){
      case AVMEDIA_TYPE_VIDEO:
        videoStreamIndex = i;
        break;
      case AVMEDIA_TYPE_AUDIO:
        audioStreamIndex = i;
        break;
    }
    
  }

  Parse(pFormatContext, videoStreamIndex, audioStreamIndex, displayWidth, displayHeight);

  return 0;
}

void Parse(AVFormatContext *pFormatContext, int videoStream, int audioStream, int displayWidth, int displayHeight){
  AVCodecContext *pCodecContext = pFormatContext->streams[videoStream]->codec;
  int videoWidth = pCodecContext->width;
  int videoHeight = pCodecContext->height;
  AVCodec *pDecoder = avcodec_find_decoder(pCodecContext->codec_id);
  AVCodecContext *pCodecContext3 = avcodec_alloc_context3(pDecoder);
  avcodec_copy_context(pCodecContext3, pCodecContext);
  avcodec_open2(pCodecContext3, pDecoder, NULL);

  AVPacket *pPacket = av_packet_alloc();
  AVFrame *pFrame = av_frame_alloc();
  uint8_t *greyscale = malloc(videoWidth * videoHeight);
  uint8_t *Cr = malloc(videoWidth * videoHeight);
  uint8_t *Cb = malloc(videoWidth * videoHeight);

  HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
  COORD *pCursorPos = malloc(sizeof(COORD));
  pCursorPos->X = 0;
  pCursorPos->Y = 0;

  const char* colours = " `.-':_,^;><+!rc*/z?sLTv)J7(|Fi{C}fI31tlu[neoZ5Yxjya]2ESwqkP6h9d4VpOGbUAKXHm8RDBg0MNWQ";
  int colLen = strlen(colours);
  
  CHAR_INFO *charInfoData = malloc(sizeof(CHAR_INFO) * displayWidth * displayHeight);
  COORD location;
  location.X = 0;
  location.Y = 0;
  COORD displaySize;
  displaySize.X = displayWidth;
  displaySize.Y = displayHeight;
  SMALL_RECT displayTarget;
  displayTarget.Top = 0;
  displayTarget.Left = 0;
  displayTarget.Right = displayWidth - 1;
  displayTarget.Bottom = displayHeight - 1;

  clock_t time = clock();
  int dataIndex;

  char *charData = malloc(sizeof(char) * displayWidth * displayHeight); 
  WORD *charAttribute = malloc(sizeof(WORD) * displayHeight * displayWidth);
  LPDWORD written = malloc(sizeof(DWORD));
  
  for (int ii = 0; ii < displayWidth * displayHeight; ii++){
    charAttribute[ii] = 15;
  }
 
   CHAR_INFO infoTest;
   infoTest.Attributes = 1;

  while(av_read_frame(pFormatContext, pPacket) >= 0){
    if (pPacket->stream_index == videoStream){
      int sendCode = avcodec_send_packet(pCodecContext3, pPacket);
      int receiveCode = 0;
      
      while(receiveCode >= 0){
        WORD previousColour = 0;
        int colourIndex;
        receiveCode = avcodec_receive_frame(pCodecContext3, pFrame);
        if(receiveCode >= 0){
          GetFrameData(pFrame, greyscale, Cb, Cr);
          for (int yy = 0; yy < displayHeight; yy++){
            for(int xx = 0; xx < displayWidth; xx++){
              int colour = greyscale[(yy * videoHeight / displayHeight) * videoWidth + xx * videoWidth / displayWidth];
              dataIndex = (yy * displayWidth + xx) * 1;
              charData[dataIndex] = colours[(int)((colour / 255.0 * colour / 255.0) * colLen)];
              colourIndex = (yy * videoHeight / displayHeight / 2) * videoWidth / 2 + xx * videoWidth / displayWidth / 2;
              charAttribute[dataIndex] = ConvertColour(Cb[colourIndex], Cr[colourIndex], colour);
              if (charData[dataIndex] == ' ') charAttribute[dataIndex] = previousColour;
              previousColour = charAttribute[dataIndex];
            }
          }
          while(clock() < time + 40);
          time = clock();          
          WriteConsoleOutputCharacterA(outputHandle, charData, displayWidth * displayHeight, location, written);
          WriteConsoleOutputAttribute(outputHandle, charAttribute, displayWidth * displayHeight, location, written);
        }
      }
    }
    av_free_packet(pPacket);
  }
}

void GetFrameData(AVFrame *pFrame, uint8_t *result, uint8_t *Cr, uint8_t *Cb){    
  int frameNumber = pFrame->coded_picture_number;
  int linesize = pFrame->linesize[0];
  int width = pFrame->width;
  int height = pFrame->height;
  uint8_t *target;
  uint8_t *targets[3] = {result, Cr, Cb};
  for(int plane = 0; plane < 3; plane++){
    target = targets[plane];
    int planeLine = pFrame->linesize[plane];
    int planeWidth = width * planeLine / linesize;
    int planeHeight = height * planeLine / linesize;
    int ii;
    for(ii = 0; ii < planeHeight; ii++){
      memcpy(target + ii * planeWidth, pFrame->data[plane] + ii * planeLine, planeWidth);
    }
  }
}

WORD ConvertColour(uint8_t cb, uint8_t cr, uint8_t y){
  WORD colour = 0;
  if (cr < 150 && cb > 100) colour += 1;
  if (cr < 170 && cb < 130 && cr + cb < 290) colour += 2;
  if (2 * cr - 1.4 * cb > 50 && 2 * cr + 1.5 * cb > 320) colour += 4;
  if (y > 100) colour += 8;
  return colour;
}