#include "stream.h"

void streamInit(ByteStream *stream, uint16_t bufferSize) {
    stream->bytesAvailable = 0;
    stream->bufferSize = bufferSize;
    stream->bytesAvailable = 0;
    stream->_pos = 0;
    stream->consumed = 0;
}

void streamPutByte(ByteStream *stream, byte b) {
    stream->buffer[stream->_pos++] = b;
    stream->bytesAvailable++;
}

void streamPutBytes(ByteStream *stream, byte *b, uint16_t nbytes) {
    for (uint16_t i = 0; i < nbytes; i++) {
        streamPutByte(stream, b[i]);
    }
}

void streamNext(ByteStream *stream) {
    stream->consumed += stream->_pos;
    stream->_pos = 0;
    stream->_next((void*)stream);
    stream->bytesAvailable = 0;
}