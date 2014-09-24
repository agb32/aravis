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
*/
//typedef unsigned int uint32;
//#define _GNU_SOURCE
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
//#include <signal.h>
#include <sched.h>

#define NBUF 50


/**
   The struct to hold info.
   If using multi cameras (ie multi SL240 cards or streams), would need to recode, so have multiple instances of this struct.
*/


typedef struct{
  int ncam;//no of cameras
  int npxls;//number of pixels in the frame (total for all cameras)
  //int pxlsRequested;//number of pixels requested by DMA for this frame
  pthread_mutex_t *camMutex;
  pthread_cond_t *camCond;//sync between main RTC
  pthread_cond_t *camCond2;//sync between main RTC
  int *blocksize;//number of bytes to transfer per DMA;
  int open;//set by RTC if the camera is open
  volatile int *waiting;//set by RTC if the RTC is waiting for pixels
  //volatile int newframeAll;//set by RTC when a new frame is starting to be requested.
  volatile int *newframe;
  unsigned int thisiter;
  char *imgdata;//interpret as uint8 or uint16 depending on mode...
  int *pxlsTransferred;//number of pixels copied into the RTC memory.
  unsigned int *userFrameNo;//pointer to the RTC frame number... to be updated for new frame.
  void *thrStruct;//pointer to an array of threadStructs.
  int *npxlsArr;
  int *npxlsArrCum;
  int *threadPriority;
  unsigned int *threadAffinity;
  int threadAffinElSize;
  char *paramNames;
  int *index;
  int *nbytes;
  void **values;
  char *dtype;
  int *readStarted;
  int *pxlx;
  int *pxly;
  int *campxlx;//for aravis - if camera image format different from actual format.
  int *campxly;
  circBuf *rtcErrorBuf;
  int *frameReady;
  int *offsetX;
  int *offsetY;
  int *byteswapInts;
  struct timeval *timestamp;
  int recordTimestamp;//option to use timestamp of last pixel arriving rather than frame number.
  int auto_socket_buffer;
  int socket_buffer_size;
  int no_packet_resend;
  unsigned int packet_timeout;
  unsigned int frame_retention;
  int *bpp;
  int bytespp;

  int *reorder;//is pixel reordering required, and if so, which pattern?
  int **reorderBuf;//pixels for reordering.
  int *reorderIndx;
  int *reorderno;
  int nReorders;

  ArvBuffer **mostRecentFilled;//a completed buffer for each camera
  ArvBuffer **currentFilling;//the buffer that is currently being written to with new camera data
  ArvBuffer **rtcReading;//the buffer that is being used to transfer data to darc
  int *camErr;
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
    for(i=0; i<camstr->ncam; i++){
      pthread_cond_destroy(&camstr->camCond[i]);
      pthread_cond_destroy(&camstr->camCond2[i]);
      pthread_mutex_destroy(&camstr->camMutex[i]);
    }
    safefree(camstr->npxlsArr);
    safefree(camstr->npxlsArrCum);
    safefree(camstr->blocksize);
    safefree(camstr->readStarted);
    safefree((void*)camstr->waiting);
    safefree((void*)camstr->newframe);
    safefree(camstr->pxlsTransferred);
    safefree(camstr->thrStruct);
    safefree(camstr->threadPriority);
    safefree(camstr->threadAffinity);
    safefree(camstr->index);
    safefree(camstr->paramNames);
    safefree(camstr->nbytes);
    safefree(camstr->dtype);
    safefree(camstr->values);
    //safefree(camstr->pxlShift);
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
    safefree(camstr->camErr);
    safefree(camstr->stream);
    safefree(camstr->camera);
    safefree(camstr->camNameList);
    safefree(camstr->bpp);
    safefree(camstr->reorder);
    safefree(camstr->reorderBuf);
    safefree(camstr->reorderno);
    safefree(camstr->reorderIndx);

    safefree(camstr->frameReady);
    safefree(camstr->bufArrList);
    for(i=0;i<camstr->ncam+2;i++)
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
	//notify...
	pthread_mutex_lock(&camstr->camMutex[cam]);
	while(buffer->contiguous_data_received>=buffer->last_data_accessed+camstr->blocksize[cam]){
	  buffer->last_data_accessed+=camstr->blocksize[cam];
	}
	//If this was the last bit of data...:
	if(buffer->contiguous_data_received==buffer->size && camstr->readStarted[cam]==1){//received it all...  for some reason, seem to get 2 calls for this one sometimes?
	//END OF FRAME...
	  if(camstr->recordTimestamp)
	    gettimeofday(&camstr->timestamp[cam],NULL);
	  camstr->readStarted[cam]=0;
	  if(camstr->mostRecentFilled[cam]!=NULL){
	    printf("Skipping [%u %s]: darc not keeping up/diff Hz/lost data\n",camstr->mostRecentFilled[cam]->frame_id,camstr->camNameList[cam]);
	  }else{
	    //printf("ok - not skipping\n");
	  }
	  if(camstr->currentFilling[cam]!=NULL){//not currently being read
	    printf("Pipeline read of cam %d not yet started - dropping frame\n",cam);
	    camstr->mostRecentFilled[cam]=NULL;//buffer;
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
      if(camstr->readStarted[cam]==1){
	//todo("Set error somehow - previous frame didn't get finished");
	//but only if someone has started reading the buffer...
	if(camstr->currentFilling[cam]==NULL){
	  camstr->camErr[cam]=1;//previous frame didn't get finished.
	  camstr->mostRecentFilled[cam]=NULL;//just incase - probably null anyway.
	  printf("Error case - setting mostRecentFilled to NULL\n");
	}
      }
      camstr->currentFilling[cam]=buffer;
      if(camstr->waiting[cam]){//is anyoen waiting?  Tell them about the new buffer.
	camstr->waiting[cam]=0;
	//printf("Broadcasting sof\n");
	pthread_cond_broadcast(&camstr->camCond[cam]);
      }
      pthread_mutex_unlock(&camstr->camMutex[cam]);
    }
    camstr->readStarted[cam]=1;
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
	if(camstr->waiting[cam]){
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
    //    camstr->ntoread[cam]=1;
  }else{//ignore the other callbacks...
  }
}


/*
  }else{
    int bufno;
    for(bufno=0;bufno<NBUF;bufno++){
      if(buffer==myData->bufArr[bufno])
	break;
    }
    if(type==ARV_STREAM_CALLBACK_TYPE_NEW_DATA){
      float frac=buffer->contiguous_data_received/(float)buffer->size;
      //if(frac>0.99)
      //printf("callback %d frac %g bufno %d rec %d\n",type,frac,bufno,buffer->contiguous_data_received);
      if(frac>=buffer->last_data_accessed/(float)buffer->size+0.2 || frac==1){
	buffer->last_data_accessed=buffer->contiguous_data_received;
	//buffer->last_data_accessed=(int)(0.2*((int)(buffer->contiguous_data_received/0.2)));
	//printf("callback %d %d %d %ld %g %p %d %s\n",type,buffer->contiguous_data_received,buffer->last_data_accessed,buffer->size,frac,buffer,bufno,myData->camName);
	//image processing here...
      }
    }else if(type==ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE){
      if(myData!=NULL){
	//myData=(ApplicationData*)user_data;
	//stream=(ArvStream*)(*(ArvStream**)user_data);
	stream=myData->stream;
	//printf("Buffer done %d %s %ld\n",bufno,myData->camName,pthread_self());
	buf = arv_stream_try_pop_buffer (stream);
	if (buf != NULL) {
	  if (buf->status == ARV_BUFFER_STATUS_SUCCESS){
	    myData->buffer_count++;
	    // Image processing here ... Note, for some reason, this seems to get called several frames after the last pixel from the NEW_DATA callback, and so shouldn't really be used.
	  }
	  arv_stream_push_buffer (stream, buf);
	}else{
	  printf("FAILED TO POP BUFFER %d %s %ld...\n",bufno,myData->camName,pthread_self());
	}
      }
      cnt++;
      //printf("Buffer done %d %d %ld %p %p %ld %s %s %d\n",type,buffer->contiguous_data_received,buffer->size,buffer,buf,buffer-buf,buffer==buf?"Same":"Different",myData->prevBuffer==buf?"Same":"Different",bufno);
      myData->prevBuffer=buffer;
      if(cancel){//cnt==100){
	gettimeofday(&t2,NULL);
	printf ("Frame rate = %g Hz %s\n", myData->buffer_count/(t2.tv_sec-myData->t1.tv_sec+1e-6*(t2.tv_usec-myData->t1.tv_usec)),myData->camName);
	if(myData->main_loop!=NULL)
	  g_main_loop_quit(myData->main_loop);
      }
    }else{
      //printf("callback %d %p\n",type,buffer);
    }
  }
}
*/

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
  //ArvBuffer *buffer;
  int i;
  //GMainContext *context=NULL;
  int rt=0;
  //camstr->buffer_count[cam] = 0;
  //camstr->t1[cam].tv_sec=0;
  //camstr->t1[cam].tv_usec=0;
  //camstr->camName=camName;
  //camstr->main_loop=NULL;
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
    //guint software_trigger_source = 0;
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
    //arv_camera_set_binning (camera, arv_option_horizontal_binning, arv_option_vertical_binning);//no binning at the moment
    //arv_camera_set_exposure_time (camera, arv_option_exposure_time_us);
    //arv_camera_set_gain (camera, arv_option_gain);  
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
    //sleep(1);
    //myData.buffer_count=0;
    //data->prevBuffer=NULL;//was myData
    //camstr->prevBuffer[cam]=NULL;
    ((ThreadStruct*)camstr->thrStruct)[cam].camNo=cam;
    ((ThreadStruct*)camstr->thrStruct)[cam].camstr=camstr;
    stream = arv_camera_create_stream (camera, cameraCallback, &((ThreadStruct*)camstr->thrStruct)[cam]);//was myData
    //camInfo->stream=stream;
    camstr->stream[cam]=stream;
    //data->stream=stream;//was myData
    //*streamPtr=stream;
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
      //data->bufArr=bufArr;
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
  int i,j,k;
  unsigned short *tmps;
  char pxlbuftype;
  int maxbpp;
  int ngot;
  int bytespp,camIDLen=0,nn;
  char **camParamName;
  printf("Initialising camera %s\n",name);
  if((*camHandle=malloc(sizeof(CamStruct)))==NULL){
    printf("Couldn't malloc camera handle\n");
    return 1;
  }
  memset(*camHandle,0,sizeof(CamStruct));
  camstr=(CamStruct*)*camHandle;
  TEST(camstr->bpp=calloc(sizeof(int),ncam));
  maxbpp=0;
  if(n>=ncam){
    for(i=0;i<ncam;i++){
      camstr->bpp[i]=args[i];
      if(args[i]>maxbpp)
	maxbpp=args[i];
    }
  }
  bytespp=((maxbpp+7)/8);//save the largest bytes per pixel - this is what the array will be, then any cameras that use fewer bytes will have to be converted.
  camstr->bytespp=bytespp;
  if(bytespp==1){
    pxlbuftype='B';
  }else if(bytespp==2){
    pxlbuftype='H';
  }else{
    printf("%d bytes per pixel not yet coded - please recode camAravis.c\n",bytespp);
    free(*camHandle);
    *camHandle=NULL;
    return 1;
  }
  if(arr->pxlbuftype!=pxlbuftype || arr->pxlbufsSize!=bytespp*npxls){
      //need to resize the pxlbufs...
    arr->pxlbufsSize=bytespp*npxls;
    arr->pxlbuftype=pxlbuftype;
    arr->pxlbufelsize=bytespp;
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
  camstr->imgdata=arr->pxlbufs;
  if(n>5*ncam)
    camstr->threadAffinElSize=args[5*ncam];
  else
    camstr->threadAffinElSize=1;
  //camstr->himgdata=arr->pxlbufs;
  //camstr->bimgdata=arr->pxlbufs;
  if(*camframenoSize<ncam){
    if(*camframeno!=NULL)
      free(*camframeno);
    if((*camframeno=calloc(sizeof(unsigned int),ncam))==NULL){
      printf("Couldn't malloc camframeno\n");
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
  TEST(camstr->readStarted=calloc(ncam,sizeof(int)));
  TEST(camstr->waiting=calloc(ncam,sizeof(int)));
  TEST(camstr->newframe=calloc(ncam,sizeof(int)));
  TEST(camstr->pxlsTransferred=calloc(ncam,sizeof(int)));
  TEST(camstr->thrStruct=calloc(ncam,sizeof(ThreadStruct)));
  TEST(camstr->camMutex=calloc(ncam,sizeof(pthread_mutex_t)));
  TEST(camstr->camCond=calloc(ncam,sizeof(pthread_cond_t)));
  TEST(camstr->camCond2=calloc(ncam,sizeof(pthread_cond_t)));
  TEST(camstr->threadAffinity=calloc(ncam*camstr->threadAffinElSize,sizeof(int)));
  TEST(camstr->threadPriority=calloc(ncam,sizeof(int)));
  TEST(camstr->pxlx=calloc(ncam,sizeof(int)));
  TEST(camstr->pxly=calloc(ncam,sizeof(int)));
  TEST(camstr->campxlx=calloc(ncam,sizeof(int)));
  TEST(camstr->campxly=calloc(ncam,sizeof(int)));
  TEST(camstr->frameReady=calloc(ncam,sizeof(int)));
  TEST(camstr->offsetX=calloc(ncam,sizeof(int)));
  TEST(camstr->offsetY=calloc(ncam,sizeof(int)));
  TEST(camstr->byteswapInts=calloc(ncam,sizeof(int)));
  TEST(camstr->reorder=calloc(ncam,sizeof(int)));
  TEST(camstr->reorderno=calloc(ncam,sizeof(int)));
  TEST(camstr->reorderIndx=calloc(ncam,sizeof(int)));
  TEST(camstr->reorderBuf=calloc(ncam,sizeof(int*)));

  TEST(camstr->timestamp=calloc(ncam,sizeof(struct timeval)));
  TEST(camstr->mostRecentFilled=calloc(ncam,sizeof(ArvBuffer*)));
  TEST(camstr->currentFilling=calloc(ncam,sizeof(ArvBuffer*)));
  TEST(camstr->rtcReading=calloc(ncam,sizeof(ArvBuffer*)));
  TEST(camstr->camErr=calloc(ncam,sizeof(int*)));
  TEST(camstr->stream=calloc(ncam,sizeof(ArvStream*)));
  TEST(camstr->camera=calloc(ncam,sizeof(ArvCamera*)));
  TEST(camstr->bufArrList=calloc(ncam,sizeof(ArvBuffer*)*NBUF));
  TEST(camstr->prevCmd=calloc(ncam+2,sizeof(char*)));
  camstr->npxlsArrCum[0]=0;
  printf("malloced things\n");
  for(i=0; i<ncam; i++){
    camstr->pxlx[i]=pxlx[i];
    camstr->pxly[i]=pxly[i];
    camstr->campxlx[i]=pxlx[i];//overwritten later (args)
    camstr->campxly[i]=pxly[i];//overwritten later (args)
    camstr->npxlsArr[i]=pxlx[i]*pxly[i];
    camstr->npxlsArrCum[i+1]=camstr->npxlsArrCum[i]+camstr->npxlsArr[i];
  }

  //Parameters are:
  //bpp[ncam] (not actually sent to camera - use aravisCmd* - but used by darc).
  //blocksize[ncam]
  //offsetX[ncam]
  //offsetY[ncam]
  //npxlx[cam]//sent to camera
  //npxly[cam]//sent to camera
  //byteswapInts[ncam]
  //reorder[ncam]
  //prio[ncam]
  //affinElSize
  //affin[ncam*elsize]
  //length of names (a string with all camera IDs, semicolon separated).
  //The names as a string.
  //recordTimestamp
  
  //So, we get these, then read the param buffer.  This will have:
  //aravisCmdN where N=0->ncam-1 which are the commands to configure the camera, semicolon separated.
  
  //So, we then apply these.  But first, some defaults.
  for(i=0;i<ncam;i++){
    camstr->blocksize[i]=2048;
    for(j=0;j<camstr->threadAffinElSize;j++)
      camstr->threadAffinity[i*camstr->threadAffinElSize+j]=0xffffffff;
  }
  //And then we set up the rest, start the streams etc.
  camstr->socket_buffer_size=npxls*16;//make it big enough for several frames. - for testing evt
  camstr->no_packet_resend=1;
  camstr->packet_timeout=20;//in ms.  Might be too low for slow cameras?  Fix if needed.
  camstr->frame_retention=100;//in ms.
  if(n>=2*ncam){
    for(i=0;i<ncam;i++)
      camstr->blocksize[i]=args[ncam+i];
  }
  if(n>=3*ncam){
    for(i=0;i<ncam;i++)
      camstr->offsetX[i]=args[2*ncam+i];
  }
  if(n>=4*ncam){
    for(i=0;i<ncam;i++)
      camstr->offsetY[i]=args[3*ncam+i];
  }
  if(n>=5*ncam){
    for(i=0;i<ncam;i++)
      camstr->campxlx[i]=args[4*ncam+i];
  }
  if(n>=6*ncam){
    for(i=0;i<ncam;i++)
      camstr->campxly[i]=args[5*ncam+i];
  }

  if(n>=7*ncam){
    for(i=0;i<ncam;i++){
      camstr->byteswapInts[i]=args[6*ncam+i];
      if(camstr->byteswapInts[i]>1 || camstr->byteswapInts[i]<0){
	printf("Error - config file out of date for camAravis - byteswapInts (and reorder) parameter has been added...\n");
	dofree(camstr);
	*camHandle=NULL;
	return 1;
      }
    }
  }
  if(n>=8*ncam){
    for(i=0;i<ncam;i++)
      camstr->reorder[i]=args[7*ncam+i];//reorder pixels
  }
  if(n>=9*ncam){
    for(i=0;i<ncam;i++)
      camstr->threadPriority[i]=args[8*ncam+i];
  }
  //Note:  threadAffinElSize==args[5*ncam] (accessed previously)
  if(n>(9+camstr->threadAffinElSize)*ncam){
    for(i=0;i<ncam;i++)
      for(j=0;j<camstr->threadAffinElSize;j++)
	camstr->threadAffinity[i*camstr->threadAffinElSize+j]=(unsigned int)(args[9*ncam+1+i*camstr->threadAffinElSize+j]);
  }
  if(n>(9+camstr->threadAffinElSize)*ncam+2){
    camIDLen=args[(9+camstr->threadAffinElSize)*ncam+1];//number of bytes to describe camera names.
    camstr->nameList=strndup((char*)&args[(9+camstr->threadAffinElSize)*ncam+2],camIDLen);
    char *namePtr=camstr->nameList;
    char *next;
    i=0;
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
    for(i=0;i<ncam;i++){
      printf("Camera: %s\n",camstr->camNameList[i]);
    }
  }else{
    if(ncam==1)
      printf("Using first available camera\n");
    else
      printf("WARNING - no cameras defined, not sure what will happen...\n");
  }
  nn=(9+camstr->threadAffinElSize)*ncam+2+((camIDLen+sizeof(int)-1)/sizeof(int));//number of args used so far...
  if(n>nn){
    camstr->recordTimestamp=args[nn];
  }else{
    camstr->recordTimestamp=0;
  }
  nn++;
  printf("got args (recordTimestamp=%d)\n",camstr->recordTimestamp);


  //now need to prepare the camera parameter buffer names: aravisCmdN, camReorderN
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


  TEST(camParamName=calloc(ncam+2+ngot,sizeof(char*)));
  for(i=0;i<ncam;i++){
    if((camParamName[i]=calloc(BUFNAMESIZE,1))==NULL){
      printf("Failed to calloc camParamName in camAravis\n");
      dofree(camstr);
      *camHandle=NULL;
      for(i=0; i<ncam; i++)
	free(camParamName[i]);
      free(camParamName);
      return 1;
    }
    snprintf(camParamName[i],BUFNAMESIZE,"aravisCmd%d",i);
  }
  if((camParamName[ncam]=calloc(BUFNAMESIZE,1))==NULL || (camParamName[ncam+1]=calloc(BUFNAMESIZE,1))==NULL){
    printf("Failed to calloc camParamName in camAravis\n");
    dofree(camstr);
    *camHandle=NULL;
    for(i=0; i<ncam+2; i++)
      free(camParamName[i]);
    free(camParamName);
    return 1;
  }
  snprintf(camParamName[ncam],BUFNAMESIZE,"aravisCmdAll");
  snprintf(camParamName[ncam+1],BUFNAMESIZE,"aravisGet");
  for(i=0;i<ngot;i++){
    if((camParamName[ncam+2+i]=calloc(BUFNAMESIZE,1))==NULL){
      printf("Failed to calloc reorders in camAravis\n");
      dofree(camstr);
      *camHandle=NULL;
      for(i=0;i<ncam+2;i++)
	free(camParamName[i]);
      for(i=0;i<ngot;i++)
	free(camParamName[ncam+2+i]);
      free(camParamName);
      return 1;
    }
    snprintf(camParamName[ncam+2+i],16,"camReorder%d",camstr->reorderno[i]);
  }
  


  //Now sort them... (actually, not necessary if ncam<10 - already sorted).
#define islt(a,b) (strcmp((*a),(*b))<0)
  QSORT(char*,camParamName,ncam+2+ngot,islt);
#undef islt
  //now capture the order (and we know the camReorders will come after aravis*)
  for(i=0;i<ngot;i++){
    j=atoi(&camParamName[i+ncam+2][10]);
    for(k=0;k<ncam;k++){
      if(camstr->reorder[k]==j){
	camstr->reorderIndx[k]=i+ncam+2;
      }
    }
  }

  //now make the parameter buffer
  if((camstr->paramNames=calloc(ncam+2+ngot,BUFNAMESIZE))==NULL){
    printf("Failed to mallocparamNames in camAravis.c\n");
    dofree(camstr);
    *camHandle=NULL;
    for(i=0; i<ncam+2+ngot; i++)
      free(camParamName[i]);
    free(camParamName);
    return 1;
  }
  for(i=0; i<ncam+2+ngot; i++){
    memcpy(&camstr->paramNames[i*BUFNAMESIZE],camParamName[i],BUFNAMESIZE);
    printf("%16s\n",&camstr->paramNames[i*BUFNAMESIZE]);
    free(camParamName[i]);
  }
  free(camParamName);
  TEST(camstr->index=calloc(sizeof(int),ncam+2+ngot));
  TEST(camstr->values=calloc(sizeof(void*),ncam+2+ngot));
  TEST(camstr->dtype=calloc(sizeof(char),ncam+2+ngot));
  TEST(camstr->nbytes=calloc(sizeof(int),ncam+2+ngot));
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
  arv_g_thread_init (NULL);//depreciated since 2.32
  arv_g_type_init ();//depreciated since 2.36
  if(camNewParam(*camHandle,pbuf,frameno,arr)!=0){
    printf("Error in camOpen->newParam...\n");
    dofree(camstr);
    *camHandle=NULL;
    return 1;
  }
  printf("Reorders:\n");
  for(i=0;i<ncam;i++)
    printf("%d %p\n",camstr->reorder[i],camstr->reorderBuf[i]);

  //Now open the cameras and start framing.
  for(i=0;i<ncam;i++){
    if(startCamera(camstr,i)){//starts a new thread...
      printf("Error opening camera %s\n",camstr->camNameList[i]);
      dofree(camstr);
      *camHandle=NULL;
      return 1;
    }
  }

  camstr->open=1;
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
  //pthread_mutex_lock(&camstr->m);
  camstr->open=0;
  for(i=0; i<camstr->ncam; i++){
    stopCamera(camstr,i);
    //pthread_cond_broadcast(&camstr->cond[i]);
  }
  //pthread_mutex_unlock(&camstr->m);
  //for(i=0; i<camstr->ncam; i++){
  // pthread_join(camstr->threadid[i],NULL);//wait for worker thread to complete
  //}
  dofree(camstr);
  *camHandle=NULL;
  printf("Camera closed\n");
  return 0;
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
      if (!ARV_IS_GC_NODE (feature)){
	printf("todo: node setting %d %s\n",i,name);
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
  //the only params needed is aravisCmdN for N=0 -> ncam-1 and camReorder if reorder!=0/
  int i,j,cam;
  CamStruct *camstr=(CamStruct*)camHandle;
  int err=0;
  printf("camNewParam\n");
  bufferGetIndex(pbuf,camstr->ncam+2+camstr->nReorders,camstr->paramNames,camstr->index,camstr->values,camstr->dtype,camstr->nbytes);
  memset(camstr->reorderBuf,0,camstr->ncam*sizeof(int*));
  for(i=0;i<camstr->ncam+2+camstr->nReorders;i++){
    printf("%16s: Index %d\n",&camstr->paramNames[i*BUFNAMESIZE],camstr->index[i]);
    if(camstr->index[i]>=0){
      if(i<camstr->ncam+2){//aravis*
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
		for(j=0;j<camstr->ncam;j++)
		  err|=sendCamCommand(camstr,j,camstr->prevCmd[i]);
	      }else{
		cam=atoi(&camstr->paramNames[i*BUFNAMESIZE+9]);
		printf("Calling sendCamCommand for cam %d, cmd %s\n",cam,camstr->prevCmd[i]);
		err=sendCamCommand(camstr,cam,camstr->prevCmd[i]);
	      }
	    }
	  }else if(!strcmp("aravisGet",&camstr->paramNames[i*BUFNAMESIZE])){
	    if(camstr->nbytes[i]>3 && ((char*)(camstr->values[i]))[0]=='?'){
	      //requesting a value. Format is ?cam:parameter where cam is the camera number and parameter is the parameter to get.  Replaces the string with the result.
	      getCamValue(camstr,(char*)camstr->values[i],camstr->nbytes[i]);
	    }
	  }
	}
      }else if(i<camstr->ncam+2+camstr->nReorders){//camReorder...
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
      }
    }
  }
  printf("Done camNewParam\n");
  return err;
}


/**
   Called when we're starting processing the next frame.  This doesn't actually wait for any pixels.
Single thread
*/
int camNewFrameSync(void *camHandle,unsigned int thisiter,double starttime){
  //printf("camNewFrame\n");
  CamStruct *camstr;
  int i;
  camstr=(CamStruct*)camHandle;
  if(camHandle==NULL){// || camstr->framing==0){
    //printf("called camNewFrame with camHandle==NULL\n");
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
  int i,gotNewFrame;
  int amp,rl,tb,pxl,j,x,y;
  //static struct timeval t1;
  //struct timeval t2;
  //struct timeval t3;
  //printf("camWaitPixels %d %d\n",n,cam);
  if(camHandle==NULL){//L || camstr->framing==0){
    //printf("called camWaitPixels with camHandle==NULL\n");
    return 1;
  }
  if(n<0)
    n=0;
  if(n>camstr->npxlsArr[cam])
    n=camstr->npxlsArr[cam];
  //printf("camWaitPixels\n");
  pthread_mutex_lock(&camstr->camMutex[cam]);
  //todo("care with mutexes...");
  //printf("camWaitPixels got mutex, newframe=%d\n",camstr->newframe[cam]);
  if(camstr->newframe[cam]){//first thread for this camera after new frame...
    //printf("First thread for new frame\n");
    camstr->newframe[cam]=0;
    camstr->frameReady[cam]=0;
    camstr->pxlsTransferred[cam]=0;
    gotNewFrame=0;
    while(gotNewFrame==0){
      //if(camstr->mostRecentFilled[cam]!=NULL && camstr->mostRecentFilled[cam]!=camstr->rtcReading[cam]){
      if(camstr->currentFilling[cam]!=NULL && camstr->currentFilling[cam]!=camstr->rtcReading[cam]){
	//have a full buffer waiting for processing.
	//printf("Setting rtcReading to mostRecentFilled, and mostRecentFilled to NULL\n");
	camstr->rtcReading[cam]=camstr->currentFilling[cam];//mostRecentFilled[cam];
	camstr->currentFilling[cam]=NULL;
	if(camstr->mostRecentFilled[cam]!=NULL)
	  printf("Skipping frame for cam %d\n",cam);
	camstr->mostRecentFilled[cam]=NULL;
	gotNewFrame=1;
      }else if(camstr->currentFilling[cam]==NULL && camstr->mostRecentFilled[cam]!=NULL && camstr->mostRecentFilled[cam]!=camstr->rtcReading[cam]){
	  //have a full buffer waiting for processing/
	  camstr->rtcReading[cam]=camstr->mostRecentFilled[cam];
	  camstr->mostRecentFilled[cam]=NULL;
	  gotNewFrame=1;
      
	  /*      }else if(camstr->currentFilling[cam]!=NULL && camstr->currentFilling[cam]!=camstr->rtcReading[cam]){
	//have a new buffer currently reading out
	camstr->rtcReading[cam]=camstr->currentFilling[cam];
	camstr->currentFilling[cam]=NULL;
	camstr->mostRecentFilled[cam]=NULL;
	//printf("setting rtcReading to currentFilling and moreRecentFilled to NULL\n");
	gotNewFrame=1;*/
      }else{
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
  }else{
    //We're not the first thread for this frame/camera.
    //Need to wait here until the frame is ready.
    while(camstr->frameReady[cam]==0){
      //todo("care with mutex");
      //printf("Waiting from frameReady\n");
      pthread_cond_wait(&camstr->camCond2[cam],&camstr->camMutex[cam]);
    }
  }
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
      if(camstr->reorder[cam]==0){//no pixel reordering
	memcpy(&camstr->imgdata[(camstr->npxlsArrCum[cam]+camstr->pxlsTransferred[cam])],&(((char*)camstr->rtcReading[cam]->data)[camstr->pxlsTransferred[cam]]),(n-camstr->pxlsTransferred[cam]));
      }else{
	if(camstr->reorderBuf[cam]!=NULL){//reordering based on parameter buffer.
	  for(i=camstr->pxlsTransferred[cam];i<n;i++){
	    camstr->imgdata[camstr->npxlsArrCum[cam]+camstr->reorderBuf[cam][i]]=(((unsigned char*)(camstr->rtcReading[cam]->data))[i]);
	  }
	}else if(camstr->reorder[cam]==1){//specific reorder for scimeasure CCID18 (CANARY LGS)
	  for(i=camstr->pxlsTransferred[cam]; i<n; i++){
	    //i%2 specifies whether bottom or top half.
	    //(i//2)%8 specifies which quadrant it is in.
	    //i//16 specifies which pixel it is in the 64x16 pixel quadrant.
	    //row==i//256
	    //col=(i//16)%16
	    //Assumes a 128x128 pixel detector...
	    j=(i%2?127-i/256:i/256)*128+((i/2)%8)*16+(i/16)%16;
	    camstr->imgdata[camstr->npxlsArrCum[cam]+j]=(((unsigned char*)(camstr->rtcReading[cam]->data))[i]);
	  }
	}else if(camstr->reorder[cam]==2){//specific reorder for OCAM2
	  //Should be 63888 pixels (121*1056/2 -> 264x242).  Each quadrant is 66x121 pixels
	  for(i=camstr->pxlsTransferred[cam]; i<n; i++){
	    amp=i%8;//the amplifier (quadrant) in question    0123 (see ocam manual)
	    //lr=i%2;//left to right amplifier (1,3,5,7)?       7654
	    rl=1-i%2;//right to left
	    //bt=amp/4;//bottom to top amplifier (4,5,6,7)?
	    tb=1-amp/4;//top to bottom amp (0,1,2,3)?
	    pxl=i/8;//the pixel within a given quadrant
	    //x=pxl%66;//x within the quad
	    //if(lr==0)
	    //  x=65-x;
	    //if(amp<4)//shift to correct amplifier
	    //  x+=66*amp;
	    //else
	    //  x=66*8-x-66*amp-1
	    //x=((1-2*rl)*(pxl%66)+rl*65);
	    x=(tb*2-1)*(((1-2*rl)*(pxl%66)+rl*65)+66*amp)+(1-tb)*(66*8-1);

	    //y=pxl/66;//y within the quad
	    //if(bt)
	    //  y=241-y;
	    y=(1-tb)*241+(2*tb-1)*(pxl/66);
	    j=y*264+x;
	    camstr->imgdata[camstr->npxlsArrCum[cam]+j]=(((unsigned char*)(camstr->rtcReading[cam]->data))[i]);
	  }
	}
      }
    }else if(camstr->bytespp==2){
      if(camstr->bpp[cam]<=8){//cast from uint8 to uint16
	for(i=camstr->pxlsTransferred[cam];i<n;i++)
	  ((unsigned short*)camstr->imgdata)[camstr->npxlsArrCum[cam]+i]=(unsigned short)(((unsigned char*)(camstr->rtcReading[cam]->data))[i]);
      }else if(camstr->bpp[cam]<=16){//copy
	if(camstr->reorder[cam]==0){//no reordering of pixels
	  memcpy(&camstr->imgdata[2*(camstr->npxlsArrCum[cam]+camstr->pxlsTransferred[cam])],&(((char*)camstr->rtcReading[cam]->data)[2*camstr->pxlsTransferred[cam]]),2*(n-camstr->pxlsTransferred[cam]));
	}else{
	  if(camstr->reorderBuf[cam]!=NULL){
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
	    //Should be 63888 pixels coming in (121*1056/2 -> 264x242).  Each quadrant is 66x121 pixels.  Then here, convert to 240x240.  Remove first 6 pixels of each row of each quad, and last row.
	    for(i=camstr->pxlsTransferred[cam]; i<n; i++){
	      pxl=i/8;//the pixel within a given quadrant
	      if(pxl%66>5 && pxl<66*120){//not an overscan pixel
		amp=i%8;//the amplifier (quadrant) in question    
		rl=1-i%2;//right to left
		tb=1-amp/4;//top to bottom amp (0,1,2,3)?
		x=(tb*2-1)*(((1-2*rl)*(pxl%66-6)+rl*59)+60*amp)+(1-tb)*(60*8-1);
		y=(1-tb)*239+(2*tb-1)*(pxl/66);
		j=y*240+x;
		((unsigned short*)camstr->imgdata)[camstr->npxlsArrCum[cam]+j]=(((unsigned short*)(camstr->rtcReading[cam]->data))[i]);
	      }
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
  if(rt!=0){//An error has arisen - probably dropped packet.  So how do we handle this?
    //printf("camWaitPixels got err %d (cam %d) frame[0]=%d frame[%d]=%d\n",rt,cam,camstr->userFrameNo[0],cam,camstr->userFrameNo[cam]);
    
  }
  pthread_mutex_unlock(&camstr->camMutex[cam]);
  return rt;
}

int camFrameFinishedSync(void *camHandle,int err,int forcewrite){//subap thread (once)
  int i;
  CamStruct *camstr=(CamStruct*)camHandle;
  if(camstr->recordTimestamp){//an option to put camera frame number as us time
    for(i=0; i<camstr->ncam; i++){
      camstr->userFrameNo[i]=camstr->timestamp[i].tv_sec*1000000+camstr->timestamp[i].tv_usec;
    }
  }
  return 0;
}
