"""
SCons Tool to invoke the Kalimba linker.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

from SCons.Tool import gnulink
import SCons.Builder

class ToolKldWarning(SCons.Warnings.Warning):
    pass

class KldNotFound(ToolKldWarning):
    pass

SCons.Warnings.enableWarningClass(ToolKldWarning)

def _detect(env):
    """Try to detect the presence of the kld tool."""
    try:
        return env['KLD']
    except KeyError:
        pass

    kld = env.WhereIs('kld', env['KCC_DIR'])
    if kld:
        return kld

    raise SCons.Errors.StopError(
        KldNotFound,
        "Could not find Kalimba linker (kld.exe)")

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    gnulink.generate(env)

    # Set up standard folder locations
    env.SetDefault(SDK_TOOLS = env['TOOLS_ROOT'] + '/tools')
    env.SetDefault(KCC_DIR = env['SDK_TOOLS'] + '/kcc/bin')

    env['KLD'] = _detect(env)
    env['LINK'] = '$KLD'

def exists(env):
    return _detect(env)

