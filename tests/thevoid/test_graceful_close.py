import pytest
import urllib
import requests
import tornado.httputil

from tornado.httputil import (
    HTTPHeaders,
    RequestStartLine
)


class ResponseHandler(tornado.httputil.HTTPMessageDelegate):
    '''Handler that receives full response including body.

    After the full response is received the connection will be closed.

    Args:
        expect_finish: whether finish() or on_connection_close() should be called.
    '''
    def __init__(self, expect_finish):
        self.expect_finish = expect_finish
        self.start_line = None
        self.headers = None
        self.data = ''

    def headers_received(self, start_line, headers):
        if (
            self.start_line is not None or
            self.headers is not None
        ):
            pytest.fail("ResponseHandler: headers may be received only once")

        self.start_line = start_line
        self.headers = headers

    def data_received(self, chunk):
        self.data += chunk

    def finish(self):
        if not self.expect_finish:
            pytest.fail("ResponseHandler: finish() is not expected")

    def on_connection_close(self):
        if self.expect_finish:
            pytest.fail("ResponseHandler: connection_close() is not expected")


class DetachedResponseHandler(tornado.httputil.HTTPMessageDelegate):
    '''Handler that receives response's headers and detaches from connection.

    Args:
        http_connection: an HTTP connection to detach from on received headers.
    '''
    def __init__(self, http_connection):
        self.http_connection = http_connection
        self.start_line = None
        self.headers = None

    def headers_received(self, start_line, headers):
        if (
            self.start_line is not None or
            self.headers is not None
        ):
            pytest.fail("DetachedResponseHandler: headers may be received only once")

        self.start_line = start_line
        self.headers = headers

        self.http_connection.detach()


@pytest.mark.server_options(
    handlers=[
        {
            'handler': 'delayed_error',
            'prefix_match': '/delayed_error'
        },
    ],
    log_level='debug',
)
@pytest.mark.parametrize(
    'data,chunk',
    [
        (None, 1024),
        (b'\x01' * 100 * 1024, 1024),
    ],
    ids=[
        'empty',
        '100 chunks',
    ],
)
@pytest.mark.async_test(timeout=1)
def test_no_graceful_close_on_full_request(server, data, chunk):
    '''Sends full request and checks that server does no graceful close.

    If server receives full request there's no need to do graceful close
    and server closes the connection in usual way.

    Args:
        server: an instance of `Server`.
        data: request's body.
        chunk: handler's internal chunk size.
    '''

    # Start reading server's log until request's access log entry
    request_log_future = server.process.stdout.read_until('access_log_entry')

    code = 403
    response = requests.post(
        url=server.request_url('/delayed_error'),
        params={
            'code': code,
            'delay': -1,
            'chunk': chunk,
        },
        data=data,
    )

    assert response.status_code == code
    assert response.headers['Connection'] == 'Close'

    request_log = yield request_log_future
    if 'gracefully close the connection' in request_log:
        print request_log

    assert 'gracefully close the connection' not in request_log


@pytest.mark.server_options(
    handlers=[
        {'handler': 'delayed_error',
         'prefix_match': '/delayed_error'}
    ],
    log_level='debug',
)
@pytest.mark.parametrize(
    'data',
    [
        b'\x01' * 100 * 1024,
    ],
    ids=[
        '100Kb',
    ],
)
@pytest.mark.async_test(timeout=1)
def test_graceful_close_after_headers_server_close(server, io_stream, http_connection, data):
    '''Sends request's headers and checks that server does graceful close.

    If server receives request's and responds with error it should read request's
    body. If client doesn't close the connection and sends the whole request server
    should close the connection.

    Args:
        server: an instance of `Server`.
        io_stream: an instance of `tornado.iostream.IOStream`.
        http_connection:
            An instance of `tornado.http1connection.HTTP1Connection` that uses `io_stream`
            as underlying stream.
        data: request's body.
    '''

    yield io_stream.connect(('localhost', server.opts['port']))

    # Start reading server's log until request's access log entry
    request_log_future = server.process.stdout.read_until('access_log_entry')

    code = 403
    response_data = 'response'
    url = '/delayed_error?{}'.format(
        urllib.urlencode({
            'code': code,
            'delay': 0,
            'response': response_data,
        })
    )

    start_line = RequestStartLine(method='POST', path=url, version='HTTP/1.1')
    headers = HTTPHeaders({'Content-Length': str(len(data))})
    yield http_connection.write_headers(start_line, headers)

    # At this point server should respond with error
    handler = DetachedResponseHandler(http_connection)
    yield http_connection.read_response(handler)

    assert handler.start_line.code == code
    assert handler.headers['Content-Length'] == str(len(response_data))
    assert handler.headers['Connection'] == 'Close'

    # Continue sending request's body
    yield io_stream.write(data)

    # read the response's body
    response = yield io_stream.read_bytes(len(response_data))
    assert response == response_data

    # At this point server should close the connection
    with pytest.raises(tornado.iostream.StreamClosedError):
        yield io_stream.read_bytes(1)

    request_log = yield request_log_future
    request_log_lines = request_log.split('\n')

    graceful_close_log_start = [
        log_line for log_line in request_log_lines
        if 'gracefully close the connection' in log_line
    ]
    # graceful close log must apper only once
    assert len(graceful_close_log_start) == 1
    # state must include graceful_close
    assert 'graceful_close' in graceful_close_log_start[0]


@pytest.mark.server_options(
    handlers=[
        {'handler': 'delayed_error',
         'prefix_match': '/delayed_error'}
    ],
    log_level='debug',
)
@pytest.mark.parametrize(
    'data,server_delay',
    [
        (b'\x01' * 100 * 1024, 50 * 1024),
        (b'\x01' * 100 * 1024, 25 * 1024),
        (b'\x01' * 100 * 1024, 0),
    ],
    ids=[
        'half',
        'one-quarter',
        'zero',
    ],
)
@pytest.mark.async_test(timeout=1)
def test_graceful_close_client_close_half_request(server, io_stream, http_connection, data,
                                                  server_delay):
    '''Sends request's headers and half of body and checks that server does graceful close.

    If server receives request and responds with error it should read request's
    body. If client closes the connection and sends only part of the request server
    should not fail on closed connection.

    Args:
        server: an instance of `Server`.
        io_stream: an instance of `tornado.iostream.IOStream`.
        http_connection:
            An instance of `tornado.http1connection.HTTP1Connection` that uses `io_stream`
            as underlying stream.
        data: request's body.
        server_delay: part of request's body to receive before sending error response.
    '''

    yield io_stream.connect(('localhost', server.opts['port']))

    # Start reading server's log until request's access log entry
    request_log_future = server.process.stdout.read_until('access_log_entry')

    code = 403
    response_data = 'response'
    url = '/delayed_error?{}'.format(
        urllib.urlencode({
            'code': code,
            'delay': server_delay,
            'response': response_data,
        })
    )

    start_line = RequestStartLine(method='POST', path=url, version='HTTP/1.1')
    headers = HTTPHeaders({'Content-Length': str(len(data))})
    yield http_connection.write_headers(start_line, headers)
    yield http_connection.write(data[:len(data) / 2])

    # At this point server should respond with error
    # handler will receive the response and close the connection
    handler = ResponseHandler(http_connection)
    yield http_connection.read_response(handler)

    assert handler.start_line.code == code
    assert handler.headers['Content-Length'] == str(len(response_data))
    assert handler.headers['Connection'] == 'Close'
    assert handler.data == response_data

    # Ensure the connection in closed
    with pytest.raises(tornado.iostream.StreamClosedError):
        yield io_stream.write('test')

    request_log = yield request_log_future
    request_log_lines = request_log.split('\n')

    graceful_close_log_start = [
        log_line for log_line in request_log_lines
        if 'gracefully close the connection' in log_line
    ]
    # graceful close log must apper only once
    assert len(graceful_close_log_start) == 1
    # state must include graceful_close
    assert 'graceful_close' in graceful_close_log_start[0]

    received_eof_log_lines = [
        log_line for log_line in request_log_lines
        if 'received new data' in log_line and 'End of file' in log_line
    ]
    # there should be only one log line with receive error
    assert len(received_eof_log_lines) == 1
    # state must include graceful_close
    assert 'graceful_close' in received_eof_log_lines[0]
    # log line must be printed on DEBUG log level as it's not an error
    # during graceful close
    assert (
        'DEBUG' in received_eof_log_lines[0] and
        'ERROR' not in received_eof_log_lines[0]
    )
