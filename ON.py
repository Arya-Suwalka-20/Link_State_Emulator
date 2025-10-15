import sys
import socket
import string
import struct

max_nodes=26
# function to read config file
def read_config_file():
    if len(sys.argv) != 2:
        print("Usage: python ON.py <config_file>")
        sys.exit(1)

    config_file = sys.argv[1]
    with open(config_file, 'r') as f:
        config_data = f.read()
    print("Configuration file read successfully!")
    return process_config_file(config_data)


def process_config_file(s):
    
    nodes2listen = 1
    li=[]
    for i in range (max_nodes):
        temp=[]
        for j in range (max_nodes):
            if i==j:
                temp.append(0)
                continue
            temp.append(-1)
        li.append(temp)
    ls=s.split('\n')
    # print(ls)
    i=0
    
    for l in ls:
        if len(l)==0:
            continue
        if l[0]=='#':
            continue
        j=1
        # print(l)
        g=l.split(" ")
        for k in g:
            if k=="":
                continue
            li[i][j+i]=int(k)
            li[j+i][i]=int(k)
            j+=1
            if i==0 :
                nodes2listen += 1

        i=i+1

    return li,nodes2listen
          


config_data,nodes2listen = read_config_file()

print(nodes2listen)

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

ORACLE_IP = "0.0.0.0"
PORT = 5000

server_socket.bind((ORACLE_IP, PORT))
server_socket.listen(max_nodes)

print(f" Oracle Node listening at {ORACLE_IP}:{PORT}")

# conn, addr = server_socket.accept()
# print(f" Connection established with VN at {addr}")

# data = conn.recv(1024).decode() 
# if data:
#     print(f" Message received from VN: {data}")
# else:
#     print(" No data received from VN.")

# conn.close()
# server_socket.close()
# print(" Connection closed.")

VN_info = []   # List to store (name, ip, port)
alphabet_names = list(string.ascii_uppercase)  # ['A', 'B', 'C', ...]

for i in range(nodes2listen):
    print(f"Waiting for VN {alphabet_names[i]} to connect...")
    conn, addr = server_socket.accept()
    print(f"✔ Connection established with VN {alphabet_names[i]} at {addr}")

    # Receive VN info
    data = conn.recv(1024).decode()
    if data:
        print(f"↳ Message from VN {alphabet_names[i]}: {data}")
        # expected format: "<VN_IP> <VN_PORT> hi"
        parts = data.strip().split()
        if len(parts) >= 2:
            vn_ip = parts[0]
            vn_port = int(parts[1])
            VN_info.append((alphabet_names[i], vn_ip, vn_port))
        else:
            print("⚠️ Invalid message format.")
    else:
        print("⚠️ No data received.")

    conn.close()
    print(f"Connection with VN {alphabet_names[i]} closed.\n")

# ---------- Step 4: Print all connected VN info ----------
print("All VN connections received successfully!\n")
print("Connected VN details:")
for vn in VN_info:
    name, ip, port = vn
    print(f"  VN {name} → IP: {ip}, UDP Port: {port}")


# ---------- Step 5: Send LINK-STATE messages ----------
print("\nSending LINK-STATE messages to all connected VNs...\n")

# message type (12 bytes, padded)
msg_type = b'LINK-STATE' + b'\x00' * (12 - len('LINK-STATE'))

for i, (vn_name, vn_ip, vn_port) in enumerate(VN_info):
    # Build tuple list for this VN
    tuples = []
    for j in range(max_nodes):
        cost = config_data[i][j]
        if cost > 0:  # valid neighbor
            neighbor_name = alphabet_names[j]
            neighbor_ip, neighbor_port = VN_info[j][1], VN_info[j][2]
            tuples.append((neighbor_name, neighbor_ip, neighbor_port, cost))

    num_tuples = len(tuples)
    body = b''

    for (name, ip, port, cost) in tuples:
        ip_bytes = ip.encode('utf-8') + b'\x00' * (16 - len(ip))
        body += struct.pack('!c16sHI', name.encode('utf-8'), ip_bytes, port, cost)

    message = msg_type + struct.pack('!B', num_tuples) + body

    # Connect to VN and send message
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((vn_ip, vn_port))
            s.sendall(message)
            print(f"LINK-STATE message sent to VN {vn_name} ({vn_ip}:{vn_port}) with {num_tuples} tuples.")
    except Exception as e:
        print(f"⚠️ Failed to send LINK-STATE to VN {vn_name}: {e}")

print("\nAll LINK-STATE messages sent successfully.")




server_socket.close()
print("\nOracle Node shutting down.")



