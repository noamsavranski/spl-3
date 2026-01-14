#include "../include/StompProtocol.h"
#include <sstream>
#include <iostream>
#include "../include/event.h"
#include <fstream>
#include <algorithm>

//Constructor
StompProtocol::StompProtocol(bool& loggedIn) : 
    gameReports(),        
    userName(""),        
    shouldContinue(loggedIn), 
    subscriptionCounter(0), 
    receiptCounter(0), 
    channelToSubId(), 
    receiptToCommand() 
{

}

//Gets a command and return string in STOMP format for the server to read
std::vector<std::string> StompProtocol::processInput(std::string line) {
    std::stringstream ss(line);
    std::string command;
    ss >> command;
    std::vector<std::string> frames;

    if (command == "login") {
        std::string hostPort, password;
        ss >> hostPort >> userName >> password; 
        return frames;
    }

    if (command == "join") {
        std::string gameName;
        ss >> gameName;
        int subId = subscriptionCounter++; // Creates a unique subscription id
        int recId = receiptCounter++;      // Creates a unique receipt id

        // Saves the state to remember this subscription and receipt
        channelToSubId[gameName] = subId;
        receiptToCommand[recId] = "JOINED " + gameName;

        // Build the SUBSCRIBE frame
        std::string frame = "SUBSCRIBE\ndestination:/" + gameName + 
                           "\nid:" + std::to_string(subId) + 
                           "\nreceipt:" + std::to_string(recId) + "\n\n";
        frames.push_back(frame);
        return frames;
    } 
    else if (command == "exit") {
        std::string gameName;
        ss >> gameName;

        if (channelToSubId.count(gameName) == 0) return frames; // Not subscribed to this channel

        int subId = channelToSubId[gameName];
        int recId = receiptCounter++;
        
        receiptToCommand[recId] = "EXITED " + gameName;
        channelToSubId.erase(gameName); // Remove from memory

        // Build the UNSUBSCRIBE frame
        std::string frame = "UNSUBSCRIBE\nid:" + std::to_string(subId) + 
                           "\nreceipt:" + std::to_string(recId) + "\n\n";
        frames.push_back(frame);
        return frames;
    }
    else if (command == "report") {
        std::string filePath;
        ss >> filePath;

        // Reads the file
        names_and_events parsedData = parseEventsFile(filePath);
        
        for (const auto& event : parsedData.events) {
            // Builds a frame for each
            std::string frame = "SEND\ndestination:/" + parsedData.team_a_name + "_" + parsedData.team_b_name + "\n\n";
            frame += "user: " + userName + "\n";
            frame += "team a: " + event.get_team_a_name() + "\n";
            frame += "team b: " + event.get_team_b_name() + "\n";
            frame += "event name: " + event.get_name() + "\n";
            frame += "time: " + std::to_string(event.get_time()) + "\n";
            
            frame += "general game updates:\n";
            for (auto const& update : event.get_game_updates()) {
                frame += "    " + update.first + ": " + update.second + "\n";
            }
            frame += "team a updates:\n";
            for (auto const& update : event.get_team_a_updates()) {
                frame += "    " + update.first + ": " + update.second + "\n";
               }
            frame += "team b updates:\n";
            for (auto const& update : event.get_team_b_updates()) {
                frame += "    " + update.first + ": " + update.second + "\n";
            }
            frame += "description:\n" + event.get_discription() + "\n";
            
            frames.push_back(frame);
        }
        return frames;
    }
   else if (command == "summary") {
        std::string gameName, userToSummarize, fileName;
        ss >> gameName >> userToSummarize >> fileName;

        // Check if we have information for this game and user in our memory
        if (gameReports.count(gameName) == 0 || gameReports[gameName].count(userToSummarize) == 0) {
            std::cout << "No reports found for user " << userToSummarize << " in game " << gameName << std::endl;
            return frames; 
        }

        // Get a reference to the events vector
        std::vector<Event>& events = gameReports[gameName][userToSummarize];
        
        // Sort the events in chronological order
        std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
            return a.get_time() < b.get_time();
        });

        // Build the output string
        std::string output = "";
        
        // Header of the teams names 
        output += events[0].get_team_a_name() + " vs " + events[0].get_team_b_name() + "\n";
        
        // Getting the latest state 
        output += "Game stats:\n";
        output += "General stats:\n";
        for (auto const& update : events.back().get_game_updates()) {
            output += "    " + update.first + ": " + update.second + "\n";
        }
        
        output += "Team a stats:\n";
        for (auto const& update : events.back().get_team_a_updates()) {
            output += "    " + update.first + ": " + update.second + "\n";
        }

        output += "Team b stats:\n";
        for (auto const& update : events.back().get_team_b_updates()) {
            output += "    " + update.first + ": " + update.second + "\n";
        }

        // Add all reports to output
        output += "Game event reports:\n";
        for (const auto& event : events) {
            output += std::to_string(event.get_time()) + " - " + event.get_name() + ":\n\n";
            output += event.get_discription() + "\n\n";
        }

        // Write output to the specified file
        std::ofstream outFile(fileName);
        if (outFile.is_open()) {
            outFile << output;
            outFile.close();
            std::cout << "Summary saved to " << fileName << std::endl;
        } else {
            std::cout << "Failed to open file: " << fileName << std::endl;
        }

        return frames; // Return empty vector because no STOMP frames are sent to the server
    }
    else if (command == "logout") {
        int recId = receiptCounter++;
        receiptToCommand[recId] = "LOGOUT";

        // Build the DISCONNECT frame
        frames.push_back("DISCONNECT\nreceipt:" + std::to_string(recId) + "\n\n");
        return frames;
    }

    return frames; // Unknown command
}


//Analyze what the server sends and prints relevant information to the client
void StompProtocol::processServerFrame(std::string frame) {
    std::stringstream ss(frame);
    std::string header;
    std::getline(ss, header); // The first line is the command (CONNECTED, MESSAGE, RECEIPT, ERROR)

    if (header == "CONNECTED") {
        std::cout << "Login successful" << std::endl; // Required message
    } 
    else if (header == "RECEIPT") {
        std::string line;
        while (std::getline(ss, line) && line.find("receipt-id:") == std::string::npos);
        
        // Extract the ID and search it in our map
        int recId = std::stoi(line.substr(line.find(":") + 1));
        
        if (receiptToCommand.count(recId)) {
            std::string action = receiptToCommand[recId];
            if (action.find("JOINED") != std::string::npos) {
                std::cout << "Joined channel " << action.substr(7) << std::endl;
            } else if (action.find("EXITED") != std::string::npos) {
                std::cout << "Exited channel " << action.substr(7) << std::endl;
            } else if (action == "LOGOUT") {
                std::cout << "Logout successful. Disconnecting..." << std::endl; 
                shouldContinue = false;
            }
        }
    }
    else if (header == "ERROR") {
        // Extract error type
        std::string line;
        std::string errorMessage = "Unknown error";
        while (std::getline(ss, line) && line != "") {
            if (line.find("message:") != std::string::npos) {
                errorMessage = line.substr(8);
            }
        }
        
        std::cout << "Server Error: " << errorMessage << std::endl; //Print error
        shouldContinue = false; //Stop loop
    }

    else if (header == "MESSAGE") {
        std::string line;
        std::string gameName = "";
        while (std::getline(ss, line) && line != "") {
            if (line.find("destination:/") != std::string::npos) {
                gameName = line.substr(13);
            }
        }
        std::string body = "";
        while (std::getline(ss, line)) {
            body += line + "\n";
        }

        size_t userPos = body.find("user: ");
        size_t endLine = body.find("\n", userPos);
        std::string reportingUser = body.substr(userPos + 6, endLine - (userPos + 6));

        Event newEvent(body); 

        //saves in memory
        gameReports[gameName][reportingUser].push_back(newEvent);
        
        std::cout << "New report from " << reportingUser << " in game " << gameName << std::endl;
}
}