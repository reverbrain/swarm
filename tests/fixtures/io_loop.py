import pytest
import tornado.ioloop


@pytest.yield_fixture
def io_loop(request):
    '''Create an instance of the `tornado.ioloop.IOLoop`.

    Each async test will be provided with this fixture automatically
    and will run within this IOLoop.
    '''
    io_loop = tornado.ioloop.IOLoop()
    io_loop.make_current()
    yield io_loop

    io_loop.clear_current()
    if (not tornado.ioloop.IOLoop.initialized() or
            io_loop is not tornado.ioloop.IOLoop.instance()):
        io_loop.close(all_fds=True)
