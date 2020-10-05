import os
import sys
import logging
import gi
import cairo
import numpy as np

gi.require_version("Gst", "1.0")
gi.require_foreign('cairo')
gi.require_version('Gtk', '3.0')
from gi.repository import Gst, GObject

os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'

VIDEO_WIDTH = 640
VIDEO_HEIGHT = 480

BOX_SIZE = 4
LABEL_SIZE = 91
DETECTION_MAX = 100

MAX_OBJECT_DETECTION = 5

class NNStreamerExample : 
    """NNStreamer example for object dectection"""

    def __init__(self, argv=None):
        self.loop = None
        self.pipeline = None
        self.running = False

        self.current_label_index = -1
        self.new_label_index = -1

        #model
        self.tf_model = ""
        self.tf_labels = []

        #output names
        self.num_detections = 0.0
        self.detection_classes = 0.0
        self.detection_scores = 0.0
        self.detection_boxes = 0.0

        self.detected_objects = []

        #drawing
        self.x = 0
        self.y = 0
        self.width = 0
        self.height = 0
        self.class_id = 0
        self.prob = 0.0
        
        #CairoOverlay
        self.valid = False
        self.vinfo = -1

    
        if not self.tf_init():
            raise Exception

        GObject.threads_init()
        Gst.init(argv)
    


    def run_example(self):
        """Init pipline and run example 
        
        :return: None
        """

        # main loop
        self.loop = GObject.MainLoop()

        # init pipline
        self.pipeline = Gst.parse_launch(
            #"v4l2src name=src ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB ! tee name=t_raw "
            "filesrc location=/home/jawthrow/project/JawThrow/ddeokho/nnstreamer-example/native/example_object_detection_tensorflow/sample.mp4 ! decodebin ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB ! tee name=t_raw "
            "t_raw. ! queue ! videoconvert ! cairooverlay name=tensor_res ! ximagesink name=img_tensor "
            "t_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! tensor_converter ! "
            "tensor_filter framework=tensorflow model="+self.tf_model+
            " input=3:640:480:1 inputname=image_tensor inputtype=uint8 "
            "output=1,100:1,100:1,4:100:1 "
            "outputname=num_detections,detection_classes,detection_scores,detection_boxes "
            "outputtype=float32,float32,float32,float32 ! "
            "tensor_sink name=tensor_sink " 
        )

        #bus and message callback
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect('message', self.on_bus_message)

        #tensor sink signal : new data callbaack
        tensor_sink = self.pipeline.get_by_name('tensor_sink')
        tensor_sink.connect('new-data', self.new_data_cb)

        #cario overlay
        cairo_overlay = self.pipeline.get_by_name('tensor_res')
        cairo_overlay.connect('draw', self.draw_overlay_cb)
        #cario_overlay.connect('caps-change', self.prepare_overlay_cb)


        #timer to update result
        #GObject.timeout_add(500, self.on_timer_update_result)


        #start pipeline
        self.pipeline.set_state(Gst.State.PLAYING)
        self.running = True


        #Set window title
        self.set_window_title("img_tensor","NNStreamer Example")

        #run main loop
        self.loop.run()

        #quit when received eos or error message
        self.running = False
        self.pipeline.set_state(Gst.State.NULL)


        bus.remove_signal_watch()


    def on_bus_message(self, bus, message):

        if message.type == Gst.MessageType.EOS:
            logging.info('received eos message')
            self.loop.quit()
        elif message.type == Gst.MessageType.ERROR:
            error, debug = message.parse_error()
            logging.warning('[error] %s : %s', error.message, debug)
            self.loop.quit()
        elif message.type == Gst.MessageType.WARNING:
            error, debug = message.parse_warning()
            logging.warning('[warning] %s : %s',error.message, debug)
            
        elif message.type == Gst.MessageType.STREAM_START:
            logging.info('recesived start message')

        elif message.type == Gst.MessageType.QOS:
            data_format, processed, dropped = message.parse_qos_stats()
            format_str = Gst.Format.get_name(data_format)
            logging.debug('[qos] format[%s] processed[%d] dropped[%d]', format_str, processed, dropped)



    def get_detected_objects(self, num_detections, detection_classes, detection_scores, detection_boxes):

        self.detected_objects=[]
        
        print("========================================================")
        print("                 Number Of Objects: "+num_detections[0])
        print("========================================================")

        for i in range(len(num_detections[0])):
            if(i<num_detections[0]):

                self.class_id = detection_classes[i]
                self.x = detection_boxes[i * BOX_SIZE + 1] * VIDEO_WIDTH
                self.y = detection_boxes[i * BOX_SIZE] * VIDEO_HEIGHT

                print("=====")
                print(detection_boxes)
                print("=====")


                self.width = (detection_boxes[i * BOX_SIZE + 3] - detection_boxes[i * BOX_SIZE + 1]) * VIDEO_WIDTH
                self.height = (detection_boxes[i * BOX_SIZE + 2] - detection_boxes[i * BOX_SIZE]) * VIDEO_HEIGHT
                self.prob = detection_scores[i]


                self.detected_objects.append(num_detections[i], detection_classes[i], detection_scores[i], detection_boxes[i], self.x, self.y, self.width, self.height, self.class_id, self.prob)

                

            else :
                break
        
        print("========================================================")



    def new_data_cb(self, sink, buffer):
    
        if(self.running == False):
            self.loop.quit()

        assert buffer.n_memory() == 4
        
        #num_detections
        mem_num = buffer.get_memory(0)
        assert mem_num.map(Gst.MapFlags.READ)
        _, info_num = mem_num.map(Gst.MapFlags.READ)
        assert info_num.size == 4
        self.num_detections=info_num.data


        #detection_classes
        mem_classes = buffer.get_memory(1)
        assert mem_classes.map(Gst.MapFlags.READ)
        _, info_calsses = mem_classes.map(Gst.MapFlags.READ)
        assert info_calsses.size == DETECTION_MAX * 4
        self.detection_classes = info_calsses.data

        #detection_score
        mem_score = buffer.get_memory(2)
        assert mem_score.map(Gst.MapFlags.READ)
        _, info_scores = mem_score.map(Gst.MapFlags.READ)
        assert info_scores.size == DETECTION_MAX * 4
        self.detection_scores = info_scores.data

        #detection_bosxs
        mem_boxes = buffer.get_memory(3)
        assert mem_boxes.map(Gst.MapFlags.READ)
        _, info_boxs = mem_boxes.map(Gst.MapFlags.READ)
        assert info_boxs.size == DETECTION_MAX * BOX_SIZE * 4
        self.detection_boxes = info_boxs.data


        self.get_detected_objects(self.num_detections, self.detection_classes, self.detection_scores, self.detection_boxes)



    def set_window_title(self, name, title):
        """Set window title.

        :param name: GstXImageSink element name
        :param title: window title
        :return: None
        """
        element = self.pipeline.get_by_name(name)
        if element is not None:
            pad = element.get_static_pad('sink')
            if pad is not None:
                tags = Gst.TagList.new_empty()
                tags.add_value(Gst.TagMergeMode.APPEND, 'title', title)
                pad.send_event(Gst.Event.new_tag(tags))



    def draw_overlay_cb(self, _overlay, context, _timestamp, _duration):
        
 
        drawed = 0

        detected = self.detected_objects
        print(detected)

        context.select_font_face('Sans', cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        context.set_font_size(20)
        
        iter = range(detected[0])
        for iter in range(len(detected)):
            if(iter != detected[-1]):

                x = iter[0]
                y = iter[1]
                width = iter[2]
                height = iter[3]
                
                #draw rectangle
                context.rectangle(x,y,width, height)
                context.set_source_rgb(1,0,0)
                context.set_line_width(1)
                context.stroke()
                context.fill_preserve()


                #draw title
                context.move_to(x + 5, y + 25)
                context.text_path(label)
                context.set_source_rgb(1, 0, 0)
                context.fill_preserve()
                context.set_source_rgb(1, 1, 1)
                context.set_line_width(0.3)
                context.stroke()
                context.fill_preserve()

                if drawed >= MAX_OBJECT_DETECTION :
                    break
                
                drawed+1

            else :
                break
        

    def tf_init(self):

        tf_model = "ssdlite_mobilenet_v2.pb"
        tf_label = "coco_labels_list.txt"
        current_folder = os.path.dirname(os.path.abspath(__file__))
        model_folder = os.path.join(current_folder,"tf_model")


        #check model file exists
        self.tf_model = os.path.join(model_folder,tf_model)
        if not os.path.exists(self.tf_model):
            print('cannot find tf model [%s]', self.tf_model)
            return False
        
        #load labels
        label_path = os.path.join(model_folder,tf_label)
        try:
            with open(label_path,'r') as label_file:
                for line in label_file.readlines():
                    self.tf_labels.append(line)

        except FileNotFoundError:
            print('cannot find tf label [%s]',label_path)
            return False
        
        print('finished to load labels, total [%d]',len(self.tf_labels))
        return True


if __name__ == '__main__':
    example = NNStreamerExample(sys.argv[1:])
    example.run_example()
