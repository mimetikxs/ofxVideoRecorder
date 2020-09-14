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
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
	void setup(string filePath, std::shared_ptr<ofThreadChannel<ofPixels>> q);
#elif defined(TARGET_WIN32)
    void setup(HANDLE pipeHandle, std::shared_ptr<ofThreadChannel<ofPixels>> q);
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
    std::shared_ptr<ofThreadChannel<ofPixels>> queue;
    bool bIsWriting;
    bool bClose;
};

//--------------------------------------------------------------
//--------------------------------------------------------------
class ofxAudioDataWriterThread : public ofThread {
public:
    ofxAudioDataWriterThread();
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
    void setup(string filePath, std::shared_ptr<ofThreadChannel<audioFrameShort *>> q);
#elif defined(TARGET_WIN32)
    void setup(HANDLE pipeHandle, std::shared_ptr<ofThreadChannel<audioFrameShort *>> q);
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
    std::shared_ptr<ofThreadChannel<audioFrameShort *>> queue;
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

struct ofxVideoRecorderSettings 
{
public:
    std::string ffmpegPath;
    bool ffmpegSilent;

    bool videoEnabled;
    int videoWidth;
    int videoHeight;
    int videoFps;
    std::string videoCodec;
    std::string videoBitrate;
    std::string pixelFormat;  // 'rgb24' or 'gray', default is 'rgb24'
    std::string outPixelFormat;
    std::string videoFileExt;

    bool audioEnabled;
    int audioSampleRate;
    int audioChannels;
    std::string audioCodec;
    std::string audioBitrate;
    std::string audioFileExt;

    bool sysClockSync;

    std::string filename;

public:
    ofxVideoRecorderSettings();

private:
    friend class ofxVideoRecorder;
};

//--------------------------------------------------------------
//--------------------------------------------------------------
class ofxVideoRecorder
{
public:
	ofxVideoRecorder();

    ofEvent<ofxVideoRecorderOutputFileCompleteEventArgs> outputFileCompleteEvent;

    bool setup(std::string filename, int videoWidth, int videoHeight, float videoFps, bool sysClockSync = false, bool ffmpegSilent = false);
    bool setup(std::string filename, int audioSampleRate, int audioChannels, bool sysClockSync = false, bool ffmpegSilent = false);
    bool setup(std::string filename, int videoWidth, int videoHeight, float videoFps, int audioSampleRate, int audioChannels, bool sysClockSync = false, bool ffmpegSilent = false);
    bool setup(ofxVideoRecorderSettings settings = ofxVideoRecorderSettings());

    bool addFrame(const ofPixels &pixels);
    void addAudioSamples(float * samples, int bufferSize, int numChannels);

    void start();
    void close();
    void setPaused(bool bPause);

    bool hasVideoError();
    bool hasAudioError();

    unsigned long long getNumVideoFramesRecorded() { return videoFramesRecorded; }
    unsigned long long getNumAudioSamplesRecorded() { return audioSamplesRecorded; }

    bool isInitialized(){ return bIsInitialized; }
    bool isRecording() { return bIsRecording; };
    bool isPaused() { return bIsPaused; };
    bool isSyncAgainstSysClock() { return bSysClockSync; };

    std::string getMoviePath(){ return moviePath; }
    int getWidth(){return settings.videoWidth;}
    int getHeight(){return settings.videoHeight;}

private:
    ofxVideoRecorderSettings settings;

    std::string moviePath;
    std::string videoPipePath;
    std::string audioPipePath;

    bool bIsInitialized;
    bool bIsRecording;
    bool bIsPaused;

    bool bSysClockSync;
    float startTime;
    float recordingDuration;
    float totalRecordingDuration;
    float systemClock();

    std::shared_ptr<ofThreadChannel<ofPixels>> videoFrames;
    std::shared_ptr<ofThreadChannel<audioFrameShort*>> audioFrames;
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

#if defined(TARGET_WIN32)

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

    HANDLE videoPipeHandle;
    HANDLE audioPipeHandle;

#endif

};
