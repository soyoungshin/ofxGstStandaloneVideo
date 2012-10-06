#include "ofMain.h"
#include "testApp.h"
#include "ofAppGlutWindow.h"

//========================================================================
int main( ){

    ofAppGlutWindow window;
	
	// for linux, is in full screen
	//ofSetupOpenGL(&window, 1680 * 2, 1050 * 3, OF_GAME_MODE);

	// for windows
	ofSetupOpenGL(&window, 800, 700, OF_WINDOW); // <-------- setup the GL context
	ofSetWindowPosition(50, 200);

	ofRunApp( new testApp());
}
