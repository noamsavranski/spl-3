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
        String[] lines = actualMessage.split("\n");
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
                int bodyStartIndex = actualMessage.indexOf("\n\n");
                String bodyOnly = (bodyStartIndex != -1) ? actualMessage.substring(bodyStartIndex + 2) : "";
                handleSend(headers, bodyOnly); 
                break;
            case "DISCONNECT":
                handleDisconnect(headers);
                break;
            default:
                System.out.println("Unknown command: " + command);
        }
    }
    private void handleConnect(java.util.Map<String, String> headers) {
        System.out.println("DEBUG: handleConnect started for user: " + headers.get("login"));
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

        String sqlCheckUser = "SELECT password FROM users WHERE username='" + login + "'";
        System.out.println("DEBUG: Sending query to DB: " + sqlCheckUser);
        String dbResponse = sendToDB(sqlCheckUser);
        System.out.println("DEBUG: DB Response was: " + dbResponse);
        if (dbResponse == null || dbResponse.trim().isEmpty() || dbResponse.equals("None")) {
            String sqlRegister = "INSERT INTO users (username, password) VALUES ('" + login + "', '" + passcode + "')";
            sendToDB(sqlRegister);
        } 
        else {
            if (!dbResponse.trim().equals(passcode)) {
                sendError("Login failed", "Wrong password");
                return;
            }
        }
        String sqlLogLogin = "INSERT INTO logins (username, login_time) VALUES ('" + login + "', datetime('now'))";
        sendToDB(sqlLogLogin);
        this.loggedInUser = login;
        activeUsers.put(login, true);
        connections.send(connectionId, "CONNECTED\n" + "version:1.2\n" + "\n"); 
    }
    private void sendError(String message, String description) {
         System.out.println("[SERVER][ERROR] " + message + " | " + description + " | connId=" + connectionId + " user=" + loggedInUser);
        String errorFrame = "ERROR\n" + "message:" + message + "\n" +"\n" +"The details:\n" + description + "\n";      
        connections.send(connectionId, errorFrame);
        if (loggedInUser != null) {
            activeUsers.remove(loggedInUser);
            loggedInUser = null;
        }
        shouldTerminate = true; 
            
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
            String response = "RECEIPT\n" + "receipt-id:" + receiptId + "\n" + "\n";
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
        ((ConnectionsImpl<String>) connections).unsubscribe(subId, connectionId);

        checkForReceipt(headers);
    }

    private void handleSend(Map<String, String> headers, String body) {
        if (!validateHeaders(headers, "destination")) return;
        String topic = headers.get("destination");
        if (!((ConnectionsImpl<String>) connections).isSubscribed(connectionId, topic)) {
            sendError("Error", "User is not subscribed to topic " + topic);
            return;
        }
        System.out.println("DEBUG: Reporting to DB for topic: " + topic);
        String sqlFileLog = "INSERT INTO Files (username, filename, upload_time) " +
                        "VALUES ('" + loggedInUser + "', '" + topic + "', datetime('now'))";
        sendToDB(sqlFileLog);
        String messageFrame = "MESSAGE\n" +
                          "destination:" + topic + "\n" +
                          "message-id:" + java.util.UUID.randomUUID() + "\n" + 
                          "user:" + loggedInUser + "\n" + "\n" + body; 
        connections.send(topic, messageFrame); 
        checkForReceipt(headers);
    }

    private void handleDisconnect(Map<String, String> headers) {
        checkForReceipt(headers);
        if (loggedInUser != null) {
            String sqlLogout = "UPDATE logins SET logout_time = datetime('now') " +
                            "WHERE username = '" + loggedInUser + "' AND logout_time IS NULL";
            sendToDB(sqlLogout);
            activeUsers.remove(loggedInUser);
            this.loggedInUser = null;
        }
        shouldTerminate = true;
    }

    private String sendToDB(String sqlCommand) {
        int pythonServerPort = 7778;

        try (java.net.Socket socket = new java.net.Socket("127.0.0.1", pythonServerPort)) {
            socket.setSoTimeout(2000);
            java.io.OutputStream out = socket.getOutputStream();
            byte[] bytes = (sqlCommand + "\u0000").getBytes(java.nio.charset.StandardCharsets.UTF_8);
            out.write(bytes);
            out.flush();
            socket.shutdownOutput();

            java.io.InputStream in = socket.getInputStream();
            java.io.ByteArrayOutputStream buf = new java.io.ByteArrayOutputStream();
            int b;
            while ((b = in.read()) != -1) {
                if (b == '\n') break; 
                buf.write(b);
            }

            return buf.toString("UTF-8").trim();

        } 
        catch (java.net.SocketTimeoutException e) {
            System.out.println("DB ERROR: timeout waiting for Python response");
            return "";
        } 
        catch (Exception e) {
            System.out.println("DB ERROR: " + e.getMessage());
            return "";
        }
    }


    public void printServerStats() {
        System.out.println("--- Server Data Report (From DB) ---");
        System.out.println("\n[Registered users]:");
        System.out.println(sendToDB("SELECT username FROM users"));
        System.out.println("\n[Login History]:");
        System.out.println(sendToDB("SELECT username, login_time, logout_time FROM logins"));
        System.out.println("\n[Uploaded Files/Reports]:");
        System.out.println(sendToDB("SELECT username, filename, upload_time FROM files"));
        System.out.println("------------------------------------");
    }
}

