#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup()
{
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	ofBackground(0);
	
	ofAddListener(repo.complete, this, &testApp::onCloneComplete);
	
	ofSetLogLevel(OF_LOG_VERBOSE);
	
	ofDirectory dir("ofxFaceTracker");
	if (dir.exists())
		dir.remove(true);
	
	if (!repo.open("ofxFaceTracker"))
	{
		// $ git clone https://github.com/kylemcdonald/ofxFaceTracker.git
		repo.clone("https://github.com/kylemcdonald/ofxFaceTracker.git", false);
	}
}

//--------------------------------------------------------------
void testApp::update()
{

}

//--------------------------------------------------------------
void testApp::draw()
{

}

//--------------------------------------------------------------
void testApp::onCloneComplete(ofEventArgs&)
{
	ofSystemAlertDialog("clone done");
	
	cout << "1: ==============================" << endl;
	
	// $ git branch -a
	repo.dump();
	cout << "cur branch: " << repo.getCurrentBranchName() << endl;
	
	cout << "2: ==============================" << endl;
	
	// $ git checkout -b origin/osc-syphon
	repo.getBranch("origin/osc-syphon")->fork("osc-syphon")->makeHead();
	repo.dump();
	cout << "cur branch: " << repo.getCurrentBranchName() << endl;
	
	cout << "3: ==============================" << endl;
	
	// $ git checkout master
	repo.getBranch("master")->makeHead();
	repo.dump();
	cout << "cur branch: " << repo.getCurrentBranchName() << endl;
	
	cout << "4: ==============================" << endl;
	
	// $ git branch -d osc-syphon
	repo.getBranch("osc-syphon")->remove();
	repo.dump();
	cout << "cur branch: " << repo.getCurrentBranchName() << endl;
	
	cout << "5: ==============================" << endl;

	
}

//--------------------------------------------------------------
void testApp::keyPressed(int key)
{

}

//--------------------------------------------------------------
void testApp::keyReleased(int key)
{

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y)
{

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button)
{

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button)
{

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button)
{

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h)
{

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg)
{

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo)
{

}