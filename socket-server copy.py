import socket

s = socket.socket()

s.bind(('0.0.0.0', 38219))
s.listen(0)

print("Server listening on port 38219")

while True:

    client, addr = s.accept()

    while True:
        content = client.recv(32)

        if len(content) == 0:
            break

        else:
            print(content)

    print("Closing connection")
    client.close()
