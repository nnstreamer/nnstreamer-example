#include "imageclassificationoffloadingservice.h"
#include <service_app.h>
#include <tizen.h>

int main(int argc, char *argv[]) {
  char ad[50] = {
      0,
  };
  service_app_lifecycle_callback_s event_callback;

  dlog_print(DLOG_ERROR, LOG_TAG,
             "Image classification offloading service start");
  return service_app_main(argc, argv, &event_callback, ad);
}
