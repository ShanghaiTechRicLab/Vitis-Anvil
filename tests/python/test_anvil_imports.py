import pytest

pytestmark = pytest.mark.fast


def test_python_modules_import():
    import anvil
    import anvil.cli
    import anvil.json
    import anvil.log
    import anvil.progress
    import anvil.table
    import anvil.toml

    assert anvil.__version__ == "0.3.0"
