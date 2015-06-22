import pytest
import socket
from tornado.iostream import IOStream

from io_loop import io_loop


@pytest.yield_fixture
def io_stream(request, io_loop):
    '''Create an instance of the `tornado.iostream.IOStream`.

    Current `tornado.ioloop.IOLoop` is used for the stream, that is
    provided by `io_loop` fixture.

    No-delay flag is set for this stream. The no-delay flag requests that data
    should be written as soon as possible, even if doing so would consume
    additional bandwidth.
    '''
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
    stream = IOStream(s)
    stream.set_nodelay(True)
    yield stream
    stream.close()
