"""
SCons Tool to invoke the GATT DB Interface Generator.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import os
import SCons.Builder
import SCons.Scanner
from SCons.Defaults import Move

import kas
import python
import gattdbgen

class ToolGattDBIFGenWarning(SCons.Warnings.Warning):
    pass

class GattDBIFGenNotFound(ToolGattDBIFGenWarning):
    pass

SCons.Warnings.enableWarningClass(ToolGattDBIFGenWarning)

def _detect(env):
    """Try to detect the presence of the gattdbifgen tool."""
    try:
        return env['gattdbifgen']
    except KeyError:
        pass

    gattdbifgen = env.WhereIs('dbgen_interface_generator.py',
                              env['ADK_TOOLS'] + '/packages/gattdbifgen',
                              pathext='.py')
    if gattdbifgen:
        return gattdbifgen

    raise SCons.Errors.StopError(
        GattDBIFGenNotFound,
        "Could not find GATT DB Interface Generator "
        "(dbgen_interface_generator.py)")

# Builder for GATT DB Interface Generator
def _DbyEmitter(target, source, env):
    target_base = os.path.splitext(str(target[0]))[0]
    target += [env.File(target_base + '.dbx')]
    return target, source

_dbxBuilder = SCons.Builder.Builder(
        action=['$cpredbif'],
        suffix='.dbx',
        src_suffix='.dbi',
        source_scanner=SCons.Scanner.C.CScanner())

_dbyBuilder = SCons.Builder.Builder(
        action=['$gattdbgen -i $SOURCE',
                Move('$TARGET', '${SOURCE.base}.h')],
        suffix='.dby',
        src_suffix='.dbx',
        source_scanner=SCons.Scanner.C.CScanner())

_gattdbifgenHBuilder = SCons.Builder.Builder(
        action=['$python $gattdbifgen --header $TARGET $SOURCES'],
        suffix='_if.h',
        src_suffix='.dby')

_gattdbifgenBuilder = SCons.Builder.Builder(
        action=['$python $gattdbifgen --source $TARGET $SOURCE'],
        suffix='_if.c',
        src_suffix='.h')

def generate(env):
    """Add Builders and construction variables to the Environment.
    """
    kas.generate(env)
    python.generate(env)
    gattdbgen.generate(env)

    env['gattdbifgen'] = _detect(env)
    env['cpredbif'] = '$cpre $CPPFLAGS -DGATT_DBI_LIB $_CPPDEFFLAGS $_CPPINCFLAGS $SOURCE -o ${TARGET.base}.dbx'
    env['BUILDERS']['DbxObject'] = _dbxBuilder
    env['BUILDERS']['DbyObject'] = _dbyBuilder
    env['BUILDERS']['DbIfHObject'] = _gattdbifgenHBuilder
    env['BUILDERS']['DbIfObject'] = _gattdbifgenBuilder

def exists(env):
    return _detect(env)
