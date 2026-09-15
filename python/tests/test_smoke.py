from vanta_hedge import __version__


def test_package_imports_and_exposes_a_version():
    assert isinstance(__version__, str)
    assert __version__
