"""
SCons Tool to invoke the Kalimba assembler.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import SCons.Builder

class ToolKasWarning(SCons.Warnings.Warning):
    pass

class KasNotFound(ToolKasWarning):
    pass

SCons.Warnings.enableWarningClass(ToolKasWarning)

def _detect(env):
    """Try to detect the presence of the kas tool."""
    try:
        return env['KAS']
    except KeyError:
        pass

    kas = env.WhereIs('kas', env['KCC_DIR'])
    if kas:
        return kas

    raise SCons.Errors.StopError(
        KasNotFound,
        "Could not find Kalimba assembler (kas.exe)")

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    # Set up standard folder locations
    env.SetDefault(SDK_TOOLS = env['TOOLS_ROOT'] + '/tools')
    env.SetDefault(KCC_DIR = env['SDK_TOOLS'] + '/kcc/bin')

    env['KAS'] = _detect(env)

def exists(env):
    return _detect(env)

