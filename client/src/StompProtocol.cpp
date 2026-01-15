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

            if (gameReports.count(gameName) == 0 || gameReports[gameName].count(userToSummarize) == 0) {
                std::cout << "No reports found for user " << userToSummarize << " in game " << gameName << std::endl;
                return frames; 
            }

            std::vector<Event>& events = gameReports[gameName][userToSummarize];
            
            // Sort events chronologically by time
            std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
                if (a.get_time() != b.get_time())
                    return a.get_time() < b.get_time();
                return a.get_name() < b.get_name(); // Secondary sort by name
            });

            // Use maps to aggregate stats 
            std::map<std::string, std::string> generalStats;
            std::map<std::string, std::string> teamAStats;
            std::map<std::string, std::string> teamBStats;

            // Iterate through all events to build the final state of the game
            for (const auto& event : events) {
                for (auto const& update : event.get_game_updates()) 
                    generalStats[update.first] = update.second;
                for (auto const& update : event.get_team_a_updates()) 
                    teamAStats[update.first] = update.second;
                for (auto const& update : event.get_team_b_updates()) 
                    teamBStats[update.first] = update.second;
            }

            std::string output = "";
            
            // Header with team names
            output += events[0].get_team_a_name() + " vs " + events[0].get_team_b_name() + "\n";
            
            output += "Game stats:\n";
            
            output += "General stats:\n";
            for (auto const& it : generalStats) {
                output += it.first + ": " + it.second + "\n";
            }
            
            output += "Team a stats:\n";
            for (auto const& it : teamAStats) {
                output += it.first + ": " + it.second + "\n";
            }

            output += "Team b stats:\n";
            for (auto const& it : teamBStats) {
                output += it.first + ": " + it.second + "\n";
            }

            // List all game event reports
            output += "Game event reports:\n";
            for (const auto& event : events) {
                output += std::to_string(event.get_time()) + " - " + event.get_name() + ":\n\n";
                output += event.get_discription() + "\n\n"; // Fixed typo from 'discription'
            }

            // Save to file (overwriting existing content)
            std::ofstream outFile(fileName);
            if (outFile.is_open()) {
                outFile << output;
                outFile.close();
                std::cout << "Summary saved to " << fileName << std::endl;
            } else {
                std::cout << "Failed to open file: " << fileName << std::endl;
            }

            return frames; 
        }
        else if (command == "logout") {
            int recId = receiptCounter++;
            receiptToCommand[recId] = "LOGOUT";

            // יצירת פריים הדיסקונקט
            std::string frame = "DISCONNECT\nreceipt:" + std::to_string(recId) + "\n\n";
            frames.push_back(frame + '\0'); // מוסיפים \0 לסיום פריים
            return frames;
        }
        return std::vector<std::string>();
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
                //connectionHandler.close(); 
                //isLoggedIn = false;
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
            std::string gameName = "Unknown";
            std::string reportingUser = "Unknown";

            // Parse headers until the blank line separator (\n\n)
            while (std::getline(ss, line) && line != "" && line != "\r") {
                // Extract game name from destination header
                if (line.find("destination:") != std::string::npos) {
                    gameName = line.substr(line.find(":") + 1);
                    // Clean up gameName: remove leading '/' and trailing '\r'
                    if (!gameName.empty() && gameName[0] == '/') gameName = gameName.substr(1);
                    if (!gameName.empty() && gameName.back() == '\r') gameName.pop_back();
                } 
                // Extract user from headers 
                else if (line.find("user:") != std::string::npos) {
                    reportingUser = line.substr(line.find(":") + 1);
                    if (!reportingUser.empty() && reportingUser.back() == '\r') reportingUser.pop_back();
                }
            }

            // Parse the body content 
            std::string body = "";
            while (std::getline(ss, line)) {
                body += line + "\n";
            }

            //  Create Event object from the received body
            Event newEvent(body); 

            // Save the event in the map for summary purposes
            gameReports[gameName][reportingUser].push_back(newEvent);
            
            //  Final output including the extracted description
            std::cout << "New report from " << reportingUser 
                    << " in game " << gameName 
                    << " - Description: " << newEvent.get_discription() << std::endl;
        }
}