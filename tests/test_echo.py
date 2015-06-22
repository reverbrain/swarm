import pytest
import requests

from tornado.httputil import (
    HTTPHeaders,
    RequestStartLine,
    HTTPMessageDelegate
)
from tornado.http1connection import HTTP1Connection


@pytest.mark.server_options(
    buffer_size=10240,
    handlers=[
        {'handler': 'echo',
         'exact_match': '/echo'}
    ])
@pytest.mark.parametrize(
    'data',
    [b'\x01', b'\x01' * 1024 * 100],
    ids=['1B', '100KB']
)
def test_echo_response_content(server, data):
    '''Sends data to the echo handler and validates response.

    The echo handler responds with 200 and received body as is.

    Args:
        server: an instance of `Server`.
        data: body of the request.
    '''
    response = requests.post(server.request_url('/echo'), data=data)

    assert response.status_code == requests.codes.ok
    assert response.content == data


@pytest.mark.server_options(
    handlers=[
        {'handler': 'echo',
         'exact_match': '/echo'}
    ])
@pytest.mark.parametrize(
    'chunks',
    [[b'\x01'], [b'\x01' * 1024] * 10],
    ids=['1 chunk of 1B', '10 chunks of 1KB']
)
@pytest.mark.async_test
def test_handler_echo_chunk_exchange(server, io_stream, chunks):
    '''Sends request by chunks and validates received chunks to exactly match the sent ones.

    Test scenario:
    1. Request's headers are exchanged.
       The client sends request's headers and waits for response's headers.
       The echo handler must respond with 200 and the same 'Content-Length'.
    2. Request's body exchanged by chunks.
       The first chunk of data is sent after response's headers are received. After that
       the client waits to receive the same chunk from the handler. Once the first chunk
       is exchanged, the process follows with the second chunk and so on.

    Args:
        server: an instance of `Server`.
        io_stream: an instance of `tornado.iostream.IOStream`.
        chunks: request's body chunks.
    '''

    class ChunkedEchoResponseHandler(HTTPMessageDelegate):
        '''Handler for chunked echo response.

        It's aimed to send request's headers and receive response's headers. After that
        HTTP connection will be detached and underlying stream can be used to send data
        to and to receive data from the client.

        Attributes:
            stream:
                An instance of `tornado.iostream.IOStream`. The stream must be connected
                to a remote address at this point.
        '''
        def __init__(self, stream):
            self.stream = stream
            self.connection = HTTP1Connection(stream, is_client=True)

            self.response_start_line = None
            self.response_headers = None

        def write_headers(self, start_line, headers):
            '''Sends request's start line and headers.

            Returns a `tornado.concurrent.Future` that resolves to None after
            the response's headers have been read and parsed.
            '''
            headers_future = self.connection.write_headers(start_line, headers)
            headers_future.result()
            return self.connection.read_response(self)

        def headers_received(self, start_line, headers):
            '''Called when the HTTP headers have been received and parsed.
            '''
            self.response_start_line = start_line
            self.response_headers = headers
            self.connection.detach()

        def on_connection_close():
            '''Called if the connection is closed without finishing the request.
            '''
            pytest.fail('connection is closed without finishing the request')

    yield io_stream.connect(('localhost', server.opts['port']))

    request_start_line = RequestStartLine(method='POST', path='/echo', version='HTTP/1.1')

    content_length = sum(map(len, chunks))
    # header's value must be a string
    request_headers = HTTPHeaders({'Content-Length': str(content_length)})

    handler = ChunkedEchoResponseHandler(io_stream)

    # send request and wait for response's headers
    yield handler.write_headers(request_start_line, request_headers)

    assert handler.response_start_line.code == requests.codes.ok
    assert int(handler.response_headers['content-length']) == content_length

    for chunk in chunks:
        yield io_stream.write(chunk)
        response_chunk = yield io_stream.read_bytes(len(chunk))
        assert response_chunk == chunk
