/*
 * ofxGstStandaloneVideoUtils.cpp
 *
 *  Created on: 16/01/2011
 *      Author: arturo
 */

#include "ofxGstStandaloneVideoPlayer.h"
#include <gst/video/video.h>


ofxGstStandaloneVideoPlayer::ofxGstStandaloneVideoPlayer(){
	nFrames						= 0;
	internalPixelFormat			= OF_PIXELS_RGB;
	bIsStream					= false;
	videoUtils.setSinkListener(this);
}

ofxGstStandaloneVideoPlayer::~ofxGstStandaloneVideoPlayer(){
	// the video player needs to be explicitly closed so threads don't 
	// continue polling after the player is done-zos.
	close();
}

void ofxGstStandaloneVideoPlayer::setPixelFormat(ofPixelFormat pixelFormat){
	internalPixelFormat = pixelFormat;
		videoUtils.setInternalPixelFormat(pixelFormat);
}

// name must be formatted file:///C:/path/to/file.mov
bool ofxGstStandaloneVideoPlayer::loadMovie(string name){
	// can add logic for streaming media here
	bIsStream = false;
	ofLog(OF_LOG_VERBOSE,"loading "+name);

	//GMainLoop* loop		=
	g_main_loop_new (NULL, FALSE);

	// a note about playbin http://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-base-plugins/html/gst-plugins-base-plugins-playbin2.html
	GstElement * gstPipeline = gst_element_factory_make("playbin2","player");
	g_object_set(G_OBJECT(gstPipeline), "uri", name.c_str(), (void*)NULL);

	// create the oF appsink for video rgb without sync to clock
	GstElement * gstSink = gst_element_factory_make("appsink", "sink");
	int bpp; // bits per pixel?
	string mime;
	switch(internalPixelFormat){
	case OF_PIXELS_MONO:
		mime = "video/x-raw-gray";
		bpp = 8;
		break;
	case OF_PIXELS_RGB:
		mime = "video/x-raw-rgb";
		bpp = 24;
		break;
	case OF_PIXELS_RGBA:
	case OF_PIXELS_BGRA:
		mime = "video/x-raw-rgb";
		bpp = 32;
		break;
	default:
		mime = "video/x-raw-rgb";
		bpp=24;
		break;
	}

	GstCaps *caps = gst_caps_new_simple(mime.c_str(),
										"bpp", G_TYPE_INT, bpp,
										//"depth", G_TYPE_INT, 24,
										/*"endianness",G_TYPE_INT,4321,
										"red_mask",G_TYPE_INT,0xff0000,
										"green_mask",G_TYPE_INT,0x00ff00,
										"blue_mask",G_TYPE_INT,0x0000ff,*/


										NULL);
	gst_app_sink_set_caps(GST_APP_SINK(gstSink), caps);
	gst_caps_unref(caps);
	gst_base_sink_set_sync(GST_BASE_SINK(gstSink), false);
	gst_app_sink_set_drop (GST_APP_SINK(gstSink),true);

	g_object_set (G_OBJECT(gstPipeline),"video-sink",gstSink,(void*)NULL);


	GstElement *audioSink = gst_element_factory_make("gconfaudiosink", NULL);
	g_object_set (G_OBJECT(gstPipeline),"audio-sink",audioSink,(void*)NULL);

	videoUtils.setPipelineWithSink(gstPipeline,gstSink,bIsStream);

	if(!bIsStream) {
		return allocate();
	} else {
		return true;
	}
}


bool ofxGstStandaloneVideoPlayer::allocate(){
	guint64 durationNanos = videoUtils.getDurationNanos();

	nFrames		  = 0;
	if(GstPad* pad = gst_element_get_static_pad(videoUtils.getSink(), "sink")){
		int width,height,bpp=24;
		if(gst_video_get_size(GST_PAD(pad), &width, &height)){
			videoUtils.allocate(width,height,bpp);
		}else{
			ofLog(OF_LOG_ERROR,"GStreamer: cannot query width and height");
			return false;
		}

		const GValue *framerate;
		framerate = gst_video_frame_rate(pad);
		fps_n=0;
		fps_d=0;
		if(framerate && GST_VALUE_HOLDS_FRACTION (framerate)){
			fps_n = gst_value_get_fraction_numerator (framerate);
			fps_d = gst_value_get_fraction_denominator (framerate);
			nFrames = static_cast<guint64>((float)(durationNanos / GST_SECOND) * (float)fps_n/(float)fps_d);
			ofLog(OF_LOG_VERBOSE,"ofxGstStandaloneUtils: framerate: %i/%i",fps_n,fps_d);
		}else{
			ofLog(OF_LOG_WARNING,"Gstreamer: cannot get framerate, frame seek won't work");
		}
		gst_object_unref(GST_OBJECT(pad));
		return true;
	}else{
		ofLog(OF_LOG_ERROR,"GStreamer: cannot get sink pad");
		return false;
	}
}

void ofxGstStandaloneVideoPlayer::on_stream_prepared(){
	allocate();
}

int	ofxGstStandaloneVideoPlayer::getCurrentFrame(){
	int frame = 0;

	// zach I think this may fail on variable length frames...
	float pos = getPosition();
	if(pos == -1) return -1;


	float  framePosInFloat = ((float)getTotalNumFrames() * pos);
	int    framePosInInt = (int)framePosInFloat;
	float  floatRemainder = (framePosInFloat - framePosInInt);
	if (floatRemainder > 0.5f) framePosInInt = framePosInInt + 1;
	//frame = (int)ceil((getTotalNumFrames() * getPosition()));
	frame = framePosInInt;

	return frame;
}

int	ofxGstStandaloneVideoPlayer::getTotalNumFrames(){
	return static_cast<int>(nFrames);
}

void ofxGstStandaloneVideoPlayer::firstFrame(){
	setFrame(0);
}

void ofxGstStandaloneVideoPlayer::nextFrame(){
	int currentFrame = getCurrentFrame();
	if(currentFrame!=-1) setFrame(currentFrame + 1);
}

void ofxGstStandaloneVideoPlayer::previousFrame(){
	int currentFrame = getCurrentFrame();
	if(currentFrame!=-1) setFrame(currentFrame - 1);
}

void ofxGstStandaloneVideoPlayer::setFrame(int frame){ // frame 0 = first frame...
	float pct = (float)frame / (float)nFrames;
	setPosition(pct);
}

bool ofxGstStandaloneVideoPlayer::isStream(){
	return bIsStream;
}

void ofxGstStandaloneVideoPlayer::update(){
	videoUtils.update();
}

void ofxGstStandaloneVideoPlayer::play(){
	videoUtils.play();
}

void ofxGstStandaloneVideoPlayer::stop(){
	videoUtils.stop();
}

void ofxGstStandaloneVideoPlayer::setPaused(bool bPause){
	videoUtils.setPaused(bPause);
}

bool ofxGstStandaloneVideoPlayer::isPaused(){
	return videoUtils.isPaused();
}

bool ofxGstStandaloneVideoPlayer::isLoaded(){
	return videoUtils.isLoaded();
}

bool ofxGstStandaloneVideoPlayer::isPlaying(){
	return videoUtils.isPlaying();
}

float ofxGstStandaloneVideoPlayer::getPosition(){
	return videoUtils.getPosition();
}

float ofxGstStandaloneVideoPlayer::getSpeed(){
	return videoUtils.getSpeed();
}

float ofxGstStandaloneVideoPlayer::getDuration(){
	return videoUtils.getDuration();
}

bool ofxGstStandaloneVideoPlayer::getIsMovieDone(){
	return videoUtils.getIsMovieDone();
}

void ofxGstStandaloneVideoPlayer::setPosition(float pct){
	videoUtils.setPosition(pct);
}

void ofxGstStandaloneVideoPlayer::setVolume(int volume){
	videoUtils.setVolume(volume);
}

void ofxGstStandaloneVideoPlayer::setLoopState(ofLoopType state){
	videoUtils.setLoopState(state);
}

int	ofxGstStandaloneVideoPlayer::getLoopState(){
	return videoUtils.getLoopState();
}

void ofxGstStandaloneVideoPlayer::setSpeed(float speed){
	videoUtils.setSpeed(speed);
}

void ofxGstStandaloneVideoPlayer::close(){
	videoUtils.close();
}

bool ofxGstStandaloneVideoPlayer::isFrameNew(){
	return videoUtils.isFrameNew();
}

unsigned char * ofxGstStandaloneVideoPlayer::getPixels(){
	return videoUtils.getPixels();
}

ofPixelsRef ofxGstStandaloneVideoPlayer::getPixelsRef(){
	return videoUtils.getPixelsRef();
}

float ofxGstStandaloneVideoPlayer::getHeight(){
	return videoUtils.getHeight();
}

float ofxGstStandaloneVideoPlayer::getWidth(){
	return videoUtils.getWidth();
}

ofxGstStandaloneVideoUtils * ofxGstStandaloneVideoPlayer::getGstVideoUtils(){
	return &videoUtils;
}


//------------------------------------
void ofxGstStandaloneVideoPlayer::draw(float _x, float _y, float _w, float _h){
	videoUtils.getTex().draw(_x, _y, _w, _h);
}

//------------------------------------
void ofxGstStandaloneVideoPlayer::draw(float _x, float _y){
	videoUtils.getTex().draw(_x, _y);
}
