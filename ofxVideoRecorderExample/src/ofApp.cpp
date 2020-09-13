#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
    sampleRate = 44100;
    channels = 2;

    ofSetFrameRate(30);
    ofSetLogLevel(OF_LOG_VERBOSE);
    vidGrabber.setDeviceID(1);
    vidGrabber.setDesiredFrameRate(30);
    vidGrabber.initGrabber(640, 480);

    ofAddListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::recordingComplete);

//    soundStream.listDevices();
//    soundStream.setDeviceID(11);
    soundStream.setup(this, 0, channels, sampleRate, 256, 4);

    ofSetWindowShape(vidGrabber.getWidth(), vidGrabber.getHeight()	);
    bRecording = false;
    ofEnableAlphaBlending();
}

//--------------------------------------------------------------
void ofApp::exit(){
    ofRemoveListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::recordingComplete);
    vidRecorder.close();
}

//--------------------------------------------------------------
void ofApp::update()
{
    vidGrabber.update();
    if(vidGrabber.isFrameNew() && bRecording)
    {
        bool success = vidRecorder.addFrame(vidGrabber.getPixels());
        if (!success) 
        {
            ofLogWarning(__FUNCTION__) << "This frame was not added!";
        }
    }

    // Check if the video recorder encountered any error while writing video frame or audio smaples.
    if (vidRecorder.hasVideoError()) 
    {
        ofLogWarning(__FUNCTION__) << "The video recorder failed to write some frames!";
    }
    if (vidRecorder.hasAudioError()) 
    {
        ofLogWarning(__FUNCTION__) << "The video recorder failed to write some audio samples!";
    }
}

//--------------------------------------------------------------
void ofApp::draw()
{
    ofSetColor(255, 255, 255);
    vidGrabber.draw(0,0);

    std::ostringstream oss;
    oss << "FPS: " << ofGetFrameRate() << std::endl
        << (bRecording ? "pause" : "start") << " recording: r" << std::endl
        << (bRecording ? "close current video file: c" : "");

    ofSetColor(0,0,0,100);
    ofDrawRectangle(0, 0, 260, 75);
    ofSetColor(255, 255, 255);
    ofDrawBitmapString(oss.str(),15,15);

    if (bRecording) 
    {
        ofSetColor(255, 0, 0);
        ofDrawCircle(ofGetWidth() - 20, 20, 5);
    }
}

//--------------------------------------------------------------
void ofApp::audioIn(float *input, int bufferSize, int nChannels){
    if(bRecording)
        vidRecorder.addAudioSamples(input, bufferSize, nChannels);
}

//--------------------------------------------------------------
void ofApp::recordingComplete(ofxVideoRecorderOutputFileCompleteEventArgs& args)
{
    ofLogNotice(__FUNCTION__) << "The recorded video file is now complete.";
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key)
{
    if (key == 'r')
    {
        bRecording = !bRecording;
        if (bRecording && !vidRecorder.isInitialized())
        {
            auto settings = ofxVideoRecorderSettings();
            // use this is you have ffmpeg installed in your data folder.
            //settings.ffmpegPath = ofFilePath::getAbsolutePath("ffmpeg");
            settings.filename = "testMovie_" + ofGetTimestampString();
            // ffmpeg uses the extension to determine the container type. 
            // run 'ffmpeg -formats' to see supported formats.
            //settings.videoFileExt = ".mov";
            settings.videoEnabled = true;
            settings.videoWidth = vidGrabber.getWidth();
            settings.videoHeight = vidGrabber.getHeight();
            settings.videoFps = 30;
            settings.audioEnabled = false;
            settings.audioSampleRate = sampleRate;
            settings.audioChannels = channels;
            // override the default codecs if you like.
            // run 'ffmpeg -codecs' to find out what your implementation supports (or -formats on some older versions)
            //settings.videoCodec = "libx264";
            //settings.videoBitrate = "800k";
            //settings.audioCodec = "mp3";
            //settings.audioBitrate = "192k";
            vidRecorder.setup(settings);

            // Start recording!
            vidRecorder.start();
        }
        else if (vidRecorder.isInitialized())
        {
            vidRecorder.setPaused(!bRecording);
        }
    }
    if (key == 'c')
    {
        bRecording = false;
        vidRecorder.close();
    }
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){

}
