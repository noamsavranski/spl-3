#pragma once
#include <string>
#include <map>
#include <vector>
#include "../include/ConnectionHandler.h"
#include "event.h"

// TODO: implement the STOMP protocol
class StompProtocol
{
    private:
        std::map<std::string, std::map<std::string, std::vector<Event>>> gameReports; //to enable summary
        std::string userName;
        bool& shouldContinue; // Variable to control the loops
        // Counters to generate unique IDs for subscriptions and receipts
        int subscriptionCounter;
        int receiptCounter;

        // State Management:
        // Maps a channel name (e.g., "germany_japan") to its Subscription ID
        // This is needed to know which ID to use when sending an UNSUBSCRIBE frame
        std::map<std::string, int> channelToSubId;

        // Maps a Receipt ID to the command that triggered it (e.g., "JOIN", "EXIT", "LOGOUT")
        // This allows the client to print "Joined channel X" when the server sends a RECEIPT
        std::map<int, std::string> receiptToCommand;

    public:
        StompProtocol(bool& loggedIn);
        /**
         * Translates a raw keyboard command (e.g., "join germany") 
         * into a valid STOMP frame string to be sent to the server.
         */
        std::vector<std::string> processInput(std::string line);
        /**
         * Processes a STOMP frame received from the server (e.g., MESSAGE, RECEIPT, ERROR)
         * and determines what should be printed to the screen or updated in the state.
         */
        void processServerFrame(std::string frame);
    };
