///**
// * Copyright (c) 2019 Samsung Electronics Co., Ltd
// *
// * Licensed under the Apache License, Version 2.0 (the "License");
// * you may not use this file except in compliance with the License.
// * You may obtain a copy of the License at
// *
// *     https://www.apache.org/licenses/LICENSE-2.0
// *
// * Unless required by applicable law or agreed to in writing, software
// * distributed under the License is distributed on an "AS IS" BASIS,
// * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// * See the License for the specific language governing permissions and
// * limitations under the License.
// */
//
//#include <detect.h>
//#include <string.h>
//#include <stdlib.h>
//#include <math.h>
//#include <gst/gst.h>
//#include <gst/app/gstappsrc.h>
//#include <gst/app/gstappsink.h>
//#include <gmodule.h>
//
//#ifndef DBG
//#define DBG FALSE
//#endif
//
//const gchar ssd_model[] = "ssd_mobilenet_v2_coco.tflite";
//const gchar ssd_label[] = "coco_labels_list.txt";
//const gchar ssd_box_priors[] = "box_priors.txt";
//
//#define BIT_IMAGE_SIZE 640*480*3
//
//// static GMainLoop *loop = NULL;
//// static GstElement *pipeline;
//int want = 0;
//int get = 0;
//// static int gst_max_cnt = 0;
//// static int gst_max_cnt_sf = 0;
//// static int gst_prev_max_cnt = 0;
//// static int gst_prev_max_cnt_sf = 0;
//// int GstTestFallFlag = 0;
//
//// static gboolean INIT = FALSE;
//// static GstElement *pngdec, *tensor_converter, *appsource,
//// *tensor_transform, *cnn_aggregator, *lstm_aggregator, *cnn_filter,
//// *lstm_filter;
//// static GstElement *fakesink, *appsink;
//// static GstElement *converter_queue, *transform_queue, *aggcnn_queue, *cnn_queue,
//// *agglstm_queue, *lstm_queue;
//// static GstCaps *msrc_cap;
//
//GCond supply_cond, demand_cond;
//GMutex supply_lock, demand_lock;
//static GQueue * detected = NULL;
////ssd_model_info_s ssd_model_info;
//
//
//static GstFlowReturn
//prepare_buffer (CustomData * data, camera_preview_data_s *frame)
//{
////	static GstClockTime timestamp = 0;
//	static unsigned int prev_timestamp = 0;
//	static unsigned int base_timestamp = 0;
//	GstBuffer *buffer;
//	GstMemory *mem;
//	GstMapInfo info;
//	GstFlowReturn ret;
//	unsigned int t1, t2;
//
////	g_mutex_lock (&supply_lock);
////	while (!want)
////		g_cond_wait (&supply_cond, &supply_lock);
////	want = 0;
////	g_mutex_unlock (&supply_lock);
//
//	g_mutex_lock (&supply_lock);
//	if (!want) {
//		g_mutex_unlock (&supply_lock);
//		return GST_FLOW_OK;
//	}
//	want = 0;
//	g_mutex_unlock (&supply_lock);
//
//	buffer = gst_buffer_new ();
//
//	if (prev_timestamp == 0) {
//		prev_timestamp = frame->timestamp;
//		base_timestamp = frame->timestamp;
//		g_mutex_lock (&supply_lock);
//		want = 1;
//		g_mutex_unlock (&supply_lock);
//		return GST_FLOW_OK;
//	}
//
//	// I420 format of emulator
//	size_t frame_size = frame->data.triple_plane.y_size + frame->data.triple_plane.u_size + frame->data.triple_plane.v_size;
//	mem = gst_allocator_alloc (NULL, frame_size, NULL);
//	gst_memory_map (mem, &info, GST_MAP_WRITE);
//	memcpy (info.data, frame->data.triple_plane.y, frame_size);
//
////	// NV12 format of TM1
////	size_t frame_size = frame->data.double_plane.y_size + frame->data.double_plane.uv_size;
////	mem = gst_allocator_alloc (NULL, frame_size, NULL);
////	gst_memory_map (mem, &info, GST_MAP_WRITE);
////	memcpy (info.data, frame->data.double_plane.y, frame_size);
//
//	gst_buffer_append_memory (buffer, mem);
//	gst_memory_unmap (mem, &info);
//
////	GST_BUFFER_PTS (buffer) = timestamp;
////	GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 4);
//
//	t1 = (frame->timestamp - base_timestamp);
//	t2 = (frame->timestamp - prev_timestamp);
//	GST_BUFFER_PTS (buffer) = (frame->timestamp - base_timestamp);
//	GST_BUFFER_DURATION (buffer) = (frame->timestamp - prev_timestamp);
//	prev_timestamp = frame->timestamp;
////	timestamp += GST_BUFFER_DURATION (buffer);
//	ret = gst_app_src_push_buffer ((GstAppSrc *) data->appsrc, buffer);
//	if (ret != GST_FLOW_OK) {
//		dlog_print (DLOG_DEBUG, LOG_TAG, "Error in pushing the buffer");
//		return GST_FLOW_ERROR;
//	}
//
//	return GST_FLOW_OK;
//}
//
////static void
////get_buffer (unsigned char * data, size_t * size)
////{
////	GstMemory *mem;
////	GstMapInfo info;
////
////
////	g_mutex_lock (&demand_lock);
////	while (!get)
////		g_cond_wait (&demand_cond, &demand_lock);
////	get = 0;
////
////	mem = (GstMemory *) g_queue_pop_head (detected);
////	g_mutex_unlock (&demand_lock);
////
////	gst_memory_map (mem, &info, GST_MAP_READ);
////	data = info.data;
////	size = info.size;
////
////	return;
////}
//
//static void
//need_data_cb (GstElement * appsrc, guint unused_size, gpointer user_data)
//{
//	g_mutex_lock (&supply_lock);
//	want = 1;
//	g_cond_signal (&supply_cond);
//	g_mutex_unlock (&supply_lock);
//}
//
////static gboolean
////read_lines (const gchar * file_name, GList ** lines)
////{
////	FILE * fp;
////	char * line = NULL;
////	size_t len = 0;
////	ssize_t read;
////
////	fp = fopen(file_name, "r");
////	if (fp == NULL)
////		return FALSE;
////
////	while ((read = getline(&line, &len, fp)) != -1) {
////		*lines = g_list_append (*lines, g_strdup (line));
////	}
////
////	fclose(fp);
////	if (line)
////		free(line);
////	return TRUE;
////}
////
////static gboolean
////load_box_priors (void)
////{
////	GList *box_priors = NULL;
////	gchar *box_row;
////
////	if (!read_lines (ssd_model_info.box_prior_path, &box_priors)) {
////		GST_ERROR ("Failed to load box prior");
////		return FALSE;
////	}
////
////	for (int row = 0; row < BOX_SIZE; row++) {
////		int column = 0;
////		int i = 0, j = 0;
////		char buff[11];
////
////		memset (buff, 0, 11);
////		box_row = (gchar *) g_list_nth_data (box_priors, row);
////
////		while ((box_row[i] != '\n') && (box_row[i] != '\0')) {
////			if (box_row[i] != ' ') {
////				buff[j] = box_row[i];
////				j++;
////			} else {
////				if (j != 0) {
////					ssd_model_info.box_priors[row][column++] = atof (buff);
////					memset (buff, 0, 11);
////				}
////				j = 0;
////			}
////			i++;
////		}
////
////		ssd_model_info.box_priors[row][column++] = atof (buff);
////	}
////
////	g_list_free_full (box_priors, g_free);
////	return TRUE;
////}
////
////static gboolean
////load_labels (void)
////{
////	return read_lines (ssd_model_info.label_path, &ssd_model_info.labels);
////}
////
////static void
////get_bounding_boxes (gfloat * boxes, gfloat * detections, struct boxes * box)
////{
////	const float threshold_score = .5f;
////  guint pos = box->num_boxes;
////
////  for (int d = 0; pos < MAX_BOXES && d < DETECTION_MAX; d++) {
////    float ycenter = boxes[0] / Y_SCALE * ssd_model_info.box_priors[2][d] +
////      ssd_model_info.box_priors[0][d];
////    float xcenter = boxes[1] / X_SCALE * ssd_model_info.box_priors[3][d] +
////      ssd_model_info.box_priors[1][d];
////    float h = (float) expf (boxes[2] / H_SCALE) * ssd_model_info.box_priors[2][d];
////    float w = (float) expf (boxes[3] / W_SCALE) * ssd_model_info.box_priors[3][d];
////
////    float ymin = ycenter - h / 2.f;
////    float xmin = xcenter - w / 2.f;
////    float ymax = ycenter + h / 2.f;
////    float xmax = xcenter + w / 2.f;
////
////    for (int c = 1; pos < MAX_BOXES && c < LABEL_SIZE; c++) {
////      gfloat score = 1.f / (1.f + expf (-1.f * detections[c]));
////      /**
////       * This score cutoff is taken from Tensorflow's demo app.
////       * There are quite a lot of nodes to be run to convert it to the useful possibility
////       * scores. As a result of that, this cutoff will cause it to lose good detections in
////       * some scenarios and generate too much noise in other scenario.
////       */
////      if (score < threshold_score)
////        continue;
////
////      pos = box->num_boxes;
////      box->x1[pos] = xmin * MODEL_WIDTH;
////      box->x2[pos] = xmax * MODEL_WIDTH;
////      box->y1[pos] = ymin * MODEL_WIDTH;
////      box->y2[pos] = ymax * MODEL_WIDTH;
////      box->probability[pos] = score;
////      box->class[pos] = c;
////      box->num_boxes += 1;
////    }
////
////    detections += LABEL_SIZE;
////    boxes += BOX_SIZE;
////  }
////
////  return;
////}
//
//
//static void
//collect_data_cb (GstElement * sink, GstBuffer * buffer, gpointer user_data)
//{
////	GstMemory *mem;
////	GstMapInfo info;
////	CustomData * data = user_data;
////
////	mem = gst_buffer_get_memory (buffer, 0);
//////	gst_memory_map (mem, &info, GST_MAP_READ);
//////
//////	ecore_pipe_write(data->pipe, info.data, info.size);
////
//////	gst_memory_unmap (mem, &info);
//////	gst_memory_unref (mem);
////	g_mutex_lock (&demand_lock);
////	get = 1;
////	g_queue_push_tail (detected, mem);
////	g_cond_signal (&demand_cond);
////	g_mutex_unlock (&demand_lock);
//
//	GstMemory *mem;
//	GstMapInfo info;
//	CustomData * data = user_data;
//
//	mem = gst_buffer_get_memory (buffer, 0);
//	gst_memory_map (mem, &info, GST_MAP_READ);
//
//	ecore_pipe_write(data->pipe, info.data, info.size);
//
//	gst_memory_unmap (mem, &info);
//	gst_memory_unref (mem);
//}
//
//
////static void
////collect_data_cb (GstElement * sink, GstBuffer * buffer, gpointer user_data)
////{
////	GstMemory *mem_boxes, *mem_detections;
////	GstMapInfo info_boxes, info_detections;
////	gfloat *boxes, *detections;
////  struct boxes * box;
////
////  int num_buffer = gst_buffer_n_memory(buffer);
////  int memory_size = 0;
////  int map_size = 0;
////
//////	if (num_buffer != 2) {
//////		GST_ERROR ("Invalid result, the number of memory blocks is different.");
//////		return;
//////	}
////
////	/* boxes */
////	mem_boxes = gst_buffer_get_memory (buffer, 0);
////	gst_memory_map (mem_boxes, &info_boxes, GST_MAP_READ);
////	memory_size = mem_boxes->size;
////	map_size = info_boxes.size;
////	guint * boxes_data = (guint *) info_boxes.data;
////	gint BUFLEN = 512;
////
////	char *file_path = (char *) malloc(sizeof(char) * BUFLEN);
////
////	        /* Create a full path to newly created file for storing the taken photo. */
////	        snprintf(file_path, BUFLEN, "/tmp/cam%d.jpg", (int) time(NULL));
////
////	        /* Open the file for writing. */
////	        FILE *file = fopen(file_path, "w+");
////
////	        /* Write the image to a file. */
////	        fwrite(boxes_data, sizeof(guint), map_size, file);
////
////	        /* Close the file. */
////	        fclose(file);
////
////
//////	/* detections */
//////	mem_detections = gst_buffer_get_memory (buffer, 1);
//////	gst_memory_map (mem_detections, &info_detections, GST_MAP_READ);
//////	memory_size = mem_boxes->size;
//////	map_size = info_boxes.size;
//////	detections = (gfloat *) info_detections.data;
////
////	box = (struct boxes *) malloc (sizeof(struct boxes));
////	box->num_boxes = 0;
////	box->img_width = MODEL_WIDTH;
////	box->img_height = MODEL_HEIGHT;
////
////  /* get all the bounding boxes */
////  get_bounding_boxes (boxes, detections, box);
////
////	gst_memory_unmap (mem_boxes, &info_boxes);
////	gst_memory_unmap (mem_detections, &info_detections);
////
////	gst_memory_unref (mem_boxes);
////	gst_memory_unref (mem_detections);
////
////	g_mutex_lock (&demand_lock);
////	get = 1;
////	g_queue_push_tail (detected, box);
////	g_cond_signal (&demand_cond);
////	g_mutex_unlock (&demand_lock);
////}
//
//static void
//error_cb (GstBus * bus, GstMessage * msg, CustomData * data)
//{
//	GError *err;
//	gchar *debug_info;
//	gchar *message;
//
//	gst_message_parse_error (msg, &err, &debug_info);
//	message =
//			g_strdup_printf ("Error received from element %s: %s",
//					GST_OBJECT_NAME (msg->src), err->message);
//	g_clear_error (&err);
//	g_free (debug_info);
//
//	dlog_print(DLOG_DEBUG, LOG_TAG, "%s", message);
//	g_free (message);
//
//
//	gst_element_set_state (data->pipeline, GST_STATE_NULL);
//}
//
//
//static void
//state_changed_cb (GstBus * bus, GstMessage * msg, CustomData * data)
//{
//	GstState old_state, new_state, pending_state;
//
//	gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
//
//	/* Only pay attention to messages coming from the pipeline, not its children */
//	if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->pipeline)) {
//		gchar *message = g_strdup_printf ("State changed to %s",
//				gst_element_state_get_name (new_state));
//
//		dlog_print (DLOG_DEBUG, LOG_TAG, "%s", message);
//		g_free (message);
//	}
//}
//
//void
//gst_initialize (CustomData * data)
//{
//	  gchar *model_path;       /**< tflite model file path */
//	  gchar *label_path;       /**< label file path */
//	  gchar *box_prior_path;   /**< box prior file path */
//
//	GstBus *bus;
//	GSource *bus_source;
//	GError *error = NULL;
//  gchar * res_path;
//  gchar * str_pipeline;
//	GstStateChangeReturn ret;
//	GstCaps *msrc_cap;
//
//  /* setup model info path */
////  memset (&ssd_model_info, 0, sizeof (ssd_model_info_s));
//	res_path = app_get_resource_path();
//	model_path = g_strdup_printf ("%s/%s", res_path, ssd_model);
//	label_path = g_strdup_printf ("%s/%s", res_path, ssd_label);
//	box_prior_path = g_strdup_printf ("%s/%s", res_path, ssd_box_priors);
//	char * filesrc_path = g_strdup_printf ("%s/%s", res_path, "result.avi");
//
//	/* Check model and label files */
//	if (!g_file_test (model_path, G_FILE_TEST_IS_REGULAR) ||
//			!g_file_test (label_path, G_FILE_TEST_IS_REGULAR) ||
//			!g_file_test (box_prior_path, G_FILE_TEST_IS_REGULAR)) {
//		GST_ERROR ("Failed to init model info");
//		goto error_free_model_info;
//	}
//
//	/* Load labels */
////	if (!load_box_priors () || !load_labels ()) {
////		GST_ERROR ("Failed to load labels/box_priors");
////		goto error_free_model_info;
////	}
//
//	gst_init (NULL, NULL);
//	g_cond_init (&supply_cond);
//	g_cond_init (&demand_cond);
//	g_mutex_init (&supply_lock);
//	g_mutex_init (&demand_lock);
//  detected = g_queue_new();
//  g_queue_init (detected);
//
//	data->context = g_main_context_new ();
//	g_main_context_push_thread_default (data->context);
//
//
////		str_pipeline = g_strdup_printf(
////			"appsrc name=res_source ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB,framerate=30/1 !"
////			"tee name=t t. ! queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter !"
////			"tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tensor_filter framework=tensorflow-lite model=%s !"
////			"tensor_decoder mode=bounding_boxes option1=tflite-ssd option2=%s option3=%s option4=640:480 option5=300:300 !"
////			"videomixer name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB,framerate=30/1 ! tensor_converter ! tensor_sink name=res_sink "
////			"t. ! queue leaky=2 max-size-buffers=10 ! mix.", ssd_model_info.model_path, ssd_model_info.label_path, ssd_model_info.box_prior_path);
//
//
////	str_pipeline = g_strdup_printf("appsrc name=res_source ! videoconvert ! videoscale ! video/x-raw, format=RGB, width=640, height=480, framerate=30/1 ! videoflip method=clockwise ! "
////            "videocrop top=80 bottom=80 ! jpegenc ! tcpserversink host=192.168.0.2 port=4000 ");
//
////	str_pipeline = g_strdup_printf("appsrc name=res_source ! videoconvert ! videoscale ! video/x-raw, format=RGB, width=640, height=480, framerate=30/1 ! videoflip method=clockwise ! "
////            "videocrop top=80 bottom=80 ! jpegenc ! tcpserversink host=192.168.0.109 port=4000 ");
//
//	data->media_width = 640;
//	data->media_height = 480;
//
////	// THIS WORKS
//	str_pipeline = g_strdup_printf(
//			"appsrc name=res_source ! videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=BGRA,framerate=30/1 !"
//			"tee name=t t. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter !"
//			"tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tensor_filter framework=tensorflow-lite model=%s !"
//			"tensor_decoder mode=bounding_boxes option1=tflite-ssd option2=%s option3=%s option4=%d:%d option5=300:300 ! videomixer name=mix sink_0::zorder=1 !"
//			"videoconvert ! video/x-raw,width=%d,height=%d,format=BGRA ! tensor_converter ! tensor_sink name=res_sink async=true", data->media_width, data->media_height,
//			model_path, label_path, box_prior_path,
//			data->media_width, data->media_height, data->media_width, data->media_height);
//
//
////		str_pipeline = g_strdup_printf(
////				"v4l2src device=/dev/video0 ! videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=RGB,framerate=30/1 !"
////				"tee name=t t. ! queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter !"
////				"tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tensor_filter framework=tensorflow-lite model=%s !"
////				"tensor_decoder mode=bounding_boxes option1=tflite-ssd option2=%s option3=%s option4=%d:%d option5=300:300 !"
////				"videomixer name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=BGRA,framerate=30/1 ! tensor_converter ! tensor_sink name=res_sink "
////				"t. ! queue leaky=2 max-size-buffers=2 ! mix.", data->media_width, data->media_height,
////				model_path, label_path, box_prior_path,
////				data->media_width, data->media_height, data->media_width, data->media_height);
//
////	str_pipeline = g_strdup_printf(
////			"appsrc name=res_source ! videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=RGB !"
////			"tee name=t t. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter !"
////			"tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tensor_filter framework=tensorflow-lite model=%s !"
////			"tensor_decoder mode=bounding_boxes option1=tflite-ssd option2=%s option3=%s option4=%d:%d option5=300:300 ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB,framerate=30/1 !"
////			"videomixer name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=BGRA ! tensor_converter ! tensor_sink name=res_sink "
////			"t. ! queue leaky=2 max-size-buffers=2 ! mix.", data->media_width, data->media_height,
////			model_path, label_path, box_prior_path,
////			data->media_width, data->media_height, data->media_width, data->media_height);
//
////		str_pipeline = g_strdup_printf(
////				"appsrc name=res_source ! videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=RGB,framerate=30/1 !"
////				"tee name=t t. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter !"
////				"tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tensor_filter framework=tensorflow-lite model=%s !"
////				"tensor_decoder mode=bounding_boxes option1=tflite-ssd option2=%s option3=%s option4=%d:%d option5=300:300 ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB,framerate=30/1 !"
////				"videomixer name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=BGRA,framerate=30/1 ! tensor_converter ! tensor_sink name=res_sink "
////				"t. ! queue leaky=2 max-size-buffers=2 ! mix.", data->media_width, data->media_height,
////				model_path, label_path, box_prior_path,
////				data->media_width, data->media_height, data->media_width, data->media_height);
//
////	str_pipeline = g_strdup_printf(
////			"appsrc name=res_source ! videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=RGB,framerate=30/1 !"
////			"tee name=t t. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=BGRA,framerate=30/1 ! tensor_converter ! tensor_sink name=res_sink ",
////			data->media_width, data->media_height,
////			data->media_width, data->media_height);
//
////	str_pipeline = g_strdup_printf("appsrc name=res_source ! videoconvert ! video/x-raw, format=RGB, width=640, height=480, framerate=30/1 ! videoflip method=clockwise ! "
////            "videocrop top=80 bottom=80 ! jpegenc ! tcpserversink host=192.168.0.135 port=3000 ");
//
////	str_pipeline = g_strdup_printf("appsrc name=res_source ! videoconvert ! videoscale ! video/x-raw, format=RGB, width=480, height=640, framerate=30/1 ! "
////            "queue ! videocrop top=80 bottom=80 ! jpegenc  ! tee name = t t. ! queue ! tcpserversink host=192.168.0.135 port=3000 "
////			"t. ! queue ! multifilesink location=/tmp/file-%d.jpeg");
////
////	str_pipeline = g_strdup_printf("appsrc name=res_source ! videoconvert ! video/x-raw, format=RGB, width=640, height=480, framerate=30/1 ! videoflip method=clockwise ! "
////			"tee name =t "
//////            "t. ! queue ! videocrop top=80 bottom=80 ! jpegenc ! tcpserversink host=192.168.0.135 port=3000 "
////			"t. ! videoconvert ! filesink location=/tmp/file_record.avi", filesrc_path);
//
////	str_pipeline = g_strdup_printf(
////			"appsrc name=res_source ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB,framerate=30/1 !"
////			"tee name=t t. ! queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter !"
////			"tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tensor_filter framework=tensorflow-lite model=%s !"
////			"tensor_decoder mode=bounding_boxes option1=tflite-ssd option2=%s option3=%s option4=640:480 option5=300:300 ! "
////			"tensor_converter ! tensor_sink name=res_sink ",ssd_model_info.model_path, ssd_model_info.label_path, ssd_model_info.box_prior_path);
//
//	data->pipeline = gst_parse_launch (str_pipeline, &error);
//	g_free (str_pipeline);
//
//	if (error) {
//		gchar *message =
//				g_strdup_printf ("Unable to build pipeline: %s", error->message);
//		g_clear_error (&error);
//
//		dlog_print (DLOG_DEBUG, LOG_TAG, "%s", message);
//		g_free (message);
//		goto error_free_model_info;
//	}
//
//
////	data->appsrc = gst_bin_get_by_name (GST_BIN (data->pipeline), "res_source");
////	g_signal_connect (data->appsrc, "need-data", G_CALLBACK (need_data_cb), NULL);
//
//	// TODO: this has to be I420 for emulator and NV12 for device
//	 msrc_cap =
//	     gst_caps_new_simple ("video/x-raw", "width", G_TYPE_INT, 640, "height",
//	     G_TYPE_INT, 480, "format", G_TYPE_STRING, "I420", "framerate",
//	     GST_TYPE_FRACTION, 30, 1, NULL);
//
//	 gst_app_src_set_caps (GST_APP_SRC (data->appsrc), msrc_cap);
//	 gst_caps_unref (msrc_cap);
//
//	data->appsink = gst_bin_get_by_name (GST_BIN (data->pipeline), "res_sink");
//	g_signal_connect (data->appsink, "new-data", G_CALLBACK (collect_data_cb), data);
//
//	ret = gst_element_set_state (GST_ELEMENT (data->pipeline), GST_STATE_NULL);
//	if (ret == GST_STATE_CHANGE_FAILURE) {
//				GST_ERROR ("This is the issue");
//			}
//
//	ret = gst_element_set_state (GST_ELEMENT (data->pipeline), GST_STATE_READY);
//	if (ret == GST_STATE_CHANGE_FAILURE) {
//				GST_ERROR ("This is the issue");
//			}
//
//
//	// data->video_sink =
//	//   gst_bin_get_by_interface (GST_BIN (data->pipeline),
//	//       GST_TYPE_VIDEO_OVERLAY);
//	// if (!data->video_sink) {
//	//   GST_ERROR ("Could not retrieve video sink");
//	//   return NULL;
//	// }
//
//	/* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
//	bus = gst_element_get_bus (data->pipeline);
//	bus_source = gst_bus_create_watch (bus);
//	g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func,
//			NULL, NULL);
//	g_source_attach (bus_source, data->context);
//	g_source_unref (bus_source);
//	g_signal_connect (G_OBJECT (bus), "message::error", (GCallback) error_cb,
//			data);
//	g_signal_connect (G_OBJECT (bus), "message::state-changed",
//			(GCallback) state_changed_cb, data);
//	gst_object_unref (bus);
//
//	ret = gst_element_set_state (GST_ELEMENT (data->pipeline), GST_STATE_PAUSED);
//	if (ret == GST_STATE_CHANGE_FAILURE) {
//			GST_ERROR ("This is the issue");
//		}
//
//	ret = gst_element_set_state (GST_ELEMENT (data->pipeline), GST_STATE_PLAYING);
//	if (ret == GST_STATE_CHANGE_FAILURE) {
//			GST_ERROR ("This is the issue");
//		}
//
//
//	GstState state, pending;
//	ret = gst_element_get_state (GST_ELEMENT (data->pipeline), &state, &pending, 0);
//	if (ret == GST_STATE_CHANGE_FAILURE) {
//		GST_ERROR ("This is the issue");
//	}
//
//
//	// /* Create a GLib Main Loop and set it to run */
//	// GST_DEBUG ("Entering main loop... (CustomData:%p)", data);
//	// data->main_loop = g_main_loop_new (data->context, FALSE);
//	// g_main_loop_run (data->main_loop);
//
//	// GST_DEBUG ("Exited main loop");
//	// g_main_loop_unref (data->main_loop);
//	// data->main_loop = NULL;
//
//	// /* Free resources */
//	// g_main_context_pop_thread_default (data->context);
//	// g_main_context_unref (data->context);
//	// gst_element_set_state (data->pipeline, GST_STATE_NULL);
//
//	// g_usleep (100 * 1000);
//	// gst_object_unref (data->video_sink);
//	// gst_object_unref (data->pipeline);
//  data->initialized = TRUE;
//  return;
//
//error_free_model_info:
////  g_free (ssd_model_info.model_path);
////  g_free (ssd_model_info.label_path);
////  g_free (ssd_model_info.box_prior_path);
//
//  return;
//}
//
//int
//gst_run (CustomData * data, camera_preview_data_s *frame)
//{
//	if (prepare_buffer (data, frame) == GST_FLOW_ERROR) {
//		dlog_print (DLOG_DEBUG, LOG_TAG, "Error in preparing the buffer");
//		return -1;
//	}
//
////	unsigned char * proc_data;
////	size_t size;
////	get_buffer (proc_data, &size);
////	g_main_context_iteration (g_main_context_default (), FALSE);
//
//	return 0;
//}
//
//void
//gst_uninitialize (CustomData * data)
//{
//  data->initialized = FALSE;
//	gst_element_set_state (data->pipeline, GST_STATE_NULL);
//	gst_object_unref (GST_OBJECT (data->pipeline));
//	gst_object_unref (GST_OBJECT (data->appsrc));
//	gst_object_unref (GST_OBJECT (data->appsink));
//}

ml_pipeline_h handle;
ml_pipeline_src_h srchandle;
ml_pipeline_sink_h sinkhandle;
ml_tensors_info_h info;

uint8_t * inArray;
gchar **labels;
gchar * pipeline;

/**
 * @brief Initialize the pipeline with model path
 */
void init_pipeline() {
  char *res_path = app_get_resource_path();
  gchar * model_path = g_strdup_printf("%s/mobilenet_v1_1.0_224_quant.tflite",
      res_path);
  ml_tensor_dimension dim = { CH, CAM_WIDTH, CAM_HEIGHT, 1 };

  pipeline =
      g_strdup_printf(
          "appsrc name=srcx ! "
              "video/x-raw,format=RGB,width=%d,height=%d,framerate=0/1 ! "
              "videoconvert ! videoscale ! video/x-raw,format=RGB,width=%d,height=%d ! "
              "videoflip method=clockwise ! "
              "tensor_converter ! tensor_filter framework=tensorflow-lite model=\"%s\" ! "
              "tensor_sink name=sinkx", CAM_WIDTH, CAM_HEIGHT,
          MODEL_WIDTH, MODEL_HEIGHT, model_path);

  inArray = (uint8_t *) g_malloc(sizeof(uint8_t) * CAM_WIDTH * CAM_HEIGHT * CH);

  ml_pipeline_construct(pipeline, NULL, NULL, &handle);

  ml_pipeline_src_get_handle(handle, "srcx", &srchandle);
  ml_pipeline_sink_register(handle, "sinkx", get_label_from_tensor_sink, NULL,
      &sinkhandle);

  /**
   *  The media type `video/x-raw` cannot get valid info handle, you should set info handle in person like below.
   *  `ml_pipeline_src_get_tensors_info(srchandle, &info);`
   */

  ml_tensors_info_create(&info);
  ml_tensors_info_set_count(info, 1);
  ml_tensors_info_set_tensor_type(info, 0, ML_TENSOR_TYPE_UINT8);
  ml_tensors_info_set_tensor_dimension(info, 0, dim);

  g_free(model_path);
  free(res_path);
}

/**
 * @brief This callback function is called once after the main loop of the application exits.
 *
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void app_terminate(void *user_data) {
  /* Release all resources. */
  ml_pipeline_sink_unregister(sinkhandle);
  ml_pipeline_src_release_handle(srchandle);
  ml_pipeline_destroy(handle);
  ml_tensors_info_destroy(info);

  g_free(pipeline);
  g_free(inArray);
  g_strfreev(labels);
}

/**
 * @brief get label with the output of tensor_sink.
 */
void
get_label_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  size_t data_size;
  uint8_t *output;
  int label_num = -1, max = -1;

  ml_tensors_data_get_tensor_data (data, 0, (void **) &output, &data_size);

  for (int i = 0; i < 1001; i++) {
    if (output[i] > 0 && output[i] > max) {
      max = output[i];
      label_num = i;
    }
  }

  G_LOCK (callback_lock);
  is_checking = 0;
  result_label = label_num;
  image_util_transform_destroy (t_handle);

  camera_set_media_packet_preview_cb (cam_data.g_camera, preview_frame_cb, NULL);
  G_UNLOCK (callback_lock);
}

/**
 * @brief when the preview frame is sent, this callback runs.
 */
static void
preview_frame_cb (media_packet_h pkt, void *user_data)
{
  camera_unset_media_packet_preview_cb (cam_data.g_camera);

  image_util_transform_create (&t_handle);
  image_util_transform_set_colorspace (t_handle, IMAGE_UTIL_COLORSPACE_RGB888);
  image_util_transform_run (t_handle, pkt, transform_completed_cb, pkt);
  return;
}

/**
 * @brief when the image transform is finished, this callback runs.
 */
static void
transform_completed_cb (media_packet_h * dst, int error_code, void *user_data)
{
  media_packet_h src = (media_packet_h) user_data;
  uint8_t *input_buf;

  media_packet_get_buffer_data_ptr (*dst, (void **) &input_buf);

  find_label (input_buf);

  media_packet_destroy (*dst);
  media_packet_destroy (src);
}

/**
 * @brief set the input and tensor data to get the result.
 */
static int
find_label (uint8_t * input_buf)
{
  ml_tensors_data_h data;
  unsigned int data_size = CH * CAM_WIDTH * CAM_HEIGHT;

  memcpy (inArray, input_buf, sizeof (uint8_t) * data_size);

  /* insert input data to appsrc */
  ml_tensors_data_create (info, &data);
  ml_tensors_data_set_tensor_data (data, 0, inArray, data_size);
  ml_pipeline_src_input_data (srchandle, data,
      ML_PIPELINE_BUF_POLICY_AUTO_FREE);

  return 0;
}
