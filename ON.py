import sys
import socket

num_nodes=26
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
    li=[]
    for i in range (num_nodes):
        temp=[]
        for j in range (num_nodes):
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
        i=i+1
    return li
          


config_data=read_config_file()

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

ORACLE_IP = "0.0.0.0"
PORT = 5000

server_socket.bind((ORACLE_IP, PORT))
server_socket.listen(num_nodes)

print(f" Oracle Node listening at {ORACLE_IP}:{PORT}")

conn, addr = server_socket.accept()
print(f" Connection established with VN at {addr}")

data = conn.recv(1024).decode() 
if data:
    print(f" Message received from VN: {data}")
else:
    print(" No data received from VN.")

conn.close()
server_socket.close()
print(" Connection closed.")

