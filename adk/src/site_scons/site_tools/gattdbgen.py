"""
SCons Tool to invoke the GATT DB Generator.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import kas
import os
import SCons.Builder
import SCons.Scanner
from SCons.Defaults import Delete

class ToolGattDBGenWarning(SCons.Warnings.Warning):
    pass

class GattDBGenNotFound(ToolGattDBGenWarning):
    pass

SCons.Warnings.enableWarningClass(ToolGattDBGenWarning)

def _detect(env):
    """Try to detect the presence of the gattdbgen tool."""
    try:
        return env['gattdbgen']
    except KeyError:
        pass

    gattdbgen = env.WhereIs('gattdbgen', env['SDK_TOOLS'] + '/bin')
    if gattdbgen:
        return gattdbgen

    raise SCons.Errors.StopError(
        GattDBGenNotFound,
        "Could not find GATT DB Generator (gattdbgen.exe)")

# Builder for GATT DB Generator
def _DbEmitter(target, source, env):
    target_base = os.path.splitext(str(target[0]))[0]
    target += [target_base + '.h', target_base + '.db_']
    return target, source

_gattdbgenBuilder = SCons.Builder.Builder(
        action=['$cprecom',
                '$gattdbgen ${TARGET.base}.db_'],
        suffix='.c',
        src_suffix='.db',
        emitter=_DbEmitter,
        source_scanner=SCons.Scanner.C.CScanner())

_gattdbigenBuilder = SCons.Builder.Builder(
        action=['$cprecom',
                '$gattdbgen -i ${TARGET.base}.db_',
                Delete('${TARGET.base}.db_')],
        suffix='.h',
        src_suffix='.dbi',
        source_scanner=SCons.Scanner.C.CScanner())

def generate(env):
    """Add Builders and construction variables to the Environment.
    """
    kas.generate(env)

    # Set up standard folder locations
    env.SetDefault(SDK_TOOLS = env['TOOLS_ROOT'] + '/tools')

    env['gattdbgen'] = _detect(env)
    # we use KAS as the kalcc pre-processor mangles 128-bit uuids
    env['cpre'] = '$KAS --preprocess-only'
    env['cprecom'] = '$cpre $CPPFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS $SOURCE -o ${TARGET.base}.db_'
    env['BUILDERS']['DbObject'] = _gattdbgenBuilder
    env['BUILDERS']['DbiObject'] = _gattdbigenBuilder

def exists(env):
    return _detect(env)
