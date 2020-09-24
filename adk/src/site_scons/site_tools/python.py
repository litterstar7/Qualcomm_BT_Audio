"""
SCons Tool to invoke this ADK's flavour Python.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import SCons.Builder

class ToolPythonWarning(SCons.Warnings.Warning):
    pass

class PythonNotFound(ToolPythonWarning):
    pass

SCons.Warnings.enableWarningClass(ToolPythonWarning)

def _detect(env):
    """Try to detect the presence of Python."""
    try:
        return env['python']
    except KeyError:
        pass

    python = env.WhereIs('python', env['SDK_TOOLS'] + '/python27')
    if python:
        return python

    raise SCons.Errors.StopError(
        PythonNotFound,
        "Could not find Python (python.exe)")

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    # Set up standard folder locations
    env.SetDefault(SDK_TOOLS = env['TOOLS_ROOT'] + '/tools')

    env['python'] = _detect(env)

def exists(env):
    return _detect(env)

