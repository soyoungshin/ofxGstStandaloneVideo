#include "ofxGstStandaloneUtils.h"
#ifndef TARGET_ANDROID
#include "ofUtils.h"
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <glib-object.h>
#include <glib.h>
#include <algorithm>




//-------------------------------------------------
//----------------------------------------- gstUtils
//-------------------------------------------------

static bool plugin_registered = false;
static bool gst_inited = false;


// called when the appsink notifies us that there is a new buffer ready for
// processing

GstFlowReturn on_new_buffer_from_source (GstAppSink * elt, void * data){
	GstBuffer *buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
	return ((ofxGstStandaloneUtils*)data)->buffer_cb(buffer);
}

GstFlowReturn on_new_preroll_from_source (GstAppSink * elt, void * data){
	GstBuffer *buffer = gst_app_sink_pull_preroll(GST_APP_SINK (elt));
	return ((ofxGstStandaloneUtils*)data)->preroll_cb(buffer);
}

void on_eos_from_source (GstAppSink * elt, void * data){
	((ofxGstStandaloneUtils*)data)->eos_cb();
}

static gboolean appsink_plugin_init (GstPlugin * plugin)
{
  gst_element_register (plugin, "appsink", GST_RANK_NONE, GST_TYPE_APP_SINK);

  return TRUE;
}



ofxGstStandaloneUtils::ofxGstStandaloneUtils() {
	bLoaded 					= false;
	speed 						= 1;
	bPaused						= false;
	bIsMovieDone				= false;
	loopMode					= OF_LOOP_NONE;

	gstPipeline					= NULL;
	gstSink						= NULL;

	posChangingPaused			= 0;
	durationNanos				= 0;

	isAppSink					= false;

	appsink						= NULL;

	if(!g_thread_supported()){
		g_thread_init(NULL);
	}
	if(!gst_inited){
		gst_init (NULL, NULL);
		gst_inited=true;
		ofLog(OF_LOG_VERBOSE,"ofxGstStandaloneUtils: gstreamer inited");
	}
	if(!plugin_registered){
		gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR,
					"appsink", (char*)"Element application sink",
					appsink_plugin_init, "0.1", "LGPL", "ofVideoPlayer", "openFrameworks",
					"http://openframeworks.cc/");
		plugin_registered=true;
	}

}

ofxGstStandaloneUtils::~ofxGstStandaloneUtils() {
	close();
}

GstFlowReturn ofxGstStandaloneUtils::preroll_cb(GstBuffer * buffer){
	bIsMovieDone = false;
	if(appsink) return appsink->on_preroll(buffer);
	else return GST_FLOW_OK;
}

GstFlowReturn ofxGstStandaloneUtils::buffer_cb(GstBuffer * buffer){
	bIsMovieDone = false;
	if(appsink) return appsink->on_buffer(buffer);
	else return GST_FLOW_OK;
}

void ofxGstStandaloneUtils::eos_cb(){
	bIsMovieDone = true;
	if(appsink) appsink->on_eos();
}

bool ofxGstStandaloneUtils::setPipelineWithSink(string pipeline, string sinkname, bool isStream){
	//GMainLoop* loop		=
	g_main_loop_new (NULL, FALSE);

	gchar* pipeline_string =
		g_strdup((pipeline).c_str());

	GError * error = NULL;
	gstPipeline = gst_parse_launch (pipeline_string, &error);

	ofLog(OF_LOG_NOTICE, "gstreamer pipeline: %s", pipeline_string);
	if(error!=NULL){
		ofLog(OF_LOG_ERROR,"couldnt create pipeline: " + string(error->message));
		return false;
	}

	gstSink = gst_bin_get_by_name(GST_BIN(gstPipeline),sinkname.c_str());

	return setPipelineWithSink(gstPipeline,gstSink,isStream);
}

bool ofxGstStandaloneUtils::setPipelineWithSink(GstElement * pipeline, GstElement * sink, bool isStream_){
	gstPipeline = pipeline;
	gstSink = sink;
	isStream = isStream_;

	if(gstSink){
		gst_base_sink_set_sync(GST_BASE_SINK(gstSink), true);
	}

	if(gstSink && string(gst_plugin_feature_get_name( GST_PLUGIN_FEATURE(gst_element_get_factory(gstSink))))=="appsink"){
		isAppSink = true;
	}else{
		isAppSink = false;
	}

	return startPipeline();
}

bool ofxGstStandaloneUtils::startPipeline(){

	bPaused 			= true;
	speed 				= 1.0f;

	// pause the pipeline
	if(gst_element_set_state(GST_ELEMENT(gstPipeline), GST_STATE_PAUSED) ==	GST_STATE_CHANGE_FAILURE) {
		ofLog(OF_LOG_ERROR, "GStreamer: unable to set pipeline to paused\n");

		return false;
	}

	// wait for paused state to query the duration
	if(!isStream){
		GstState state = GST_STATE_PAUSED;
		if(gst_element_get_state(gstPipeline,&state,NULL,2*GST_SECOND)==GST_STATE_CHANGE_FAILURE){
			return false;
		}
	}

	bLoaded = true;


	if(isAppSink){
		// set the appsink to not emit signals, we are using callbacks instead
		// and get buffers by callback (not frame by frame)
		g_object_set (G_OBJECT (gstSink), "emit-signals", FALSE, "sync", true, (void*)NULL);

		GstAppSinkCallbacks gstCallbacks;
		gstCallbacks.eos = &on_eos_from_source;
		gstCallbacks.new_preroll = &on_new_preroll_from_source;
		gstCallbacks.new_buffer = &on_new_buffer_from_source;

		gst_app_sink_set_callbacks(GST_APP_SINK(gstSink), &gstCallbacks, this, NULL);

	}

	setSpeed(1.0);

	ofAddListener(ofEvents().update,this,&ofxGstStandaloneUtils::update);

	return true;
}

void ofxGstStandaloneUtils::play(){
	bPlaying = true;
	setPaused(false);
	//this is if we set the speed first but it only can be set when we are playing.
	setSpeed(speed);
}

void ofxGstStandaloneUtils::setPaused(bool _bPause){
	bPaused = _bPause;
	//timeLastIdle = ofGetElapsedTimeMillis();
	if(bLoaded){
		if(bPaused)
			gst_element_set_state (gstPipeline, GST_STATE_PAUSED);
		else
			gst_element_set_state (gstPipeline, GST_STATE_PLAYING);
	}
}

float ofxGstStandaloneUtils::getPosition(){
	if(gstPipeline){
		gint64 pos=0;
		GstFormat format=GST_FORMAT_TIME;
		if(!gst_element_query_position(GST_ELEMENT(gstPipeline),&format,&pos)){
			ofLog(OF_LOG_VERBOSE,"GStreamer: cannot query position");
			return -1;
		}
		return (float)pos/(float)durationNanos;
	}else{
		return -1;
	}
}

float ofxGstStandaloneUtils::getSpeed(){
	return speed;
}

float ofxGstStandaloneUtils::getDuration(){
	return (float)getDurationNanos()/(float)GST_SECOND;
}

guint64 ofxGstStandaloneUtils::getDurationNanos(){
	GstFormat format = GST_FORMAT_TIME;

	if(!gst_element_query_duration(getPipeline(),&format,&durationNanos))
		ofLog(OF_LOG_WARNING,"GStreamer: cannot query time duration");

	return durationNanos;

}

bool  ofxGstStandaloneUtils::getIsMovieDone(){
	if(isAppSink){
		return gst_app_sink_is_eos(GST_APP_SINK(gstSink));
	}else{
		return bIsMovieDone;
	}
}

void ofxGstStandaloneUtils::setPosition(float pct){
	//pct = CLAMP(pct, 0,1);// check between 0 and 1;
	GstFormat format = GST_FORMAT_TIME;
	GstSeekFlags flags = (GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE);
	gint64 pos = (guint64)((double)pct*(double)durationNanos);

	/*if(bPaused){
		seek_lock();
		gst_element_set_state (gstPipeline, GST_STATE_PLAYING);
		posChangingPaused=true;
		seek_unlock();
	}*/
	if(speed>0){
		if(!gst_element_seek(GST_ELEMENT(gstPipeline),speed, 	format,
				flags,
				GST_SEEK_TYPE_SET,
				pos,
				GST_SEEK_TYPE_SET,
				-1)) {
		ofLog(OF_LOG_WARNING,"GStreamer: unable to seek");
		}
	}else{
		if(!gst_element_seek(GST_ELEMENT(gstPipeline),speed, 	format,
				flags,
				GST_SEEK_TYPE_SET,
				0,
				GST_SEEK_TYPE_SET,
				pos)) {
		ofLog(OF_LOG_WARNING,"GStreamer: unable to seek");
		}
	}
}

void ofxGstStandaloneUtils::setVolume(int volume){
	gdouble gvolume = CLAMP(volume,0,10);
	g_object_set(G_OBJECT(gstPipeline), "volume", gvolume, (void*)NULL);
}

void ofxGstStandaloneUtils::setLoopState(ofLoopType state){
	loopMode = state;
}

void ofxGstStandaloneUtils::setSpeed(float _speed){
	GstFormat format = GST_FORMAT_TIME;
	GstSeekFlags flags = (GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SKIP | GST_SEEK_FLAG_ACCURATE);
	gint64 pos;

	if(_speed==0){
		gst_element_set_state (gstPipeline, GST_STATE_PAUSED);
		return;
	}

	if(!gst_element_query_position(GST_ELEMENT(gstPipeline),&format,&pos) || pos<0){
		//ofLog(OF_LOG_ERROR,"GStreamer: cannot query position");
		return;
	}

	speed = _speed;
	//pos = (float)gstData.lastFrame * (float)fps_d / (float)fps_n * GST_SECOND;

	if(!bPaused)
		gst_element_set_state (gstPipeline, GST_STATE_PLAYING);

	if(speed>0){
		if(!gst_element_seek(GST_ELEMENT(gstPipeline),speed, 	format,
				flags,
				GST_SEEK_TYPE_SET,
				pos,
				GST_SEEK_TYPE_SET,
				-1)) {
		ofLog(OF_LOG_WARNING,"GStreamer: unable to change speed");
		}
	}else{
		if(!gst_element_seek(GST_ELEMENT(gstPipeline),speed, 	format,
				flags,
				GST_SEEK_TYPE_SET,
				0,
				GST_SEEK_TYPE_SET,
				pos)) {
		ofLog(OF_LOG_WARNING,"GStreamer: unable to change speed");
		}
	}

	ofLog(OF_LOG_VERBOSE,"Gstreamer: speed change to %f", speed);

}

void ofxGstStandaloneUtils::close(){
	if(bLoaded){
		gst_element_set_state(GST_ELEMENT(gstPipeline), GST_STATE_NULL);
		gst_object_unref(gstPipeline);
	}

	bLoaded = false;
	ofRemoveListener(ofEvents().update,this,&ofxGstStandaloneUtils::update);
}

string getName(GstState state){
	switch(state){
	case   GST_STATE_VOID_PENDING:
		return "void pending";
	case   GST_STATE_NULL:
		return "null";
	case   GST_STATE_READY:
		return "ready";
	case   GST_STATE_PAUSED:
		return "paused";
	case   GST_STATE_PLAYING:
		return "playing";
	default:
		return "";
	}
}

void ofxGstStandaloneUtils::update(ofEventArgs & args){
	gstHandleMessage();
}

void ofxGstStandaloneUtils::gstHandleMessage(){
	GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(gstPipeline));
	while(gst_bus_have_pending(bus)) {
		GstMessage* msg = gst_bus_pop(bus);
		if(appsink && appsink->on_message(msg)) continue;

		ofLog(OF_LOG_VERBOSE,"GStreamer: Got %s message", GST_MESSAGE_TYPE_NAME(msg));

		switch (GST_MESSAGE_TYPE (msg)) {

			case GST_MESSAGE_BUFFERING:
				gint pctBuffered;
				gst_message_parse_buffering(msg,&pctBuffered);
				ofLog(OF_LOG_VERBOSE,"GStreamer: buffering %i%", pctBuffered);
				if(isStream && !bLoaded && appsink){
					appsink->on_stream_prepared();
				}
				if(pctBuffered<100){
					gst_element_set_state (gstPipeline, GST_STATE_PAUSED);
				}else if(!bPaused){
					gst_element_set_state (gstPipeline, GST_STATE_PLAYING);
				}
			break;

			case GST_MESSAGE_DURATION:{
				GstFormat format=GST_FORMAT_TIME;
				gst_element_query_duration(gstPipeline,&format,&durationNanos);
			}break;

			case GST_MESSAGE_STATE_CHANGED:{
				GstState oldstate, newstate, pendstate;
				gst_message_parse_state_changed(msg, &oldstate, &newstate, &pendstate);
				if(isStream && !bLoaded && appsink){
					appsink->on_stream_prepared();
				}
				/*seek_lock();
				if(posChangingPaused && newstate==GST_STATE_PLAYING){
					gst_element_set_state (gstPipeline, GST_STATE_PAUSED);
					posChangingPaused=false;
				}
				seek_unlock();*/

				ofLog(OF_LOG_VERBOSE,"GStreamer: state changed from " + getName(oldstate) + " to " + getName(newstate) + " (" + getName(pendstate) + ")");
			}break;

			case GST_MESSAGE_ASYNC_DONE:
				ofLog(OF_LOG_VERBOSE,"GStreamer: async done");
			break;

			case GST_MESSAGE_ERROR: {
				GError *err;
				gchar *debug;
				gst_message_parse_error(msg, &err, &debug);

				ofLog(OF_LOG_ERROR, "GStreamer Plugin: Embedded video playback halted; module %s reported: %s",
					  gst_element_get_name(GST_MESSAGE_SRC (msg)), err->message);

				g_error_free(err);
				g_free(debug);

				gst_element_set_state(GST_ELEMENT(gstPipeline), GST_STATE_NULL);

			}break;

			case GST_MESSAGE_EOS:
				ofLog(OF_LOG_VERBOSE,"GStreamer: end of the stream.");
				bIsMovieDone = true;

				switch(loopMode){

					case OF_LOOP_NORMAL:{
						GstFormat format = GST_FORMAT_TIME;
						GstSeekFlags flags = (GstSeekFlags) (GST_SEEK_FLAG_FLUSH |GST_SEEK_FLAG_KEY_UNIT);
						gint64 pos;
						gst_element_query_position(GST_ELEMENT(gstPipeline),&format,&pos);

						float loopSpeed;
						if(pos>0)
							loopSpeed=-speed;
						else
							loopSpeed=speed;
						if(!gst_element_seek(GST_ELEMENT(gstPipeline),
											speed,
											format,
											flags,
											GST_SEEK_TYPE_SET,
											0,
											GST_SEEK_TYPE_SET,
											durationNanos)) {
							ofLog(OF_LOG_WARNING,"GStreamer: unable to seek");
						}
					}break;

					case OF_LOOP_PALINDROME:{
						GstFormat format = GST_FORMAT_TIME;
						GstSeekFlags flags = (GstSeekFlags) (GST_SEEK_FLAG_FLUSH |GST_SEEK_FLAG_KEY_UNIT);
						gint64 pos;
						gst_element_query_position(GST_ELEMENT(gstPipeline),&format,&pos);
						float loopSpeed;
						if(pos>0)
							loopSpeed=-speed;
						else
							loopSpeed=speed;
						if(!gst_element_seek(GST_ELEMENT(gstPipeline),
											loopSpeed,
											GST_FORMAT_UNDEFINED,
											flags,
											GST_SEEK_TYPE_NONE,
											0,
											GST_SEEK_TYPE_NONE,
											0)) {
							ofLog(OF_LOG_WARNING,"GStreamer: unable to seek");
						}
					}break;

					default:
					break;
				}

			break;

			default:
				ofLog(OF_LOG_VERBOSE,"GStreamer: unhandled message");
			break;
		}
		gst_message_unref(msg);
	}

	gst_object_unref(GST_OBJECT(bus));
}

GstElement 	* ofxGstStandaloneUtils::getPipeline(){
	return gstPipeline;
}

GstElement 	* ofxGstStandaloneUtils::getSink(){
	return gstSink;
}


void ofxGstStandaloneUtils::setSinkListener(ofxGstStandaloneAppSink * appsink_){
	appsink = appsink_;
}




//-------------------------------------------------
//----------------------------------------- videoUtils
//-------------------------------------------------



ofxGstStandaloneVideoUtils::ofxGstStandaloneVideoUtils(){
	bIsFrameNew					= false;
	bHavePixelsChanged			= false;

}

ofxGstStandaloneVideoUtils::~ofxGstStandaloneVideoUtils(){
}

void ofxGstStandaloneVideoUtils::close(){
	ofxGstStandaloneUtils::close();
	pixels.clear();
	backPixels.clear();
}

void ofxGstStandaloneVideoUtils::setInternalPixelFormat(ofPixelFormat pixelFormat){
	internalPixelFormat = pixelFormat;
}


bool ofxGstStandaloneVideoUtils::isFrameNew(){
	return bIsFrameNew;
}

unsigned char * ofxGstStandaloneVideoUtils::getPixels(){
	return pixels.getPixels();
}

ofPixelsRef ofxGstStandaloneVideoUtils::getPixelsRef(){
	return pixels;
}

ofTexture ofxGstStandaloneVideoUtils::getTex() {
	return tex;
}

ofTexture & ofxGstStandaloneVideoUtils::getTextureReference(){
	return tex;
}

void ofxGstStandaloneVideoUtils::update(){
	if (isLoaded()){
		mutex.lock();

			bHavePixelsChanged = bBackPixelsChanged;
			if (bHavePixelsChanged){
				bBackPixelsChanged=false;
				pixels = backPixels;
				tex.loadData(pixels.getPixels(), static_cast<int>(getWidth()), static_cast<int>(getHeight()), hfGetGLTypeFromPixelFormat(internalPixelFormat));
			}

		mutex.unlock();
		
	}else{
		ofLog(OF_LOG_WARNING,"not loaded");
	}
	bIsFrameNew = bHavePixelsChanged;
	bHavePixelsChanged = false;
}

float ofxGstStandaloneVideoUtils::getHeight(){
	return static_cast<float>(pixels.getHeight());
}

float ofxGstStandaloneVideoUtils::getWidth(){
	return static_cast<float>(pixels.getWidth());
}

bool ofxGstStandaloneVideoUtils::setPipeline(string pipeline, int bpp, bool isStream, int w, int h){
	string caps;
	if(bpp==8)
		caps="video/x-raw-gray, width=%d, height=%d, depth=8, bpp=8";
	else if(bpp==32)
		caps="video/x-raw-rgb, width=%d, height=%d, depth=32, bpp=32";
	else
		caps="video/x-raw-rgb, width=%d, height=%d, depth=24, bpp=24";

	gchar* pipeline_string =
		g_strdup_printf((pipeline + " ! appsink name=sink caps=\"" + caps + "\"").c_str(),w,h); // caps=video/x-raw-rgb

	if(allocate(w,h,bpp)){
		return setPipelineWithSink(pipeline_string);
	}else{
		return false;
	}
}

bool ofxGstStandaloneVideoUtils::allocate(int w, int h, int bpp){
	pixels.allocate(w,h,bpp/8);
	backPixels.allocate(w,h,bpp/8);
	tex.allocate(w, h, hfGetGLTypeFromPixelFormat(internalPixelFormat));

	pixels.set(0);
	pixels.set(0);

	bHavePixelsChanged = pixels.isAllocated();
	return pixels.isAllocated();
}


GstFlowReturn ofxGstStandaloneVideoUtils::preroll_cb(GstBuffer * buffer){
	guint size;

	size = GST_BUFFER_SIZE (buffer);
	if(backPixels.isAllocated() && backPixels.getWidth()*backPixels.getHeight()*backPixels.getBytesPerPixel()!=(int)size){
		ofLog(OF_LOG_ERROR, "on_preproll: error preroll buffer size: " + ofToString(size) + "!= init size: " + ofToString(backPixels.getWidth()*backPixels.getHeight()*backPixels.getBytesPerPixel()));
		gst_buffer_unref (buffer);
		return GST_FLOW_ERROR;
	}
	//mutex.lock();
	if(backPixels.isAllocated()){
		backPixels.setFromPixels(GST_BUFFER_DATA (buffer),backPixels.getWidth(),backPixels.getHeight(),backPixels.getNumChannels());
		bBackPixelsChanged=true;
		ofNotifyEvent(prerollEvent,backPixels);
	}else{
		ofLog(OF_LOG_WARNING,"received a preroll without allocation");
	}
	//mutex.unlock();


	/// we don't need the appsink buffer anymore
	gst_buffer_unref (buffer);

	return ofxGstStandaloneUtils::preroll_cb(buffer);
}

GstFlowReturn ofxGstStandaloneVideoUtils::buffer_cb(GstBuffer * buffer){
	guint size;

	size = GST_BUFFER_SIZE (buffer);


	if(backPixels.isAllocated() && backPixels.getWidth()*backPixels.getHeight()*backPixels.getBytesPerPixel()!=(int)size){
		ofLog(OF_LOG_ERROR, "on_buffer: error preroll buffer size: " +  ofToString(size) + "!= init size: " + ofToString(backPixels.getWidth()*backPixels.getHeight()*backPixels.getBytesPerPixel()));
		gst_buffer_unref (buffer);
		return GST_FLOW_OK;
	}
	mutex.lock();
	if(backPixels.isAllocated()){
		backPixels.setFromPixels(GST_BUFFER_DATA (buffer),backPixels.getWidth(),backPixels.getHeight(),backPixels.getNumChannels());
		bBackPixelsChanged=true;
		ofNotifyEvent(bufferEvent,backPixels);
	}else{
		ofLog(OF_LOG_WARNING, "received a preroll without allocation");
	}
	mutex.unlock();


	/// we don't need the appsink buffer anymore
	gst_buffer_unref (buffer);

	return ofxGstStandaloneUtils::buffer_cb(buffer);
}

void ofxGstStandaloneVideoUtils::eos_cb(){
	ofxGstStandaloneUtils::eos_cb();
	ofEventArgs args;
	ofNotifyEvent(eosEvent,args);
}
#endif
