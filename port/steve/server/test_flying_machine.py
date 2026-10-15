"""Compatibility entry point for the sustained bomber regression."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('test_bomber_sustained.py')),run_name='__main__')
