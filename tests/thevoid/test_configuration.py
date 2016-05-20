import pytest
import requests


@pytest.mark.server_options(
    backlog=256,
    threads=16,
    buffer_size=1024,
    log_level='debug',
)
def test_server_options(server):
    opts = server.opts

    assert opts['backlog'] == 256
    assert opts['threads'] == 16
    assert opts['buffer_size'] == 1024
    assert opts['log_level'] == 'debug'


@pytest.mark.server_options(
    handlers=[
        {'handler': 'ok',
         'exact_match': '/ping'}
    ])
@pytest.mark.parametrize(
    'url',
    ['/ping', pytest.mark.xfail('/pingg')]
)
def test_handler_exact_match_ping(server, url):
    response = requests.get(server.request_url(url))

    assert response.status_code == requests.codes.ok


@pytest.mark.server_options(
    handlers=[
        {'handler': 'ok',
         'prefix_match': '/ping'}
    ])
@pytest.mark.parametrize(
    'url', ['/pingg', pytest.mark.xfail('/pin')]
)
def test_handler_prefix_match_ping(server, url):
    response = requests.get(server.request_url(url))

    assert response.status_code == requests.codes.ok


@pytest.mark.server_options(
    handlers=[
        {'handler': 'ok',
         'regex_match': '/(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])'}
    ])
@pytest.mark.parametrize(
    'url', ['/127.0.0.1', pytest.mark.xfail('fe80::0202:b3ff:fe1e:8329')]
)
def test_handler_regex_match_ipv4_address(server, url):
    response = requests.get(server.request_url(url))

    assert response.status_code == requests.codes.ok


@pytest.mark.server_options(
    handlers=[
        {'handler': 'ok',
         'methods': ['GET', 'POST']}
    ])
@pytest.mark.parametrize(
    'method', ['GET', 'POST', pytest.mark.xfail('HEAD')]
)
def test_handler_methods_GET_POST(server, method):
    response = requests.request(method, server.request_url('/ping'))

    assert response.status_code == requests.codes.ok


@pytest.mark.server_options(
    handlers=[
        {'handler': 'ok',
         'headers': {'User-Agent': 'pytest'}}
    ])
@pytest.mark.parametrize(
    'user_agent', ['pytest', pytest.mark.xfail('Firefox/21.0')]
)
def test_handler_headers_user_agent_pytest(server, user_agent):
    response = requests.get(server.request_url('/ping'),
                            headers={'User-Agent': user_agent})

    assert response.status_code == requests.codes.ok


def test_handler_not_found(server):
    response = requests.get(server.request_url('/no-such-handler'))
    assert response.status_code == requests.codes.not_found


@pytest.mark.server_options(
    log_level='error',
)
@pytest.mark.async_test(timeout=0.5)
@pytest.mark.xfail
def test_invalid_server_log_level(server):
    # on 'error' log level necessary log lines ('Start to listen address')
    # will not be printed and server's start will fail
    pass
