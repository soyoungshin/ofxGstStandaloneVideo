#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
	player.setPixelFormat(OF_PIXELS_RGB);
	string file = ofFilePath::getAbsolutePath("fingers.mov", true);
	std::replace(file.begin(), file.end(), '\\', '/');

	// prepend the protocol
	file = "file:///" + file;
	
	if(!player.loadMovie(file)) {
		ofLogError("setup", "movie loading failed: \n" + file);
	}

	player.play();
}

//--------------------------------------------------------------
void testApp::update(){
	player.update();
}

//--------------------------------------------------------------
void testApp::draw(){
	player.draw(0, 0, ofGetWidth(), ofGetHeight());
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){

	// some code for bezel adjustment.
	// TODO: incorporate this in to a control panel gui.
	switch(key) {
		case ' ':
			player.setPaused(!player.isPaused());
			break;
		default:
			break;
	}	
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 

}