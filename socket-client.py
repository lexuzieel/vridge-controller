import time
import zmq

print("Current libzmq version is %s" % zmq.zmq_version())
print("Current  pyzmq version is %s" % zmq.__version__)

context = zmq.Context()
socket = context.socket(zmq.REQ)
# socket.set_string(zmq.IDENTITY, 'mlem')
socket.connect("tcp://192.168.88.20:5555")

while True:
    #  Send reply back to client
    socket.send(b"Hello")

    print("Sent")

    time.sleep(10)
