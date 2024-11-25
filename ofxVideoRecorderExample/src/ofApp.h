#pragma once

#include "ofMain.h"
#include "ofxVideoRecorder.h"

// 
// https://stackoverflow.com/questions/51903888/is-it-possible-to-send-ffmpeg-images-by-using-pipe
// https://rmauro.dev/how-to-record-video-and-audio-with-ffmpeg-a-minimalist-approach/
// https://stackoverflow.com/questions/63762351/ffmpeg-raw-video-and-audio-stdin
// https://askubuntu.com/questions/803925/using-ffmpeg-to-encode-from-yuv-to-m4v-with-mpeg-4-encoder
// 

class ofApp : public ofBaseApp{

public:
    void setup();
    void update();
    void draw();
    void exit();
    void keyReleased(int key);
    void audioIn(ofSoundBuffer& buffer);

    ofVideoGrabber vidGrabber;
    ofSoundStream soundStream;
    ofxVideoRecorder vidRecorder;
    bool bRecording;
    int sampleRate;
    int channels;

    void recordingComplete(ofxVideoRecorderOutputFileCompleteEventArgs& args);

    ofFbo recordFbo;
    ofPixels recordPixels;
};
