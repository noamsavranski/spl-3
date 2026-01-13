import socket
import sys

def test_login():
    host = '127.0.0.1'
    port = 7777
    
    try:
        # 1. יצירת חיבור לשרת
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        print(f"[CLIENT] Connected to server at port {port}")

        # 2. בניית הודעת CONNECT לפי התקן
        # שימי לב ל- \x00 בסוף - זה ה-Null Byte הקריטי!
        connect_frame = (
            "CONNECT\n"
            "accept-version:1.2\n"
            "host:stomp.cs.bgu.ac.il\n"
            "login:guest\n"
            "passcode:guest\n"
            "\n" # שורה ריקה שמפרידה בין ה-Headers ל-Body
            "\x00" # תו הסיום
        )

        print(f"[CLIENT] Sending CONNECT frame...")
        sock.sendall(connect_frame.encode())

        # 3. האזנה לתשובה
        response = sock.recv(1024)
        print("\n[SERVER RESPONSE]:")
        print("-------------------")
        print(response.decode())
        print("-------------------")

        sock.close()

    except ConnectionRefusedError:
        print("Error: Could not connect. Is the server running?")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_login()