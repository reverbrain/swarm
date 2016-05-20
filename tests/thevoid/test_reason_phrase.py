import pytest
import requests
import httplib

import tornado.httpclient
import tornado.httputil


def default_status_codes():
    '''Returns list of HTTP status codes with default reason phrase.
    '''
    status_codes = httplib.responses.keys()

    # Remove 306 status code as it's not used
    status_codes.remove(306)

    # Remove 100 status code as python.requests doesn't support it
    status_codes.remove(100)

    return status_codes


@pytest.mark.server_options(
    handlers=[
        {
            'handler': 'echo',
            'exact_match': '/echo',
        }
    ]
)
@pytest.mark.parametrize(
    'status_codes',
    [
        default_status_codes()
    ],
)
def test_response_default_reason_phrase(server, status_codes):
    '''Sends request to the echo handler and validates response's default reason phrase.

    Response's status code is set by request url's query parameter 'code'.
    Response's reason phrase should match the default one from
    http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html.

    Args:
        server: an instance of `Server`.
        status_codes: list of status codes that have default reason phrase.
    '''
    for status_code in status_codes:
        response = requests.post(
            url=server.request_url('/echo'),
            params={
                'code': status_code,
            },
        )

        assert response.reason == httplib.responses[status_code], (
            (
                'invalid reason phrase for status code {code}'
                " received: '{received}', expected: '{expected}'"
            ).format(
                code=status_code,
                received=response.reason,
                expected=httplib.responses[status_code],
            )
        )


@pytest.mark.server_options(
    handlers=[
        {
            'handler': 'echo',
            'exact_match': '/echo',
        }
    ]
)
@pytest.mark.parametrize(
    'status_code',
    [
        399,
        499,
        599,
    ],
)
def test_response_empty_reason_phrase(server, status_code):
    '''Sends request to the echo handler and validates response's empty reason phrase.

    Empty reason phrase ('-') is sent for status codes that have no default reason
    phrase.
    Response's status code is set by request url's query parameter 'code'.

    Args:
        server: an instance of `Server`.
        status_code: expected response's status code.
    '''
    response = requests.post(
        url=server.request_url('/echo'),
        params={
            'code': status_code,
        },
    )

    assert response.reason == '-'


@pytest.mark.server_options(
    handlers=[
        {
            'handler': 'echo',
            'exact_match': '/echo',
        }
    ],
    log_level='debug',
)
@pytest.mark.parametrize(
    'status_code,reason_phrase',
    [
        (222, 'Reason\t with\t \'special\' symbols'),
        (200, 'All is OK'),
        (201, 'Internal Server Error'),
        (302, 'Found'),
        (499, ''),
        (599, b'\x01' * 100 * 1024),
    ],
    ids=[
        'special symbols',
        'custom',
        'different default',
        'default',
        'empty',
        '100KB',
    ],
)
@pytest.mark.async_test
def test_response_custom_reason_phrase(server, status_code, reason_phrase):
    '''Sends request to the echo handler and validates response's custom reason phrase.

    Response's status code is set by request url's query parameter 'code'.
    Response's reason phrase is set by request url's query parameter 'reason'.

    Args:
        server: an instance of `Server`.
        status_code: expected response's status code.
        reason_phrase: expected response's reason phrase.
    '''
    # Invoke asyncronous stdout read until the end of the request.
    # Otherwise, internal kernel's pipe buffer will overflow and
    # subsequent server's log writes will block
    server.process.stdout.read_until_close()

    # requests.PreparedRequest is used just to correctly construct url
    request = requests.PreparedRequest()
    request.prepare(
        url=server.request_url('/echo'),
        params={
            'code': status_code,
            'reason': reason_phrase,
        },
    )
    request_url = request.url

    first_line = []
    headers = {}

    # custom header_callback is used because default one from SimpleAsyncHTTPClient:
    # - is too smart and may perform some unnecessary actions
    # - doesn't support empty reason phrase, but it's allowed
    def header_callback(header_line):
        if header_line.startswith('HTTP/'):
            first_line.append(header_line)
        elif header_line != '\r\n':
            k, v = header_line.split(':', 1)
            headers[k.lower()] = v.strip()

    # max_header_size must be big enough to hold responses with large reason phrase
    tornado.httpclient.AsyncHTTPClient.configure(
        'tornado.simple_httpclient.SimpleAsyncHTTPClient',
        max_header_size=1024*1024,
    )

    http_client = tornado.httpclient.AsyncHTTPClient()
    yield http_client.fetch(
        request_url,
        raise_error=False,
        method='POST',
        body='',
        follow_redirects=False,  # it's needed to not handle 3xx codes
        header_callback=header_callback,
    )

    start_line = tornado.httputil.parse_response_start_line(first_line[0])
    assert start_line.reason == reason_phrase
