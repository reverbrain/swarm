import re

import pytest
import requests
import tornado.gen


@pytest.mark.parametrize(
    'headers',
    [
        {
            'Accept-Charset': 'ISO-8859-1,utf-8;q=0.7,*;q=0.7',
            'Accept-Encoding': '',
            'Accept': 'application/json, text/json, text/x-json, text/javascript',
            'Cookie': 'PHPSESSID=r2t5uvjq435r4q7ib3vtdjq120',
            'Custom-Header': '"{\"custom\"\t\"header\"\t\"value\"}"',
            'X-Pingback': 'http://www.example.org/xmlrpc.php',
            'X-Cookie': '\tCustom \"Cookie\"\t',
        },
    ],
)
class TestLogRequestHeaders(object):
    '''Sends empty GET request with custom headers and validates request's log headers.

    Request's headers are logged within 'received new request' log line.
    '''

    def parse_headers(self, log_line):
        # Header's key consists of non-whitespace characters except for double quote
        #
        # (?!") - asserts that what immediately follows the current position in the
        # string is not a double quote
        header_key_pattern = r'((?!")\S)+'

        # Header's value consists of any character except for unescaped double quote
        #
        # (?<=\\)" - asserts that what immediately precedes the current position in
        # the string is escape character and double quote is on the current position
        #
        # (?=(?<!\\)(\\\\)*") - asserts that double quote is on the current position
        # in the string and even number of escapes immediately precede the current
        # position
        header_value_pattern = r'((?<=\\)"|[^"])*(?=(?<!\\)(\\\\)*")'

        # Header is enclosed in double quoted and is followed by either ', ' or '}'
        header_pattern = r'"(?P<key>{key}): (?P<value>{value})"(?=, |\}})'.format(
            key=header_key_pattern,
            value=header_value_pattern,
        )

        # Headers is a list of headers enclosed in '{', '}' and separated by ', '
        #
        # (, |(?=\}})) - asserts that ', ' is on the current position in the string
        # or '}' immediately follows the current position
        headers_pattern = r'headers: (?P<headers>\{{({header}(, |(?=\}})))*\}})'.format(
            header=header_pattern,
        )

        headers_log_line = re.search(headers_pattern, log_line).group('headers')
        log_headers = {
            header.group('key'): header.group('value')
            for header in re.finditer(header_pattern, headers_log_line)
        }

        return log_headers

    @tornado.gen.coroutine
    def request_log_line(self, server, method, url, headers):
        requests.request(
            method=method,
            url=server.request_url(url),
            headers=headers,
        )

        yield server.process.stdout.read_until('received new request: ')
        log_line = yield server.process.stdout.read_until_regex('\n')

        raise tornado.gen.Return(log_line)

    @pytest.mark.server_options(
        log_request_headers=[
            'Accept-Charset',
            'Accept-Encoding',
            'Accept',
            'Cookie',
            'Custom-Header',
        ],
        handlers=[
            {
                'handler': 'echo',
                'methods': ['GET'],
                'exact_match': '/echo',
            },
        ],
    )
    @pytest.mark.async_test(timeout=0.5)
    def test_log_request_headers(self, server, headers):
        '''Test that only configured headers are printed.

        Headers that start with 'X-' (like 'X-Pingback', 'X-Cookie') are not configured
        to be logged, thus, they are checked to be not presented in the log.

        Args:
            server: an instance of `Server`.
            headers: custom headers to send request with.
        '''
        log_line = yield self.request_log_line(
            server=server,
            method='GET',
            url='/echo',
            headers=headers,
        )

        log_headers = self.parse_headers(log_line)

        for header_key, header_value in headers.iteritems():
            if header_key.startswith('X-'):
                assert header_key not in log_headers
            else:
                assert header_key in log_headers

                log_header_value = log_headers[header_key]
                assert log_header_value.decode('string_escape') == header_value

    @pytest.mark.server_options(
        log_request_headers=[],
        handlers=[
            {
                'handler': 'echo',
                'methods': ['GET'],
                'exact_match': '/echo',
            },
        ],
    )
    @pytest.mark.async_test(timeout=0.5)
    def test_log_request_headers_empty(self, server, headers):
        '''Test that no logs are printed if config's log_request_headers is empty.

        Args:
            server: an instance of `Server`.
            headers: custom headers to send request with.
        '''
        log_line = yield self.request_log_line(
            server=server,
            method='GET',
            url='/echo',
            headers=headers,
        )

        log_headers = self.parse_headers(log_line)
        assert not log_headers
