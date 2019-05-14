/**
 * @file
 * video decoding with libavcodec API example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdbool.h>

//����
#include "libavcodec/avcodec.h"
//��װ��ʽ����
#include "libavformat/avformat.h"
//frame��ʽת��
#include "libswscale/swscale.h"



int main(int argc, char **argv)
{
    const char *filename, *outfilename;

    if (argc <= 2) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(0);
    }
    filename    = argv[1];
    outfilename = argv[2];

    //1.ע���������
    av_register_all();

    //��װ��ʽ�����ģ�ͳ��ȫ�ֵĽṹ�壬������Ƶ�ļ���װ��ʽ�����Ϣ
    AVFormatContext *pFormatCtx = avformat_alloc_context();

    //2.��������Ƶ�ļ�
    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
    {
        printf("open %s failed\n",filename);
        return 1;
    }

    //3.��ȡ��Ƶ�ļ���Ϣ
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        printf("avformat_find_stream_info fail\n");
        return 1;
    }

    //��ȡ��Ƶ��������λ��
    //�����������͵���(��Ƶ������Ƶ������Ļ��)���ҵ���Ƶ��
    int v_stream_idx = -1;
    int i = 0;

    //number of streams
    for(; i < pFormatCtx->nb_streams; i++)
    {
        //������
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            v_stream_idx = i;
            break;
        }
    }

    if(v_stream_idx == -1)
    {
        printf("can't find video stream!\n");
        return 1;
    }

    //ֻ��֪����Ƶ�ı��뷽ʽ�����ܹ����ݱ��뷽ʽȥ�ҵ�������
    //��ȡ��Ƶ���еı����������
    AVCodecContext *pCodecCtx = pFormatCtx->streams[v_stream_idx]->codec;

    //4.���ݱ�����������еı���id���Ҷ�Ӧ�Ľ���
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec == NULL)
    {
        printf("can't find codec: %d\n",pCodecCtx->codec_id);
        return 1;
    }

    //5.�򿪽�����
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        printf("avcodec_open2 fail\n");
        return 1;
    }

    //�����Ƶ��Ϣ
    printf("video format: %s\n",pFormatCtx->iformat->name);
    printf("video duration: %d\n",(pFormatCtx->duration)/1000000);
    printf("video width: %d,height: %d\n",pCodecCtx->width,pCodecCtx->height);
    printf("video decode name: %s\n",pCodec->name);

    //׼����ȡ
    //AVPacket���ڴ洢һ֡һ֡�ı�������(H264)
    AVPacket *packet = (AVPacket*)av_malloc(sizeof(AVPacket));

    //AVFrame���ڴ洢��������������(YUV)
    //ֻ�����˽ṹ��ռ䣬��û�з���ʵ�ʵ����ݻ�������
    AVFrame *pFrame = av_frame_alloc();

    AVFrame *pFrameYUV = av_frame_alloc();

    //ֻ��ָ����AVFrame�����ظ�ʽ�������С�������������ڴ�
    //��ʼ�������仺�������ڴ�
    uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));

    //��ʼ��������
    avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);

    //����ת��(����)�Ĳ�����ת֮ǰ�Ŀ�ߣ�ת֮��Ŀ�ߣ���ʽ��
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    int got_picture, ret;

    FILE *fp_yuv = fopen(outfilename, "wb+");

    int frame_count = 0;

    //6.һ֡һ֡�Ķ�ȡѹ������
    while(av_read_frame(pFormatCtx, packet) >= 0)
    {
        //ֻҪ��Ƶѹ������(������������λ���ж�)
        if(packet->stream_index == v_stream_idx)
        {
            //7.����һ֡��Ƶѹ������
            //ret = avcodec_decode_video2(pFormatCtx, pFrame, &got_picture, packet);

            ret = avcodec_send_packet(pCodecCtx, packet);
            if(ret < 0)
            {
                printf("avcodec_send_packet fail!\n");
                return 1;
            }

            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                return;
            else if (ret < 0)
            {
                fprintf(stderr, "Error during decoding\n");
                exit(1);
            }

            //printf("saving frame %3d\n", pCodecCtx->frame_number);
            fflush(stdout);
        
            //if(got_picture)
            {
                //AVFrameתΪ���ظ�ʽYUV420
                //����2,6Ϊ���룬�������
                //����3,7Ϊ���룬�������һ�е����ݴ�С
                //����4Ϊ�������ݵ�һ��Ҫת���λ�ô�0��ʼ
                //����5Ϊ���뻭��ĸ߶�
                sws_scale(sws_ctx, pFrame, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV, pFrameYUV->linesize);

                //���浽YUV�ļ�
                int y_size = pCodecCtx->width * pCodecCtx->height;

                fwrite(pFrameYUV->data[0],1,y_size,fp_yuv);
                fwrite(pFrameYUV->data[1],1,y_size/4,fp_yuv);
                fwrite(pFrameYUV->data[2],1,y_size/4,fp_yuv);
                frame_count++;
                //printf("Decoded %d frames\n",frame_count);
            }
        }
        
        av_free_packet(packet);
    }

    fclose(fp_yuv);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_free_context(pFormatCtx);

    return 0;

}
