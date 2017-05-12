#pragma once

#include "ofMain.h"
#include <set>


class execThread : public ofThread{
public:
    execThread();
    void setup(string command);
    void threadedFunction();
    bool isInitialized() { return initialized; }
private:
    string execCommand;
    bool initialized;
};

struct audioFrameShort {
    short * data;
    int size;
};

//--------------------------------------------------------------
//--------------------------------------------------------------
class ofxVideoDataWriterThread : public ofThread {
public:
    ofxVideoDataWriterThread();
//    void setup(ofFile *file, lockFreeQueue<ofPixels *> * q);
	void setup(string filePath, ofThreadChannel<ofPixels> * q);
	void threadedFunction();
    void setPipeNonBlocking();
    bool isWriting() { return bIsWriting; }
	void close() { bClose = true; queue->close(); stopThread(); waitForThread(); }
    bool bNotifyError;
private:
    string filePath;
    int fd;
	ofThreadChannel<ofPixels> * queue;
    bool bIsWriting;
    bool bClose;
};

//--------------------------------------------------------------
//--------------------------------------------------------------
class ofxAudioDataWriterThread : public ofThread {
public:
    ofxAudioDataWriterThread();
//    void setup(ofFile *file, lockFreeQueue<audioFrameShort *> * q);
	void setup(string filePath, ofThreadChannel<audioFrameShort *> * q);
	void threadedFunction();
    void setPipeNonBlocking();
    bool isWriting() { return bIsWriting; }
	void close() { bClose = true; queue->close(); stopThread(); waitForThread();  }
    bool bNotifyError;
private:
//    ofFile * writer;
    string filePath;
    int fd;
	ofThreadChannel<audioFrameShort *> * queue;
    bool bIsWriting;
    bool bClose;
};


//--------------------------------------------------------------
//--------------------------------------------------------------
class ofxVideoRecorderOutputFileCompleteEventArgs
: public ofEventArgs
{
public:
    string fileName;
};

//--------------------------------------------------------------
//--------------------------------------------------------------
class ofxVideoRecorder
{
public:
	ofxVideoRecorder();

    ofEvent<ofxVideoRecorderOutputFileCompleteEventArgs> outputFileCompleteEvent;

    bool setup(string fname, int w, int h, float fps, int sampleRate=0, int channels=0, bool sysClockSync=false, bool silent=false);
    bool setupCustomOutput(int w, int h, float fps, string outputString, bool sysClockSync=false, bool silent=false);
    bool setupCustomOutput(int w, int h, float fps, int sampleRate, int channels, string outputString, bool sysClockSync=false, bool silent=false);

    bool addFrame(const ofPixels &pixels);
    void addAudioSamples(float * samples, int bufferSize, int numChannels);

    void start();
    void close();
    void setPaused(bool bPause);

    bool hasVideoError();
    bool hasAudioError();

    void setFfmpegLocation(string loc) { ffmpegLocation = loc; }
    void setVideoCodec(string codec) { videoCodec = codec; }
    void setAudioCodec(string codec) { audioCodec = codec; }
    void setVideoBitrate(string bitrate) { videoBitrate = bitrate; }
    void setAudioBitrate(string bitrate) { audioBitrate = bitrate; }

    void setPixelFormat( string pixelF){ //rgb24 || gray, default is rgb24
        pixelFormat = pixelF;
    };
    void setOutputPixelFormat(string pixelF) {
        outputPixelFormat = pixelF;
    }

    unsigned long long getNumVideoFramesRecorded() { return videoFramesRecorded; }
    unsigned long long getNumAudioSamplesRecorded() { return audioSamplesRecorded; }

//    int getVideoQueueSize(){ return frames.size(); }
//    int getAudioQueueSize(){ return audioFrames.size(); }

    bool isInitialized(){ return bIsInitialized; }
    bool isRecording() { return bIsRecording; };
    bool isPaused() { return bIsPaused; };
    bool isSyncAgainstSysClock() { return bSysClockSync; };

    string getMoviePath(){ return moviePath; }
    int getWidth(){return width;}
    int getHeight(){return height;}

private:
    string fileName;
    string moviePath;
    string videoPipePath, audioPipePath;
    string ffmpegLocation;
    string videoCodec, audioCodec, videoBitrate, audioBitrate, pixelFormat, outputPixelFormat;
    int width, height, sampleRate, audioChannels;
    float frameRate;

    bool bIsInitialized;
    bool bRecordAudio;
    bool bRecordVideo;
    bool bIsRecording;
    bool bIsPaused;
    bool bFinishing;
    bool bIsSilent;

    bool bSysClockSync;
    float startTime;
    float recordingDuration;
    float totalRecordingDuration;
    float systemClock();

	ofThreadChannel<ofPixels> frames;
	ofThreadChannel<audioFrameShort *> audioFrames;
    unsigned long long audioSamplesRecorded;
    unsigned long long videoFramesRecorded;
    ofxVideoDataWriterThread videoThread;
    ofxAudioDataWriterThread audioThread;
    execThread ffmpegThread;
//    ofFile videoPipe, audioPipe;
    int videoPipeFd, audioPipeFd;
    int pipeNumber;

    static set<int> openPipes;
    static int requestPipeNumber();
    static void retirePipeNumber(int num);

    void outputFileComplete();
};
