//
//  ofxVideoRecorder.cpp
//  ofxVideoRecorderExample
//
//  Created by Timothy Scaffidi on 9/23/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "ofxVideoRecorder.h"
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
#include <unistd.h>
#endif
#ifdef TARGET_WIN32
#include <io.h>
#endif
#include <fcntl.h>

//--------------------------------------------------------------
//--------------------------------------------------------------
int setNonBlocking(int fd){
#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
	int flags;

	/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
	/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	/* Otherwise, use the old way of doing it */
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
#endif
	return 0;
}

//--------------------------------------------------------------
//--------------------------------------------------------------
execThread::execThread(){
    execCommand = "";
    initialized = false;
}

//--------------------------------------------------------------
void execThread::setup(string command){
    execCommand = command;
    initialized = false;
	startThread();
}

//--------------------------------------------------------------
void execThread::threadedFunction(){
    if(isThreadRunning()){
        ofLogVerbose("execThread") << "starting command: " <<  execCommand;
        int result = system(execCommand.c_str());
        if (result == 0) {
            ofLogVerbose("execThread") << "command completed successfully.";
            initialized = true;
        } else {
            ofLogError("execThread") << "command failed with result: " << result;
        }
    }
}

//--------------------------------------------------------------
//--------------------------------------------------------------
ofxVideoDataWriterThread::ofxVideoDataWriterThread(){
}

//--------------------------------------------------------------
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
void ofxVideoDataWriterThread::setup(string filePath, ofThreadChannel<ofPixels> * q)
#elif defined(TARGET_WIN32)
void ofxVideoDataWriterThread::setup(HANDLE pipeHandle, ofThreadChannel<ofPixels>* q)
#endif
{
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
    this->filePath = filePath;
    fd = -1;
#elif defined(TARGET_WIN32)
    videoHandle = pipeHandle;
#endif
    queue = q;
    bIsWriting = false;
    bClose = false;
    bNotifyError = false;
	startThread();
}

//--------------------------------------------------------------
void ofxVideoDataWriterThread::threadedFunction()
{
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
    if(fd == -1){
        ofLogVerbose("ofxVideoDataWriterThread") << "opening pipe: " <<  filePath;
        fd = ::open(filePath.c_str(), O_WRONLY);
        ofLogWarning("ofxVideoDataWriterThread") << "got file descriptor " << fd;
    }
#endif

	ofPixels frame;
	while(queue->receive(frame)){
		bIsWriting = true;
		int b_offset = 0;
		int b_remaining = frame.getWidth()*frame.getHeight()*frame.getBytesPerPixel();

		while(b_remaining > 0 && isThreadRunning())
		{
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
			errno = 0;
			int b_written = ::write(fd, ((char *)frame.getData())+b_offset, b_remaining);
#elif defined(TARGET_WIN32)
            DWORD b_written;
            if (!WriteFile(videoHandle, ((char *)frame.getData()) + b_offset, b_remaining, &b_written, 0)) 
            {
                LPTSTR errorText = NULL;

                FormatMessage(
                    // use system message tables to retrieve error text
                    FORMAT_MESSAGE_FROM_SYSTEM
                    // allocate buffer on local heap for error text
                    | FORMAT_MESSAGE_ALLOCATE_BUFFER
                    // Important! will fail otherwise, since we're not 
                    // (and CANNOT) pass insertion parameters
                    | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&errorText,  // output 
                    0, // minimum size for output buffer
                    NULL);   // arguments - see note 
                //wstring ws = errorText;
                string error(errorText);
                ofLogNotice("Video Thread") << "WriteFile to pipe failed: " << error;
                break;
            }
#endif

			if(b_written > 0){
				b_remaining -= b_written;
				b_offset += b_written;
				if (b_remaining != 0) {
					ofLogWarning("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - b_remaining is not 0 -> " << b_written << " - " << b_remaining << " - " << b_offset << ".";
					// break;
				}
			}
			else if (b_written < 0) {
				ofLogError("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - write to PIPE failed with error -> " << errno << " - " << strerror(errno) << ".";
				bNotifyError = true;
				break;
			}
			else {
				if(bClose){
					ofLogVerbose("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - Nothing was written and bClose is TRUE.";
					break; // quit writing so we can close the file
				}
				ofLogWarning("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - Nothing was written. Is this normal?";
			}

			if (!isThreadRunning()) {
				ofLogWarning("ofxVideoDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - The thread is not running anymore let's get out of here!";
			}
		}
		bIsWriting = false;
	}

    ofLogVerbose("ofxVideoDataWriterThread") << "closing pipe: " <<  filePath;
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
    ::close(fd);
#elif defined(TARGET_WIN32)
    FlushFileBuffers(videoHandle);
    DisconnectNamedPipe(videoHandle);
    CloseHandle(videoHandle);
#endif
}

//--------------------------------------------------------------
void ofxVideoDataWriterThread::setPipeNonBlocking(){
    setNonBlocking(fd);
}

//--------------------------------------------------------------
//--------------------------------------------------------------
ofxAudioDataWriterThread::ofxAudioDataWriterThread(){
}

//--------------------------------------------------------------
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
void ofxAudioDataWriterThread::setup(string filePath, ofThreadChannel<audioFrameShort *> *q)
#elif defined(TARGET_WIN32)
void ofxAudioDataWriterThread::setup(HANDLE pipeHandle, ofThreadChannel<audioFrameShort*>* q)
#endif
{
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
    this->filePath = filePath;
    fd = -1;
#elif defined(TARGET_WIN32)
    audioHandle = pipeHandle;
#endif
    queue = q;
    bIsWriting = false;
    bNotifyError = false;
	startThread();
}

//--------------------------------------------------------------
void ofxAudioDataWriterThread::threadedFunction()
{
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
    if(fd == -1){
        ofLogVerbose("ofxAudioDataWriterThread") << "opening pipe: " <<  filePath;
        fd = ::open(filePath.c_str(), O_WRONLY);
        ofLogWarning("ofxAudioDataWriterThread") << "got file descriptor " << fd;
    }
#endif

	audioFrameShort * frame = NULL;
	while(queue->receive(frame) && frame){
		bIsWriting = true;
		int b_offset = 0;
		int b_remaining = frame->size*sizeof(short);
		while(b_remaining > 0 && isThreadRunning()){
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
			int b_written = ::write(fd, ((char *)frame->data)+b_offset, b_remaining);
#elif defined(TARGET_WIN32)
            DWORD b_written;
            if (!WriteFile(audioHandle, ((char*)frame->data) + b_offset, b_remaining, &b_written, 0)) 
            {
                LPTSTR errorText = NULL;

                FormatMessage(
                    // use system message tables to retrieve error text
                    FORMAT_MESSAGE_FROM_SYSTEM
                    // allocate buffer on local heap for error text
                    | FORMAT_MESSAGE_ALLOCATE_BUFFER
                    // Important! will fail otherwise, since we're not 
                    // (and CANNOT) pass insertion parameters
                    | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&errorText,  // output 
                    0, // minimum size for output buffer
                    NULL);   // arguments - see note 
                //wstring ws = errorText;
                string error(errorText);
                ofLogNotice("Audio Thread") << "WriteFile to pipe failed: " << error;
            }
#endif
			if(b_written > 0){
				b_remaining -= b_written;
				b_offset += b_written;
			}
			else if (b_written < 0) {
				ofLogError("ofxAudioDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - write to PIPE failed with error -> " << errno << " - " << strerror(errno) << ".";
				bNotifyError = true;
				break;
			}
			else {
				if(bClose){
					// quit writing so we can close the file
					break;
				}
			}

			if (!isThreadRunning()) {
				ofLogWarning("ofxAudioDataWriterThread") << ofGetTimestampString("%H:%M:%S:%i") << " - The thread is not running anymore let's get out of here!";
			}
		}
		bIsWriting = false;
		delete [] frame->data;
		delete frame;
	}

    ofLogVerbose("ofxAudioDataWriterThread") << "closing pipe: " <<  filePath;
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
    ::close(fd);
#elif defined(TARGET_WIN32)
    FlushFileBuffers(audioHandle);
    DisconnectNamedPipe(audioHandle);
    CloseHandle(audioHandle);
#endif
}

//--------------------------------------------------------------
void ofxAudioDataWriterThread::setPipeNonBlocking(){
    setNonBlocking(fd);
}

//--------------------------------------------------------------
//--------------------------------------------------------------
ofxVideoRecorder::ofxVideoRecorder(){
    bIsInitialized = false;
    ffmpegLocation = "ffmpeg";
    videoCodec = "mpeg4";
    audioCodec = "pcm_s16le";
    videoBitrate = "2000k";
    audioBitrate = "128k";
    pixelFormat = "rgb24";
    outputPixelFormat = "";
    videoFileExt = ".mp4";
    audioFileExt = ".m4a";
    aThreadRunning = false;
    vThreadRunning = false;
}

ofxVideoRecorder::ofxVideoRecorder(size_t maxFrames)
:frames(maxFrames)
,audioFrames(maxFrames)
{
	bIsInitialized = false;
	ffmpegLocation = "ffmpeg";
	videoCodec = "mpeg4";
	audioCodec = "pcm_s16le";
	videoBitrate = "2000k";
	audioBitrate = "128k";
	pixelFormat = "rgb24";
	outputPixelFormat = "";
}

//--------------------------------------------------------------
bool ofxVideoRecorder::setup(string fname, int w, int h, float fps, int sampleRate, int channels, bool sysClockSync, bool silent){
    if(bIsInitialized)
    {
        close();
    }

    fileName = fname;
    string absFilePath = ofFilePath::getAbsolutePath(fileName);

    moviePath = ofFilePath::getAbsolutePath(fileName);

    stringstream outputSettings;
	outputSettings
	<< " -c:v " << videoCodec
	<< " -b:v " << videoBitrate
	<< " -c:a " << audioCodec
	<< " -b:a " << audioBitrate
    << " \"" << absFilePath << "\"";

    return setupCustomOutput(w, h, fps, sampleRate, channels, outputSettings.str(), sysClockSync, silent);
}

//--------------------------------------------------------------
bool ofxVideoRecorder::setupCustomOutput(int w, int h, float fps, string outputString, bool sysClockSync, bool silent){
    return setupCustomOutput(w, h, fps, 0, 0, outputString, sysClockSync, silent);
}

//--------------------------------------------------------------
bool ofxVideoRecorder::setupCustomOutput(int w, int h, float fps, int sampleRate, int channels, string outputString, bool sysClockSync, bool silent){
    if(bIsInitialized)
    {
        close();
    }

    bIsSilent = silent;
    bSysClockSync = sysClockSync;

    bRecordAudio = (sampleRate > 0 && channels > 0);
    bRecordVideo = (w > 0 && h > 0 && fps > 0);
    bFinishing = false;

    videoFramesRecorded = 0;
    audioSamplesRecorded = 0;

    if(!bRecordVideo && !bRecordAudio) {
        ofLogWarning() << "ofxVideoRecorder::setupCustomOutput(): invalid parameters, could not setup video or audio stream.\n"
        << "video: " << w << "x" << h << "@" << fps << "fps\n"
        << "audio: " << "channels: " << channels << " @ " << sampleRate << "Hz\n";
        return false;
    }
    videoPipePath = "";
    audioPipePath = "";
    pipeNumber = requestPipeNumber();
    if(bRecordVideo) {
        width = w;
        height = h;
        frameRate = fps;

        // recording video, create a FIFO pipe
#if defined(TARGET_OSX) || defined(TARGET_LINUX)

        videoPipePath = ofFilePath::getAbsolutePath("ofxvrpipe" + ofToString(pipeNumber));
        if(!ofFile::doesFileExist(videoPipePath)){
            string cmd = "bash --login -c 'mkfifo " + videoPipePath + "'";
            system(cmd.c_str());
        }

#elif defined(TARGET_WIN32)

        char vpip[128];
        int num = ofRandom(1024);
        sprintf(vpip, "\\\\.\\pipe\\videoPipe%d", num);
        vPipename = vpip;

        hVPipe = CreateNamedPipe(
            vPipename, // name of the pipe
            PIPE_ACCESS_OUTBOUND, // 1-way pipe -- send only
            PIPE_TYPE_BYTE, // send data as a byte stream
            1, // only allow 1 instance of this pipe
            0, // outbound buffer defaults to system default
            0, // no inbound buffer
            0, // use default wait time
            NULL // use default security attributes
        );

        if (!(hVPipe != INVALID_HANDLE_VALUE)) {
            if (GetLastError() != ERROR_PIPE_BUSY)
            {
                ofLogError("Video Pipe") << "Could not open video pipe.";
            }
            // All pipe instances are busy, so wait for 5 seconds. 
            if (!WaitNamedPipe(vPipename, 5000))
            {
                ofLogError("Video Pipe") << "Could not open video pipe: 5 second wait timed out.";
            }
        }

#endif

    }

    if(bRecordAudio) {
        this->sampleRate = sampleRate;
        audioChannels = channels;

        // recording video, create a FIFO pipe
#if defined(TARGET_OSX) || defined(TARGET_LINUX)

        audioPipePath = ofFilePath::getAbsolutePath("ofxarpipe" + ofToString(pipeNumber));
        if(!ofFile::doesFileExist(audioPipePath)){
            string cmd = "bash --login -c 'mkfifo " + audioPipePath + "'";
            system(cmd.c_str());
        }

#elif defined(TARGET_WIN32)

        char apip[128];
        int num = ofRandom(1024);
        sprintf(apip, "\\\\.\\pipe\\videoPipe%d", num);
        aPipename = apip; //convertCharArrayToLPCWSTR(apip);

        hAPipe = CreateNamedPipe(
            aPipename,
            PIPE_ACCESS_OUTBOUND, // 1-way pipe -- send only
            PIPE_TYPE_BYTE, // send data as a byte stream
            1, // only allow 1 instance of this pipe
            0, // outbound buffer defaults to system default
            0, // no inbound buffer
            0, // use default wait time
            NULL // use default security attributes
        );

        if (!(hAPipe != INVALID_HANDLE_VALUE)) {
            if (GetLastError() != ERROR_PIPE_BUSY)
            {
                ofLogError("Audio Pipe") << "Could not open audio pipe.";
            }
            // All pipe instances are busy, so wait for 5 seconds. 
            if (!WaitNamedPipe(aPipename, 5000))
            {
                ofLogError("Audio Pipe") << "Could not open pipe: 5 second wait timed out.";
            }
        }

#endif

    }

    stringstream cmd;

#if defined(TARGET_OSX) || defined(TARGET_LINUX)

    // basic ffmpeg invocation, -y option overwrites output file
    cmd << "bash --login -c '" << ffmpegLocation << (bIsSilent?" -loglevel quiet ":" ") << "-y";
    if(bRecordAudio){
        cmd << " -acodec pcm_s16le -f s16le -ar " << sampleRate << " -ac " << audioChannels << " -i \"" << audioPipePath << "\"";
    }
    else { // no audio stream
        cmd << " -an";
    }
    if(bRecordVideo){ // video input options and file
        cmd << " -r "<< fps << " -s " << w << "x" << h << " -f rawvideo -pix_fmt " << pixelFormat <<" -i \"" << videoPipePath << "\" -r " << fps;
        if (outputPixelFormat.length() > 0)
            cmd << " -pix_fmt " << outputPixelFormat;
    }
    else { // no video stream
        cmd << " -vn";
    }
    cmd << " "+ outputString +"' &";

    // start ffmpeg thread. Ffmpeg will wait for input pipes to be opened.
    ffmpegThread.setup(cmd.str());

    // wait until ffmpeg has started
    while (!ffmpegThread.isInitialized()) {
        usleep(10);
    }

    if(bRecordAudio){
        audioThread.setup(audioPipePath, &audioFrames);
    }
    if(bRecordVideo){
        videoThread.setup(videoPipePath, &frames);
    }

#elif defined(TARGET_WIN32)

    if (bRecordAudio && bRecordVideo) 
    {
        bool fSuccess;

        // Audio Thread

        stringstream aCmd;
        aCmd << ffmpegLocation << " -y " << " -f s16le -acodec " << audioCodec << " -ar " << sampleRate << " -ac " << audioChannels;
        aCmd << " -i " << aPipename << " -b:a " << audioBitrate << " " << moviePath << "_atemp" << audioFileExt;

        ffmpegAudioThread.setup(aCmd.str());
        ofLogNotice("FFMpeg Command") << aCmd.str() << endl;

        fSuccess = ConnectNamedPipe(hAPipe, NULL);
        if (!fSuccess)
        {
            LPTSTR errorText = NULL;

            FormatMessage(
                // use system message tables to retrieve error text
                FORMAT_MESSAGE_FROM_SYSTEM
                // allocate buffer on local heap for error text
                | FORMAT_MESSAGE_ALLOCATE_BUFFER
                // Important! will fail otherwise, since we're not 
                // (and CANNOT) pass insertion parameters
                | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
                GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&errorText,  // output 
                0, // minimum size for output buffer
                NULL);   // arguments - see note 
            //wstring ws = errorText;
            string error(errorText);
            ofLogError("Audio Pipe") << "SetNamedPipeHandleState failed: " << error;
        }
        else {
            ofLogNotice("Audio Pipe") << "\n==========================\nAudio Pipe Connected Successfully\n==========================\n" << endl;
            audioThread.setup(hAPipe, &audioFrames);
        }

        // Video Thread

        stringstream vCmd;
        vCmd << ffmpegLocation << " -y " << " -r " << fps << " -s " << w << "x" << h << " -f rawvideo -pix_fmt " << pixelFormat;
        vCmd << " -i " << vPipename << " -vcodec " << videoCodec << " -b:v " << videoBitrate << " " << moviePath << "_vtemp" << videoFileExt;

        ffmpegVideoThread.setup(vCmd.str());
        ofLogNotice("FFMpeg Command") << vCmd.str() << endl;

        fSuccess = ConnectNamedPipe(hVPipe, NULL);
        if (!fSuccess)
        {
            LPTSTR errorText = NULL;

            FormatMessage(
                // use system message tables to retrieve error text
                FORMAT_MESSAGE_FROM_SYSTEM
                // allocate buffer on local heap for error text
                | FORMAT_MESSAGE_ALLOCATE_BUFFER
                // Important! will fail otherwise, since we're not 
                // (and CANNOT) pass insertion parameters
                | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
                GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&errorText,  // output 
                0, // minimum size for output buffer
                NULL);   // arguments - see note 
            //wstring ws = errorText;
            string error(errorText);
            ofLogError("Video Pipe") << "SetNamedPipeHandleState failed: " << error;
        }
        else {
            ofLogNotice("Video Pipe") << "\n==========================\nVideo Pipe Connected Successfully\n==========================\n" << endl;
            videoThread.setup(hVPipe, &frames);
        }
    }
    else {
        cmd << ffmpegLocation << " -y ";
        if (bRecordAudio) {
            cmd << " -f s16le -acodec " << audioCodec << " -ar " << sampleRate << " -ac " << audioChannels << " -i " << aPipename;
        }
        else { // no audio stream
            cmd << " -an";
        }
        if (bRecordVideo) { // video input options and file
            cmd << " -r " << fps << " -s " << w << "x" << h << " -f rawvideo -pix_fmt " << pixelFormat << " -i " << vPipename;
        }
        else { // no video stream
            cmd << " -vn";
        }
        if (bRecordAudio)
            cmd << " -b:a " << audioBitrate;
        if (bRecordVideo)
            cmd << " -vcodec " << videoCodec << " -b:v " << videoBitrate;
        cmd << " " << outputString << videoFileExt;

        ofLogNotice("FFMpeg Command") << cmd.str() << endl;

        ffmpegThread.setup(cmd.str()); // start ffmpeg thread, will wait for input pipes to be opened

        if (bRecordAudio) {
            //this blocks, so we have to call it after ffmpeg is listening for a pipe
            bool fSuccess = ConnectNamedPipe(hAPipe, NULL);
            if (!fSuccess)
            {
                LPTSTR errorText = NULL;

                FormatMessage(
                    // use system message tables to retrieve error text
                    FORMAT_MESSAGE_FROM_SYSTEM
                    // allocate buffer on local heap for error text
                    | FORMAT_MESSAGE_ALLOCATE_BUFFER
                    // Important! will fail otherwise, since we're not 
                    // (and CANNOT) pass insertion parameters
                    | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&errorText,  // output 
                    0, // minimum size for output buffer
                    NULL);   // arguments - see note 
                //wstring ws = errorText;
                string error(errorText);
                ofLogError("Audio Pipe") << "SetNamedPipeHandleState failed: " << error;
            }
            else {
                ofLogNotice("Audio Pipe") << "\n==========================\nAudio Pipe Connected Successfully\n==========================\n" << endl;
                audioThread.setup(hAPipe, &audioFrames);
            }
        }
        if (bRecordVideo) {
            //this blocks, so we have to call it after ffmpeg is listening for a pipe
            bool fSuccess = ConnectNamedPipe(hVPipe, NULL);
            if (!fSuccess)
            {
                LPTSTR errorText = NULL;

                FormatMessage(
                    // use system message tables to retrieve error text
                    FORMAT_MESSAGE_FROM_SYSTEM
                    // allocate buffer on local heap for error text
                    | FORMAT_MESSAGE_ALLOCATE_BUFFER
                    // Important! will fail otherwise, since we're not 
                    // (and CANNOT) pass insertion parameters
                    | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
                    GetLastError(),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&errorText,  // output 
                    0, // minimum size for output buffer
                    NULL);   // arguments - see note 
                //wstring ws = errorText;
                string error(errorText);
                ofLogError("Video Pipe") << "SetNamedPipeHandleState failed: " << error;
            }
            else {
                ofLogNotice("Video Pipe") << "\n==========================\nVideo Pipe Connected Successfully\n==========================\n" << endl;
                videoThread.setup(hVPipe, &frames);
            }
        }

    }

#endif

    bIsInitialized = true;
    bIsRecording = false;
    bIsPaused = false;

    startTime = 0;
    recordingDuration = 0;
    totalRecordingDuration = 0;

    return bIsInitialized;
}

//--------------------------------------------------------------
bool ofxVideoRecorder::addFrame(const ofPixels &pixels){
    if (!bIsRecording || bIsPaused) return false;

    if(bIsInitialized && bRecordVideo)
	{
        int framesToAdd = 1; // default add one frame per request

        if((bRecordAudio || bSysClockSync) && !bFinishing){

            double syncDelta;
            double videoRecordedTime = videoFramesRecorded / frameRate;

            if (bRecordAudio) {
                // if also recording audio, check the overall recorded time for audio and video to make sure audio is not going out of sync
                // this also handles incoming dynamic framerate while maintaining desired outgoing framerate
                double audioRecordedTime = (audioSamplesRecorded/audioChannels)  / (double)sampleRate;
                syncDelta = audioRecordedTime - videoRecordedTime;
            }
            else {
                // if just recording video, synchronize the video against the system clock
                // this also handles incoming dynamic framerate while maintaining desired outgoing framerate
                syncDelta = systemClock() - videoRecordedTime;
            }

            if(syncDelta > 1.0/frameRate) {
                // not enought video frames, we need to send extra video frames.
                while(syncDelta > 1.0/frameRate) {
                    framesToAdd++;
                    syncDelta -= 1.0/frameRate;
                }
                ofLogNotice(__FUNCTION__) << "ofxVideoRecorder: recDelta = " << syncDelta << ". Not enough video frames for desired frame rate, copied this frame " << framesToAdd << " times.\n";
            }
            else if(syncDelta < -1.0/frameRate){
                // more than one video frame is waiting, skip this frame
                framesToAdd = 0;
                ofLogNotice(__FUNCTION__) << "ofxVideoRecorder: recDelta = " << syncDelta << ". Too many video frames, skipping.\n";
            }
        }

		for(int i=0;i<framesToAdd;i++){
            // add desired number of frames
			frames.send(pixels);
            videoFramesRecorded++;
		}

        return true;
    }

    return false;
}

//--------------------------------------------------------------
void ofxVideoRecorder::addAudioSamples(float *samples, int bufferSize, int numChannels){
    if (!bIsRecording || bIsPaused) return;

    if(bIsInitialized && bRecordAudio){
        int size = bufferSize*numChannels;
        audioFrameShort * shortSamples = new audioFrameShort;
        shortSamples->data = new short[size];
        shortSamples->size = size;

        for(int i=0; i < size; i++){
            shortSamples->data[i] = (short)(samples[i] * 32767.0f);
        }
		audioFrames.send(shortSamples);
        audioSamplesRecorded += size;
    }
}

//--------------------------------------------------------------
void ofxVideoRecorder::start(){
    if(!bIsInitialized) return;

    if (bIsRecording) {
        // We are already recording. No need to go further.
       return;
    }

    // Start a recording.
    bIsRecording = true;
    bIsPaused = false;
    startTime = ofGetElapsedTimef();

    ofLogVerbose() << "Recording." << endl;
}

//--------------------------------------------------------------
void ofxVideoRecorder::setPaused(bool bPause){
    if(!bIsInitialized) return;

    if (!bIsRecording || bIsPaused == bPause) {
        //  We are not recording or we are already paused. No need to go further.
        return;
    }

    // Pause the recording
    bIsPaused = bPause;

    if (bIsPaused) {
        totalRecordingDuration += recordingDuration;

        // Log
        ofLogVerbose() << "Paused." << endl;
    } else {
        startTime = ofGetElapsedTimef();

        // Log
        ofLogVerbose() << "Recording." << endl;
    }
}

//--------------------------------------------------------------
void ofxVideoRecorder::close(){
    if(!bIsInitialized) return;

	while(!frames.empty() || !audioFrames.empty()){
		ofSleepMillis(100);
	}

    bIsRecording = false;

	outputFileComplete();
}

//--------------------------------------------------------------
void ofxVideoRecorder::outputFileComplete()
{
    // at this point all data that ffmpeg wants should have been consumed
    // one of the threads may still be trying to write a frame,
    // but once close() gets called they will exit the non_blocking write loop
    // and hopefully close successfully

    bIsInitialized = false;

    if (bRecordVideo) {
        videoThread.close();
    }
    if (bRecordAudio) {
        audioThread.close();
    }

#if defined(TARGET_OSX) || defined(TARGET_LINUX)

    retirePipeNumber(pipeNumber);

#elif defined(TARGET_WIN32)

    //at this point all data that ffmpeg wants should have been consumed
    // one of the threads may still be trying to write a frame,
    // but once close() gets called they will exit the non_blocking write loop
    // and hopefully close successfully

    if (bRecordAudio && bRecordVideo) {
        ffmpegAudioThread.waitForThread();
        ffmpegVideoThread.waitForThread();

        //need to do one last script here to join the audio and video recordings

        stringstream finalCmd;

        /*finalCmd << ffmpegLocation << " -y " << " -i " << filePath << "_vtemp" << movFileExt << " -i " << filePath << "_atemp" << movFileExt << " \\ ";
        finalCmd << "-filter_complex \"[0:0] [1:0] concat=n=2:v=1:a=1 [v] [a]\" \\";
        finalCmd << "-map \"[v]\" -map \"[a]\" ";
        finalCmd << " -vcodec " << videoCodec << " -b:v " << videoBitrate << " -b:a " << audioBitrate << " ";
        finalCmd << filePath << movFileExt;*/

        finalCmd << ffmpegLocation << " -y " << " -i " << moviePath << "_vtemp" << videoFileExt << " -i " << moviePath << "_atemp" << audioFileExt << " ";
        finalCmd << "-c:v copy -c:a copy -strict experimental ";
        finalCmd << moviePath << videoFileExt;

        ofLogNotice("FFMpeg Merge") << "\n==============================================\n Merge Command \n==============================================\n";
        ofLogNotice("FFMpeg Merge") << finalCmd.str();
        //ffmpegThread.setup(finalCmd.str());
        system(finalCmd.str().c_str());

        //delete the unmerged files
        stringstream removeCmd;
        ofStringReplace(moviePath, "/", "\\");
        removeCmd << "DEL " << moviePath << "_vtemp" << videoFileExt << " " << moviePath << "_atemp" << audioFileExt;
        system(removeCmd.str().c_str());

    }

#endif

    ffmpegThread.waitForThread();
    // TODO: kill ffmpeg process if its taking too long to close for whatever reason.

    // Notify the listeners.
    ofxVideoRecorderOutputFileCompleteEventArgs args;
    args.fileName = fileName;
    ofNotifyEvent(outputFileCompleteEvent, args);
}

//--------------------------------------------------------------
bool ofxVideoRecorder::hasVideoError(){
    return videoThread.bNotifyError;
}

//--------------------------------------------------------------
bool ofxVideoRecorder::hasAudioError(){
    return audioThread.bNotifyError;
}

//--------------------------------------------------------------
float ofxVideoRecorder::systemClock(){
    recordingDuration = ofGetElapsedTimef() - startTime;
    return totalRecordingDuration + recordingDuration;
}

//--------------------------------------------------------------
set<int> ofxVideoRecorder::openPipes;

//--------------------------------------------------------------
int ofxVideoRecorder::requestPipeNumber(){
    int n = 0;
    while (openPipes.find(n) != openPipes.end()) {
        n++;
    }
    openPipes.insert(n);
    return n;
}

//--------------------------------------------------------------
void ofxVideoRecorder::retirePipeNumber(int num){
    if(!openPipes.erase(num)){
        ofLogNotice() << "ofxVideoRecorder::retirePipeNumber(): trying to retire a pipe number that is not being tracked: " << num << endl;
    }
}
