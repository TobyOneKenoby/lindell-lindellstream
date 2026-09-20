import socket, ssl, threading, time, sys
context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(sys.argv[1],sys.argv[2])
def handle(raw):
    try:
        with context.wrap_socket(raw,server_side=True) as conn:
            while True:
                data=conn.recv(65536)
                if not data: break
                for i in range(0,len(data),3):
                    conn.sendall(data[i:i+3])
                    time.sleep(.001)
    except (OSError,ssl.SSLError): pass
    finally: raw.close()
s=socket.socket();s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);s.bind(('127.0.0.1',18443));s.listen()
print('ready',flush=True)
while True:
    raw,_=s.accept();threading.Thread(target=handle,args=(raw,),daemon=True).start()
