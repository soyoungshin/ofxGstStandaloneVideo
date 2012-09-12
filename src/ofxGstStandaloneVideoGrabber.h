#pragma once

#include "ofxGstStandaloneUtils.h"

struct ofxGstStandaloneFramerate{
  int numerator;
  int denominator;
};

struct ofxGstStandaloneVideoFormat{
  string mimetype;
  int    width;
  int    height;
  vector<ofxGstStandaloneFramerate> framerates;
  ofxGstStandaloneFramerate choosen_framerate;
};

struct ofxGstStandaloneDevice{
  string video_device;
  string gstreamer_src;
  string product_name;
  vector<ofxGstStandaloneVideoFormat> video_formats;
  int current_format;
};

struct ofxGstStandaloneCamData{
  vector<ofxGstStandaloneDevice> webcam_devices;
  bool bInited;
};

class ofxGstStandaloneVideoGrabber: public ofBaseVideoGrabber{
public:
	ofxGstStandaloneVideoGrabber();
	~ofxGstStandaloneVideoGrabber();

	/// needs to be called before initGrabber
	void setPixelFormat(ofPixelFormat pixelFormat);
	void videoSettings(){};//TODO: what is this??

	void listDevices();
	void setDeviceID(int id);
	void setDesiredFrameRate(int framerate);
	bool initGrabber(int w, int h);

	void 			update();
	bool 			isFrameNew();

	unsigned char * getPixels();
	ofPixelsRef		getPixelsRef();

	float 			getHeight();
	float 			getWidth();
	void 			close();

	void			setVerbose(bool bVerbose);

	ofxGstStandaloneVideoUtils *	getGstVideoUtils();
private:
	ofxGstStandaloneVideoFormat&	selectFormat(int w, int h, int desired_framerate);

	ofxGstStandaloneCamData		camData;
	bool				bIsCamera;
	int					attemptFramerate;
	int					deviceID;
	ofPixelFormat		internalPixelFormat;
	ofxGstStandaloneVideoUtils		videoUtils;
};
