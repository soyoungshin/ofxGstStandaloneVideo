#pragma once

#include "ofConstants.h"
#ifndef TARGET_ANDROID
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include "ofConstants.h"
#include "ofBaseTypes.h"
#include "ofPixels.h"
#include "ofTypes.h"
#include "ofEvents.h"

#include "ofTexture.h"

class ofxGstStandaloneAppSink;

//-------------------------------------------------
//----------------------------------------- ofxGstStandaloneUtils
//-------------------------------------------------

class ofxGstStandaloneUtils{
public:
	ofxGstStandaloneUtils();
	virtual ~ofxGstStandaloneUtils();

	bool 	setPipelineWithSink(string pipeline, string sinkname="sink", bool isStream=false);
	bool 	setPipelineWithSink(GstElement * pipeline, GstElement * sink, bool isStream=false);

	void 	play();
	void 	stop(){setPaused(true);}
	void 	setPaused(bool bPause);
	bool 	isPaused(){return bPaused;}
	bool 	isLoaded(){return bLoaded;}
	bool 	isPlaying(){return bPlaying;}

	float	getPosition();
	float 	getSpeed();
	float 	getDuration();
	guint64 getDurationNanos();
	bool  	getIsMovieDone();

	void 	setPosition(float pct);
	void 	setVolume(int volume);
	void 	setLoopState(ofLoopType state);
	int		getLoopState(){return loopMode;}
	void 	setSpeed(float speed);

	GstElement 	* getPipeline();
	GstElement 	* getSink();

	virtual void close();

	void setSinkListener(ofxGstStandaloneAppSink * appsink);

protected:
	// callbacks to get called from gstreamer
	virtual GstFlowReturn preroll_cb(GstBuffer * buffer);
	virtual GstFlowReturn buffer_cb(GstBuffer * buffer);
	virtual void 		  eos_cb();

private:
	void 				gstHandleMessage();
	void				update(ofEventArgs & args);
	bool				startPipeline();

	bool 				bPlaying;
	bool 				bPaused;
	bool				bIsMovieDone;
	bool 				bLoaded;
	ofLoopType			loopMode;

	GstElement  *		gstSink;
	GstElement 	*		gstPipeline;
	ofxGstStandaloneAppSink * 		appsink;

	bool				posChangingPaused;
	int					pipelineState;
	float				speed;
	gint64				durationNanos;
	bool				isAppSink;
	bool				isStream;

	// the gst callbacks need to be friended to be able to call us
	friend GstFlowReturn on_new_buffer_from_source (GstAppSink * elt, void * data);
	friend GstFlowReturn on_new_preroll_from_source (GstAppSink * elt, void * data);
	friend void on_eos_from_source (GstAppSink * elt, void * data);

};





//-------------------------------------------------
//----------------------------------------- videoUtils
//-------------------------------------------------

class ofxGstStandaloneVideoUtils: public ofBaseVideo, public ofxGstStandaloneUtils{
public:

	ofxGstStandaloneVideoUtils();
	virtual ~ofxGstStandaloneVideoUtils();

	bool 			setPipeline(string pipeline, int bpp=24, bool isStream=false, int w=-1, int h=-1);

	bool 			allocate(int w, int h, int bpp);

	bool 			isFrameNew();
	unsigned char * getPixels();
	ofPixelsRef		getPixelsRef();
	void 			update();

	float 			getHeight();
	float 			getWidth();

	ofTexture		getTex();
	ofTexture &		getTextureReference();

	void 			close();

	void setInternalPixelFormat(ofPixelFormat internalPixelFormat);

	// this events happen in a different thread
	// do not use them for opengl stuff
	ofEvent<ofPixels> prerollEvent;
	ofEvent<ofPixels> bufferEvent;
	ofEvent<ofEventArgs> eosEvent;

protected:
	GstFlowReturn 	preroll_cb(GstBuffer * buffer);
	GstFlowReturn 	buffer_cb(GstBuffer * buffer);
	void			eos_cb();


	ofPixels		pixels;				// 24 bit: rgb
	ofPixels		backPixels;
	ofTexture		tex;
private:
	bool			bIsFrameNew;			// if we are new
	bool			bHavePixelsChanged;
	bool			bBackPixelsChanged;
	ofMutex			mutex;

	// added
	ofPixelFormat	internalPixelFormat;
};

// this is yanked from here:
//https://github.com/joshuajnoble/openFrameworks/blob/fb52134aef276ce40e9ed7ad300c7b18e1320673/libs/openFrameworks/gl/ofGLUtils.h
inline int hfGetGLTypeFromPixelFormat(ofPixelFormat pixelFormat){
	switch(pixelFormat){
	case OF_PIXELS_BGRA:
		return GL_RGBA;
	case OF_PIXELS_MONO:
		return GL_LUMINANCE;
	case OF_PIXELS_RGB:
		return GL_RGB;
	case OF_PIXELS_RGBA:
		return GL_RGBA;
    default:
        return GL_RGB;
	}
}


//-------------------------------------------------
//----------------------------------------- appsink listener
//-------------------------------------------------

class ofxGstStandaloneAppSink{
public:
	virtual GstFlowReturn	on_preroll(GstBuffer * buffer){
		return GST_FLOW_OK;
	}
	virtual GstFlowReturn	on_buffer(GstBuffer * buffer){
		return GST_FLOW_OK;
	}
	virtual void			on_eos(){}

	// return true to set the message as attended so upstream doesn't try to process it
	virtual bool on_message(GstMessage* msg){return false;};

	// pings when enough data has arrived to be able to get sink properties
	virtual void on_stream_prepared(){};
};

#endif

