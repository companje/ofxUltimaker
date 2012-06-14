#pragma once
#include "ofMain.h"
#include "ofxGCode.h"
#include "ofxExtras.h"

//struct ofxUltimakerResponse {
//    string cmd;
//    string response;
//};
//
//class ofxUltimakerListener {
//public:
//    void onUltimakerResponse(ofxUltimakerResponse response);
//};

class ofxUltimaker : public ofSerial {
public:
    ofxUltimaker();
    bool connect(int portnumber=0, int speed=115200);
    bool connect(string portname="", int speed=115200);
    void readTemperature();
    void setTemperature(float temperature); //, bool waitUtilReached=false);
    void moveTo(float x, float y);
    bool isOk(string str);
    void physicalHomeXYZ();
    void setAbsolute();
    void setRelative();
    void extrude(float amount, float speed);

    void load(string filename);
    void startPrint();
    void stopPrint();
    void update(ofEventArgs &e);

    bool isPrinting;
    bool isBusy;
    ofxGCode gcode;
    int currentLine;
    string prevCmd;
    float temperature;
    int waitTimer;

    string request(string cmd);
    void send(string cmd);

    //ofxUltimakerListener *listener;

};
