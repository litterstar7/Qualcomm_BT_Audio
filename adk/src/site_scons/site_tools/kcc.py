"""
SCons Tool to invoke the Kalimba C compiler.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

from SCons.Tool import gcc
import SCons.Builder

class ToolKccWarning(SCons.Warnings.Warning):
    pass

class KccNotFound(ToolKccWarning):
    pass

SCons.Warnings.enableWarningClass(ToolKccWarning)

def _detect(env):
    """Try to detect the presence of the kcc tool."""
    try:
        return env['KCC']
    except KeyError:
        pass

    kcc = env.WhereIs('kcc', env['KCC_DIR'])
    if kcc:
        return kcc

    raise SCons.Errors.StopError(
        KccNotFound,
        "Could not find Kalimba C compiler (kcc.exe)")

# Builder for assembly files
_kccAsmBuilder = SCons.Builder.Builder(
        action='$CCCOM',
        suffix='.o',
        src_suffix='.asm')

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    gcc.generate(env)

    # Set up standard folder locations
    env.SetDefault(SDK_TOOLS = env['TOOLS_ROOT'] + '/tools')
    env.SetDefault(KCC_DIR = env['SDK_TOOLS'] + '/kcc/bin')

    env['KCC'] = _detect(env)
    env['AS'] = '$KCC'
    env['CC'] = '$KCC'
    env['OBJSUFFIX'] = '.o'
    env['BUILDERS']['AsmObject'] = _kccAsmBuilder

def exists(env):
    return _detect(env)
