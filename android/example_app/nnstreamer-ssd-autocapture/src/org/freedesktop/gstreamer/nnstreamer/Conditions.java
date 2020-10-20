package org.freedesktop.gstreamer.nnstreamer;

public class Conditions {
    String name;
    int count;

    Conditions() {};

    void setName(String _name){
        this.name = _name;
    }
    String getName(){
        return this.name;
    }

    void setCount(int _count){
        this.count = _count;
    }
    int getCount(){
        return this.count;
    }
}
