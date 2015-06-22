import pytest
import json
import random
import jinja2
import tempfile
import subprocess
import os
import socket


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
    "application": {
        "handlers": {{ handlers | tojson | safe }}
    }
}'''

    def __init__(self, **kwargs):
        def random_port():
            def available(port):
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(0.1)
                result = sock.connect_ex(('localhost', port))
                return result != 0

            while True:
                port = random.choice(range(1024, 65536))
                if available(port):
                    return port

        self.opts = {}
        self.opts['port'] = kwargs.get('port', random_port())
        self.opts['backlog'] = kwargs.get('backlog', 128)
        self.opts['threads'] = kwargs.get('threads', 2)
        self.opts['buffer_size'] = kwargs.get('buffer_size', 65536)
        self.opts['log_level'] = kwargs.get('log_level', 'info')
        self.opts['monitor_port'] = kwargs.get('monitor_port', random_port())
        self.opts['handlers'] = kwargs.get('handlers', [])
        self.config_file = None
        self.base_url = 'http://localhost:{port}/'.format(port=self.opts['port'])

    def __del__(self):
        self.terminate()

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

    def start(self):
        '''Starts server's process.

        To determine when the server is actually started, the following log line is looked for:
            'Started to listen adress: 0.0.0.0:{port},'
        '''
        self.process = subprocess.Popen(['./test_server', '-c', self.config_file.name],
                                        bufsize=1, stdout=subprocess.PIPE)

        start_line = 'Started to listen address: 0.0.0.0:{},'.format(self.opts['port'])
        while self.process.poll() is None:
            line = self.process.stdout.readline()
            if not line:
                continue
            if start_line in line:
                break

        if self.process.returncode:
            pytest.fail('server failed to start, rc: {}'.format(self.process.returncode))

    def terminate(self):
        '''Stops server's process and deletes config file.
        '''
        if self.process.poll() is None:
            self.process.terminate()
            self.process.wait()
        if self.config_file:
            os.remove(self.config_file.name)
            self.config_file = None

    def request_url(self, url):
        '''Returns full request's URL from relative request's url.
        '''
        from urlparse import urljoin
        return urljoin(self.base_url, url)


@pytest.yield_fixture
def server(request):
    '''Create an instance of the `Server`.

    Server's options may be passed to the constructor with `pytest.mark.server_options`.
    '''
    opts = request.node.get_marker('server_options')
    opts = opts.kwargs if opts else {}
    s = Server(**opts)
    s.configure()
    s.start()
    yield s
    s.terminate()
