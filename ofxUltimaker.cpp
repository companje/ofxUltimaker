#include "ofxUltimaker.h"

ofxUltimaker::ofxUltimaker() {
    isBusy = false;
    isPrinting = false;
    temperature = 0;
    isStartTagFound = false;
}

bool ofxUltimaker::autoConnect() {
    //without params autodetect ultimaker
    listDevices();
    buildDeviceList();
    for (int k=0; k < (int)devices.size(); k++){
        if (devices[k].getDeviceName().find("usbmodem")!=string::npos ||
            devices[k].getDeviceName().find("usbserial")!=string::npos) { //osx
            return connect(k);
        }
    }
}

bool ofxUltimaker::connect(int portnumber, int speed) {
    return connect(devices[portnumber].getDeviceName(), speed);
}

bool ofxUltimaker::connect(string portname, int speed) {
    if ((isConnected = setup(portname,speed))) {
        #if OF_VERSION_MINOR==0
            ofAddListener(ofEvents.update, this, &ofxUltimaker::update);
        #else
            ofAddListener(ofEvents().update, this, &ofxUltimaker::update);
        #endif
    }
    return isConnected;
}

void ofxUltimaker::readTemperature() {
    send("M105");
}
//    string result = request("M105");
//
//    if (isOk(result)) {
//        return ofToFloat(ofSplitString(ofSplitString(result,",")[0],":")[1]);
//    } else {
//        ofLogError() << "Problem reading Ultimaker temperature" << endl;
//        return 0;
//    }
//}
////
void ofxUltimaker::setTemperature(float temperature) { //, bool waitUtilReached) {
//    if (waitUtilReached) {
//        isBusy =
//    }
    request("M104 S"+ofToString(temperature));
    //string str = request( (waitUtilReached ? "M104" : "M109") + " S" + ofToString(temperature));
    //send("M104 S"+ofToString(temperature));
    //isBu
}
//
//void ofxUltimaker::setTemperature(float temperature) {
//
//}

void ofxUltimaker::load(string filename) {
    cout << "ultimaker load: " << filename << endl;
    gcode.load(filename);
}

void ofxUltimaker::startPrint() {
    isPrinting = true;
    currentLine = 0;
    cout << "start" << endl;
    processQueue();
}

void ofxUltimaker::processQueue() {
    //ignore comments and empty lines
    while (1) {
        if (currentLine>=gcode.lines.size()) return;
        else if (gcode.lines.at(currentLine)=="") currentLine++;
        else if (gcode.lines.at(currentLine).at(0)==';') currentLine++;
        else { 
            send(gcode.lines.at(currentLine));
            currentLine++;
            return; 
        }
    }    
}

void ofxUltimaker::update(ofEventArgs &e) {
    
    for (int i=0; i<100; i++) {
        string str = ofxGetSerialString(*this,'\n');
        if (str!="") {

            cout << "> " << str << endl;
            
            if (str.find("start")!=string::npos) isStartTagFound=true;

            if (isOk(str)) {

                if (str.size()>4) {
                    if (str[3]=='T') {
                        temperature = ofToFloat(ofSplitString(ofSplitString(str,":")[1],",")[0]);
                    }
                }

                isBusy = false;
                processQueue();
                
            } else {

                if (str[0]=='T') { //got temperature
                    temperature = ofToFloat(ofSplitString(str,":")[1]);
                }
            }
        }
    }
}

void ofxUltimaker::stopPrint() {
    cout << "stop" << endl;
    isPrinting = false;
    isBusy = false;
    gcode.lines.clear();
}

void ofxUltimaker::setAbsolute() {
    request("G90 (absolute)");
}

void ofxUltimaker::setRelative() {
    request("G91 (relative)");
}

void ofxUltimaker::extrude(float amount, float speed) {
    send("G1 F"+ofToString(speed)+" E"+ofToString(amount));
}

void ofxUltimaker::physicalHomeXYZ() {
    send("G28 X0 Y0 Z0 (physical home)");
}

bool ofxUltimaker::isOk(string str) {
    return (str.length()>=2 && str[0]=='o' && str[1]=='k');
}

bool ofxUltimaker::isComment(string str) {
    return (str.length()>0 && str.at(0)==';');
}

void ofxUltimaker::moveTo(float x, float y) {
    //send(ofVAArgsToString("G1 X%03f Y%03f",x,y));
}

string ofxUltimaker::request(string cmd) {
    if (!isConnected) return "";
    
    //dit moet worden vervangen door async event. Of kan het naast elkaar leven?
    send(cmd);
    string str;
    int ttl=100000;
    while (str=="" && ttl-->0) str=ofxGetSerialString(*this,'\n');
    if (str!="") {
        cout << str << endl;
        isBusy=false;
    }
    //if (isOk(str))
    return str;
}

void ofxUltimaker::send(string cmd) {
    if (!isConnected) return;
    if (isBusy) {
        cout << "Ultimaker is busy processing '"<<prevCmd<<"'. Please resend '"<<cmd<<"' again later" << endl;
    } else {
        cout << "Sending: " << cmd << endl;
        isBusy = true;
        ofxSerialWriteLine(*this,cmd);
        prevCmd = cmd;
    }
}
