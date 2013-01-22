#pragma once

#include "ofMain.h"
#include "ofxSerial.h"

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
    deque<string> messages;
    
    ofxUltimaker() {
        isConnectedToPort = false;
        isStartTagFound = false;
        deviceSpeed = DEFAULT_DEVICE_SPEED;
        isReadyForNextCommand = false;
        frameRate = 100;
        temperature = 0;
    }
    
    void setup(string _deviceName="", int _deviceSpeed=DEFAULT_DEVICE_SPEED) {
        deviceName = _deviceName;
        deviceSpeed = _deviceSpeed;
        
        if (deviceName=="") {
             deviceName = guessDeviceName();
        }
        
        startThread(true, false);   // blocking, verbose
    }
    
    void update() {
        if (!checkConnection()) return;
        
        processQueue();
    }
    
    bool checkConnection() {
        if (!isConnectedToPort) {
            isConnectedToPort = serial.setup(deviceName, deviceSpeed);
            
            if (!isConnectedToPort) {
                stopThread();
                ofLogError() << "could not connect to '" << deviceName << "' at " << deviceSpeed << " bps";
                vector<string> devices = getArduinoDevices();
                if (devices.size()>0) cout << "other options: " << endl;
                for (int i=0; i<devices.size(); i++) {
                    cout << devices.at(i) << endl;
                }
                return;
            }
        }
        
        if (isConnectedToPort && !isStartTagFound && deviceSpeed==250000) {
            isStartTagFound = waitForStartTag();
            if (!isStartTagFound) {
                ofLogNotice() << "no firmware 'start' found at " << deviceSpeed << " bps";
                serial.close();
                deviceSpeed = 115200;
                isConnectedToPort = serial.setup(deviceName, deviceSpeed);
            } else {
                ofLogNotice() << "successfully connected to firmware at " << deviceSpeed << " bps";
                isReadyForNextCommand = true;
            }
        }
        
        if (isConnectedToPort && !isStartTagFound && deviceSpeed==115200) {
            isStartTagFound = waitForStartTag();
            if (!isStartTagFound) {
                ofLogNotice() << "no firmware 'start' found at " << deviceSpeed << " bps";
            } else {
                ofLogNotice() << "successfully connected to firmware at speed " << deviceSpeed << " bps";
                isReadyForNextCommand = true;
            }
        }
                
        return isConnectedToPort && isStartTagFound;
    }

    bool waitForStartTag(int timeToWait=10) {
        for (int i=0; i<10*timeToWait; i++) { //max 10*sec tries at 100fps
            string str = serial.readLine();
            string key = "start";
            size_t idx = str.rfind(key);
            if (idx!=string::npos && idx==(str.length()-key.length())) return true;
            ofSleepMillis(10);
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
        if (clearQueue) queue.clear();
        ifstream f(ofToDataPath(filename).c_str(),ios::in);
        string line;
        while (getline(f,line)) {
            addToQueue(line);
        }
        f.close();
        
    }
    
    void processQueue() {
        string s = serial.readLine();
        if (s!="") messages.push_back(s);
        
        if (s.find("ok")==0) {
            isReadyForNextCommand = true;
        }
        
        if (s.size()>1 && s[0]=='T') {
            temperature = ofToFloat(ofSplitString(s,":")[1]);
        }
        
        if (s.size()>4 && s[3]=='T') {
            temperature = ofToFloat(ofSplitString(s,":")[1]);
        }
        
        if (isReadyForNextCommand && !queue.empty()) {
            string cmd = queue.front();
            queue.pop_front();
            messages.push_back(cmd);
            serial.writeLine(cmd);
            isReadyForNextCommand = false;
        }
        
        while (messages.size()>MSG_HISTORY_COUNT) {
            messages.pop_front();
        }
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
    
    string guessDeviceName() {
        vector<string> devices = getArduinoDevices();
        return devices.size()>0 ? devices.at(0) : "";
    }
    
    vector<string> getArduinoDevices() {
        vector<string> names;
        vector<ofSerialDeviceInfo> devices = serial.getDeviceList();
        for (size_t i=0; i<devices.size(); i++) {
            if (devices[i].getDevicePath().find("usbmodem")!=string::npos ||
                (devices[i].getDevicePath().find("usbserial")!=string::npos)) {
                names.push_back(devices[i].getDevicePath());
            }
        }
        return names;
    }
    
    ~ofxUltimaker() {
        cout << "~ofxUltimaker" << endl;
        stopThread();
    }
    
};
