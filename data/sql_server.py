#!/usr/bin/env python3
"""
Basic Python Server for STOMP Assignment – Stage 3.3

IMPORTANT:
DO NOT CHANGE the server name or the basic protocol.
Students should EXTEND this server by implementing
the methods below.
"""

import socket
import sys
import threading
import sqlite3

SERVER_NAME = "STOMP_PYTHON_SQL_SERVER"
DB_FILE = "stomp_server.db"

def recv_null_terminated(sock):
    data = b""
    while True:
        chunk = sock.recv(1024)
        if not chunk:
            return ""
        data += chunk
        if b"\0" in data:
            msg, _ = data.split(b"\0", 1)
            return msg.decode("utf-8", errors="replace")

def init_database():
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password TEXT NOT NULL)')
    cursor.execute('CREATE TABLE IF NOT EXISTS logins (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT, login_time DATETIME, logout_time DATETIME)')
    cursor.execute('CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT, filename TEXT, upload_time DATETIME)')
    conn.commit()
    conn.close()

def execute_sql_command(sql_command):
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute(sql_command)
        conn.commit()
        conn.close()
        return "done"
    except Exception as e:
        return f"ERROR:{str(e)}"

def execute_sql_query(sql_query):
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute(sql_query)
        rows = cursor.fetchall()
        conn.close()

        if not rows:
            return ""

        if len(rows[0]) == 1:
            return str(rows[0][0])

        result = ""
        for row in rows:
            row_str = ", ".join([str(item) for item in row])
            result += row_str + "\n"
        return result.strip()

    except Exception as e:
        return f"ERROR:{str(e)}"

def handle_client(client_socket, addr):
    try:
        while True:
            message = recv_null_terminated(client_socket)
            if message == "":
                break

            response = ""
            command = message.strip().upper()

            if command.startswith("SELECT"):
                response = execute_sql_query(message)
            else:
                response = execute_sql_command(message)

            response_with_newline = response + "\n"
            client_socket.sendall(response_with_newline.encode("utf-8"))
            break

    except Exception:
        pass
    finally:
        try:
            client_socket.close()
        except Exception:
            pass

def start_server(host="127.0.0.1", port=7778):
    init_database()
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server_socket.bind((host, port))
        server_socket.listen(5)
        
        while True:
            client_socket, addr = server_socket.accept()
            t = threading.Thread(target=handle_client, args=(client_socket, addr), daemon=True)
            t.start()

    except KeyboardInterrupt:
        pass
    finally:
        try:
            server_socket.close()
        except Exception:
            pass

if __name__ == "__main__":
    port = 7778
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            pass
    start_server(port=port)