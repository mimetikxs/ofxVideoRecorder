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
#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#endif
#include <fcntl.h>

#include "ofLog.h"

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
void execThread::setup(const std::string& command){
    execCommand = command;
    initialized = false;
	startThread();
}

//--------------------------------------------------------------
void execThread::threadedFunction(){
    if(isThreadRunning()){
        ofLogVerbose(__FUNCTION__) << "Starting command: " <<  execCommand;
        int result = system(execCommand.c_str());
        if (result == 0) {
            ofLogVerbose(__FUNCTION__) << "Command completed successfully.";
            initialized = true;
        } else {
            ofLogError(__FUNCTION__) << "Command failed with result: " << result;
        }
    }
}

//--------------------------------------------------------------
//--------------------------------------------------------------
ofxVideoDataWriterThread::ofxVideoDataWriterThread(){
}

//--------------------------------------------------------------
#if defined(TARGET_OSX) || defined(TARGET_LINUX)
void ofxVideoDataWriterThread::setup(std::string filePath, std::shared_ptr<ofThreadChannel<ofPixels>> q)
#elif defined(TARGET_WIN32)
void ofxVideoDataWriterThread::setup(HANDLE pipeHandle, std::shared_ptr<ofThreadChannel<ofPixels>> q)
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
        ofLogVerbose(__FUNCTION__) << "Opening pipe: " <<  filePath;
        fd = ::open(filePath.c_str(), O_WRONLY);
        ofLogWarning(__FUNCTION__) << "Got file descriptor " << fd;
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
                std::string error(errorText);
                ofLogError(__FUNCTION__) << "WriteFile to pipe failed: " << error;
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
void ofxAudioDataWriterThread::setup(std::string filePath, std::shared_ptr<ofThreadChannel<audioFrameShort*>> q)
#elif defined(TARGET_WIN32)
void ofxAudioDataWriterThread::setup(HANDLE pipeHandle, std::shared_ptr<ofThreadChannel<audioFrameShort*>> q)
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
                std::string error(errorText);
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
ofxVideoRecorderSettings::ofxVideoRecorderSettings()
    : ffmpegPath("ffmpeg")
    , ffmpegSilent(false)
    , videoEnabled(true)
    , videoWidth(640)
    , videoHeight(480)
    , videoFps(30)
    , videoCodec("mpeg4")
    , videoBitrate("2000k")
    , pixelFormat("rgb24")
    , outPixelFormat("")
    , videoFileExt(".mov")
    , audioEnabled(true)
    , audioSampleRate(44100)
    , audioChannels(2)
    , audioCodec("pcm_s16le")
    , audioBitrate("128k")
    , audioFileExt(".wav")
    , sysClockSync(false)
{
    filename = "video_" + ofGetTimestampString();
}

//--------------------------------------------------------------
//--------------------------------------------------------------
ofxVideoRecorder::ofxVideoRecorder()
    : bIsInitialized(false)
    , bIsRecording(false)
    , bIsPaused(false)
{ 

}

bool ofxVideoRecorder::setup(std::string filename, int videoWidth, int videoHeight, float videoFps, bool sysClockSync, bool ffmpegSilent)
{
    return setup(filename, videoWidth, videoHeight, videoFps, 0, 0, sysClockSync, ffmpegSilent);
}

bool ofxVideoRecorder::setup(std::string filename, int audioSampleRate, int audioChannels, bool sysClockSync, bool ffmpegSilent)
{
    return setup(filename, 0, 0, 0, audioSampleRate, audioChannels, sysClockSync, ffmpegSilent);
}

bool ofxVideoRecorder::setup(std::string filename, int videoWidth, int videoHeight, float videoFps, int audioSampleRate, int audioChannels, bool sysClockSync, bool ffmpegSilent)
{
    auto settings = ofxVideoRecorderSettings();
    settings.filename = filename;
    settings.videoWidth = videoWidth;
    settings.videoHeight = videoHeight;
    settings.videoFps = videoFps;
    settings.audioSampleRate = audioSampleRate;
    settings.audioChannels = audioChannels;
    settings.sysClockSync = sysClockSync;
    settings.ffmpegSilent = ffmpegSilent;
    return setup(settings);
}

bool ofxVideoRecorder::setup(ofxVideoRecorderSettings inSettings)
{
    settings = inSettings;
    settings.videoEnabled &= (settings.videoWidth > 0 && settings.videoHeight > 0 && settings.videoFps > 0);
    settings.audioEnabled &= (settings.audioSampleRate > 0 && settings.audioChannels > 0);

    moviePath = ofFilePath::getAbsolutePath(settings.filename);

    if (bIsInitialized)
    {
        close();
    }

    videoFramesRecorded = 0;
    audioSamplesRecorded = 0;

    if (!settings.videoEnabled && !settings.audioEnabled) 
    {
        ofLogError(__FUNCTION__) << "Invalid parameters, could not setup video or audio stream!" << std::endl
            << "video: " << settings.videoWidth << "x" << settings.videoHeight << "@" << settings.videoFps << "fps" << std::endl
            << "audio: " << settings.audioChannels << "ch@" << settings.audioSampleRate << "Hz";
        return false;
    }

    pipeNumber = requestPipeNumber();

    if (settings.videoEnabled)
    {
        // Recording video, create a FIFO pipe.

        videoFrames = std::make_shared<ofThreadChannel<ofPixels>>();

#if defined(TARGET_OSX) || defined(TARGET_LINUX)

        videoPipePath = "";
        videoPipePath = ofFilePath::getAbsolutePath("ofxvrpipe" + ofToString(pipeNumber));
        if (!ofFile::doesFileExist(videoPipePath)) 
        {
            std::string cmd = "bash --login -c 'mkfifo " + videoPipePath + "'";
            system(cmd.c_str());
        }

#elif defined(TARGET_WIN32)

        videoPipePath = "\\\\.\\pipe\\ofxvrpipe" + ofToString(pipeNumber);

        videoPipeHandle = CreateNamedPipe(
            videoPipePath.c_str(), // name of the pipe
            PIPE_ACCESS_OUTBOUND, // 1-way pipe -- send only
            PIPE_TYPE_BYTE, // send data as a byte stream
            1, // only allow 1 instance of this pipe
            0, // outbound buffer defaults to system default
            0, // no inbound buffer
            0, // use default wait time
            NULL // use default security attributes
        );

        if (!(videoPipeHandle != INVALID_HANDLE_VALUE))
        {
            if (GetLastError() != ERROR_PIPE_BUSY)
            {
                ofLogError(__FUNCTION__) << "Could not open video pipe.";
            }
            // All pipe instances are busy, so wait for 5 seconds. 
            if (!WaitNamedPipe(videoPipePath.c_str(), 5000))
            {
                ofLogError(__FUNCTION__) << "Could not open video pipe: 5 second wait timed out.";
            }
        }

#endif

    }

    if (settings.audioEnabled) {
        
        // Recording audio, create a FIFO pipe.

        audioFrames = std::make_shared<ofThreadChannel<audioFrameShort*>>();

#if defined(TARGET_OSX) || defined(TARGET_LINUX)

        audioPipePath = ofFilePath::getAbsolutePath("ofxarpipe" + ofToString(pipeNumber));
        if (!ofFile::doesFileExist(audioPipePath)) 
        {
            std::string cmd = "bash --login -c 'mkfifo " + audioPipePath + "'";
            system(cmd.c_str());
        }

#elif defined(TARGET_WIN32)

        audioPipePath = "\\\\.\\pipe\\ofxarpipe" + ofToString(pipeNumber);

        audioPipeHandle = CreateNamedPipe(
            audioPipePath.c_str(),
            PIPE_ACCESS_OUTBOUND, // 1-way pipe -- send only
            PIPE_TYPE_BYTE, // send data as a byte stream
            1, // only allow 1 instance of this pipe
            0, // outbound buffer defaults to system default
            0, // no inbound buffer
            0, // use default wait time
            NULL // use default security attributes
        );

        if (!(audioPipeHandle != INVALID_HANDLE_VALUE))
        {
            if (GetLastError() != ERROR_PIPE_BUSY)
            {
                ofLogError(__FUNCTION__) << "Could not open audio pipe.";
            }
            // All pipe instances are busy, so wait for 5 seconds. 
            if (!WaitNamedPipe(audioPipePath.c_str(), 5000))
            {
                ofLogError(__FUNCTION__) << "Could not open pipe: 5 second wait timed out.";
            }
        }

#endif

    }

#if defined(TARGET_OSX) || defined(TARGET_LINUX)

    // Basic ffmpeg invocation, -y option overwrites output file
    std::stringstream cmd;
    cmd << "bash --login -c '" << settings.ffmpegPath << (settings.ffmpegSilent ? " -loglevel quiet" : "") << " -y";
    if (settings.audioEnabled)
    {
        cmd << " -ar " << settings.audioSampleRate << " -ac " << settings.audioChannels 
            << " -f s16le"
            << " -thread_queue_size 1024"
            << " -i \"" << audioPipePath << "\"";
    }
    else 
    { 
        // No audio stream.
        cmd << " -an";
    }
    if (settings.videoEnabled) 
    {
        cmd << " -r " << settings.videoFps << " -s " << settings.videoWidth << "x" << settings.videoHeight
            << " -f rawvideo -pix_fmt " << settings.pixelFormat
            << " -thread_queue_size 1024"
            << " -i \"" << videoPipePath << "\" -r " << settings.videoFps
            << " -c:v " << settings.videoCodec << " -b:v " << settings.videoBitrate;
        if (!settings.outPixelFormat.empty())
        {
            cmd << " -pix_fmt " << settings.outPixelFormat;
        }
        // NOTE: Audio ouput options must be placed after inputs are defined
        if (settings.audioEnabled)
        {
            cmd << " -c:a " << settings.audioCodec << " -b:a " << settings.audioBitrate << " -ac " << settings.audioChannels;
        }
    }
    else 
    { 
        // No video stream.
        cmd << " -vn";
    }
    cmd << " \"" << moviePath << settings.videoFileExt << "\""
        << "' &";

    // Start ffmpeg thread, ffmpeg will wait for input pipes to be opened.
    ffmpegThread.setup(cmd.str());

    // Wait until ffmpeg has started.
    while (!ffmpegThread.isInitialized()) 
    {
        usleep(10);
    }

    if (settings.audioEnabled) {
        audioThread.setup(audioPipePath, audioFrames);
    }
    if (settings.videoEnabled)
    {
        videoThread.setup(videoPipePath, videoFrames);
    }

#elif defined(TARGET_WIN32)

    if (settings.audioEnabled && settings.videoEnabled)
    {
        bool fSuccess;

        // Audio Thread

        std::stringstream aCmd;
        aCmd << settings.ffmpegPath << (settings.ffmpegSilent ? " -loglevel quiet" : "") << " -y"
            << " -ar " << settings.audioSampleRate << " -ac " << settings.audioChannels
            << " -f s16le"
            << " -i \"" << audioPipePath << "\""
            << " -c:a " << settings.audioCodec << " -b:a " << settings.audioBitrate
            << " \"" << moviePath << "_atmp" << settings.audioFileExt << "\"";

        ffmpegAudioThread.setup(aCmd.str());
        ofLogNotice(__FUNCTION__) << "Audio command: " << aCmd.str();

        fSuccess = ConnectNamedPipe(audioPipeHandle, NULL);
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
            std::string error(errorText);
            ofLogError(__FUNCTION__) << "Audio ConnectNamedPipe() failed: " << error;
        }
        else 
        {
            ofLogNotice(__FUNCTION__) << "Audio pipe connected successfully.";
            audioThread.setup(audioPipeHandle, audioFrames);
        }

        // Video Thread

        std::stringstream vCmd;
        vCmd << settings.ffmpegPath << (settings.ffmpegSilent ? " -loglevel quiet" : "") << " -y"
            << " -r " << settings.videoFps << " -s " << settings.videoWidth << "x" << settings.videoHeight
            << " -f rawvideo -pix_fmt " << settings.pixelFormat
            << " -i \"" << videoPipePath << "\" -r " << settings.videoFps
            << " -c:v " << settings.videoCodec << " -b:v " << settings.videoBitrate;
        if (!settings.outPixelFormat.empty())
        {
            vCmd << " -pix_fmt " << settings.outPixelFormat;
        }
        vCmd << " \"" << moviePath << "_vtmp" << settings.videoFileExt << "\"";

        ffmpegVideoThread.setup(vCmd.str());
        ofLogNotice(__FUNCTION__) << "Video command: " << vCmd.str();

        fSuccess = ConnectNamedPipe(videoPipeHandle, NULL);
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
            std::string error(errorText);
            ofLogError(__FUNCTION__) << "Video ConnectNamedPipe() failed: " << error;
        }
        else 
        {
            ofLogNotice(__FUNCTION__) << "Video pipe connected successfully.";
            videoThread.setup(videoPipeHandle, videoFrames);
        }
    }
    else 
    {
        std::stringstream cmd;
        cmd << settings.ffmpegPath << (settings.ffmpegSilent ? " -loglevel quiet" : "") << " -y";
        if (settings.audioEnabled)
        {
            cmd << " -ar " << settings.audioSampleRate << " -ac " << settings.audioChannels
                << " -f s16le"
                << " -i \"" << audioPipePath << "\""
                << " -c:a " << settings.audioCodec << " -b:a " << settings.audioBitrate;
        }
        else
        {
            // No audio stream.
            cmd << " -an";
        }
        if (settings.videoEnabled)
        {
            cmd << " -r " << settings.videoFps << " -s " << settings.videoWidth << "x" << settings.videoHeight
                << " -f rawvideo -pix_fmt " << settings.pixelFormat
                << " -i \"" << videoPipePath << "\" -r " << settings.videoFps
                << " -c:v " << settings.videoCodec << " -b:v " << settings.videoBitrate;
            if (!settings.outPixelFormat.empty())
            {
                cmd << " -pix_fmt " << settings.outPixelFormat;
            }
        }
        else
        {
            // No video stream.
            cmd << " -vn";
        }
        cmd << " \"" << moviePath << settings.videoFileExt << "\"";

        ofLogNotice(__FUNCTION__) << "ffmpeg command: " << cmd.str();

        // Start ffmpeg thread, will wait for input pipes to be opened.
        ffmpegThread.setup(cmd.str());

        if (settings.audioEnabled) 
        {
            // This blocks, so we have to call it after ffmpeg is listening for a pipe.
            bool fSuccess = ConnectNamedPipe(audioPipeHandle, NULL);
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
                std::string error(errorText);
                ofLogError(__FUNCTION__) << "Audio ConnectNamedPipe() failed: " << error;
            }
            else
            {
                ofLogNotice(__FUNCTION__) << "Audio pipe connected successfully.";
                audioThread.setup(audioPipeHandle, audioFrames);
            }
        }

        if (settings.videoEnabled)
        {
            // This blocks, so we have to call it after ffmpeg is listening for a pipe.
            bool fSuccess = ConnectNamedPipe(videoPipeHandle, NULL);
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
                std::string error(errorText);
                ofLogError(__FUNCTION__) << "Video ConnectNamedPipe() failed: " << error;
            }
            else
            {
                ofLogNotice(__FUNCTION__) << "Video pipe connected successfully.";
                videoThread.setup(videoPipeHandle, videoFrames);
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
bool ofxVideoRecorder::addFrame(const ofPixels &pixels)
{
    if (!bIsRecording || bIsPaused) return false;

    if (bIsInitialized && settings.videoEnabled)
	{
        int framesToAdd = 1; // default add one frame per request

        if ((settings.audioEnabled || settings.sysClockSync))
        {
            double syncDelta;
            double videoRecordedTime = videoFramesRecorded / (double)settings.videoFps;

            if (settings.audioEnabled) 
            {
                // If also recording audio, check the overall recorded time for audio and video to make sure audio is not going out of sync.
                // This also handles incoming dynamic framerate while maintaining desired outgoing framerate.
                double audioRecordedTime = (audioSamplesRecorded / settings.audioChannels)  / (double)settings.audioSampleRate;
                syncDelta = audioRecordedTime - videoRecordedTime;
            }
            else 
            {
                // If just recording video, synchronize the video against the system clock.
                // This also handles incoming dynamic framerate while maintaining desired outgoing framerate.
                syncDelta = systemClock() - videoRecordedTime;
            }

            if (syncDelta > 1.0 / settings.videoFps)
            {
                // Not enough video frames, we need to add extra video frames.
                while (syncDelta > 1.0/ settings.videoFps) 
                {
                    ++framesToAdd;
                    syncDelta -= 1.0/ settings.videoFps;
                }
                ofLogNotice(__FUNCTION__) << "recDelta = " << syncDelta << "; Not enough video frames for desired frame rate, copying current " << framesToAdd << " times.";
            }
            else if (syncDelta < -1.0 / settings.videoFps) 
            {
                // More than one video frame is waiting, skip this frame
                framesToAdd = 0;
                ofLogNotice(__FUNCTION__) << "recDelta = " << syncDelta << "; Too many video frames, skipping current.\n";
            }
        }

        for (int i = 0; i < framesToAdd; ++i)
        {
            // Add desired number of frames
			videoFrames->send(pixels);
            ++videoFramesRecorded;
		}

        return true;
    }

    return false;
}

//--------------------------------------------------------------
void ofxVideoRecorder::addAudioSamples(float *samples, int bufferSize, int numChannels)
{
    if (!bIsRecording || bIsPaused) return;

    if (bIsInitialized && settings.audioEnabled) 
    {
        int size = bufferSize * numChannels;
        audioFrameShort* shortSamples = new audioFrameShort;
        shortSamples->data = new short[size];
        shortSamples->size = size;

        for (int i = 0; i < size; ++i) 
        {
            shortSamples->data[i] = static_cast<short>(samples[i] * 32767.0f);
        }
        audioFrames->send(shortSamples);
        audioSamplesRecorded += size;
    }
}

//--------------------------------------------------------------
void ofxVideoRecorder::addAudioSamples(ofSoundBuffer& buffer) {
    if (!bIsRecording || bIsPaused) return;

    if(bIsInitialized && settings.audioEnabled){
        int size = buffer.getNumFrames() * buffer.getNumChannels();
        audioFrameShort * shortSamples = new audioFrameShort;
        shortSamples->data = new short[size];
        shortSamples->size = size;

        for(int i=0; i < buffer.getNumFrames(); i++){
            for(int j=0; j < buffer.getNumChannels(); j++){
                shortSamples->data[i * buffer.getNumChannels() + j] = (short) (buffer.getSample(i, j) * 32767.0f);
            }
        }
        audioFrames->send(shortSamples);
        audioSamplesRecorded += size;
    }
}

//--------------------------------------------------------------
void ofxVideoRecorder::start()
{
    if(!bIsInitialized) return;

    if (bIsRecording) return;

    // Start a recording.
    bIsRecording = true;
    bIsPaused = false;
    startTime = ofGetElapsedTimef();

    ofLogVerbose(__FUNCTION__) << "Start recording.";
}

//--------------------------------------------------------------
void ofxVideoRecorder::setPaused(bool bPause)
{
    if(!bIsInitialized) return;

    if (!bIsRecording || bIsPaused == bPause) return;

    // Pause the recording.
    bIsPaused = bPause;

    if (bIsPaused) 
    {
        totalRecordingDuration += recordingDuration;
        ofLogVerbose(__FUNCTION__) << "Pause recording.";
    }
    else
    {
        startTime = ofGetElapsedTimef();
        ofLogVerbose(__FUNCTION__) << "Unpause recording.";
    }
}

//--------------------------------------------------------------
void ofxVideoRecorder::close()
{
    if(!bIsInitialized) return;

    bIsRecording = false;

    while ((settings.videoEnabled && !videoFrames->empty()) 
        || (settings.audioEnabled && !audioFrames->empty()))
    {
		ofSleepMillis(100);
	}

    ofLogVerbose(__FUNCTION__) << "Close recording.";

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

    if (settings.videoEnabled)
    {
        videoThread.close();
        videoFrames.reset();
    }
    if (settings.audioEnabled)
    {
        audioThread.close();
        audioFrames.reset();
    }

#if defined(TARGET_WIN32)

    if (settings.audioEnabled && settings.videoEnabled)
    {
        ffmpegAudioThread.waitForThread();
        ffmpegVideoThread.waitForThread();

        // Merge the audio and video recordings.

        std::stringstream mergeCmd;
        mergeCmd << settings.ffmpegPath << (settings.ffmpegSilent ? " -loglevel quiet" : "") << " -y"
            << " -i \"" << moviePath << "_atmp" << settings.audioFileExt << "\""
            << " -i \"" << moviePath << "_vtmp" << settings.videoFileExt << "\""
            << " -c:v copy -c:a copy -strict experimental "
            << " \"" << moviePath << settings.videoFileExt << "\"";

        ofLogNotice(__FUNCTION__) << "merge command: " << mergeCmd.str();

        system(mergeCmd.str().c_str());

        std::stringstream delCmd;
        ofStringReplace(moviePath, "/", "\\");
        delCmd << "DEL \"" << moviePath << "_atmp" << settings.audioFileExt << "\" \"" << moviePath << "_vtmp" << settings.videoFileExt << "\"";
        system(delCmd.str().c_str());
    }

#endif

    retirePipeNumber(pipeNumber);

    ffmpegThread.waitForThread();
    
    // TODO: kill ffmpeg process if its taking too long to close for whatever reason.

    // Notify the listeners.
    ofxVideoRecorderOutputFileCompleteEventArgs args;
    args.fileName = settings.filename;
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
std::set<int> ofxVideoRecorder::openPipes;

//--------------------------------------------------------------
int ofxVideoRecorder::requestPipeNumber()
{
    int n = 0;
    do
    {
        n = ofRandom(1024);
    } while (openPipes.find(n) != openPipes.end());
    openPipes.insert(n);
    return n;
}

//--------------------------------------------------------------
void ofxVideoRecorder::retirePipeNumber(int num)
{
    if(!openPipes.erase(num))
    {
        ofLogNotice(__FUNCTION__) << "Trying to retire pipe number " << num << " but it's not being tracked.";
    }
}
