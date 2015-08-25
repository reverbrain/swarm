import pytest
from tornado.http1connection import HTTP1Connection
from tornado.http1connection import HTTP1ConnectionParameters


@pytest.fixture
def http_connection(request, io_stream):
    '''Create an instance of the `tornado.http1connection.HTTP1Connection`.

    `io_stream` fixture will be used for the connection.
    '''
    opts = request.node.get_marker('http_connection_options')
    opts = opts.kwargs if opts else {}
    params = HTTP1ConnectionParameters(**opts)

    return HTTP1Connection(
        io_stream,
        is_client=True,
        params=params,
    )
