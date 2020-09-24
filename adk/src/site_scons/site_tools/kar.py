"""
SCons Tool to invoke the Kalimba archiver.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

from SCons.Tool import ar
import SCons.Builder

class ToolKarWarning(SCons.Warnings.Warning):
    pass

class KarNotFound(ToolKarWarning):
    pass

SCons.Warnings.enableWarningClass(ToolKarWarning)

def _detect(env):
    """Try to detect the presence of the kar tool."""
    try:
        return env['KAR']
    except KeyError:
        pass

    kar = env.WhereIs('kar', env['KCC_DIR'])
    if kar:
        return kar

    raise SCons.Errors.StopError(
        KarNotFound,
        "Could not find Kalimba archiver (kar.exe)")

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    ar.generate(env)

    # Set up standard folder locations
    env.SetDefault(SDK_TOOLS = env['TOOLS_ROOT'] + '/tools')
    env.SetDefault(KCC_DIR = env['SDK_TOOLS'] + '/kcc/bin')

    env['KAR'] = _detect(env)
    env['AR'] = '$KAR'
    env['ARFLAGS'] = 'cru'

def exists(env):
    return _detect(env)

