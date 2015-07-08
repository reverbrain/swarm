import pytest
import inspect
import types
import functools
import tornado.gen

from fixtures import *


def pytest_addoption(parser):
    parser.addoption('--async-test-timeout', type=float,
                     default=5,
                     help='timeout in seconds before failing the test')


def pytest_configure(config):
    config.addinivalue_line("markers",
                            "async_test(timeout=None): "
                            "mark the test as asynchronous, it will be "
                            "run using tornado's event loop")


@pytest.mark.tryfirst
def pytest_pycollect_makeitem(collector, name, obj):
    '''Returns custom item/collector for a python object in a module, or None.

    Args:
        collector: a 'Collector' instance representing the node in a collection tree.
        name: name of the node in a collection tree.
        obj: actual object of the node (e.g., class, function, etc.)
    '''
    if collector.funcnamefilter(name) and inspect.isgeneratorfunction(obj):
        item = pytest.Function(name, parent=collector)
        # accept test function only if it's marked with 'pytest.mark.async_test'
        if 'async_test' in item.keywords:
            # test may be parametrized, generate multiple tests
            return list(collector._genfunctions(name, obj))


@pytest.mark.tryfirst
def pytest_runtest_setup(item):
    '''Called before 'pytest_runtest_call(item)' to prepare test.

    Args:
        item: an 'Item' instance representing terminal node in a collection tree.
    '''
    if 'async_test' in item.keywords and 'io_loop' not in item.fixturenames:
        # inject an event loop fixture for all async tests
        item.fixturenames.append('io_loop')


@pytest.mark.tryfirst
def pytest_pyfunc_call(pyfuncitem):
    '''Called to execute the test 'pyfuncitem'.

    Args:
        pyfuncitem: a 'Function' instance representing test function.
    '''
    if 'async_test' in pyfuncitem.keywords:
        test_io_loop = pyfuncitem.funcargs.get('io_loop')

        def _argnames(func):
            spec = inspect.getargspec(func)
            if spec.defaults:
                return spec.args[:-len(spec.defaults)]
            if isinstance(func, types.FunctionType):
                return spec.args
            # Func is a bound method, skip "self"
            return spec.args[1:]

        funcargs = dict((arg, pyfuncitem.funcargs[arg])
                        for arg in _argnames(pyfuncitem.obj))

        def _timeout(item):
            default_timeout = item.config.getoption('async_test_timeout')
            async_test = item.get_marker('async_test')
            if async_test:
                return async_test.kwargs.get('timeout', default_timeout)
            return default_timeout

        coroutine = tornado.gen.coroutine(pyfuncitem.obj)
        test_io_loop.run_sync(functools.partial(coroutine, **funcargs),
                              timeout=_timeout(pyfuncitem))

        # prevent other pyfunc calls from executing
        return True
