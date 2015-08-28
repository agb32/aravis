#include <stdio.h>
#include <arv.h>
int main(int argc, char **argv){
  ArvCamera *camera;
  arv_g_thread_init (NULL);//depreciated since 2.32
  arv_g_type_init ();//depreciated since 2.36
  camera=arv_camera_new(NULL);
  if(camera!=NULL){
    if(argc==1){
      printf("Starting camera\n");
      arv_camera_start_acquisition (camera);//starts a new thread...
    }else{
      printf("Stopping camera\n");
      arv_camera_stop_acquisition(camera);
    }
  }else{
    printf("No camera found\n");
  }
  return 0;
}
