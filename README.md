# VRidge API

https://github.com/RiftCat/vridge-api/wiki/Control-channel

# ZMTP

https://rfc.zeromq.org/spec:23/ZMTP/
- No security

# Protobuf

Python 2.7 is mandatory apparently?

```
$ pip install protobuf
$ protoc -oproto/vridge-api.pb proto/vridge-api.proto
$ mkdir src/proto
$ /c/Python27/python nanopb/generator/nanopb_generator.py proto/vridge-api.pb --output-dir=src
```
