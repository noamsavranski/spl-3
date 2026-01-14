#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"

int main(int argc, char *argv[]) {
    // TODO: implement the STOMP client
    ConnectionHandler* handler = nullptr;
    bool loggedIn = false;
    StompProtocol protocol(loggedIn); 
    
    while (!loggedIn) {
        std::string line; //saves what the client entered
        if (!std::getline(std::cin, line)) break; //waits for the client to type and press enter, insert each line typed to "line"
        std::stringstream ss(line); //makes a stream from string
        
        //takes the first word
        std::string command;
        ss >> command;
        
        if (command == "login") {
            std::string hostPort, username, password;
            if (!(ss >> hostPort >> username >> password)) {
                std::cout << "Invalid login command format" << std::endl;
                continue;
            }
            
            //seperating host and port
            size_t colonPos = hostPort.find(':');
            if (colonPos == std::string::npos) {
                std::cout << "Invalid host:port format" << std::endl;
                continue;
            }
            
            std::string host = hostPort.substr(0, colonPos);
            short port = static_cast<short>(std::stoi(hostPort.substr(colonPos + 1)));
            
            // Initialize connection handler and try to connect to the server 
            if(handler) {delete handler;} //if the client tries to login twice
            handler = new ConnectionHandler(host, port);
            if (!handler->connect()) {
                std::cout << "Could not connect to server" << std::endl; // Required error message 
                delete handler;
                handler = nullptr;
                continue;
            }
            
            // make and send the STOMP CONNECT 
            std::string connectFrame = "CONNECT\n"
                                       "accept-version:1.2\n"
                                       "host:stomp.cs.bgu.ac.il\n"
                                       "login:" + username + "\n"
                                       "passcode:" + password + "\n"
                                       "\n";

            if (handler->sendFrameAscii(connectFrame, '\0')) {
                // Connection sent successfully
                loggedIn = true; 
            }
            else {
                std::cout << "Failed to send CONNECT frame" << std::endl;
                delete handler;
                handler = nullptr;
            }
        }
        else {
            std::cout << "You must login first" << std::endl;
        }
    }

    if (loggedIn && handler != nullptr) { //if logged  and connected succesfully
        //Socket Thread- listens for frames from the server 
        std::thread socketThread([handler, &protocol, &loggedIn]() {
            while (loggedIn) {
                std::string frame;
                if (!handler->getFrameAscii(frame, '\0')) {
                    std::cout << "Disconnected from server" << std::endl;
                    loggedIn = false;
                    break;
                }
                protocol.processServerFrame(frame); 
            }
        });

        // Reads user input and sends frames to the server 
        while (loggedIn) {
            std::string line;
            if (!std::getline(std::cin, line)) break;

            // gets all the messages (for the report)
            std::vector<std::string> framesToSend = protocol.processInput(line);
            // sending all frames
            for (const std::string& frame : framesToSend) {
                if (!frame.empty()) {
                    if (!handler->sendFrameAscii(frame, '\0')) {
                        loggedIn = false;
                        break;
                    }
                }
            }

            if (!loggedIn) break; 
        }
        //Waiting for the socket thread to finish before exiting 
        if (socketThread.joinable()) {
            socketThread.join();
        }
    }

    // Closing and deleting the handler resource
    if (handler) {
        handler->close();
        delete handler;
    }

    return 0;
}