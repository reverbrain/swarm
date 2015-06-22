import pytest
from tornado.http1connection import HTTP1Connection

from io_stream import io_stream


@pytest.fixture
def http_connection(request, io_stream):
    '''Create an instance of the `tornado.http1connection.HTTP1Connection`.

    `io_stream` fixture will be used for the connection.
    '''
    return HTTP1Connection(io_stream, is_client=True)
