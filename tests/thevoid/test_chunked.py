import pytest
import random
import requests

@pytest.mark.server_options(
    buffer_size=10240,
    handlers=[
        {
            'handler': 'chunked',
            'exact_match': '/chunked'
        }
    ]
)
@pytest.mark.parametrize(
    'chunks',
    [[b'\x02'] * random.randint(1, 10), [b'\x03' * 1024] * random.randint(10, 20)],
    ids=['some chunks of 1B', 'some chunks of 1KB']
)
def test_chunked_transfer_encoding(server, chunks):
    '''Sends request using chunked transfer encoding and validates data to exactly match combined chunks.
    
    Args:
        server: an instance of `Server`.
        chunks: set of chunks to be sent.

    '''

    def gen(c):
        for l in c:
            for i in l:
                yield i

    resp = requests.post(server.request_url('/chunked'), data=gen(chunks))
    assert resp.status_code == requests.codes.ok
    assert resp.content == ''.join(gen(chunks))
