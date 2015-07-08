import pytest
import requests
import tornado.httputil

from tornado.httputil import (
    HTTPHeaders,
    RequestStartLine
)
from tornado.http1connection import HTTP1Connection


@pytest.mark.server_options(
    handlers=[
        {'handler': 'echo',
         'prefix_match': '/echo'}
    ])
@pytest.mark.parametrize(
    'send_data',
    [b'\x01' * 100 * 1024],
    ids=['100KB']
)
@pytest.mark.async_test(timeout=1)
def test_connection_close_before_request_finished(server, io_stream, http_connection, send_data):
    '''Sends incomplete data to the echo handler and closes the connection.

    The request's 'Content-Length' is greater than the data to be sent.
    The server should write access_log_entry with 499 status.

    Args:
        server: an instance of `Server`.
        io_stream: an instance of `tornado.iostream.IOStream`.
        http_connection:
            An instance of `tornado.http1connection.HTTP1Connection` that uses `io_stream`
            as underlying stream.
        send_data: data to be sent before connection close.
    '''

    yield io_stream.connect(('localhost', server.opts['port']))

    start_line = RequestStartLine(method='POST', path='/echo', version='HTTP/1.1')
    headers = HTTPHeaders({'Content-Length': str(len(send_data) + 1)})
    yield http_connection.write_headers(start_line, headers)
    yield http_connection.write(send_data)
    io_stream.close()

    # wait for 'access_log_entry' and check its status
    for log_line in iter(server.process.stdout.readline, b''):
        if 'access_log_entry' in log_line:
            assert 'status: 499' in log_line
            break
    else:
        pytest.fail('no access_log_entry in server log')
