package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;

public class ConnectionsImpl<T> implements Connections<T> {
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> handlers = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<String, ConcurrentLinkedQueue<Integer>> channels = new ConcurrentHashMap<>();
    private ConcurrentHashMap<Integer, ConcurrentHashMap<Integer, String>> clientSubscriptions = new ConcurrentHashMap<>();
    private final AtomicInteger messageIdCounter = new AtomicInteger(0);

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = handlers.get(connectionId);
        if (handler != null) {                                     
            handler.send(msg);                                  
            return true;
        }
        return false;
    }
    @Override
    public void send(String channel, T msg){
        ConcurrentLinkedQueue<Integer> subscribers = channels.get(channel);
        if(subscribers == null){
            return;
        }
        for (Integer connectionId : subscribers) {
            Integer subscriptionId = getSubscriptionId(connectionId, channel);
            if (subscriptionId == null) {
                return;
            }
            int msgId = messageIdCounter.incrementAndGet();
            String frame = "MESSAGE\n" + "subscription:" + subscriptionId + "\n" + "message-id:" + msgId + "\n" +
            "destination:" + channel + "\n" + "\n" + msg + "\n";
            send(connectionId, (T) frame);
        }
    }
    private Integer getSubscriptionId(int connectionId, String channel) {
        ConcurrentHashMap<Integer, String> userSubs = clientSubscriptions.get(connectionId);
        if (userSubs == null) return null;
        for (java.util.Map.Entry<Integer, String> entry : userSubs.entrySet()) {
            if (entry.getValue().equals(channel)) {
                return entry.getKey();
            }
        }
        return null;
    }

    @Override
    public void disconnect(int connectionId) {
        handlers.remove(connectionId);
        for (ConcurrentLinkedQueue<Integer> subscribers : channels.values()) {
            subscribers.remove(connectionId);
        }
        clientSubscriptions.remove(connectionId);
    }
    public void addConnection(int connectionId, ConnectionHandler<T> handler) {
        handlers.put(connectionId, handler);
    }

    @Override
    public void subscribe(String channel, int connectionId, int subscriptionId) {
        clientSubscriptions.putIfAbsent(connectionId, new ConcurrentHashMap<>());
        ConcurrentHashMap<Integer, String> userSubs = clientSubscriptions.get(connectionId);
        if (userSubs.containsKey(subscriptionId)) {
            String oldChannel = userSubs.get(subscriptionId);
            if (channels.containsKey(oldChannel)) {
                channels.get(oldChannel).remove(Integer.valueOf(connectionId));
            }
        }
        userSubs.put(subscriptionId, channel);
        channels.computeIfAbsent(channel, k -> new ConcurrentLinkedQueue<>()).add(connectionId);
    }

    @Override
    public void unsubscribe(String subscriptionId, int connectionId) {
        int subId = Integer.parseInt(subscriptionId);
        if (clientSubscriptions.containsKey(connectionId)) {
            ConcurrentHashMap<Integer, String> userSubs = clientSubscriptions.get(connectionId);
            if (userSubs.containsKey(subId)) {
                String channel = userSubs.remove(subId);
                if (channels.containsKey(channel)) {
                    channels.get(channel).remove(Integer.valueOf(connectionId));
                }
            }
        }
    }
    public boolean isSubscribed(int connectionId, String channel) {
        ConcurrentHashMap<Integer, String> userSubs = clientSubscriptions.get(connectionId);
        return userSubs != null && userSubs.containsValue(channel);
    }
}
