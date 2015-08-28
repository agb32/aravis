#include <arv.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>

#define NBUF 50

static char *arv_option_camera_name = NULL;
static char *arv_option_debug_domains = NULL;
static gboolean arv_option_snaphot = FALSE;
static char *arv_option_trigger = NULL;
static double arv_option_software_trigger = -1;
static double arv_option_frequency = -1.0;
static int arv_option_width = -1;
static int arv_option_height = -1;
static int arv_option_horizontal_binning = -1;
static int arv_option_vertical_binning = -1;
static double arv_option_exposure_time_us = -1;
static int arv_option_gain = -1;
static gboolean arv_option_auto_socket_buffer = FALSE;
static gboolean arv_option_no_packet_resend = FALSE;
static unsigned int arv_option_packet_timeout = 20;
static unsigned int arv_option_frame_retention = 100;

static const GOptionEntry arv_option_entries[] =
{
	{
		"name",					'n', 0, G_OPTION_ARG_STRING,
		&arv_option_camera_name,		"Camera name", NULL
	},
	{
		"snapshot",				's', 0, G_OPTION_ARG_NONE,
		&arv_option_snaphot,			"Snapshot", NULL
	},
	{
		"frequency", 				'f', 0, G_OPTION_ARG_DOUBLE,
		&arv_option_frequency,			"Acquisition frequency", NULL
	},
	{
		"trigger",				't', 0, G_OPTION_ARG_STRING,
		&arv_option_trigger,			"External trigger", NULL
	},
	{
		"software-trigger",			'o', 0, G_OPTION_ARG_DOUBLE,
		&arv_option_software_trigger,		"Emit software trigger", NULL
	},
	{
		"width", 				'w', 0, G_OPTION_ARG_INT,
		&arv_option_width,			"Width", NULL
	},
	{
		"height", 				'h', 0, G_OPTION_ARG_INT,
		&arv_option_height, 			"Height", NULL
	},
	{
	       "h-binning", 				'\0', 0, G_OPTION_ARG_INT,
		&arv_option_horizontal_binning,		"Horizontal binning", NULL
	},
	{
		"v-binning", 				'\0', 0, G_OPTION_ARG_INT,
		&arv_option_vertical_binning, 		"Vertical binning", NULL
	},
	{
		"exposure", 				'e', 0, G_OPTION_ARG_DOUBLE,
		&arv_option_exposure_time_us, 		"Exposure time (µs)", NULL
	},
	{
		"gain", 				'g', 0, G_OPTION_ARG_INT,
		&arv_option_gain,	 		"Gain (dB)", NULL
	},
	{
		"auto",					'a', 0, G_OPTION_ARG_NONE,
		&arv_option_auto_socket_buffer,		"Auto socket buffer size", NULL
	},
	{
		"no-packet-resend",			'r', 0, G_OPTION_ARG_NONE,
		&arv_option_no_packet_resend,		"No packet resend", NULL
	},
	{
		"packet-timeout", 			'p', 0, G_OPTION_ARG_INT,
		&arv_option_packet_timeout, 		"Packet timeout (ms)", NULL
	},
	{
		"frame-retention", 			'm', 0, G_OPTION_ARG_INT,
		&arv_option_frame_retention, 		"Frame retention (ms)", NULL
	},
	{
		"debug", 				'd', 0, G_OPTION_ARG_STRING,
		&arv_option_debug_domains, 		"Debug domains", NULL
	},
	{ NULL }
};

typedef struct {
  GMainLoop *main_loop;
  int buffer_count;
  ArvStream *stream;
  ArvBuffer *prevBuffer;
  struct timeval t1;
  ArvBuffer **bufArr;
} ApplicationData;

static gboolean cancel = FALSE;

static void
set_cancel (int signal)
{
	cancel = TRUE;
}

/*typedef struct{
  ArvStream *stream;
  ArvBuffer *prevBuffer;
  guint32 buffer_count;
} MyDataStruct;
*/

void mycallback(void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer){
  ArvStream *stream;
  ArvBuffer *buf=NULL;
  //MyDataStruct *myData;
  ApplicationData *myData=(ApplicationData*)user_data;
  static int cnt=0;
  struct timeval t2;
  if(buffer==NULL){
    printf("callback %d, null buffer\n",type);
    if(type==ARV_STREAM_CALLBACK_TYPE_START_BUFFER){
      if(myData!=NULL && myData->t1.tv_sec==0 && myData->t1.tv_usec==0)
	gettimeofday(&myData->t1,NULL);
    }
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
	printf("callback %d %d %d %ld %g %p %d\n",type,buffer->contiguous_data_received,buffer->last_data_accessed,buffer->size,frac,buffer,bufno);
      }
    }else if(type==ARV_STREAM_CALLBACK_TYPE_START_BUFFER){
      printf("Start buffer %d %p %d\n",buffer->last_data_accessed,buffer,bufno);
    }else if(type==ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE){
      if(myData!=NULL){
	//myData=(ApplicationData*)user_data;
	//stream=(ArvStream*)(*(ArvStream**)user_data);
	stream=myData->stream;
	printf("Buffer done %d\n",bufno);
	buf = arv_stream_try_pop_buffer (stream);
	if (buf != NULL) {
	  if (buf->status == ARV_BUFFER_STATUS_SUCCESS){
	    myData->buffer_count++;
	    // Image processing here 
	  }
	  arv_stream_push_buffer (stream, buf);
	}else{
	  printf("FAILED TO POP BUFFER...\n");
	}
      }
      cnt++;
      printf("Buffer done %d %d %ld %p %p %ld %s %s %d\n",type,buffer->contiguous_data_received,buffer->size,buffer,buf,buffer-buf,buffer==buf?"Same":"Different",myData->prevBuffer==buf?"Same":"Different",bufno);
      myData->prevBuffer=buffer;
      if(cancel){//cnt==100){
	printf("Exiting main loop\n");
	gettimeofday(&t2,NULL);
	printf ("Frame rate = %g Hz\n", myData->buffer_count/(t2.tv_sec-myData->t1.tv_sec+1e-6*(t2.tv_usec-myData->t1.tv_usec)));
	g_main_loop_quit(myData->main_loop);
      }
    }else{
      printf("callback %d %p\n",type,buffer);
    }
  }
}

/*static void
new_buffer_cb (ArvStream *stream, ApplicationData *data)
{
	ArvBuffer *buffer;

	buffer = arv_stream_try_pop_buffer (stream);
	if (buffer != NULL) {
		if (buffer->status == ARV_BUFFER_STATUS_SUCCESS)
			data->buffer_count++;
		 // Image processing here 
		arv_stream_push_buffer (stream, buffer);
	}
}*/

 /*static gboolean
periodic_task_cb (void *abstract_data)
{
	ApplicationData *data = abstract_data;

	printf ("Frame rate = %d Hz\n", data->buffer_count);
	data->buffer_count = 0;

	if (cancel) {
		g_main_loop_quit (data->main_loop);
		return FALSE;
	}

	return TRUE;
	}*/

static gboolean
emit_software_trigger (void *abstract_data)
{
	ArvCamera *camera = abstract_data;

	arv_camera_software_trigger (camera);

	return TRUE;
}

static void
control_lost_cb (ArvGvDevice *gv_device)
{
	printf ("Control lost\n");

	cancel = TRUE;
}

int
main (int argc, char **argv)
{
	ApplicationData data;
	ArvCamera *camera;
	ArvStream *stream;
	//ArvStream *streamPtr[1];
	//MyDataStruct myData;
	ArvBuffer *buffer;
	GOptionContext *context;
	GError *error = NULL;
	int i;

	data.buffer_count = 0;
	data.t1.tv_sec=0;
	data.t1.tv_usec=0;
	//arv_g_thread_init (NULL);//depreciated since 2.32
	//arv_g_type_init ();//depreciated since gobject version 2.36

	arv_g_thread_init (NULL);
	arv_g_type_init ();

	//command line parsing:
	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, arv_option_entries, NULL);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_option_context_free (context);
		g_print ("Option parsing failed: %s\n", error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}
	g_option_context_free (context);

	//arv_debug_enable (arv_option_debug_domains);

	if (arv_option_camera_name == NULL)
		g_print ("Looking for the first available camera\n");
	else
		g_print ("Looking for camera '%s'\n", arv_option_camera_name);

	camera = arv_camera_new (arv_option_camera_name);
	if (camera != NULL) {
		void (*old_sigint_handler)(int);
		gint payload;
		gint x, y, width, height;
		gint dx, dy;
		double exposure;
		guint64 n_completed_buffers;
		guint64 n_failures;
		guint64 n_underruns;
		int gain;
		guint software_trigger_source = 0;
		ArvBuffer **bufArr;
		bufArr=calloc(sizeof(ArvBuffer*),NBUF);

		arv_camera_set_region (camera, 0, 0, arv_option_width, arv_option_height);
		arv_camera_set_binning (camera, arv_option_horizontal_binning, arv_option_vertical_binning);
		arv_camera_set_exposure_time (camera, arv_option_exposure_time_us);
		arv_camera_set_gain (camera, arv_option_gain);

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
		printf ("exposure            = %g µs\n", exposure);
		printf ("gain                = %d dB\n", gain);
		//myData.buffer_count=0;
		data.prevBuffer=NULL;//was myData
		stream = arv_camera_create_stream (camera, mycallback, &data);//was myData
		data.stream=stream;//was myData
		//*streamPtr=stream;
		if (stream != NULL) {
			if (ARV_IS_GV_STREAM (stream)) {
				if (arv_option_auto_socket_buffer)
					g_object_set (stream,
						      "socket-buffer", ARV_GV_STREAM_SOCKET_BUFFER_AUTO,
						      "socket-buffer-size", 0,
						      NULL);
				if (arv_option_no_packet_resend)
					g_object_set (stream,
						      "packet-resend", ARV_GV_STREAM_PACKET_RESEND_NEVER,
						      NULL);
				g_object_set (stream,
					      "packet-timeout", (unsigned) arv_option_packet_timeout * 1000,
					      "frame-retention", (unsigned) arv_option_frame_retention * 1000,
					      NULL);
			}
			for (i = 0; i < NBUF; i++){
			  bufArr[i]=arv_buffer_new(payload,NULL);
			  arv_stream_push_buffer (stream, bufArr[i]);
			}
			data.bufArr=bufArr;
			arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS);

			if (arv_option_frequency > 0.0)
				arv_camera_set_frame_rate (camera, arv_option_frequency);

			if (arv_option_trigger != NULL)
				arv_camera_set_trigger (camera, arv_option_trigger);

			if (arv_option_software_trigger > 0.0) {
				arv_camera_set_trigger (camera, "Software");
				software_trigger_source = g_timeout_add ((double) (0.5 + 1000.0 /
										   arv_option_software_trigger),
									 emit_software_trigger, camera);
			}

			arv_camera_start_acquisition (camera);

			//g_signal_connect (stream, "new-buffer", G_CALLBACK (new_buffer_cb), &data);
			//arv_stream_set_emit_signals (stream, TRUE);

			//g_signal_connect (arv_camera_get_device (camera), "control-lost",
			//		  G_CALLBACK (control_lost_cb), NULL);

			//g_timeout_add_seconds (1, periodic_task_cb, &data);

			data.main_loop = g_main_loop_new (NULL, FALSE);

			old_sigint_handler = signal (SIGINT, set_cancel);

			g_main_loop_run (data.main_loop);

			if (software_trigger_source > 0)
				g_source_remove (software_trigger_source);

			signal (SIGINT, old_sigint_handler);

			//g_main_loop_unref (data.main_loop);

			arv_stream_get_statistics (stream, &n_completed_buffers, &n_failures, &n_underruns);

			printf ("Completed buffers = %Lu\n", (unsigned long long) n_completed_buffers);
			printf ("Failures          = %Lu\n", (unsigned long long) n_failures);
			printf ("Underruns         = %Lu\n", (unsigned long long) n_underruns);

			arv_camera_stop_acquisition (camera);

			g_object_unref (stream);
		} else
			printf ("Can't create stream thread (check if the device is not already used)\n");

		g_object_unref (camera);
	} else
		printf ("No camera found\n");

	return 0;
}
