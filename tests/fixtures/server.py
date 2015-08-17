import pytest
import json
import jinja2
import tempfile
import os
import tornado.process


class Server(object):
    '''TheVoid test server.

    Attributes:
        port:
            Server's port. Defaults to some random port.
        backlog:
            Socket's backlog. Defaults to 128.
        threads:
            Number of worker threads. Defaults to 2.
        buffer_size:
            Size of read buffer. Defaults to 65536.
        log_level:
            Server's log level. Defaults to 'info'.
        monitor_port:
            Server's monitor port. Defaults to some random port.
        handlers:
            List of handlers to register.
            Each handler should be a dict with the following keys:
                handler: name of the handler (e.g., 'ok', 'echo')
                exact_match: string to match URL.
                prefix_match: string to match URL prefix.
                regex_match: regex to match URL.
                methods: list of supported methods.
                headers: dict of necessary headers and their values.
    '''

    CONFIG_TEMPLATE = '''\
{
    "endpoints": [
        "0.0.0.0:{{ port }}"
    ],
    "backlog": {{ backlog }},
    "threads": {{ threads }},
    "buffer_size": {{ buffer_size }},
    "logger": {
        "level": "{{ log_level }}",
        "frontends": [
            {
                "formatter": {
                    "type": "string",
                    "pattern": "%(timestamp)s %(request_id)s/%(lwp)s/%(pid)s %(severity)s: %(message)s, %(...L)s"
                },
                "sink": {
                    "type": "files",
                    "path": "/dev/stdout",
                    "autoflush": true,
                    "rotation": { "move": 0 }
                }
            }
        ]
    },
    "monitor-port": {{ monitor_port }},
    "log_request_headers": {{ log_request_headers | tojson | safe }},
    "application": {
        "handlers": {{ handlers | tojson | safe }}
    }
}'''

    def __init__(self, **kwargs):
        self.opts = {}
        self.opts['port'] = kwargs.get('port', 0)
        self.opts['backlog'] = kwargs.get('backlog', 128)
        self.opts['threads'] = kwargs.get('threads', 2)
        self.opts['buffer_size'] = kwargs.get('buffer_size', 65536)
        self.opts['log_level'] = kwargs.get('log_level', 'info')
        self.opts['monitor_port'] = kwargs.get('monitor_port', 0)
        self.opts['log_request_headers'] = kwargs.get('log_request_headers', [])
        self.opts['handlers'] = kwargs.get('handlers', [])
        self.config_file = None

    def configure(self):
        '''Configures server with initialized options.

        Temporary file is created to store server's config.
        '''
        if self.config_file:
            os.remove(self.config_file.name)
            self.config_file = None

        self.config_file = tempfile.NamedTemporaryFile(delete=False)
        env = jinja2.Environment()
        env.filters['tojson'] = json.dumps
        template = env.from_string(Server.CONFIG_TEMPLATE)
        self.config_file.write(template.render(**self.opts))
        self.config_file.close()

    def _read_stdout_until(self, delimiter, io_loop, timeout):
        return io_loop.run_sync(
            func=lambda: self.process.stdout.read_until(delimiter),
            timeout=timeout,
        )

    def _read_stdout_until_regex(self, regex, io_loop, timeout):
        return io_loop.run_sync(
            func=lambda: self.process.stdout.read_until_regex(regex),
            timeout=timeout,
        )

    def _wait_for_bind_port(self, io_loop, timeout):
        start_listen_log_line = 'Started to listen address: 0.0.0.0:'

        self._read_stdout_until(
            delimiter=start_listen_log_line,
            io_loop=io_loop,
            timeout=timeout,
        )

        port_regex = '^\d+\D'
        port = self._read_stdout_until_regex(
            regex=port_regex,
            io_loop=io_loop,
            timeout=timeout,
        )

        # remove last character as it's not a digit: '^\d+\D'
        return int(port[:-1])

    def start(self, io_loop, timeout):
        '''Starts server's process.

        To determine when the server is actually started, the following log line is looked for:
            'Started to listen adress: 0.0.0.0:{port},'
        '''
        self.process = tornado.process.Subprocess(
            ['./test_server', '-c', self.config_file.name],
            stdout=tornado.process.Subprocess.STREAM,
        )

        # wait for endpoint bind log
        endpoint_port = self._wait_for_bind_port(
            io_loop=io_loop,
            timeout=timeout,
        )
        self.opts['port'] = endpoint_port

        self.base_url = 'http://localhost:{port}/'.format(
            port=self.opts['port'],
        )

        # wait for monitor bind log
        monitor_port = self._wait_for_bind_port(
            io_loop=io_loop,
            timeout=timeout,
        )
        self.opts['monitor_port'] = monitor_port

        if self.process.returncode:
            pytest.fail('server failed to start, rc: {}'.format(self.process.returncode))

    def terminate(self):
        '''Stops server's process and deletes config file.
        '''
        if hasattr(self, 'process'):
            self.process.proc.terminate()
            self.process.proc.wait()

        if self.config_file:
            os.remove(self.config_file.name)
            self.config_file = None

    def request_url(self, url):
        '''Returns full request's URL from relative request's url.
        '''
        from urlparse import urljoin
        return urljoin(self.base_url, url)


@pytest.fixture
def server(request, io_loop):
    '''Create an instance of the `Server`.

    Server's options may be passed to the constructor with `pytest.mark.server_options`.
    '''
    opts = request.node.get_marker('server_options')
    opts = opts.kwargs if opts else {}
    s = Server(**opts)

    def terminate_server():
        s.terminate()

    request.addfinalizer(terminate_server)

    s.configure()

    def _timeout(item):
        default_timeout = item.config.getoption('async_test_timeout')
        async_test = item.get_marker('async_test')
        if async_test:
            return async_test.kwargs.get('timeout', default_timeout)
        return default_timeout

    s.start(
        io_loop=io_loop,
        timeout=_timeout(request.node),
    )

    return s
