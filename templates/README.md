# Template Codes for Subplugins of NNStreamer
  
## What is subplugin?


- NNStreamer's elements (e.g., ```tensor_filter``` and ```tensor_decoder```) are already plugins of GStreamer. Thus, the plugins for such plugins are called subplugins; e.g., ```tensorflow-lite``` subplugin of ```tensor_filter```.

## Description of provided templates

- [tensor_filter_subplugin](tensor_filter_subplugin): Subplugin template of a standard tensor_filter with static input and output tensor dimensions.
    - Execute ```tensor_fiter_subplugin/$ deploy.sh [name] [deploy-path]``` to create a new git repo based on the template code.
    - Packaging file for Tizen (>= 5.5 M2) is provided.
    - Example:

```
tensor_filter_subplugin$ deploy sample /tmp/X/
..
..
tensor_filter_subplugin$ cd /tmp/X/
/tmp/X/$ gbs build
..
..
/tmp/X/$
```
