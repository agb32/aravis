/*
darc, the Durham Adaptive optics Real-time Controller.
Copyright (C) 2010 Alastair Basden.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
   The code here is used to create a shared object library, which can then be swapped around depending on which cameras you have in use, ie you simple rename the camera file you want to camera.so (or better, change the soft link), and restart the coremain.

The library is written for a specific camera configuration - ie in multiple camera situations, the library is written to handle multiple cameras, not a single camera many times.

This one handles SL240 and (some) gigE cameras simultaneously.

*/
//#define RESYNC
#ifndef NOSL240
#include <nslapi.h>
#else
typedef unsigned int uint32;
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
//#include <unistd.h>
#include <pthread.h>
#include "darc.h"
#include "rtccamera.h"
#include "qsort.h"
#include "buffer.h"

#include <arv.h>


#define HDRSIZE 8 //the size of a WPU header - 4 bytes for frame no, 4 bytes for something else.

//we use 4 buffers (instead of double buffering).
#define NBUF 4
#define BUFMASK 0x3
/**
   The struct to hold info.
   If using multi cameras (ie multi SL240 cards or streams), would need to recode, so have multiple instances of this struct.
*/

typedef struct{
  ArvBuffer **buf;
  struct timeval tstart;
  struct timeval tend;
} arvbuf;


typedef struct{
  int ncam;//no of cameras
  int npxls;//number of pixels in the frame (total for all cameras)
  int *transferframe;//the frame currently being passed into the RTC (piecewise)
  //volatile int last;//the last frame past to the RTC
  int *latestframe;//the latest whole frame arrived
  int *curframe;//the current frame

  //  char **curfillingbuf;//current buffer being filled from the sl240 card.  One per camera.
  //char **latestfilledbuf;//latest whole frame arrived from sl240 card. 1 per cam.
  //char **readingbuf;//the buffer currently being passed to rest of darc. 1 per cam.



  volatile int *pxlcnt;//number of pixels received for this frame and buffer
  //int pxlsRequested;//number of pixels requested by DMA for this frame
#ifdef RESYNC
  pthread_mutex_t m;
  pthread_cond_t thrcond;
#endif
  pthread_mutex_t *camMutex;
  pthread_cond_t *camCond;//sync between main RTC
  pthread_cond_t *camCond2;//sync between main RTC
  int *blocksize;//number of pixels to transfer per DMA;
  int **DMAbuf;//a buffer for receiving the DMA - 4*(sizeof(short)*npxls+HDRSIZE).  If a DMA requires a specific word alignment, we may need to reconsider this...
  int ncamSL240;
  int open;//set by RTC if the camera is open
  volatile int *waiting;//set by RTC if the RTC is waiting for pixels
  volatile int newframeAll;//set by RTC when a new frame is starting to be requested.
  volatile int *newframe;
  //int transferRequired;
  //int frameno;
  unsigned int thisiter;
  unsigned short *imgdata;
  int *pxlsTransferred;//number of pixels copied into the RTC memory.
  pthread_t *threadid;
#ifndef NOSL240
  nslDeviceInfo info;
  nslHandle *handle;
#endif
  uint32 *timeout;//in ms
  int *fibrePort;//the port number on sl240 card.
  unsigned int *userFrameNo;//pointer to the RTC frame number... to be updated for new frame.
  int *setFrameNo;//tells thread to set the userFrameNo.
  void *thrStruct;//pointer to an array of threadStructs.
  int *npxlsArr;
  int *npxlsArrCum;
  int thrcnt;
  int *sl240Opened;//which cameras have been opened okay.
  int *err;
  int *threadPriority;
  unsigned int *threadAffinity;
  int threadAffinElSize;
  int *reorder;//is pixel reordering required, and if so, which pattern?
  int **reorderBuf;//pixels for reordering.
  int *reorderIndx;
  int *reorderno;
  int nReorders;
  char *paramNames;
  int *index;
  int *nbytes;
  void **values;
  char *dtype;
  int *ntoread;//number of frames to read this iteration - usually 1, unless frame numbers are different (ie cameras have become out of sync).
  //int resync;//if set, will attempt to resynchronise cameras that have different frame numbers, by reading more frames from this one (number of extra frames is equal to the value of resync
  //int wpuCorrection;//whether to apply the correction if a camera is missing frames occasionally.
  int *readStarted;
#ifdef RESYNC
  int *readHasStarted;
  int *ncurrentlyReading;//number of frames for aravis cam to ignore.
  int doneSyncCheck;
#endif
  int *gotsyncdv;//flag to whether syncdv has already been received while reading a truncated frame.
  int skipFrameAfterBad;//flag - whether to skip a frame after a bad frame.
  int *testLastPixel;//value for each camera - if nonzero, and one of the last this many pixels pixel are non-zero, flags as a bad frame.  Assumes that at least one subap will require all ccd pixels to be read (set in the config file - though this may increase latency, if not all pixels required).
  int pxlRowStartSkipThreshold;//If a pixel at the start of a row falls below this threshold, then this pixel is discarded - meaning that all future pixels are shifted one to the left.  Any required padding will take this value.
  int pxlRowEndInsertThreshold;//If a pixel at the end of a row falls below this threshold, then an extra pixel is inserted here - meaning that all future pixels are shifted one to the right.
  int *pxlShift;//the total shift of pixels (-1 for pixel removed, +1 for pixel inserted).
  int *pxlx;
  int *pxly;
  int *campxlx;//for aravis - if camera image format different from actual format.
  int *campxly;
  circBuf *rtcErrorBuf;
  int *frameReady;
  int *offsetX;
  int *offsetY;
  int *byteswapInts;//for iport pleora
  struct timeval *timestamp;
  int recordTimestamp;//option to use timestamp of last pixel arriving rather than frame number.
  int *camErr;

  int auto_socket_buffer;
  int socket_buffer_size;
  int no_packet_resend;
  unsigned int packet_timeout;
  unsigned int frame_retention;
  int *bpp;
  int bytespp;
  ArvBuffer **mostRecentFilled;//a completed buffer for each camera
  ArvBuffer **currentFilling;//the buffer that is currently being written to with new camera data
  ArvBuffer **rtcReading;//the buffer that is being used to transfer data to darc
  ArvStream **stream;
  ArvCamera **camera;
  char **camNameList;
  char *nameList;
  ArvBuffer **bufArrList;
  char **prevCmd;


}CamStruct;

typedef struct{
  CamStruct *camstr;
  int camNo;
}ThreadStruct;


void safefree(void *ptr){
  if(ptr!=NULL)
    free(ptr);
}

void dofree(CamStruct *camstr){
  int i;
  printf("dofree called\n");
  if(camstr!=NULL){
    if(camstr->DMAbuf!=NULL){
      for(i=0; i<camstr->ncam-camstr->ncamSL240; i++){
	if(camstr->DMAbuf[i]!=NULL)
	  free(camstr->DMAbuf[i]);
      }
      free(camstr->DMAbuf);
    }
    for(i=0; i<camstr->ncam; i++){
      pthread_cond_destroy(&camstr->camCond[i]);
      pthread_cond_destroy(&camstr->camCond2[i]);
    }
#ifdef RESYNC
    pthread_mutex_destroy(&camstr->m);
    pthread_cond_destroy(&camstr->thrcond);
#endif
#ifndef NOSL240
    if(camstr->sl240Opened!=NULL){
      if(camstr->handle!=NULL){
	for(i=0; i<camstr->ncamSL240; i++){
	  if(camstr->sl240Opened[i])
	    nslClose(&camstr->handle[i]);
	}
	free(camstr->handle);
	camstr->handle=NULL;
      }
      safefree(camstr->sl240Opened);
    }
    safefree(camstr->handle);//incase its not already freed.
#endif
    safefree(camstr->npxlsArr);
    safefree(camstr->npxlsArrCum);
    safefree(camstr->blocksize);
    safefree((void*)camstr->pxlcnt);
    safefree(camstr->ntoread);
    safefree(camstr->readStarted);
#ifdef RESYNC
    safefree(camstr->readHasStarted);
    safefree(camstr->ncurrentlyReading);
#endif
    safefree((void*)camstr->waiting);
    safefree((void*)camstr->newframe);
    safefree(camstr->camErr);
    safefree(camstr->pxlsTransferred);
    safefree(camstr->setFrameNo);
    safefree(camstr->timeout);
    safefree(camstr->fibrePort);
    safefree(camstr->thrStruct);
    safefree(camstr->threadid);
    safefree(camstr->err);
    safefree(camstr->threadPriority);
    safefree(camstr->threadAffinity);
    safefree(camstr->reorder);
    safefree(camstr->reorderBuf);
    safefree(camstr->reorderno);
    safefree(camstr->reorderIndx);
    safefree(camstr->testLastPixel);
    safefree(camstr->index);
    safefree(camstr->paramNames);
    safefree(camstr->nbytes);
    safefree(camstr->dtype);
    safefree(camstr->values);
    safefree(camstr->gotsyncdv);
    safefree(camstr->pxlShift);
    safefree(camstr->latestframe);
    safefree(camstr->curframe);
    safefree(camstr->transferframe);

    safefree(camstr->pxlx);
    safefree(camstr->pxly);
    safefree(camstr->campxlx);
    safefree(camstr->campxly);

    safefree(camstr->timestamp);
    safefree(camstr->offsetX);
    safefree(camstr->offsetY);
    safefree(camstr->byteswapInts);
    safefree(camstr->mostRecentFilled);
    safefree(camstr->currentFilling);
    safefree(camstr->rtcReading);
    safefree(camstr->stream);
    safefree(camstr->camera);
    safefree(camstr->camNameList);
    safefree(camstr->bpp);
    safefree(camstr->frameReady);
    safefree(camstr->bufArrList);
    for(i=0;i<camstr->ncam+3;i++)
      safefree(camstr->prevCmd[i]);
    safefree(camstr->prevCmd);
    safefree(camstr->nameList);

    free(camstr);
  }
}


int camSetThreadAffinityAndPriority(unsigned int *threadAffinity,int threadPriority,int threadAffinElSize){
  int i;
  cpu_set_t mask;
  int ncpu;
  struct sched_param param;
  printf("Getting CPUs\n");
  ncpu= sysconf(_SC_NPROCESSORS_ONLN);
  printf("Got %d CPUs\n",ncpu);
  CPU_ZERO(&mask);
  printf("Setting %d CPUs\n",ncpu);
  for(i=0; i<ncpu && i<threadAffinElSize*32; i++){
    if(((threadAffinity[i/32])>>(i%32))&1){
      CPU_SET(i,&mask);
    }
  }
  if(sched_setaffinity(0,sizeof(cpu_set_t),&mask))
    printf("Error in sched_setaffinity: %s\n",strerror(errno));
  printf("Setting setparam\n");
  param.sched_priority=threadPriority;
  if(sched_setparam(0,&param)){
    printf("Error in sched_setparam: %s - probably need to run as root if this is important\n",strerror(errno));
  }
  if(sched_setscheduler(0,SCHED_RR,&param))
    printf("sched_setscheduler: %s - probably need to run as root if this is important\n",strerror(errno));
  if(pthread_setschedparam(pthread_self(),SCHED_RR,&param))
    printf("error in pthread_setschedparam - maybe run as root?\n");
  return 0;
}
#define MASKED_MODIFY(oldval, modval, mask) (((oldval) & ~(mask)) | ((modval) & (mask)))
/**
   This does the same as nslmon -u X --clrf
   Clears the receive fifo.
   Copied from the source code for nslmon.
*/

int clearReceiveBuffer(CamStruct *camstr,int cam){
  uint32 state=0;
  printf("clearing receive buffer (clrf)\n");
#ifndef NOSL240
  state = nslReadCR(&camstr->handle[cam], 0x8);
#endif
  state = MASKED_MODIFY(state, 0x2000, 0x00002000);
  
#ifndef NOSL240
  nslWriteCR(&camstr->handle[cam], 0x8, state);
#endif  
  //usysMsTimeDelay(10);
  usleep(10000);
  
  state = MASKED_MODIFY(state, 0, 0x00002000);
#ifndef NOSL240
  nslWriteCR(&camstr->handle[cam], 0x8, state);
#endif
  printf("clearing receive buffer (clrf) DONE\n");
  return 0;
}

#undef MASKED_MODIFY


/**
   Start the DMA going
*/
int getData(CamStruct *camstr,int cam,int nbytes,int *dest){
  int rt=0;
#ifndef NOSL240
  uint32 flagsIn,bytesXfered,flagsOut,status;
  flagsIn = NSL_DMA_USE_SYNCDV;//0;
  //pthread_mutex_lock(&camstr->m);

  status = nslRecv(&camstr->handle[cam], (void *)dest, nbytes, flagsIn, camstr->timeout[cam],&bytesXfered, &flagsOut, NULL);
  //pthread_mutex_unlock(&camstr->m);
  if (status == NSL_TIMEOUT) {
    printf("Received timeout\n");
    rt=1;
  } else if (status == NSL_LINK_ERROR) {
    printf("Link error detected\n");
    rt=1;
  } else if (status == NSL_SUCCESS){
    if(flagsOut&NSL_DMA_USE_SYNCDV){
      printf("SYNCDV received while waiting for data - truncated frame (%d/%d bytes)\n",bytesXfered,nbytes);
      //So, have already got the sof for the next frame...
      camstr->gotsyncdv[cam]=1;
      camstr->camErr[cam]=1;//previous frame didn't get finished.
      rt=1;
    }else if(nbytes!=bytesXfered){
      printf("%d bytes requested, %d bytes received\n", nbytes, bytesXfered);
      rt=1;
    }else{
      //printf("got data okay\n");
    }
  }else{
    printf("%s\n", nslGetErrStr(status));
    rt=1;
  }
#endif
  return rt;
}
/**
   Wait for a DMA to complete
*/
int waitStartOfFrame(CamStruct *camstr,int cam){
  int rt=0;
#ifndef NOSL240
  int done=0;
  int syncerrmsg=0;
  uint32 flagsIn;
  uint32 sofWord[1024];
  uint32 bytesXfered;
  uint32 flagsOut;
  uint32 status;
  int nbytes=0;
#endif
  //nslSeq seq;
  if(camstr->gotsyncdv[cam]){//have previously got the start of frame while reading a truncated frame...
    camstr->gotsyncdv[cam]=0;
#ifndef NOSL240
    done=1;
    nbytes=sizeof(uint32);//so that the message isn't printed out below.
#endif
  }
#ifndef NOSL240
  flagsIn = NSL_DMA_USE_SYNCDV;
  while(done==0){
    //pthread_mutex_lock(&camstr->m);
    status = nslRecv(&camstr->handle[cam], (void *)sofWord, sizeof(uint32)*1024, flagsIn, camstr->timeout[cam], &bytesXfered, &flagsOut, NULL);
    //pthread_mutex_unlock(&camstr->m);
    if (status == NSL_SUCCESS) {
      if (flagsOut & NSL_DMA_USE_SYNCDV) {
	//printf("SYNC frame data = 0x%x bytes %d\n", sofWord,bytesXfered);
	done=1;
	rt=0;
	nbytes+=bytesXfered;
	
	//printf("New frame %d\n",cam);
      }else{//it may be here that the buffer is partially filled - in which case, we should continue reading 4 bytes for up to a full frame size, and see if we get the SYNC_DV.
	if(syncerrmsg==0){
	  printf("WARNING: SYNCDV not set - may be out of sync, sof[0]=%#x bytes %d timeout %d npxls %d cam %d\n",sofWord[0],bytesXfered,camstr->timeout[cam],camstr->npxlsArr[cam],cam);
	}
	syncerrmsg++;
	rt=1;
	nbytes+=bytesXfered;
      }
    }else if(status!=NSL_TIMEOUT){
      printf("camerror: %s\n",nslGetErrStr(status));
      rt=1;
      done=1;
    }else{
      printf("Timeout waiting for new frame (cam %d)\n",cam);
      rt=1;
      done=1;
    }
  }
  if((syncerrmsg>0 || nbytes!=sizeof(uint32)) && rt==0)//previously printed a sync warning... so now print an ok msg
    printf("Start of frame received okay for cam %d after %d tries (%d bytes, %d pixels)\n",cam,syncerrmsg,nbytes,nbytes/(int)sizeof(uint32));
#endif
  return rt;
}
#ifdef RESYNC
int endFrameWait(CamStruct *camstr,int cam,int err){
  int doExtra=0,i;
  pthread_mutex_lock(&camstr->m);
  if(err!=0){
    if(camstr->skipFrameAfterBad>0){
      if(camstr->gotsyncdv[cam]){
	camstr->ntoread[cam]+=camstr->skipFrameAfterBad;
      }
    }
  }
  camstr->thrcnt++;
  //if(camstr->thrcnt==1){//first thread to have completed - check that all the others have started.
  //if(camstr->doneSyncCheck==0 && cam>=camstr->ncamSL240-1){//haven't yet done a sync check, and an aravis cam has just finished...
  if(camstr->doneSyncCheck==0){// && cam<camstr->ncamSL240){//haven't yet done a sync check,  - only for the SL240s.
    //Checking camera number is a bodge.  Found that the 2 ngs pairs would play each other off, so didn't want to use them.  Tried doing this sync check with the aravis, but then the LGS was out of sync.  So, now trying both...
    camstr->doneSyncCheck=1;
    for(i=0;i<camstr->ncam;i++){
      if(camstr->readHasStarted[i]==0){//a camera hasn't started a read - so we'd better do an extra one to let it keep up.
	printf("Read cam%d not yet started - prob frame missing\n",i);
	doExtra=1;
	//break;
      }
    }
  }
  if(doExtra){
    for(i=0;i<camstr->ncam;i++){
      if(camstr->readHasStarted[i]!=0)
	camstr->ntoread[i]++;
    }
    //      for(i=camstr->ncamSL240;i<camstr->ncam;i++)
    //	camstr->ntoread[i]++;
  }
  
  //Block until all threads have completed...
  if(camstr->thrcnt==camstr->ncam){//last thread to have completed this frame
    camstr->thrcnt=0;
    camstr->doneSyncCheck=0;
    pthread_cond_broadcast(&camstr->thrcond);
  }else{
    //Threads should all wait here until all completed this frame...
    pthread_cond_wait(&camstr->thrcond,&camstr->m);
  }
  camstr->readHasStarted[cam]=0;
  pthread_mutex_unlock(&camstr->m);
  return 0;
}
#endif



void cameraCallbackTest(void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer){
  ArvStream *stream;
  ArvBuffer *buf=NULL;
  ThreadStruct *thrStruct=(ThreadStruct*)user_data;
  CamStruct *camstr=thrStruct->camstr;
  int cam=thrStruct->camNo;
  if(type==ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE){
    stream=camstr->stream[cam];
    buf=arv_stream_try_pop_buffer(stream);
    if(buf!=NULL){
      //if(buf->status==ARV_BUFFER_STATUS_SUCCESS)
      //camstr->buffer_count[cam]++;
      arv_stream_push_buffer(stream,buf);
    }else{
      printf("Failed to pop buffer for %s\n",camstr->camNameList[cam]);
    }
  }
}
void cameraCallback(void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer){
  ArvStream *stream;
  ArvBuffer *buf=NULL;
  //MyDataStruct *myData;
  //ApplicationData *myData=(ApplicationData*)user_data;
  ThreadStruct *thrStruct=(ThreadStruct*)user_data;
  CamStruct *camstr=thrStruct->camstr;
  int cam=thrStruct->camNo;
  struct timeval t1;
  char tbuf[16];
  //copy into dmabuf?  Or into pxlarr?
  //No, I think leave it where it is.  Need some scheme of putting buffers on a stack, and letting the rtcs pick most recent one.
  //3 pointers (for each camera):  rtcReading, currentFilling, mostRecentFilled.  
  //At start of darc frame, check mostRecentFilled, if !Null use it (copy to rtcReading), and set to null.
  //If mostRecentFilled is Null, then copy currentFilling to rtcReading, and set a flag to say we're using this one.
  //etc.


  if(type==ARV_STREAM_CALLBACK_TYPE_NEW_DATA){//this one first, since called most often.
    if(buffer!=NULL){
      if(buffer->contiguous_data_received>=buffer->last_data_accessed+camstr->blocksize[cam] || (buffer->contiguous_data_received==buffer->size && camstr->readStarted[cam]==1)){//a new block filled - or have finished the frame
#ifdef RESYNC
	int frameFinished=0;
#endif
	//notify...
	pthread_mutex_lock(&camstr->camMutex[cam]);
	while(buffer->contiguous_data_received>=buffer->last_data_accessed+camstr->blocksize[cam]){
	  buffer->last_data_accessed+=camstr->blocksize[cam];
	}
	//If this was the last bit of data...:
	if(buffer->contiguous_data_received==buffer->size && camstr->readStarted[cam]==1){//received it all...  for some reason, seem to get 2 calls for this one sometimes?
	//END OF FRAME...
#ifdef RESYNC
	  frameFinished=1;
#endif
	  if(camstr->recordTimestamp){
	    gettimeofday(&camstr->timestamp[cam],NULL);
	    //printf("Cam %d end time %d\n",cam,(int)camstr->timestamp[cam].tv_usec);
	  }
	  camstr->readStarted[cam]=0;
	  if(camstr->mostRecentFilled[cam]!=NULL){
	    printf("Skipping [%u %s]: darc not keeping up/diff Hz/lost data\n",camstr->mostRecentFilled[cam]->frame_id,camstr->camNameList[cam]);
	  }else{
	    //printf("ok - not skipping\n");
	  }
	  if(camstr->currentFilling[cam]!=NULL){//not currently being read
	    //camstr->mostRecentFilled[cam]=buffer;
	    printf("Pipeline read of cam %d not yet started - dropping frame\n",cam);
	    camstr->mostRecentFilled[cam]=NULL;
	    camstr->currentFilling[cam]=NULL;
	    //printf("setting mostRecentFilled to frame %u\n",buffer->frame_id);
	  }else{
	    camstr->mostRecentFilled[cam]=NULL;//should be anyway!
	    //printf("setting mostRecentFilled to NULL\n");
	  }
	}
	//wake up the main threads...
	if(camstr->waiting[cam]){
	  camstr->waiting[cam]=0;
	  //printf("Broadcasting - newdata\n");
	  pthread_cond_broadcast(&camstr->camCond[cam]);
	}
	pthread_mutex_unlock(&camstr->camMutex[cam]);

#ifdef RESYNC
	if(frameFinished){//wait for other threads.
	  endFrameWait(camstr,cam,0);
	}
#endif
      }else if(buffer->contiguous_data_received<buffer->last_data_accessed){
	printf("Resetting last_data_accessed (probably shouldn't see this)\n");
	buffer->last_data_accessed=0;
      }
    }
  }else if(type==ARV_STREAM_CALLBACK_TYPE_START_BUFFER){
    //New frame starting.  buffer may be NULL - but now probably not.
    //if(myData!=NULL && myData->t1.tv_sec==0 && myData->t1.tv_usec==0)
    //gettimeofday(&myData->t1,NULL);
    if(buffer==NULL){
      printf("null buffer... hmmm.\n");
    }else{
      buffer->last_data_accessed=0;
      pthread_mutex_lock(&camstr->camMutex[cam]);
#ifdef RESYNC
      if(camstr->ncurrentlyReading[cam]<=0){
	pthread_mutex_lock(&camstr->m);
	camstr->ncurrentlyReading[cam]=camstr->ntoread[cam];
	camstr->ntoread[cam]=1;
	pthread_mutex_unlock(&camstr->m);
      }
      if(camstr->ncurrentlyReading[cam]>0)
	camstr->ncurrentlyReading[cam]--;
#endif

      if(camstr->readStarted[cam]==1){
	//todo("Set error somehow - previous frame didn't get finished");
	//but only if someone has started reading the buffer...
	if(camstr->currentFilling[cam]==NULL){
	  camstr->camErr[cam]=1;//previous frame didn't get finished.
	  camstr->mostRecentFilled[cam]=NULL;//just incase - probably null anyway.
	  printf("Error case - setting mostRecentFilled to NULL\n");
	}
      }
      //gettimeofday(&t1,NULL);
      //printf("Cam %d start time %d\n",cam,(int)t1.tv_usec);

      camstr->currentFilling[cam]=buffer;
      if(camstr->waiting[cam] 
#ifdef RESYNC
	 && camstr->ncurrentlyReading[cam]==0
#endif
	 ){//is anyoen waiting?  Tell them about the new buffer.
	camstr->waiting[cam]=0;
	//printf("Broadcasting sof\n");
	pthread_cond_broadcast(&camstr->camCond[cam]);
      }
      pthread_mutex_unlock(&camstr->camMutex[cam]);
    }
#ifdef RESYNC
    if(camstr->ncurrentlyReading[cam]==0){//this is a valid frame (not one we're skipping):
#endif
      camstr->readStarted[cam]=1;
#ifdef RESYNC
      pthread_mutex_lock(&camstr->m);
      camstr->readHasStarted[cam]=1;
      pthread_mutex_unlock(&camstr->m);
    }
#endif
  }else if(type==ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE){
    stream=camstr->stream[cam];
    //Really, should check status of buffer->status, but I think there may be a bug.  So, here, instead check whether buffer->contiguous_data_received==buffer->size
    //printf("Frame status: %d;  Success: %d\n",buffer->status,buffer->status==ARV_BUFFER_STATUS_SUCCESS);
    if(buffer->contiguous_data_received!=buffer->size){
      gettimeofday(&t1,NULL);
      strftime(tbuf,sizeof(tbuf),"%H:%M:%S",gmtime(&t1.tv_sec));
      printf("Not all frame received (sta=%d, suc=%d, contig: %d/%ld) %s.%06d\n",buffer->status,ARV_BUFFER_STATUS_SUCCESS,buffer->contiguous_data_received,buffer->size,tbuf,(int)t1.tv_usec);
      //Notify the main threads, after setting an error.
      //Note - this is only an error if something has started accessing the data in the first place - in which case currentFilling will be NULL.
      pthread_mutex_lock(&camstr->camMutex[cam]);
      if(camstr->currentFilling[cam]==NULL){
	//printf("Not all frame received - setting mostRecentFilled to NULL\n");
	camstr->mostRecentFilled[cam]=NULL;//just in case.
	camstr->camErr[cam]=1;//previous frame didn't get finished.
	if(camstr->waiting[cam]
#ifdef RESYNC
	   && camstr->ncurrentlyReading[cam]==0
#endif
	   ){
	  camstr->waiting[cam]=0;
	  //todo("set an error");
	  camstr->readStarted[cam]=0;
	  //printf("Broadcasting (error)\n");
	  pthread_cond_broadcast(&camstr->camCond[cam]);
	}
      }else//nothing has started on this one yet, so simply set to null.
	camstr->currentFilling[cam]=NULL;
      pthread_mutex_unlock(&camstr->camMutex[cam]);
    }
    //And now release a stream.
    buf=arv_stream_try_pop_buffer(stream);
    if(buf!=NULL){
      //if(buf->status==ARV_BUFFER_STATUS_SUCCESS)
      //camstr->buffer_count[cam]++;
      arv_stream_push_buffer(stream,buf);
    }else{
      printf("Failed to pop buffer for %s\n",camstr->camNameList[cam]);
    }
    //camstr->prevBuffer[cam]=buffer;
  }else if(type==ARV_STREAM_CALLBACK_TYPE_INIT){//this would be a good place to change thread priority and affinity.  Called about once.
    printf("Initialised stream for %s, thread %ld, setting affinity/priority\n",camstr->camNameList[cam],pthread_self());
    camSetThreadAffinityAndPriority(&camstr->threadAffinity[cam*camstr->threadAffinElSize],camstr->threadPriority[cam],camstr->threadAffinElSize);
    camstr->ntoread[cam]=1;
  }else{//ignore the other callbacks...
  }
}

int stopCamera(CamStruct *camstr,int cam){
  guint64 n_completed_buffers;
  guint64 n_failures;
  guint64 n_underruns;
  if(camstr->camera[cam]!=NULL){
    if(camstr->stream[cam]!=NULL){
      //if (camInfo->software_trigger_source > 0)
      //g_source_remove (camInfo->software_trigger_source);
      arv_stream_get_statistics (camstr->stream[cam], &n_completed_buffers, &n_failures, &n_underruns);
      printf ("Completed buffers = %Lu (%s)\n", (unsigned long long) n_completed_buffers,camstr->camNameList[cam]);
      printf ("Failures          = %Lu (%s)\n", (unsigned long long) n_failures,camstr->camNameList[cam]);
      printf ("Underruns         = %Lu (%s)\n", (unsigned long long) n_underruns,camstr->camNameList[cam]);
      arv_camera_stop_acquisition (camstr->camera[cam]);
      g_object_unref (camstr->stream[cam]);
    } else{
      printf("No stream to stop\n");
    }
    g_object_unref (camstr->camera[cam]);
    camstr->camera[cam]=NULL;//do I want to do this?  Care with the callbacks...
  }else{
    printf("No cam to stop\n");
  }
  return 0;
}
int byteswap(int a){
  int len=4;
  char *p=(char*)&a;
  int i;
  char tmp;
  for(i = 0; i < len/2; i++) {
    tmp = p[len-i-1];
    p[len-i-1] = p[i];
    p[i] = tmp;
  }
  return a;
}

int startCamera(CamStruct *camstr,int cam){
  ArvCamera *camera;
  ArvStream *stream;
  int i;
  int rt=0;
  printf("Looking for camera '%s'\n",camstr->camNameList[cam]);
  camera = arv_camera_new (camstr->camNameList[cam]);
  camstr->camera[cam]=camera;
  if (camera != NULL) {
    gint payload;
    gint x, y, width, height;
    gint dx, dy;
    double exposure;
    int gain;
    int ox,oy,nx,ny;
    ArvBuffer **bufArr;
    bufArr=&camstr->bufArrList[cam*NBUF];
    if(camstr->byteswapInts[cam]){
      ox=byteswap(camstr->offsetX[cam]);
      oy=byteswap(camstr->offsetY[cam]);
      nx=byteswap(camstr->campxlx[cam]);
      ny=byteswap(camstr->campxly[cam]);
    }else{
      ox=camstr->offsetX[cam];
      oy=camstr->offsetY[cam];
      nx=camstr->campxlx[cam];
      ny=camstr->campxly[cam];
    }
    arv_camera_set_region (camera, ox,oy,nx,ny);
    //arv_camera_set_region (camera, camstr->offsetX[cam],camstr->offsetY[cam], camstr->pxlx[cam], camstr->pxly[cam]);
    arv_camera_get_region (camera, &x, &y, &width, &height);
    arv_camera_get_binning (camera, &dx, &dy);
    exposure = arv_camera_get_exposure_time (camera);
    payload = arv_camera_get_payload (camera);
    gain = arv_camera_get_gain (camera);
    
    printf ("vendor name         = %s\n", arv_camera_get_vendor_name (camera));
    printf ("model name          = %s\n", arv_camera_get_model_name (camera));
    printf ("device id           = %s\n", arv_camera_get_device_id (camera));
    printf ("image width         = %d\n", width);
    printf ("image height        = %d\n", height);
    printf ("horizontal binning  = %d\n", dx);
    printf ("vertical binning    = %d\n", dy);
    printf ("payload             = %d bytes\n", payload);
    printf ("exposure (?)        = %g µs\n", exposure);
    printf ("gain                = %d dB\n", gain);
    ((ThreadStruct*)camstr->thrStruct)[cam].camNo=cam;
    ((ThreadStruct*)camstr->thrStruct)[cam].camstr=camstr;
    stream = arv_camera_create_stream (camera, cameraCallback, &((ThreadStruct*)camstr->thrStruct)[cam]);//was myData
    camstr->stream[cam]=stream;
    if (stream != NULL) {
      if (ARV_IS_GV_STREAM (stream)) {
	if (camstr->auto_socket_buffer)
	  g_object_set (stream,"socket-buffer", ARV_GV_STREAM_SOCKET_BUFFER_AUTO,"socket-buffer-size", 0,NULL);
	else
	  g_object_set (stream,"socket-buffer-size",camstr->socket_buffer_size,0,NULL);
	if (camstr->no_packet_resend)
	  g_object_set (stream,"packet-resend", ARV_GV_STREAM_PACKET_RESEND_NEVER,NULL);
	g_object_set (stream,"packet-timeout", (unsigned) camstr->packet_timeout * 1000,"frame-retention", (unsigned) camstr->frame_retention * 1000,NULL);
      }
      for (i = 0; i < NBUF; i++){
	bufArr[i]=arv_buffer_new(payload,NULL);
	arv_stream_push_buffer (stream, bufArr[i]);
      }
      arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS);
      arv_camera_start_acquisition (camera);//starts a new thread...
    }else{
      printf ("Can't create stream thread (check if the device is not already used)\n");
      rt=1;
    }
  }else{
    printf ("No camera found\n");
    rt=1;
  }
  return rt;
}

//void* workerAravis(void *thrstrv){
//  return NULL;
//}

/**
   The threads that does the work...
   One per camera interface...
   Threads here are independent - just reading out the camera as fast as they can
*/
void* workerSL240(void *thrstrv){
  ThreadStruct *thrstr=(ThreadStruct*)thrstrv;
  CamStruct *camstr=thrstr->camstr;
  int cam=thrstr->camNo;
  int req,extra,off,err;
  int nRead;
  int pxlcnt;
  struct timeval t1;
  int bufindx=-1;
  
  printf("Calling setThreadAffinityAndPriority\n");
  camSetThreadAffinityAndPriority(&camstr->threadAffinity[cam*camstr->threadAffinElSize],camstr->threadPriority[cam],camstr->threadAffinElSize);
  //pthread_mutex_lock(&camstr->camMutex[cam]);
#ifdef RESYNC
  pthread_mutex_lock(&camstr->m);
#endif
  camstr->ntoread[cam]=1;
#ifdef RESYNC
  pthread_mutex_unlock(&camstr->m);
#endif
  //if(camstr->thrcnt==0){//first frame...
  camstr->transferframe[cam]=0;//rtcReading
  //camstr->last[cam]=-1;
  camstr->latestframe[cam]=-1;//mostRecentFilled
  camstr->curframe[cam]=-1;//currentFilling
  //}
  clearReceiveBuffer(camstr,cam);
  while(camstr->open){
    //if(camstr->thrcnt==0){//first frame...
      //camstr->thrcnt++;
    bufindx++;//new frame
    if(bufindx>=NBUF)
      bufindx=0;
    //camstr->curframe[cam]=bufindx;
      //set the pixels to zero (actually, possibly no need).
    //todo("Set pixels to zero?\n");
      //for(i=0; i<camstr->ncam; i++){
      //camstr->pxlcnt[NBUF*i+(camstr->curframe&BUFMASK)]=0;
      //}
      //}
    //camstr->thrcnt++;
    //camstr->pxlsRequested=0;
    err=0;
#ifdef RESYNC
    pthread_mutex_lock(&camstr->m);
#endif
    nRead=camstr->ntoread[cam];
    camstr->ntoread[cam]=1;
#ifdef RESYNC
    pthread_mutex_unlock(&camstr->m);
#endif
    if(nRead!=1)
      printf("nRead %d for cam %d\n",nRead,cam);
    //pthread_mutex_unlock(&camstr->camMutex[cam]);
    while(err==0 && nRead>0 && camstr->open!=0){
      nRead--;
      pxlcnt=0;
      //Read the start of frame...
      //gettimeofday(&t1,NULL);
      //printf("Cam %d wait start time %d\n",cam,(int)t1.tv_usec);
      err=waitStartOfFrame(camstr,cam);
      //printf("waitstartofframe %d (err %d), nRead %d\n",cam,err,nRead);
      if(err==0){
	if(nRead==0){
	  //gettimeofday(&t1,NULL);
	  //printf("Cam %d start time %d\n",cam,(int)t1.tv_usec);
	  camstr->curframe[cam]=bufindx;
#ifdef RESYNC
	  pthread_mutex_lock(&camstr->m);
	  camstr->readHasStarted[cam]=1;
	  pthread_mutex_unlock(&camstr->m);
#endif
	}
	//now loop until we've read all the pixels.
	((int*)(&camstr->DMAbuf[cam][bufindx*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(unsigned int))]))[0]=0;//set frame counter to zero.
	while(pxlcnt<camstr->npxlsArr[cam] && err==0){
	  req=camstr->npxlsArr[cam]-pxlcnt<camstr->blocksize[cam]?camstr->npxlsArr[cam]-pxlcnt:camstr->blocksize[cam];
	  if(pxlcnt==0){//first pixel - also transfer header.
	    extra=HDRSIZE;
	    off=0;
	  }else{
	    extra=0;
	    off=HDRSIZE/sizeof(int)+pxlcnt;
	  }
	  err=getData(camstr,cam,req*sizeof(int)+extra,&(camstr->DMAbuf[cam][bufindx*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(int))+off]));
	  pxlcnt+=req;
	  if(nRead==0){//have just read part of the frame that is to be sent... none left to read this iteration - so send data to the RTC.
	    //Is this mutex required?  Possibly not...
	    pthread_mutex_lock(&camstr->camMutex[cam]);
	    
	    camstr->err[NBUF*cam+bufindx]=err;
	    camstr->pxlcnt[NBUF*cam+bufindx]=pxlcnt;//+=req;
	    if(pxlcnt>=camstr->npxlsArr[cam]){//all pixels have arrived...
	      if(camstr->latestframe[cam]!=-1)
		printf("Cam %d skipping: darc not keeping up/diff Hz/lost data\n",cam);
	      if(camstr->curframe[cam]!=-1 && err==0){//not currently being read
		printf("Pipeline read of cam %d not yet started\n",cam);
		//camstr->latestframe[cam]=camstr->curframe[cam];
		camstr->latestframe[cam]=-1;
	      }else{
		camstr->latestframe[cam]=-1;//should be anyway.
	      }
	      camstr->curframe[cam]=-1;
	      if(camstr->recordTimestamp){//an option to put camera frame number as time in us of last pixel arriving... useful for synchronising different cameras so that last pixel arrives at same time.
		gettimeofday(&t1,NULL);
		((unsigned int*)(&camstr->DMAbuf[cam][bufindx*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(unsigned int))]))[0]=(unsigned int)(t1.tv_sec*1000000+t1.tv_usec);
		//printf("Cam %d end time %d\n",cam,(int)t1.tv_usec);
	      }
	    }
	    if(camstr->waiting[cam]==1){//the RTC is waiting for the newest pixels, so wake it up.
	      camstr->waiting[cam]=0;
	      pthread_cond_broadcast(&camstr->camCond[cam]);//signal should do.
	    }
	    pthread_mutex_unlock(&camstr->camMutex[cam]);
	  }
	}
      }else{//error getting start of frame
	((int*)(&camstr->DMAbuf[cam][bufindx*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(int))]))[0]=0;//set frame counter to zero.
	memset(&camstr->DMAbuf[cam][bufindx*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(int))+HDRSIZE/sizeof(int)],0,sizeof(int)*camstr->npxlsArr[cam]);
	//printf("memset dmabuf\n");
      }
    }
    //printf("ntoread %d\n",camstr->ntoread[cam]);
    //camstr->ntoread[cam]-=camstr->resync;
    //camstr->ntoread[cam]=1;
    camstr->err[NBUF*cam+bufindx]=err;
    if(err && camstr->waiting[cam]){//the RTC is waiting for the newest pixels, so wake it up, but an error has occurred.
      camstr->waiting[cam]=0;
      pthread_cond_broadcast(&camstr->camCond[cam]);
    }
    //We've finished this frame, so:
    /*
    if(err==0){
      if(camstr->latestframe[cam]!=-1)
	printf("Cam %d skipping: darc not keeping up/diff Hz/lost data\n",cam);
      if(camstr->curframe[cam]!=-1){//not currently being read
	camstr->latestframe[cam]=camstr->curframe[cam];
	camstr->curframe[cam]=-1;
      }else{
	camstr->latestframe[cam]=-1;//should be anyway.
      }
      }else{*/
#ifdef RESYNC
    endFrameWait(camstr,cam,err);
#else
  if(err!=0){
    if(camstr->skipFrameAfterBad>0){
      if(camstr->gotsyncdv[cam]){
	camstr->ntoread[cam]+=camstr->skipFrameAfterBad;
      }
    }
  }
#endif
    

  }
  //pthread_mutex_unlock(&camstr->camMutex[cam]);
  return 0;
}

/**
   Open a camera of type name.  Args are passed in a int32 array of size n, which can be cast if necessary.  Any state data is returned in camHandle, which should be NULL if an error arises.
   pxlbuf is the array that should hold the data. The library is free to use the user provided version, or use its own version as necessary (ie a pointer to physical memory or whatever).  It is of size npxls*sizeof(short).
   ncam is number of cameras, which is the length of arrays pxlx and pxly, which contain the dimensions for each camera.  Currently, ncam must equal 1.
   Name is used if a library can support more than one camera.
   frameno is a pointer to an array with a value for each camera in which the frame number should be placed.

   This is for getting data from the SL240.
   args here currently contains the blocksize (int32) and other things
*/


#define TEST(a) if((a)==NULL){printf("calloc error\n");dofree(camstr);*camHandle=NULL;return 1;}

int camOpen(char *name,int n,int *args,paramBuf *pbuf,circBuf *rtcErrorBuf,char *prefix,arrayStruct *arr,void **camHandle,int nthreads,unsigned int frameno,unsigned int **camframeno,int *camframenoSize,int npxls,int ncam,int *pxlx,int* pxly){
  CamStruct *camstr;
#ifndef NOSL240
  uint32 status;
#endif
  int i,ngot,j,k,es;
  unsigned short *tmps;
  int ncamSL240;
  int camIDLen=0;
  char **camParamName;
  printf("Initialising camera %s\n",name);
  /*if(npxls&1){
    printf("Error - odd number of pixels not supported by SL240 card\n");
    return 1;
    }*/
  if((*camHandle=malloc(sizeof(CamStruct)))==NULL){
    printf("Couldn't malloc camera handle\n");
    return 1;
  }
  printf("Malloced camstr\n");
  memset(*camHandle,0,sizeof(CamStruct));
  camstr=(CamStruct*)*camHandle;
  if(arr->pxlbuftype!='H' || arr->pxlbufsSize!=sizeof(unsigned short)*npxls){
    //need to resize the pxlbufs...
    arr->pxlbufsSize=sizeof(unsigned short)*npxls;
    arr->pxlbuftype='H';
    arr->pxlbufelsize=sizeof(unsigned short);
    tmps=realloc(arr->pxlbufs,arr->pxlbufsSize);
    if(tmps==NULL){
      if(arr->pxlbufs!=NULL)
	free(arr->pxlbufs);
      printf("pxlbuf malloc error in camfile.\n");
      arr->pxlbufsSize=0;
      free(*camHandle);
      *camHandle=NULL;
      return 1;
    }
    arr->pxlbufs=tmps;
    memset(arr->pxlbufs,0,arr->pxlbufsSize);
  }
  if(n>0)
    camstr->threadAffinElSize=args[0];
  camstr->imgdata=arr->pxlbufs;
  //camstr->frameno=frameno;
  if(*camframenoSize<ncam){
    if(*camframeno!=NULL)
      free(*camframeno);
    if((*camframeno=calloc(sizeof(unsigned int),ncam))==NULL){
      printf("Couldn't calloc camframeno\n");
      *camframenoSize=0;
      dofree(camstr);
      *camHandle=NULL;
      return 1;
    }else{
      *camframenoSize=ncam;
    }
  }
  camstr->userFrameNo=*camframeno;
  camstr->ncam=ncam;
  camstr->rtcErrorBuf=rtcErrorBuf;
  camstr->npxls=npxls;//*pxlx * *pxly;
  TEST(camstr->camNameList=calloc(ncam,sizeof(char*)));
  TEST(camstr->npxlsArr=calloc(ncam,sizeof(int)));
  TEST(camstr->npxlsArrCum=calloc((ncam+1),sizeof(int)));
  TEST(camstr->blocksize=calloc(ncam,sizeof(int)));
  TEST(camstr->pxlcnt=calloc(ncam*NBUF,sizeof(int)));
  TEST(camstr->ntoread=calloc(ncam,sizeof(int)));
  TEST(camstr->readStarted=calloc(ncam,sizeof(int)));
#ifdef RESYNC
  TEST(camstr->readHasStarted=calloc(ncam,sizeof(int)));
  TEST(camstr->ncurrentlyReading=calloc(ncam,sizeof(int)));
#endif
  TEST(camstr->waiting=calloc(ncam,sizeof(int)));
  TEST(camstr->newframe=calloc(ncam,sizeof(int)));
  TEST(camstr->pxlsTransferred=calloc(ncam,sizeof(int)));
  TEST(camstr->setFrameNo=calloc(ncam,sizeof(int)));
  TEST(camstr->sl240Opened=calloc(ncam,sizeof(int)));
  TEST(camstr->timeout=calloc(ncam,sizeof(uint32)));
  TEST(camstr->fibrePort=calloc(ncam,sizeof(int)));
  TEST(camstr->err=calloc(ncam*NBUF,sizeof(int)));
  TEST(camstr->thrStruct=calloc(ncam,sizeof(ThreadStruct)));
  TEST(camstr->threadid=calloc(ncam,sizeof(pthread_t)));
  TEST(camstr->camCond=calloc(ncam,sizeof(pthread_cond_t)));
  TEST(camstr->camCond2=calloc(ncam,sizeof(pthread_cond_t)));
  TEST(camstr->camMutex=calloc(ncam,sizeof(pthread_mutex_t)));
  TEST(camstr->threadAffinity=calloc(ncam*camstr->threadAffinElSize,sizeof(int)));
  TEST(camstr->threadPriority=calloc(ncam,sizeof(int)));
  TEST(camstr->reorder=calloc(ncam,sizeof(int)));
  TEST(camstr->reorderno=calloc(ncam,sizeof(int)));
  TEST(camstr->reorderIndx=calloc(ncam,sizeof(int)));
  TEST(camstr->reorderBuf=calloc(ncam,sizeof(int*)));
  TEST(camstr->testLastPixel=calloc(ncam,sizeof(int)));
  TEST(camstr->gotsyncdv=calloc(ncam,sizeof(int)));
  TEST(camstr->pxlShift=calloc(ncam*2,sizeof(int)));
  TEST(camstr->pxlx=calloc(ncam,sizeof(int)));
  TEST(camstr->pxly=calloc(ncam,sizeof(int)));
  TEST(camstr->campxlx=calloc(ncam,sizeof(int)));
  TEST(camstr->campxly=calloc(ncam,sizeof(int)));
  TEST(camstr->frameReady=calloc(ncam,sizeof(int)));
  TEST(camstr->curframe=calloc(ncam,sizeof(int)));
  TEST(camstr->latestframe=calloc(ncam,sizeof(int)));
  TEST(camstr->transferframe=calloc(ncam,sizeof(int)));
  TEST(camstr->camErr=calloc(ncam,sizeof(int*)));
  TEST(camstr->offsetX=calloc(ncam,sizeof(int)));
  TEST(camstr->offsetY=calloc(ncam,sizeof(int)));
  TEST(camstr->byteswapInts=calloc(ncam,sizeof(int)));
  TEST(camstr->timestamp=calloc(ncam,sizeof(struct timeval)));
  TEST(camstr->mostRecentFilled=calloc(ncam,sizeof(ArvBuffer*)));
  TEST(camstr->currentFilling=calloc(ncam,sizeof(ArvBuffer*)));
  TEST(camstr->rtcReading=calloc(ncam,sizeof(ArvBuffer*)));
  TEST(camstr->stream=calloc(ncam,sizeof(ArvStream*)));
  TEST(camstr->camera=calloc(ncam,sizeof(ArvCamera*)));
  TEST(camstr->bufArrList=calloc(ncam,sizeof(ArvBuffer*)*NBUF));
  TEST(camstr->prevCmd=calloc(ncam+3,sizeof(char*)));

  TEST(camstr->bpp=calloc(sizeof(int),ncam));

  camstr->npxlsArrCum[0]=0;
  printf("malloced things\n");
  for(i=0; i<ncam; i++){
    camstr->pxlx[i]=pxlx[i];
    camstr->pxly[i]=pxly[i];
    camstr->campxlx[i]=pxlx[i];//overwritten later (args)
    camstr->campxly[i]=pxly[i];//overwritten later (args)
    camstr->npxlsArr[i]=pxlx[i]*pxly[i];
    camstr->npxlsArrCum[i+1]=camstr->npxlsArrCum[i]+camstr->npxlsArr[i];
    /*if(camstr->npxlsArr[i]&1){
      printf("Error - odd number of pixels not supported by SL240 card cam %d",i);
      dofree(camstr);
      *camHandle=NULL;
      return 1;
      }*/
  }
  //args:
  //0 = threadAffinElSize
  //1 = ncamSL240
  //2->. +ncamSL240*(6+elsize) - per cam info for sl240.
  //. ->. +(ncam-ncamSL240)*(5+elsize)

  //bpp,blocksize,offsetx,offsety,threadprio,affin
  //camIDLen, gigE camera IDs
  es=args[0];
  camstr->ncamSL240=args[1];
  ncamSL240=camstr->ncamSL240;
  if(n>=(6+es)*ncamSL240+(9+es)*(ncam-ncamSL240)+7){
    int j;
    for(i=0; i<ncamSL240; i++){
      camstr->blocksize[i]=args[i*(6+es)+2];//blocksize in pixels.
      camstr->timeout[i]=args[i*(6+es)+3];//timeout in ms.
      camstr->fibrePort[i]=args[i*(6+es)+4];//fibre port
      camstr->threadPriority[i]=args[i*(6+es)+5];//thread priority
      camstr->reorder[i]=args[i*(6+es)+6];//reorder pixels
      camstr->testLastPixel[i]=args[i*(6+es)+7];//test last pixel
      for(j=0;j<es;j++)
	camstr->threadAffinity[i*es+j]=((unsigned int*)&args[i*(6+es)+8])[j];//thread affinity
    }
    for(i=0;i<ncam-ncamSL240;i++){
      camstr->bpp[i+ncamSL240]=args[ncamSL240*(6+es)+2+i*(9+es)];//bits/pixel
      camstr->blocksize[i+ncamSL240]=args[ncamSL240*(6+es)+3+i*(9+es)];//blocksize
      camstr->offsetX[i+ncamSL240]=args[ncamSL240*(6+es)+4+i*(9+es)];//offsetx
      camstr->offsetY[i+ncamSL240]=args[ncamSL240*(6+es)+5+i*(9+es)];//offsety
      camstr->campxlx[i+ncamSL240]=args[ncamSL240*(6+es)+6+i*(9+es)];//campxlx
      camstr->campxly[i+ncamSL240]=args[ncamSL240*(6+es)+7+i*(9+es)];//campxly
      camstr->byteswapInts[i+ncamSL240]=args[ncamSL240*(6+es)+8+i*(9+es)];//byte swap ints?
      camstr->reorder[i+ncamSL240]=args[ncamSL240*(6+es)+9+i*(9+es)];//reorder

      camstr->threadPriority[i+ncamSL240]=args[ncamSL240*(6+es)+10+i*(9+es)];//threadprio
      for(j=0;j<es;j++)
	camstr->threadAffinity[(i+ncamSL240)*es+j]=((unsigned int*)&args[ncamSL240*(6+es)+11+i*(9+es)])[j];//affin
    }
    //if(n>=(6+es)*ncam+2){
    // camstr->resync=args[(6+es)*ncam+1];
    //}else{
    //  camstr->resync=10;
    //}
    //if(n>=(6+es)*ncam+3){
    //  camstr->wpuCorrection=args[(6+es)*ncam+2];
    //}else{
    //  camstr->wpuCorrection=0;
    //}
    if(n>ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+2){
      camstr->skipFrameAfterBad=args[ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+2];
      printf("skipFrameAfterBad %d\n",camstr->skipFrameAfterBad);
    }else{
      camstr->skipFrameAfterBad=0;
    }
    /*if(n>=(6+es)*ncam+5){
      camstr->testLastPixel=args[(6+es)*ncam+4];
      printf("testLastPixel %d\n",camstr->testLastPixel);
    }else{
      camstr->testLastPixel=0;
      }*/
    if(n>ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+3){
      camstr->pxlRowStartSkipThreshold=args[ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+3];
      printf("pxlRowStartSkipThreshold %d\n",camstr->pxlRowStartSkipThreshold);
    }else{
      camstr->pxlRowStartSkipThreshold=0;
    }
    if(n>ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+4){
      camstr->pxlRowEndInsertThreshold=args[ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+4];
      printf("pxlRowEndInsertThreshold %d\n",camstr->pxlRowEndInsertThreshold);
    }else{
      camstr->pxlRowEndInsertThreshold=0;
    }
    if(n>ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+5){
      camstr->recordTimestamp=args[ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+5];
    }else{
      camstr->recordTimestamp=0;
    }
    if(n>ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+7){
      camIDLen=args[ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+6];//number of bytes describing camera names
      camstr->nameList=strndup((char*)&args[ncamSL240*(6+es)+(ncam-ncamSL240)*(9+es)+7],camIDLen);
      char *namePtr=camstr->nameList;
      char *next;
      i=ncamSL240;
      while((next=strchr(namePtr,';'))!=NULL){
	next[0]='\0';
	printf("Camera %d: %s\n",i,namePtr);
	camstr->camNameList[i]=namePtr;
	namePtr=next+1;
	i++;
	if(i==ncam)
	  break;
      }
      //and the last one
      if(i<ncam){
	camstr->camNameList[i]=namePtr;
      }else{
	printf("Ignoring %s\n",namePtr);
      }
      for(i=ncamSL240;i<ncam;i++){
	printf("Camera: %s\n",camstr->camNameList[i]);
      }
    }else{
      if(ncam-ncamSL240==1)
	printf("Using first available camera\n");
      else
	printf("WARNING - no cameras defined, not sure what will happen...\n");
    }
      

  }else{
    printf("wrong number of cmd args, should be Naffin,nSL240, (blocksize, timeout, fibreport, thread priority, reorder, testLastPixel, thread affinity[Naffin]) repeated for nSL240,( bpp,blocksize,offsetx,offsety,camnpxlx,camnpxly,byteswapInts,reorder,priority, affinity[Naffin]) repeated for ncam-nSL240 + optional value: whether to skip a frame after a bad frame, and 3 more optional flags, pxlRowStartSkipThreshold, pxlRowEndInsertThreshold if doing a WPU correction based on dark column detection, recordTimestamp if want frame numbers to be last pixel received time in us.\n");
    dofree(camstr);
    *camHandle=NULL;
    return 1;
  }
  printf("got args\n");
  for(i=0; i<ncam; i++){
    printf("%d %d %d %d %d\n",camstr->blocksize[i],camstr->timeout[i],camstr->fibrePort[i],camstr->testLastPixel[i],camstr->reorder[i]);
  }
  /*
  maxbpp=0;
  for(i=0;i<ncam-ncamSL240;i++){
    if(camstr->bpp[i+ncamSL240]>maxbpp)
      maxbpp=camstr->bpp[i+ncamSL240];
  }
  bytespp=((maxbpp+7)/8);
  camstr->bytespp=bytespp;*/
  camstr->bytespp=2;//to match the SL240
  //And then we set up the rest, start the streams etc.
  camstr->socket_buffer_size=npxls*16;//make it big enough for several frames. - for testing evt
  camstr->no_packet_resend=1;
  camstr->packet_timeout=20;//in ms.  Might be too low for slow cameras?  Fix if needed.
  camstr->frame_retention=100;//in ms.

  //now need to prepare the parameter buffer names.
  ngot=0;
  for(i=0; i<ncam; i++){
    if(camstr->reorder[i]!=0){
      for(j=0;j<ngot;j++){//have we already got this reorder?
	if(camstr->reorderno[j]==camstr->reorder[i]){
	  break;
	}
      }
      if(j==ngot){//a new entry
	camstr->reorderno[j]=camstr->reorder[i];
	ngot++;
      }
    }
  }
  memset(camstr->reorderIndx,-1,sizeof(int)*ncam);

  //now need to prepare the camera parameter buffer names: aravisCmdN
  TEST(camParamName=calloc(ncam-ncamSL240+3+ngot+1,sizeof(char*)));
  for(i=0;i<ncam-ncamSL240;i++){
    if((camParamName[i]=calloc(BUFNAMESIZE,1))==NULL){
      printf("Failed to calloc camParamName in camSL240Aravis\n");
      dofree(camstr);
      *camHandle=NULL;
      for(i=0; i<ncam-ncamSL240; i++)
	free(camParamName[i]);
      free(camParamName);
      return 1;
    }
    snprintf(camParamName[i],BUFNAMESIZE,"aravisCmd%d",i+ncamSL240);
  }
  if((camParamName[ncam-ncamSL240]=calloc(BUFNAMESIZE,1))==NULL || (camParamName[ncam-ncamSL240+1]=calloc(BUFNAMESIZE,1))==NULL  || (camParamName[ncam-ncamSL240+2]=calloc(BUFNAMESIZE,1))==NULL || (camParamName[ncam-ncamSL240+3+ngot]=calloc(BUFNAMESIZE,1))==NULL){
    printf("Failed to calloc camParamName in camSL240Aravis\n");
    dofree(camstr);
    *camHandle=NULL;
    for(i=0; i<ncam-ncamSL240+3; i++)
      free(camParamName[i]);
    free(camParamName);
    return 1;
  }
  snprintf(camParamName[ncam-ncamSL240],BUFNAMESIZE,"aravisCmdAll");
  snprintf(camParamName[ncam-ncamSL240+1],BUFNAMESIZE,"aravisGet");
  snprintf(camParamName[ncam-ncamSL240+2],BUFNAMESIZE,"aravisMem");
  snprintf(camParamName[ncam-ncamSL240+3+ngot],BUFNAMESIZE,"recordTimestamp");
  for(i=0;i<ngot;i++){
    if((camParamName[i+ncam-ncamSL240+3]=calloc(BUFNAMESIZE,1))==NULL){
      printf("Failed to calloc reorders in camSL240Aravis\n");
      dofree(camstr);
      *camHandle=NULL;
      for(i=0; i<ncam-ncamSL240+3; i++)
	free(camParamName[i]);
      for(i=0;i<ngot+1;i++)
	free(camParamName[i+ncam-ncamSL240+3]);
      free(camParamName);
      return 1;
    }
    snprintf(camParamName[i+ncam-ncamSL240+3],16,"camReorder%d",camstr->reorderno[i]);
  }

  //Now sort them...
#define islt(a,b) (strcmp((*a),(*b))<0)
  QSORT(char*,camParamName,ncam-ncamSL240+3+ngot+1,islt);
#undef islt

  //now capture the order (and we know the camReorders will come after aravis*)
  for(i=0; i<ngot; i++){
    j=atoi(&camParamName[i+ncam-ncamSL240+3][10]);
    for(k=0;k<ncam;k++){
      if(camstr->reorder[k]==j){
	camstr->reorderIndx[k]=i+ncam-ncamSL240+3;
      }
    }
  }

  //now make the parameter buffer
  if((camstr->paramNames=calloc(ncam-ncamSL240+3+ngot+1,BUFNAMESIZE))==NULL){
    printf("Failed to mallocparamNames in camSL240Aravis.c\n");
    dofree(camstr);
    *camHandle=NULL;
    for(i=0; i<ncam-ncamSL240+3+ngot+1; i++)
      free(camParamName[i]);
    free(camParamName);
    return 1;
  }
  for(i=0; i<ncam-ncamSL240+3+ngot+1; i++){
    memcpy(&camstr->paramNames[i*BUFNAMESIZE],camParamName[i],BUFNAMESIZE);
    printf("%16s\n",&camstr->paramNames[i*BUFNAMESIZE]);
    free(camParamName[i]);
  }
  free(camParamName);
  TEST(camstr->index=calloc(sizeof(int),ncam-ncamSL240+3+ngot+1));
  TEST(camstr->values=calloc(sizeof(void*),ncam-ncamSL240+3+ngot+1));
  TEST(camstr->dtype=calloc(sizeof(char),ncam-ncamSL240+3+ngot+1));
  TEST(camstr->nbytes=calloc(sizeof(int),ncam-ncamSL240+3+ngot+1));
  camstr->nReorders=ngot;
  for(i=0; i<ncam; i++){
    if(pthread_cond_init(&camstr->camCond[i],NULL)!=0){
      printf("Error initialising condition variable %d\n",i);
      dofree(camstr);
      *camHandle=NULL;
      return 1;
    }
    if(pthread_cond_init(&camstr->camCond2[i],NULL)!=0){
      printf("Error initialising condition variable2 %d\n",i);
      dofree(camstr);
      *camHandle=NULL;
      return 1;
    }
    if(pthread_mutex_init(&camstr->camMutex[i],NULL)!=0){
      printf("Error initialising mutex variable\n");
      dofree(camstr);
      *camHandle=NULL;
      return 1;
    }
  }
#ifdef RESYNC
  if(pthread_mutex_init(&camstr->m,NULL)!=0){
    printf("Error initialising mutex variable\n");
    dofree(camstr);
    *camHandle=NULL;
    return 1;
  }
  if(pthread_cond_init(&camstr->thrcond,NULL)!=0){
    printf("Error initialising condition variable3\n");
    dofree(camstr);
    *camHandle=NULL;
    return 1;
  }
#endif
  if((camstr->DMAbuf=malloc(ncam*sizeof(int*)))==NULL){
    printf("Couldn't allocate DMA buffer\n");
    dofree(camstr);
    *camHandle=NULL;
    return 1;
  }
  printf("memset dmabuf\n");
  memset(camstr->DMAbuf,0,sizeof(int*)*ncam);
  printf("doingf dmabuf\n");
  for(i=0; i<ncamSL240; i++){
    if((camstr->DMAbuf[i]=malloc((sizeof(int)*camstr->npxlsArr[i]+HDRSIZE)*NBUF))==NULL){
      printf("Couldn't allocate DMA buffer %d\n",i);
      dofree(camstr);
      *camHandle=NULL;
      return 1;
    }
    printf("memset dmabuf...\n");
    memset(camstr->DMAbuf[i],0,(sizeof(int)*camstr->npxlsArr[i]+HDRSIZE)*NBUF);
  }
  printf("done dmabuf\n");
  arv_g_thread_init (NULL);//depreciated since 2.32
  arv_g_type_init ();//depreciated since 2.36

  if(camNewParam(*camHandle,pbuf,frameno,arr)!=0){
    printf("Error in camOpen->newParam...\n");
    dofree(camstr);
    *camHandle=NULL;
    return 1;
  }
  printf("Reorders:\n");
  for(i=0; i<ncam; i++)
    printf("%d %p\n",camstr->reorder[i],camstr->reorderBuf[i]);
#ifndef NOSL240
  camstr->handle=malloc(sizeof(nslHandle)*ncam);
  memset(camstr->handle,0,sizeof(nslHandle)*ncam);
  //Now do the SL240 stuff...
  for(i=0; i<ncamSL240; i++){
    status = nslOpen(camstr->fibrePort[i], &camstr->handle[i]);
    if (status != NSL_SUCCESS) {
      printf("Failed to open SL240 port %d: %s\n\n",camstr->fibrePort[i], nslGetErrStr(status));
      dofree(camstr);
      *camHandle=NULL;
      return 1;
    }
    camstr->sl240Opened[i]=1;
    status = nslGetDeviceInfo(&camstr->handle[i], &camstr->info);
    if (status != NSL_SUCCESS) {
      printf("Failed to get SL240 device info: ");
      dofree(camstr);
      *camHandle=NULL;
      return 1;
    }
    printf("\n\nSL240 Device info:\n");
    printf("Unit no.\t %d\n", camstr->info.unitNum);
    printf("Board loc.\t %s\n", camstr->info.boardLocationStr);
    printf("Serial no.\t 0x%x.%x\n", camstr->info.serialNumH, camstr->info.serialNumL);
    printf("Firmware rev.\t 0x%x\n", camstr->info.revisionID);
    printf("Driver rev.\t %s\n", camstr->info.driverRevisionStr);
    printf("Fifo size\t %dM\n", camstr->info.popMemSize/0x100000);
    printf("Link speed\t %d MHz\n", camstr->info.linkSpeed);
    printf("No. links\t %d\n\n\n", camstr->info.numLinks);
    //set up card state.
    status = nslSetState(&camstr->handle[i], NSL_EN_EWRAP, 0);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    status = nslSetState(&camstr->handle[i],NSL_EN_RECEIVE, 1);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    status = nslSetState(&camstr->handle[i],NSL_EN_RETRANSMIT, 0);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    status = nslSetState(&camstr->handle[i],NSL_EN_CRC, 1);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    status = nslSetState(&camstr->handle[i],NSL_EN_FLOW_CTRL, 0);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    status = nslSetState(&camstr->handle[i],NSL_EN_LASER, 1);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    status = nslSetState(&camstr->handle[i],NSL_EN_BYTE_SWAP, 0);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    status = nslSetState(&camstr->handle[i],NSL_EN_WORD_SWAP, 0);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    status = nslSetState(&camstr->handle[i], NSL_STOP_ON_LNK_ERR, 1);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    status = nslSetState(&camstr->handle[i],NSL_EN_RECEIVE, 1);
    if (status != NSL_SUCCESS){
      printf("%s\n",nslGetErrStr(status));dofree(camstr);*camHandle=NULL;return 1;}
    clearReceiveBuffer(camstr,i);

  }
  printf("done nsl\n");

#endif

  camstr->open=1;
  for(i=0; i<ncam; i++){
    ((ThreadStruct*)camstr->thrStruct)[i].camNo=i;
    ((ThreadStruct*)camstr->thrStruct)[i].camstr=camstr;
    if(i<camstr->ncamSL240)
      pthread_create(&camstr->threadid[i],NULL,workerSL240,&((ThreadStruct*)camstr->thrStruct)[i]);
    else{
      //pthread_create(&camstr->threadid[i],NULL,workerAravis,&((ThreadStruct*)camstr->thrStruct)[i]);
      if(startCamera(camstr,i)){//starts a new thread
	printf("Error opening camera %s\n",camstr->camNameList[i]);
	dofree(camstr);
	*camHandle=NULL;
	return 1;
      }
    }
  }
  printf("created threads (%d)\n",ncam);

  return 0;
}


/**
   Close a camera of type name.  Args are passed in the int32 array of size n, and state data is in camHandle, which should be freed and set to NULL before returning.
*/
int camClose(void **camHandle){
  CamStruct *camstr;
  int i;
  printf("Closing camera\n");
  if(*camHandle==NULL)
    return 1;
  camstr=(CamStruct*)*camHandle;
  camstr->open=0;
  for(i=0; i<camstr->ncam; i++){
    pthread_mutex_lock(&camstr->camMutex[i]);
    pthread_cond_broadcast(&camstr->camCond[i]);
    pthread_mutex_unlock(&camstr->camMutex[i]);
  }
  for(i=0; i<camstr->ncam; i++){
    if(i<camstr->ncamSL240)
      pthread_join(camstr->threadid[i],NULL);//wait for worker thread to complete
    else
      stopCamera(camstr,i);
  }
  dofree(camstr);
  *camHandle=NULL;
  printf("Camera closed\n");
  return 0;
}
int writeCamMemory(CamStruct *camstr,int cam,unsigned int addr,void *data,unsigned int len){
  int rt=0; 
  int deviceOpened=0;
  ArvDevice *device;
  if(camstr->camera[cam]==NULL){
    device=arv_open_device (camstr->camNameList[cam]);
    deviceOpened=1;
  }else{
    //device[i] = arv_open_device (camNameList[i]);
    device=arv_camera_get_device(camstr->camera[cam]);
  }
  if (!ARV_IS_DEVICE (device)) {
    printf ("Device '%s' not found\n", camstr->camNameList[cam]);
    rt=1;
  }else{
    printf("Writing %d bytes at address %#x for camera %d\n",len,addr,cam);
    arv_device_write_memory(device,addr,len,data,NULL);
  }
  if(deviceOpened)
    g_object_unref(device);
  return rt;
}

int sendCamCommand(CamStruct *camstr,int i,char *thecmd){
  //First, close cameras if they're already open.
  //Then send command
  //Then, reopen cameras if closed.
  int rt=0;
  //char *cmd=camstr->prevCmd[i];//commands in formate name=val;name=val...;
  char *cmdptr=strdup(thecmd);
  char *cmd=cmdptr;
  char *next,*name,*val;
  GError *err=NULL;
  int deviceOpened=0;
  ArvDevice *device;
  ArvGcNode *feature;
  printf("camstr->camera[%d]==%p, cmd %s\n",i,camstr->camera[i],cmd);
  if(camstr->camera[i]==NULL){
    device=arv_open_device (camstr->camNameList[i]);
    deviceOpened=1;
  }else{
    //device[i] = arv_open_device (camNameList[i]);
    device=arv_camera_get_device(camstr->camera[i]);
  }
  if (!ARV_IS_DEVICE (device)) {
    printf ("Device '%s' not found\n", camstr->camNameList[i]);
    rt=1;
  }else{
    while((next=strchr(cmd,';'))!=NULL){
      next[0]='\0';
      name=cmd;
      val=strchr(name,'=');
      if(val!=NULL){
	val[0]='\0';
	val++;
	printf("Sending: %s = %s\n",name,val);
      }else
	printf("Sending: %s\n",name);
      feature=arv_device_get_feature (device, name);
      if (!ARV_IS_GC_FEATURE_NODE (feature)){
	if(name[0]=='R' && name[1]=='['){
	  guint32 value;
	  guint32 address;
	  address=(unsigned int)strtol(&name[2],NULL,0);
	  printf("Address %#x\n",address);
	  if(val!=NULL){
	    value=(unsigned int)strtol(val,NULL,0);
	    arv_device_write_register(device,address,value, NULL);
	  }
	  arv_device_read_register(device,address,&value,NULL);
	  printf("Got: R[0x%08x] = 0x%08x\n",address,value);
	}else{
	  printf("Feature '%s' not found\n",name);
	}
      }else{
	if (ARV_IS_GC_COMMAND (feature)) {
	  arv_gc_command_execute (ARV_GC_COMMAND (feature), NULL);
	  printf ("%s executed\n",name);
	}else{
	  if(val!=NULL){
	    arv_gc_feature_node_set_value_from_string (ARV_GC_FEATURE_NODE (feature),val,&err);
	    if(err!=NULL){
	      printf("Error %d: %s\n",i,err->message);
	      g_error_free(err);
	      err=NULL;
	      rt=1;
	    }
	  }
	  //Now get the values to report back (useful, but not essential)...
	  if(ARV_IS_GC_STRING(feature))
	    printf ("%s = %s\n", name,arv_gc_string_get_value (ARV_GC_STRING (feature), NULL));
	  else if (ARV_IS_GC_FLOAT (feature))
	    printf ("%s = %g (min:%g-max:%g)\n", name,arv_gc_float_get_value (ARV_GC_FLOAT (feature), NULL),arv_gc_float_get_min (ARV_GC_FLOAT (feature), NULL),arv_gc_float_get_max (ARV_GC_FLOAT (feature), NULL));
	  else if (ARV_IS_GC_INTEGER (feature))
	    printf ("%s = %" G_GINT64_FORMAT " (min:%" G_GINT64_FORMAT "-max:%" G_GINT64_FORMAT ")\n", name,arv_gc_integer_get_value (ARV_GC_INTEGER (feature), NULL),arv_gc_integer_get_min (ARV_GC_INTEGER (feature), NULL),arv_gc_integer_get_max (ARV_GC_INTEGER (feature), NULL));
	  else if (ARV_IS_GC_BOOLEAN(feature))
	    printf("%s = %s\n",name,(arv_gc_boolean_get_value(ARV_GC_BOOLEAN(feature),NULL)==0?"false":"true"));
	  else
	    printf ("%s = %s\n", name,arv_gc_feature_node_get_value_as_string (ARV_GC_FEATURE_NODE (feature),NULL));
	}
      }
      cmd=next+1;
    }
  }
  printf("Commands sent\n");
  if(deviceOpened)
    g_object_unref(device);
  //if(restart)
  // startCamera(camstr,i);
  free(cmdptr);
  return rt;
}

int getCamValue(CamStruct *camstr,char* nameres,int nbytes){
  //nameres is expected to start with '?cam:', and upon return will hold the result 
  int rt=0;
  int cam;
  char *name;
  int deviceOpened=0;
  ArvDevice *device;
  ArvGcNode *feature;
  const char *res;
  gint64 ires;
  double fres;
  if(nbytes<3 || nameres[0]!='?')
    return 1;
  cam=atoi(&nameres[1]);
  if(cam<camstr->ncamSL240 || cam>=camstr->ncam){
    printf("Illegal camera number %d specified for aravisGet - ignoring\n",cam);
    return 1;
  }
  if((name=strchr(nameres,':'))==NULL)
    return 1;
  name++;//move to start of parameter.
  if(camstr->camera[cam]==NULL){
    device=arv_open_device (camstr->camNameList[cam]);
    deviceOpened=1;
  }else{
    //device[cam] = arv_open_device (camNameList[cam]);
    device=arv_camera_get_device(camstr->camera[cam]);
  }
  if (!ARV_IS_DEVICE (device)) {
    printf ("Device '%s' not found\n", camstr->camNameList[cam]);
    rt=1;
  }else{
    printf("Sending: %s\n",name);
    feature=arv_device_get_feature (device, name);
    if (!ARV_IS_GC_NODE (feature)){
      printf("todo: node getting %s\n",name);
      memset(nameres,0,nbytes);
      snprintf(nameres,nbytes,"Error");
    }else{
      if (ARV_IS_GC_COMMAND (feature)) {
	//arv_gc_command_execute (ARV_GC_COMMAND (feature), NULL);
	printf ("Warning - cannot execute commands using aravisGet (%s)\n",name);
	memset(nameres,0,nbytes);
	snprintf(nameres,nbytes,"Error");
      }else{
	//Now get the values to report back (useful, but not essential)...
	if(ARV_IS_GC_STRING(feature)){
	  res=arv_gc_string_get_value (ARV_GC_STRING (feature), NULL);
	  printf ("%s = %s\n",name, res);
	  memset(nameres,0,nbytes);
	  snprintf(nameres,nbytes,"%s",res);
	}else if (ARV_IS_GC_FLOAT (feature)){
	  fres=arv_gc_float_get_value (ARV_GC_FLOAT (feature), NULL);
	  printf ("%s = %g (min:%g-max:%g)\n", name,fres,arv_gc_float_get_min (ARV_GC_FLOAT (feature), NULL),arv_gc_float_get_max (ARV_GC_FLOAT (feature), NULL));
	  memset(nameres,0,nbytes);
	  snprintf(nameres,nbytes,"%g",fres);
	}else if (ARV_IS_GC_INTEGER (feature)){
	  ires=arv_gc_integer_get_value (ARV_GC_INTEGER (feature), NULL);
	  printf ("%s = %" G_GINT64_FORMAT " (min:%" G_GINT64_FORMAT "-max:%" G_GINT64_FORMAT ")\n", name,ires,arv_gc_integer_get_min (ARV_GC_INTEGER (feature), NULL),arv_gc_integer_get_max (ARV_GC_INTEGER (feature), NULL));
	  memset(nameres,0,nbytes);
	  snprintf(nameres,nbytes,"%" G_GINT64_FORMAT,ires);
	}else if (ARV_IS_GC_BOOLEAN(feature)){
	  res=(arv_gc_boolean_get_value(ARV_GC_BOOLEAN(feature),NULL)==0?"false":"true");
	  printf("%s = %s\n",name,res);
	  memset(nameres,0,nbytes);
	  snprintf(nameres,nbytes,"%s",res);
	}else{
	  res=arv_gc_feature_node_get_value_as_string (ARV_GC_FEATURE_NODE (feature),NULL);
	  printf ("%s = %s\n", name,res);
	  memset(nameres,0,nbytes);
	  snprintf(nameres,nbytes,"%s",res);
	}
      }
    }
  }
  if(deviceOpened)
    g_object_unref(device);
  return rt;
}




/**
   New parameters in the buffer (optional)...
*/
int camNewParam(void *camHandle,paramBuf *pbuf,unsigned int frameno,arrayStruct *arr){
  //the only param needed is camReorder if reorder!=0, aravisCmd* aravisGet, aravisMem
  int i,j,cam;
  CamStruct *camstr=(CamStruct*)camHandle;
  int err=0;
  unsigned int addr;
  unsigned int len;
  void *data;
  printf("camNewParam\n");
  bufferGetIndex(pbuf,camstr->ncam-camstr->ncamSL240+3+camstr->nReorders+1,camstr->paramNames,camstr->index,camstr->values,camstr->dtype,camstr->nbytes);
  memset(camstr->reorderBuf,0,camstr->ncam*sizeof(int*));
  for(i=0;i<camstr->ncam-camstr->ncamSL240+3+camstr->nReorders+1;i++){
    printf("%16s: Index %d\n",&camstr->paramNames[i*BUFNAMESIZE],camstr->index[i]);
    if(camstr->index[i]>=0){
      if(i<camstr->ncam-camstr->ncamSL240+3){//aravis*
	if(camstr->nbytes[i]==0){
	  printf("Unsetting command %16s\n",&camstr->paramNames[i*BUFNAMESIZE]);
	  if(camstr->prevCmd[i]!=NULL)
	    free(camstr->prevCmd[i]);
	  camstr->prevCmd[i]=NULL;
	}else{//&& camstr->nbytes[i]>0 && camstr->dtype[i]=='s'){//params found.
	  if(!strncmp("aravisCmd",&camstr->paramNames[i*BUFNAMESIZE],9)){//a command...
	    if(camstr->prevCmd[i]==NULL || strncmp(camstr->prevCmd[i],camstr->values[i],camstr->nbytes[i])!=0 || strlen(camstr->prevCmd[i])!=camstr->nbytes[i]-1){
	      if(camstr->prevCmd[i]!=NULL)
		free(camstr->prevCmd[i]);
	      camstr->prevCmd[i]=strndup(camstr->values[i],camstr->nbytes[i]);
	      if(!strncmp("aravisCmdAll",&camstr->paramNames[i*BUFNAMESIZE],12)){
		printf("Calling sendCamCommand for all cameras, cmd %s\n",camstr->prevCmd[i]);
		for(j=camstr->ncamSL240;j<camstr->ncam;j++)
		  err|=sendCamCommand(camstr,j,camstr->prevCmd[i]);
	      }else{
		cam=atoi(&camstr->paramNames[i*BUFNAMESIZE+9]);
		printf("Calling sendCamCommand for cam %d, cmd %s\n",cam,camstr->prevCmd[i]);
		err=sendCamCommand(camstr,cam,camstr->prevCmd[i]);
	      }
	    }
	  }else if(!strcmp("aravisGet",&camstr->paramNames[i*BUFNAMESIZE])){
	    if(camstr->nbytes[i]>3 && ((char*)(camstr->values[i]))[0]=='?'){
	      //requesting a value. Format is ?cam:parameter where cam is the camera number (starting from ncamSL240 for first aravis camera) and parameter is the parameter to get.  Replaces the string with the result.
	      getCamValue(camstr,(char*)camstr->values[i],camstr->nbytes[i]);
	    }
	  }else if(!strcmp("aravisMem",&camstr->paramNames[i*BUFNAMESIZE])){
	    if(camstr->nbytes[i]>8){
	      //Write some memory in one of the cameras.  The first element is the camera number (4 bytes), next 4 bytes is the address, followed by the data...
	      cam=((int*)(camstr->values[i]))[0];
	      ((int*)(camstr->values[i]))[0]=-1;//and unset, so that don't write in the next buffer swap...
	      if(cam>=camstr->ncamSL240 && cam<camstr->ncam){
		addr=((unsigned int*)(camstr->values[i]))[1];
		data=&(((char*)(camstr->values[i]))[2*sizeof(int)]);
		len=camstr->nbytes[i]-2*sizeof(int);
		writeCamMemory(camstr,cam,addr,(void*)data,len);
	      }else{
		printf("aravisMem invalid argument\n");
	      }
	    }
	  }
	}
      }else if(i<camstr->ncam-camstr->ncamSL240+3+camstr->nReorders){//camReorder
	if(camstr->nbytes[i]>0){
	  if(camstr->dtype[i]=='i'){
	    //for which camera(s) is this?
	    for(j=0; j<camstr->ncam; j++){
	      if(camstr->reorderIndx[j]==i){//a reorder for this camera
		if(camstr->nbytes[i]==sizeof(int)*camstr->npxlsArr[j]){
		  camstr->reorderBuf[j]=(int*)camstr->values[i];
		}else{
		  printf("Wrong size for camReorder\n");
		  err=1;
		}
	      }
	    }
	  }else{
	    printf("Wrong dtype for camReorder\n");
	    err=1;
	  }
	}
      }else{//recordTimestamp
	if(camstr->nbytes[i]==sizeof(int) && camstr->dtype[i]=='i'){
	  camstr->recordTimestamp=*((int*)camstr->values[i]);
	  printf("recordTimestamp %d\n",camstr->recordTimestamp);
	}else{
	  printf("Wrong type/size for recordTimestamp\n");
	}
      }
    }
  }
  printf("Done camNewParam\n");
  return err;
}


/**
   Called when we're starting processing the next frame.  This doesn't actually wait for any pixels.
*/
int camNewFrameSync(void *camHandle,unsigned int thisiter,double starttime){
  //printf("camNewFrame\n");
  CamStruct *camstr;
  int i;
  //int maxf;
  //int extratoread;
  camstr=(CamStruct*)camHandle;
  if(camHandle==NULL){
    return 1;
  }
  //pthread_mutex_lock(&camstr->m);
  camstr->thisiter=thisiter;
  //printf("New frame\n");
  //camstr->newframeAll=1;
  for(i=0;i<camstr->ncam; i++)
    camstr->newframe[i]=1;
  //pthread_mutex_unlock(&camstr->m);
  return 0;
}

/**
   Wait for the next n pixels of the current frame to arrive from camera cam.
   Note - this can be called by multiple threads at same time.  Need to make sure its thread safe.

*/

int camWaitPixels(int n,int cam,void *camHandle){
  //printf("camWaitPixels %d, camera %d\n",n,cam);
  CamStruct *camstr=(CamStruct*)camHandle;
  int rt=0;
  int i,j;
  int amp,rl,tb,pxl,x,y;
  int gotNewFrame;
  //static struct timeval t1;
  //struct timeval t2;
  //struct timeval t3;
  //printf("camWaitPixels %d %d\n",n,cam);
  if(camHandle==NULL){
    //printf("called camWaitPixels with camHandle==NULL\n");
    return 1;
  }
  if(n<0)
    n=0;
  if(n>camstr->npxlsArr[cam])
    n=camstr->npxlsArr[cam];
  //printf("camWaitPixels\n");
  pthread_mutex_lock(&camstr->camMutex[cam]);
  //printf("camWaitPixels got mutex, newframe=%d\n",camstr->newframe[cam]);
  if(camstr->newframe[cam]){//first thread for this camera after new frame...
    camstr->newframe[cam]=0;
    camstr->frameReady[cam]=0;
    camstr->pxlShift[cam*2]=0;
    camstr->pxlShift[cam*2+1]=0;
    camstr->setFrameNo[cam]=1;
    camstr->pxlsTransferred[cam]=0;
    gotNewFrame=0;
    if(cam<camstr->ncamSL240){//this is waiting for sl240 pixels...
      while(gotNewFrame==0){
	if(camstr->curframe[cam]!=-1 && camstr->curframe[cam]!=camstr->transferframe[cam]){
	  //have a new buffer currently reading out
	  camstr->transferframe[cam]=camstr->curframe[cam];
	  camstr->curframe[cam]=-1;
	  if(camstr->latestframe[cam]!=-1)
	    printf("Skipping frame for cam %d\n",cam);
	  camstr->latestframe[cam]=-1;
	  gotNewFrame=1;
	}else if(camstr->curframe[cam]==-1 && camstr->latestframe[cam]!=-1 && camstr->latestframe[cam]!=camstr->transferframe[cam]){
	  //have a full buffer waiting for processing.
	  camstr->transferframe[cam]=camstr->latestframe[cam];
	  camstr->latestframe[cam]=-1;
	  gotNewFrame=1;
	}else{//wait for next frame to start.

	  /*	if(camstr->latestframe[cam]!=-1 && camstr->latestframe[cam]!=camstr->transferframe[cam]){
	  //have a full buffer waiting for processing.
	  camstr->transferframe[cam]=camstr->latestframe[cam];
	  camstr->latestframe[cam]=-1;
	  gotNewFrame=1;
	}else if(camstr->curframe[cam]!=-1 && camstr->curframe[cam]!=camstr->transferframe[cam]){
	  //have a new buffer currently reading out
	  camstr->transferframe[cam]=camstr->curframe[cam];
	  camstr->curframe[cam]=-1;
	  camstr->latestframe[cam]=-1;
	  gotNewFrame=1;*/
	  //what should we do about errors here?  
	  //We don't care about errors here.  Since we're waiting for a new frame, it means we've completed previous frames.  So, can simply reset camErr to zero.
	  camstr->waiting[cam]=1;
	  pthread_cond_wait(&camstr->camCond[cam],&camstr->camMutex[cam]);
	}
	camstr->camErr[cam]=0;//we're waiting for a new frame - so don't care about errors - and if a frame has an error it won't appear on curframe, or latestframe, so we're okay to ignore.
      }
      //wake the other threads for this camera that are waiting
      //camstr->userFrameNo[cam]=xxx;
      camstr->frameReady[cam]=1;
      pthread_cond_broadcast(&camstr->camCond2[cam]);
    }else{//wait for aravis pixels
      while(gotNewFrame==0){
	if(camstr->currentFilling[cam]!=NULL && camstr->currentFilling[cam]!=camstr->rtcReading[cam]){
	  //havea new buffer currently reading out.
	  camstr->rtcReading[cam]=camstr->currentFilling[cam];
	  camstr->currentFilling[cam]=NULL;
	  if(camstr->mostRecentFilled[cam]!=NULL)
	    printf("Skilling frame for cam %d\n",cam);
	  camstr->mostRecentFilled[cam]=NULL;
	  gotNewFrame=1;
	}else if(camstr->currentFilling[cam]==NULL && camstr->mostRecentFilled[cam]!=NULL && camstr->mostRecentFilled[cam]!=camstr->rtcReading[cam]){
	  //have a full buffer waiting for processing/
	  camstr->rtcReading[cam]=camstr->mostRecentFilled[cam];
	  camstr->mostRecentFilled[cam]=NULL;
	  gotNewFrame=1;
	}else{

	  /*if(camstr->mostRecentFilled[cam]!=NULL && camstr->mostRecentFilled[cam]!=camstr->rtcReading[cam]){
	  //have a full buffer waiting for processing.
	  //printf("Setting rtcReading to mostRecentFilled, and mostRecentFilled to NULL\n");
	  camstr->rtcReading[cam]=camstr->mostRecentFilled[cam];
	  camstr->mostRecentFilled[cam]=NULL;
	  gotNewFrame=1;
	}else if(camstr->currentFilling[cam]!=NULL && camstr->currentFilling[cam]!=camstr->rtcReading[cam]){
	  //have a new buffer currently reading out
	  camstr->rtcReading[cam]=camstr->currentFilling[cam];
	  camstr->currentFilling[cam]=NULL;
	  camstr->mostRecentFilled[cam]=NULL;
	  //printf("setting rtcReading to currentFilling and moreRecentFilled to NULL\n");
	  gotNewFrame=1;
	  }else{*/
	  //wait for the next frame to start.
	  //What should we do about errors here?
	  //We don't care about errors here.  Since we're waiting for a new frame, it means that we've completed previous frames.  So, we can simple reset camErr to zero.
	  camstr->waiting[cam]=1;
	  //printf("waiting for sof\n");
	  pthread_cond_wait(&camstr->camCond[cam],&camstr->camMutex[cam]);
	}
	camstr->camErr[cam]=0;//we're waiting for new frame - so don't care about errors.  And if a frame has an error, it won't appear on currentFilling, or mostRecentFilled, so we're ok to ignore.
      }
      //wake the other threads for this camera that are waiting.
      camstr->userFrameNo[cam]=camstr->rtcReading[cam]->frame_id;
      camstr->frameReady[cam]=1;
      //printf("Broadcasting frameReady\n");
      pthread_cond_broadcast(&camstr->camCond2[cam]);
    }

  }else{//we're not the first thread for this frame/camera
    while(camstr->frameReady[cam]==0){
      pthread_cond_wait(&camstr->camCond2[cam],&camstr->camMutex[cam]);
    }
  }
  if(cam<camstr->ncamSL240){//this is a SL240 camera...
    if(camstr->transferframe[cam]==-1){//no current frame...
      rt=1;
      camstr->setFrameNo[cam]=0;
    }
    while(camstr->pxlcnt[NBUF*cam+camstr->transferframe[cam]]<n && rt==0 && camstr->camErr[cam]==0){
      camstr->waiting[cam]=1;
      pthread_cond_wait(&camstr->camCond[cam],&camstr->camMutex[cam]);
      rt=camstr->err[NBUF*cam+camstr->transferframe[cam]];
    }
    if(camstr->setFrameNo[cam]){//save the frame counter...
      camstr->setFrameNo[cam]=0;
      camstr->userFrameNo[cam]=*((unsigned int*)(&(camstr->DMAbuf[cam][(camstr->transferframe[cam])*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(int))])));
      //printf("frameno %d\n",camstr->userFrameNo[cam]);
    }
    //now copy the data.
    if(n>camstr->pxlsTransferred[cam] && rt==0){
      if(camstr->pxlRowStartSkipThreshold!=0 || camstr->pxlRowEndInsertThreshold!=0){
	int pxlno;
	if(camstr->reorder[cam]!=0){
	  printf("Warning - pixel reordering not implemented yet with pxlRowStartSkipThreshold or pxlRowEndInsertThreshold - if you need this please recode camera interface\n");
	}
	for(i=camstr->pxlsTransferred[cam]; i<n; i++){
	  pxlno=camstr->npxlsArrCum[cam]+i+camstr->pxlShift[cam*2+i%2];
	  camstr->imgdata[pxlno]=(unsigned short)(camstr->DMAbuf[cam][(camstr->transferframe[cam])*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(int))+HDRSIZE/sizeof(int)+i]);
	  //check the first pixel (of each camera - there are 2 cameras in each cam interface).
	  if(camstr->pxlRowStartSkipThreshold!=0 && ((i+camstr->pxlShift[cam*2+i%2])%camstr->pxlx[cam])==i%2 && camstr->imgdata[pxlno]<camstr->pxlRowStartSkipThreshold){
	    camstr->pxlShift[cam*2+i%2]--;//if the dark pixel is in first colum, need to remove a pixel.
	    printf("Removing pixel at frame %u cam %d i %d\n",camstr->thisiter,cam,i);
	  }else if(camstr->pxlRowEndInsertThreshold!=0 && ((i+camstr->pxlShift[cam*2+i%2])%camstr->pxlx[cam])==camstr->pxlx[cam]-4+i%2 && camstr->imgdata[pxlno]<camstr->pxlRowEndInsertThreshold){//If the dark pixel is in the 2nd last column, need to add a pixel (it should fall in the last column with the andors).
	    camstr->pxlShift[cam*2+i%2]++;
	    camstr->imgdata[pxlno+1]=camstr->imgdata[pxlno];
	    camstr->imgdata[pxlno]=camstr->pxlRowEndInsertThreshold;
	    printf("Inserting pixel at frame %u cam %d i %d\n",camstr->thisiter,cam,i);
	  }
	}
	if(n==camstr->npxlsArr[cam]){//requesting the last pixels...
	//so, if we've lost any pixels, insert the last ones with the pixel threshold value.
	  for(i=n-camstr->pxlShift[cam*2]*2; i<n; i+=2){
	    camstr->imgdata[camstr->npxlsArrCum[cam]+i]=camstr->pxlRowStartSkipThreshold;
	  }
	  for(i=n-camstr->pxlShift[cam*2+1]*2+1; i<n; i+=2){
	    camstr->imgdata[camstr->npxlsArrCum[cam]+i]=camstr->pxlRowStartSkipThreshold;
	  }
	}
      }else{
	if(camstr->reorder[cam]==0){//no pixel reordering
	  //printf("Cam %d transferring up to %d, transferFrame %d\n",cam,n,camstr->transferframe[cam]);
	  for(i=camstr->pxlsTransferred[cam]; i<n; i++){
	    camstr->imgdata[camstr->npxlsArrCum[cam]+i]=(unsigned short)(camstr->DMAbuf[cam][(camstr->transferframe[cam])*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(int))+HDRSIZE/sizeof(int)+i]);
	  }
	}else{
	  if(camstr->reorderBuf[cam]!=NULL){//reordering based on parameter buffer
	  //Note - todo - need to create reorderbuf and use camNewParam()...
	  //Look for a parameter called "camReorder%d" where %d is reorder[cam]
	    for(i=camstr->pxlsTransferred[cam]; i<n; i++){
	      camstr->imgdata[camstr->npxlsArrCum[cam]+camstr->reorderBuf[cam][i]]=(unsigned short)(camstr->DMAbuf[cam][(camstr->transferframe[cam])*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(int))+HDRSIZE/sizeof(int)+i]);
	    }
	  }else if(camstr->reorder[cam]==1){//this is a standard reorder for scimeasure CCID18 (CANARY LGS) specific reordering
	    for(i=camstr->pxlsTransferred[cam]; i<n; i++){
	    //i%2 specifies whether bottom or top half.
	    //(i//2)%8 specifies which quadrant it is in.
	    //i//16 specifies which pixel it is in the 64x16 pixel quadrant.
	    //row==i//256
	    //col=(i//16)%16
	    //Assumes a 128x128 pixel detector...
	      j=(i%2?127-i/256:i/256)*128+((i/2)%8)*16+(i/16)%16;
	      camstr->imgdata[camstr->npxlsArrCum[cam]+j]=(unsigned short)(camstr->DMAbuf[cam][(camstr->transferframe[cam])*(camstr->npxlsArr[cam]+HDRSIZE/sizeof(int))+HDRSIZE/sizeof(int)+i]);
	    }
	  }//else if(camstr->reorder[cam]==2){}//can specify other reorderings here
	}
      }
      //printf("pxlsTransferred[%d]=%d\n",cam,n);
      camstr->pxlsTransferred[cam]=n;
    }
    //Fix for a camera bug.  Test the last pixels to see if they are zero, if not, raise an error.  Note, only make this test if pxlRowStartSkipThreshold==0, because otherwise, we have already applied some sort of correction
    if(n==camstr->npxlsArr[cam] && camstr->testLastPixel[cam]!=0 && camstr->pxlRowStartSkipThreshold==0){
      for(i=camstr->npxlsArr[cam]-camstr->testLastPixel[cam]; i<camstr->npxlsArr[cam];i++){
	if(camstr->imgdata[camstr->npxlsArrCum[cam]+i]!=0){
	  rt|=1;
	  printf("non-zero final pixel %d - glitch at about frame %u, cam %d i %d\n",camstr->imgdata[camstr->npxlsArrCum[cam]+i],camstr->thisiter,cam,i);
	}
      }
    }
  }else{//this is an aravis camera...
    while(camstr->rtcReading[cam]->contiguous_data_received<n*((camstr->bpp[cam]+7)/8) && (rt=camstr->camErr[cam])==0){
    //printf("waiting for data\n");
      camstr->waiting[cam]=1;
      pthread_cond_wait(&camstr->camCond[cam],&camstr->camMutex[cam]);
      //rt=camstr->camErr[cam];
    }
    //camstr->camErr[cam]=0;//reset for next time.  BAD IDEA!
    //Now copy the data.
    if(rt==0 && n>camstr->pxlsTransferred[cam]){
      if(camstr->bytespp==1){//all cameras are <=8 bits per pixel.
	memcpy(&camstr->imgdata[(camstr->npxlsArrCum[cam]+camstr->pxlsTransferred[cam])],&(((char*)camstr->rtcReading[cam]->data)[camstr->pxlsTransferred[cam]]),(n-camstr->pxlsTransferred[cam]));
      }else if(camstr->bytespp==2){
	if(camstr->bpp[cam]<=8){//cast from uint8 to uint16
	  //printf("Copying pixels for cam %d from %d to %d at offset %d\n",cam,camstr->pxlsTransferred[cam],n,camstr->npxlsArrCum[cam]);
	  for(i=camstr->pxlsTransferred[cam];i<n;i++)
	    ((unsigned short*)camstr->imgdata)[camstr->npxlsArrCum[cam]+i]=(unsigned short)(((unsigned char*)(camstr->rtcReading[cam]->data))[i]);
	}else if(camstr->bpp[cam]<=16){//copy
	  if(camstr->reorder[cam]==0){//no reordering
	    memcpy(&camstr->imgdata[(camstr->npxlsArrCum[cam]+camstr->pxlsTransferred[cam])],&(((char*)camstr->rtcReading[cam]->data)[2*camstr->pxlsTransferred[cam]]),2*(n-camstr->pxlsTransferred[cam]));
	  }else if(camstr->reorderBuf[cam]!=NULL){
	    for(i=camstr->pxlsTransferred[cam];i<n;i++){
	      ((unsigned short*)camstr->imgdata)[camstr->npxlsArrCum[cam]+camstr->reorderBuf[cam][i]]=(((unsigned short*)(camstr->rtcReading[cam]->data))[i]);
	    }
	  }else if(camstr->reorder[cam]==1){//specific reorder for scimeasure CCID18 (CANARY LGS)
	    for(i=camstr->pxlsTransferred[cam];i<n;i++){
	      j=(i%2?127-i/256:i/256)*128+((i/2)%8)*16+(i/16)%16;
	      ((unsigned short*)camstr->imgdata)[camstr->npxlsArrCum[cam]+j]=(((unsigned short*)(camstr->rtcReading[cam]->data))[i]);
	    }
	  }else if(camstr->reorder[cam]==2){//specific reorder for OCAM2
	    //Should be 63888 pixels (121*1056/2 -> 264x242).  Each quadrant is 66x121 pixels
	    for(i=camstr->pxlsTransferred[cam]; i<n; i++){
	      amp=i%8;//the amplifier (quadrant) in question    
	      rl=1-i%2;//right to left
	      tb=1-amp/4;//top to bottom amp (0,1,2,3)?
	      pxl=i/8;//the pixel within a given quadrant
	      x=(tb*2-1)*(((1-2*rl)*(pxl%66)+rl*65)+66*amp)+(1-tb)*(66*8-1);
	      y=(1-tb)*241+(2*tb-1)*(pxl/66);
	      j=y*264+x;
	      ((unsigned short*)camstr->imgdata)[camstr->npxlsArrCum[cam]+j]=(((unsigned short*)(camstr->rtcReading[cam]->data))[i]);
	    }
	    
	  }else if(camstr->reorder[cam]==3){//specific reorder for OCAM2 without keeping the overscan regions.
	    //Should be 63888 pixels coming in (121*1056/2 -> 264x242).  Each quadrant is 66x121 pixels.  Then here, convert to 240x240 (inset in the 264x242).  Remove first 6 pixels of each row of each quad, and last row.
	    for(i=camstr->pxlsTransferred[cam]; i<n; i++){
	      pxl=i/8;//the pixel within a given quadrant
	      if(pxl%66>5 && pxl<66*120){//not an overscan pixel
		amp=i%8;//the amplifier (quadrant) in question    
		rl=1-i%2;//right to left
		tb=1-amp/4;//top to bottom amp (0,1,2,3)?
		x=(tb*2-1)*(((1-2*rl)*(pxl%66-6)+rl*59)+60*amp)+(1-tb)*(60*8-1);
		y=(1-tb)*239+(2*tb-1)*(pxl/66);
		j=y*264+x;
		((unsigned short*)camstr->imgdata)[camstr->npxlsArrCum[cam]+j]=(((unsigned short*)(camstr->rtcReading[cam]->data))[i]);
	      }
	    }
	  }
	}else{
	  printf("Can't yet handle >16 bits per pixel in camAravis - please recode\n");
	}
      }else{
	printf("Unknown bytes per pixel %d in camAravis\n",camstr->bytespp);
      }
      camstr->pxlsTransferred[cam]=n;
    }


  }
  if(rt!=0){
    printf("camWaitPixels got err %d (cam %d) %ld %ld, frame[0] %d [%d] %d\n",rt,cam,(long)camstr->transferframe[cam],(long)camstr->curframe[cam],camstr->userFrameNo[0],cam,camstr->userFrameNo[cam]);
  }
  pthread_mutex_unlock(&camstr->camMutex[cam]);
  return rt;
}
int camFrameFinishedSync(void *camHandle,int err,int forcewrite){//subap thread (once)
  int i;
  CamStruct *camstr=(CamStruct*)camHandle;
 

  if(camstr->recordTimestamp){//an option to put camera frame number as us time
    for(i=0; i<camstr->ncam; i++){
      if(i<camstr->ncamSL240)
	camstr->userFrameNo[i]=*((unsigned int*)(&(camstr->DMAbuf[i][(camstr->transferframe[i])*(camstr->npxlsArr[i]+HDRSIZE/sizeof(int))])));
      else
	camstr->userFrameNo[i]=camstr->timestamp[i].tv_sec*1000000+camstr->timestamp[i].tv_usec;
    }
  }else{
    for(i=1; i<camstr->ncam; i++){
      if(camstr->userFrameNo[0]!=camstr->userFrameNo[i]){
	
	writeErrorVA(camstr->rtcErrorBuf,CAMSYNCERROR,camstr->thisiter,"Error - camera frames not in sync");
	break;
      }
    }
  }
  return 0;
}
