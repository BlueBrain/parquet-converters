#!/bin/sh

set -e

python -mvenv venv
. ./venv/bin/activate
pip install libsonata pandas pyarrow
python tests/test_integration.py
