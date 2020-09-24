"""
SCons Tool to invoke the AT Parser.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import os
import SCons.Builder

class ToolGenParseWarning(SCons.Warnings.Warning):
    pass

class GenParseNotFound(ToolGenParseWarning):
    pass

SCons.Warnings.enableWarningClass(ToolGenParseWarning)

def _detect(env):
    """Try to detect the presence of the genparse tool."""
    try:
        return env['genparse']
    except KeyError:
        pass

    genparse = env.WhereIs('genparse', env['SDK_TOOLS'] + '/bin')
    if genparse:
        return genparse

    raise SCons.Errors.StopError(
        GenParseNotFound,
        "Could not find AT Parser (genparse.exe)")

# Builder for Chain Generator
def _GenParseEmitter(target, source, env):
    target_base = os.path.splitext(str(target[0]))[0]
    target.append(target_base + '.h')
    return target, source

_GenParseBuilder = SCons.Builder.Builder(
        action=['$genparse ${TARGET.base} $SOURCE'],
        suffix='.c',
        src_suffix='.parse',
        emitter=_GenParseEmitter)

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    # Set up standard folder locations
    env.SetDefault(SDK_TOOLS = env['TOOLS_ROOT'] + '/tools')

    env['genparse'] = _detect(env)
    env['BUILDERS']['ParseObject'] = _GenParseBuilder

def exists(env):
    return _detect(env)

