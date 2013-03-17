#pragma once

#include "ofMain.h"
#include "ofxSerial.h"
#include "ofxExtras.h"

class ofxUltimaker : public ofThread {
public:

    ofxSerial serial;
    string deviceName;
    int deviceSpeed;
    bool isConnectedToPort;
    bool isStartTagFound;
    bool isReadyForNextCommand;
    static const int DEFAULT_DEVICE_SPEED = 250000;
    static const int MSG_HISTORY_COUNT = 20;
    int counterForPort;
    int frameRate;
    deque<string> queue;
    float temperature;
//    deque<string> messages;
    vector<string> deviceNames;
    int deviceIndex;
    int preBuffer;
    
    ofxUltimaker() {
        preBuffer = 0;
        isConnectedToPort = false;
        isStartTagFound = false;
        deviceSpeed = DEFAULT_DEVICE_SPEED;
        isReadyForNextCommand = false;
        frameRate = 800;
        temperature = 0;
        deviceIndex = 0;
    }
    
    void setup() {
        startThread(true, false);   // blocking=true, verbose=false
    }
    
    void update() {
        if (!checkConnection()) return;
        
        lockDevice(deviceName); //touch lock-file
   
        processQueue();
    }
    
    void reconnect(string deviceName, int deviceSpeed) {
        cout << "reconnect: " << deviceName << " @ " << deviceSpeed << endl;
        ofSleepMillis(500);
        serial.close();
        ofSleepMillis(500);
        serial.close();
        ofSleepMillis(500);
        isConnectedToPort = serial.setup(deviceName, deviceSpeed);
        ofSleepMillis(500);
        serial.close();
        serial.close();
        serial.flush();
        ofSleepMillis(500);
        isConnectedToPort = serial.setup(deviceName, deviceSpeed);
        ofSleepMillis(500);
        cout << (isConnectedToPort ? "successfully re-connected" : "could not re-connect") << endl;
    }
    
    bool isDeviceLocked(string path) {
        string name = ofxStringAfterFirst(path,".")+".lock";
        unsigned int age = ofxGetFileAge(ofFile(name).getAbsolutePath());
        ofLogNotice() << "age of lock file: " << age;
        if (age>20) unlockDevice(path);
        return ofFile(name).exists();
    }
    
    void lockDevice(string path) {
        string name = ofxStringAfterFirst(path,".");
        ofxSaveString(name+".lock", "locked");
    }
    
    void unlockDevice(string path) {
        string name = ofxStringAfterFirst(path,".");
        if (ofFile(name+".lock").exists()) {
            ofLogNotice() << "unlocking " << name;
            ofFile(name+".lock").remove();
        }
    }
    
    void unlockAllDevices() {
        deviceNames = serial.getArduinoDevices();
        for (int i=0; i<deviceNames.size(); i++) {
            unlockDevice(deviceNames[i]);
        }
    }
    
    bool checkConnection() {

        if (!isConnectedToPort) {
        
            deviceNames = serial.getArduinoDevices();
            deviceSpeed = DEFAULT_DEVICE_SPEED;

            for (int i=0; i<deviceNames.size(); i++) {
                cout << i << ": " << deviceNames[i] << endl;
                
                if (!isDeviceLocked(deviceNames[i])) {
                    deviceName = deviceNames[i];
                    lockDevice(deviceName);
                    //isConnectedToPort = serial.setup(deviceName, deviceSpeed);
                    reconnect(deviceName, deviceSpeed);
                    break;
                }
            }
            
            if (!isConnectedToPort) {
                ofLogError() << "no ports available to connect to";
                ofSleepMillis(1000);
                //stopThread();
            }
            
        }
        
        if (isConnectedToPort && !isStartTagFound && deviceSpeed==250000) { 
            
            isStartTagFound = waitForStartTag();
            if (!isStartTagFound) {
                ofLogNotice() << "no firmware 'start' found at " << deviceSpeed << " bps";
                //disconnect();
                deviceSpeed = 115200;
                ofLogNotice() << "Fall back to " << deviceSpeed << " bps";
                //isConnectedToPort = serial.setup(deviceName, deviceSpeed);
                reconnect(deviceName,deviceSpeed);
            } else {
                ofLogNotice() << "successfully connected to firmware at " << deviceSpeed << " bps";
                isReadyForNextCommand = true;
            }
        }
        
        if (isConnectedToPort && !isStartTagFound && deviceSpeed==115200) {
            isStartTagFound = waitForStartTag();
            if (!isStartTagFound) {
                ofLogNotice() << "no firmware 'start' found at " << deviceSpeed << " bps";
                deviceSpeed = 57600;
                ofLogNotice() << "Fall back to " << deviceSpeed << " bps";
                reconnect(deviceName,deviceSpeed);
            } else {
                ofLogNotice() << "successfully connected to firmware at speed " << deviceSpeed << " bps";
                isReadyForNextCommand = true;
            }
        }
        
        if (isConnectedToPort && !isStartTagFound && deviceSpeed==57600) {
            isStartTagFound = waitForStartTag();
            if (!isStartTagFound) {
                ofLogNotice() << "no firmware 'start' found at " << deviceSpeed << " bps";
                ofLogNotice() << "starting all over again..." << endl;
                isConnectedToPort = false;
                isStartTagFound = false;
                unlockDevice(deviceName);
                //stopThread(); //everything failed
            } else {
                ofLogNotice() << "successfully connected to firmware at speed " << deviceSpeed << " bps";
                isReadyForNextCommand = true;
            }
        }
        
        if (isConnectedToPort && isStartTagFound) {
            //still check for disconnected device
            deviceNames = serial.getArduinoDevices();
            if (!ofxContains(deviceNames,deviceName)) {
                isStartTagFound = false;
                isConnectedToPort = false;
                unlockDevice(deviceName);
            }
        }
        
        return isConnectedToPort && isStartTagFound;
    }

    bool waitForStartTag(int timeToWait=5) {
        cout << "wait for start tag..." << timeToWait << " sec" << endl;
        for (int i=0; i<10*timeToWait; i++) { //max 10*sec tries at 10fps
            string str = ofxTrimString(serial.readLine());
            if (str!="") cout << str << endl;
            string key = "start";
            size_t idx = str.rfind(key);
            if (str=="start" || (idx!=string::npos && idx==(str.length()-key.length()))) {
                ofLogNotice() << "start tag found!";
                return true;
            }
            ofSleepMillis(100);
        }
        return false;
    }
    
    void addToQueue(string command, int priority=0) {  //0=back, 1=front, 2=overwrite queue
        switch (priority) {
            case 0: queue.push_back(command); break;
            case 1: queue.insert(queue.begin(),command); break;
            case 2: queue.clear(); queue.push_back(command); break;
        }
    }
    
    void sendCommand(string s, int priority=0) {
        addToQueue(s,priority);
    }
    
    void sendCommandsFromFile(string filename, bool clearQueue=true) {
        if (lock()) {
            if (clearQueue) {
                queue.clear();
                preBuffer=3;
            }
            ifstream f(ofToDataPath(filename).c_str(),ios::in);
            string line;
            while (getline(f,line)) {
                addToQueue(line);
            }
            f.close();
            unlock();
        }
    }
    
    void processQueue() {
        string s = serial.readLine();
        //if (s!="") messages.push_back(s);
        
        if (s.find("ok")==0) {
            isReadyForNextCommand = true;
        }
        
        if (s.find("rs")==0) { //for old firmware
            isReadyForNextCommand = true;
        }
        
        if (s.size()>1 && s[0]=='T') {
            temperature = ofToFloat(ofSplitString(s,":")[1]);
        }
        
        if (s.size()>4 && s[3]=='T') {
            temperature = ofToFloat(ofSplitString(s,":")[1]);
        }
        
        if ((preBuffer>0 || isReadyForNextCommand)) {
            if (preBuffer>0) preBuffer--;

            if (!queue.empty()) {
                string cmd = queue.front();
                queue.pop_front();
                //messages.push_back(cmd);
                serial.writeLine(cmd);
                isReadyForNextCommand = false;
            }
        }
        
//        while (messages.size()>MSG_HISTORY_COUNT) {
//            messages.pop_front();
//        }
    }
    
    void threadedFunction() {
        while (isThreadRunning() != 0 ){
            if (lock()) {
                update();
                unlock();
                ofSleepMillis(1000/frameRate); //1000/frameRate = milliseconds
            }
        }
    }
    
    ~ofxUltimaker() {
        cout << "~ofxUltimaker" << endl;
        unlockDevice(deviceName);
        if (isThreadRunning()) stopThread();
    }
    
};
