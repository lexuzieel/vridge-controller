import time
import zmq

print("Current libzmq version is %s" % zmq.zmq_version())
print("Current  pyzmq version is %s" % zmq.__version__)

context = zmq.Context()
sockets = []

for n in range(0, 2):
    socket = context.socket(zmq.REP)
    # socket.set_string(zmq.IDENTITY, 'mlem')
    bound = "tcp://*:%d" % (5555 + n)
    socket.bind(bound)

    print("Listening on " + bound)

    sockets.append(socket)

next = time.time()

while True:
    print("Waiting...")

    for i in range(len(sockets)):

        socket = sockets[i]
        #  Wait for next request from client
        # message = socket.recv_string()
        try:
            print("Trying %d..." % i)
            message = socket.recv(zmq.NOBLOCK)
            print("Received request: %s" % message)

        except zmq.error.Again:
            pass

        try:
            #  Send reply back to client
            s = b""
            for c in range(0, 256):
                s += b"a" if i == 0 else b"b"
            socket.send(b'\x001')
            print("Sent message to %d" % i)
        except zmq.error.ZMQError:
            pass

    #  Do some 'work'
    time.sleep(1)
