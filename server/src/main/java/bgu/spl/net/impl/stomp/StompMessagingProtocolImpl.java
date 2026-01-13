package bgu.spl.net.impl.stomp;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {
    private int connectionId;
    private Connections<String> connections;
    private boolean shouldTerminate = false;
    private String loggedInUser = null;
    private static final java.util.concurrent.ConcurrentHashMap<String, String> users = new java.util.concurrent.ConcurrentHashMap<>();
    private static final ConcurrentHashMap<String, Boolean> activeUsers = new ConcurrentHashMap<>();

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }
    @Override
    public void process(String message) {
        String actualMessage = message.replace("\u0000", "");
        String[] lines = message.split("\n");
        if (lines.length == 0) {
            return;
        }
        String command = lines[0].trim();
        Map<String, String> headers = new java.util.HashMap<>();
        int i = 1;
        while (i < lines.length && !lines[i].isEmpty()) {
            String[] parts = lines[i].split(":", 2);
            if (parts.length == 2) { // just if we manage to split 
                headers.put(parts[0].trim(), parts[1].trim());
            }
            i++;
        }
        String body = "";
        if (i < lines.length) {
            StringBuilder bodyBuilder = new StringBuilder();
             for (int j = i + 1; j < lines.length; j++) {
                 bodyBuilder.append(lines[j]).append("\n");
             }
             body = bodyBuilder.toString();
        }
        switch (command) {
            case "CONNECT":
                handleConnect(headers);
                break;
            case "SUBSCRIBE":
                handleSubscribe(headers);
                break;
            case "UNSUBSCRIBE":
                handleUnsubscribe(headers);
                break;
            case "SEND":
                 handleSend(headers, body);
                break;
            case "DISCONNECT":
                handleDisconnect(headers);
                break;
            default:
                System.out.println("Unknown command: " + command);
        }
    }
    private void handleConnect(java.util.Map<String, String> headers) {
        if (!headers.containsKey("accept-version") || !headers.containsKey("host") || !headers.containsKey("login") || !headers.containsKey("passcode")) {
            sendError("Malfromed Frame", "Missing mandatory headers for CONNECT");
            return;
        }
        String login = headers.get("login");
        String passcode = headers.get("passcode");

        if (loggedInUser != null) {
            sendError("Connection error", "Client already logged in");
            return;
        }
        if (activeUsers.containsKey(login)) {
            sendError("User already logged in", "User is active on another connection");
            return;
        }

        if (users.containsKey(login)) {
            String storedPass = users.get(login);
            if (!storedPass.equals(passcode)) {
                sendError("Login failed", "Wrong password");
                return;
            }
        }
        else {
            users.put(login, passcode);
        }
        
        loggedInUser = login;
        activeUsers.put(login, true);
        String response = "CONNECTED\n" +
                          "version:1.2\n" +
                          "\n";
        connections.send(connectionId, response); 
    }
    private void sendError(String message, String description) {
        String errorFrame = "ERROR\n" + "message:" + message + "\n" +"\n" +"The details:\n" + description + "\n";      
        connections.send(connectionId, errorFrame);
        shouldTerminate = true; 
        connections.disconnect(connectionId);
    }
    private void handleSubscribe(java.util.Map<String, String> headers) {
        if (!validateHeaders(headers, "destination", "id")) return;
        String topic = headers.get("destination");
        String subId = headers.get("id");
        connections.subscribe(topic, connectionId, Integer.parseInt(subId));
        System.out.println("Client " + connectionId + " subscribed to " + topic);
        checkForReceipt(headers);
    }

    private void checkForReceipt(Map<String, String> headers) {
        if (headers.containsKey("receipt")) {
            String receiptId = headers.get("receipt");
            String response = "RECEIPT\n" + "receipt-id:" + receiptId + "\n" + "\n" +"\u0000";
            connections.send(connectionId, response);
        }
    }

    private boolean validateHeaders(java.util.Map<String, String> headers, String... requiredHeaders) {
        for (String key : requiredHeaders) {
            if (!headers.containsKey(key)) {
                sendError("Malformed Frame", "Missing header: " + key);
                return false; 
            }
        }
        return true;
    }
    private void handleUnsubscribe(Map<String, String> headers) {
        if (!validateHeaders(headers, "id")) return; 
        String subId = headers.get("id");
        checkForReceipt(headers);
    }

    private void handleSend(Map<String, String> headers, String body) {
        if (!validateHeaders(headers, "destination")) return;
        String topic = headers.get("destination");
        if (!((ConnectionsImpl<String>) connections).isSubscribed(connectionId, topic)) {
            sendError("Error", "User is not subscribed to topic " + topic);
            return;
        }
        connections.send(topic, body); 
        checkForReceipt(headers);
    }

    private void handleDisconnect(Map<String, String> headers) {
        checkForReceipt(headers);
        shouldTerminate = true;
        if (loggedInUser != null) {
            activeUsers.remove(loggedInUser); 
        }
        connections.disconnect(connectionId);
    }
}

