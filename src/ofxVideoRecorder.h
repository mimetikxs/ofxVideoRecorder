#pragma once

#include "ofMain.h"
#include <set>

#ifdef TARGET_WIN32
#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#endif

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
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
	void setup(string filePath, ofThreadChannel<ofPixels> * q);
#elif defined(TARGET_WIN32)
    void setup(HANDLE pipeHandle, ofThreadChannel<ofPixels>* q);
#endif
	void threadedFunction();
    void setPipeNonBlocking();
    bool isWriting() { return bIsWriting; }
	void close() { bClose = true; queue->close(); stopThread(); waitForThread(); }
    bool bNotifyError;
private:
    string filePath;
#if defined(TARGET_WIN32)
    HANDLE videoHandle;
    HANDLE fileHandle;
#endif
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
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
    void setup(string filePath, ofThreadChannel<audioFrameShort *> * q);
#elif defined(TARGET_WIN32)
    void setup(HANDLE pipeHandle, ofThreadChannel<audioFrameShort *> * q);
#endif
	void threadedFunction();
    void setPipeNonBlocking();
    bool isWriting() { return bIsWriting; }
	void close() { bClose = true; queue->close(); stopThread(); waitForThread();  }
    bool bNotifyError;
private:
    string filePath;
#if defined(TARGET_WIN32)
    HANDLE audioHandle;
    HANDLE fileHandle;
#endif
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
	ofxVideoRecorder(size_t maxFrames);

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
    string videoFileExt;
    string audioFileExt;
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
    execThread ffmpegVideoThread;
    execThread ffmpegAudioThread;
    bool vThreadRunning, aThreadRunning;
    int videoPipeFd, audioPipeFd;
    int pipeNumber;

    static set<int> openPipes;
    static int requestPipeNumber();
    static void retirePipeNumber(int num);

    void outputFileComplete();

#ifdef TARGET_WIN32
    std::wstring convertNarrowToWide(const std::string& as)
    {
        if (as.empty())    return std::wstring();
        size_t reqLength = ::MultiByteToWideChar(CP_UTF8, 0, as.c_str(), (int)as.length(), 0, 0);
        std::wstring ret(reqLength, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, as.c_str(), (int)as.length(), &ret[0], (int)ret.length());
        return ret;
    }

    wchar_t* convertCharArrayToLPCWSTR(const char* charArray)
    {
        wchar_t* wString = new wchar_t[4096];
        MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
        return wString;
    }

    std::string convertWideToNarrow(const wchar_t* s, char dfault = '?', const std::locale& loc = std::locale())
    {
        std::ostringstream stm;
        while (*s != L'\0')
        {
            stm << std::use_facet< std::ctype<wchar_t> >(loc).narrow(*s++, dfault);
        }
        return stm.str();
    }

    HANDLE hVPipe;
    HANDLE hAPipe;
    LPTSTR vPipename;
    LPTSTR aPipename;
#endif
};
