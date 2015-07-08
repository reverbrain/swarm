import pytest
import requests

from tornado.httputil import (
    HTTPHeaders,
    RequestStartLine,
    HTTPMessageDelegate
)
from tornado.http1connection import HTTP1Connection
from tornado.iostream import StreamClosedError


class PingResponseHandler(HTTPMessageDelegate):
    '''Handler for ping response.

    Assumed that ping is served by the 'ok' handler. The 'ok' handler just responds
    with 200 OK with no body.
    '''
    def __init__(self):
        super(PingResponseHandler, self).__init__()

        # response's start line and headers
        self.start_line = None
        self.headers = None

    def headers_received(self, start_line, headers):
        '''Called when the HTTP headers have been received and parsed.
        '''
        self.start_line = start_line
        self.headers = headers

    def on_connection_close():
        '''Called if the connection is closed without finishing the request.
        '''
        pytest.fail('connection is closed without finishing the request')


@pytest.mark.server_options(
    handlers=[{'handler': 'ok', 'exact_match': '/ping'}]
)
@pytest.mark.parametrize(
    'num_requests', [2, 3]
)
@pytest.mark.async_test
def test_connection_reuse_for_HTTP_1_1(server, io_stream, num_requests):
    '''Sends multiple '/ping' requests to the server using the same connection.

    The ok handler that just responds with 200 is used for serving requests.
    Keep-Alive is assumed by default for each request as it's HTTP/1.1.

    Args:
        server: an instance of `Server`.
        io_stream:
            an instance of `tornado.iostream.IOStream` that is used for
            consecutive requests.
    '''

    yield io_stream.connect(('localhost', server.opts['port']))

    # common request start line and headers for all ping requests
    start_line = RequestStartLine(method='GET', path='/ping', version='HTTP/1.1')
    headers = HTTPHeaders()

    for request in range(num_requests):
        connection = HTTP1Connection(io_stream, is_client=True)
        yield connection.write_headers(start_line, headers)
        connection.finish()

        response = PingResponseHandler()
        yield connection.read_response(response)

        assert response.start_line.code == requests.codes.ok


@pytest.mark.server_options(
    handlers=[{'handler': 'ok', 'exact_match': '/ping'}]
)
@pytest.mark.async_test(timeout=1)
def test_no_connection_reuse_with_connection_close(server, io_stream):
    '''Sends '/ping' request to the server with 'Connection: Close' and validate
    that the connection is closed by the server.

    To check that the server closes the connection, read all the data from the
    socket. If the server fails to close the socket, the test will hang for the
    specified async_test's timeout.

    Args:
        server: an instance of `Server`.
        io_stream: an instance of `tornado.iostream.IOStream`.
    '''

    yield io_stream.connect(('localhost', server.opts['port']))

    start_line = RequestStartLine(method='GET', path='/ping', version='HTTP/1.1')
    headers = HTTPHeaders({'Connection': 'Close'})

    connection = HTTP1Connection(io_stream, is_client=True)
    yield connection.write_headers(start_line, headers)
    connection.finish()

    response = PingResponseHandler()
    yield connection.read_response(response)

    assert response.start_line.code == requests.codes.ok

    # read all remaining data from the connection and validate it (there should be no data)
    remaining_data = yield io_stream.read_until_close()
    assert len(remaining_data) == 0
