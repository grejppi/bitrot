#!/usr/bin/env python

from __future__ import print_function

top = '.'
out = 'build'

APPNAME = 'bitrot'
VERSION = '0.7.0'

import sys
import os
import re
import shutil

def options(opt):
    opt.load('compiler_cxx')
    opt.add_option('-p', '--platform', dest='platform', default=None,
                   help='cross-compilation target')
    opt.add_option('-d', '--debug', dest='debug', default=None,
                   help='create debuggable build')
    opt.add_option('--use-upstream-dpf', dest='use_upstream_dpf',
                   action='store_true', default=False,
                   help='use upstream (non-customized) version of DPF')

def configure(conf):
    conf.env.append_value('CXXFLAGS', ['-std=c++11'])
    if conf.options.use_upstream_dpf:
        conf.env.append_value('CXXFLAGS',
            ['-DkParameterIsAutomatable=kParameterIsAutomable'])
        conf.env.append_value('CXXFLAGS', ['-DkParameterIsTrigger=0'])

    conf.load('compiler_cxx')
    conf.env.store('.default_env')

    cxxflags = []
    ldflags = []
    toolchain = ''

    if conf.options.debug:
        cxxflags.append('-g')

    if conf.options.platform in ('w32', 'win32', 'windows32'):
        toolchain = 'i686-w64-mingw32-'
        conf.env.PLATFORM = 'win32'
        ldflags.append('-static-libgcc')
        ldflags.append('-static-libstdc++')
    elif conf.options.platform in ('w64', 'win64', 'windows64'):
        toolchain = 'x86_64-w64-mingw32-'
        conf.env.PLATFORM = 'win32'
        ldflags.append('-static-libgcc')
        ldflags.append('-static-libstdc++')
    elif conf.options.platform in ('l32', 'lin32', 'linux32'):
        cxxflags.append('-m32')
        ldflags.append('-m32')
        conf.env.PLATFORM = 'linux'
    elif conf.options.platform in ('l64', 'lin64', 'linux64'):
        cxxflags.append('-m64')
        ldflags.append('-m64')
        conf.env.PLATFORM = 'linux'
    else:
        if sys.platform != 'win32':
            cxxflags.append('-fPIC')
        conf.env.PLATFORM = sys.platform

    conf.env.append_value('CXXFLAGS', cxxflags)
    conf.env.append_value('LINKFLAGS', ldflags)
    conf.env.AR       = toolchain + 'ar'
    conf.env.CXX      = toolchain + 'g++'
    conf.env.LINK_CXX = toolchain + 'g++'
    conf.load('compiler_cxx')

    from waflib.Tools.compiler_cxx import cxx_compiler
    cxx_compiler[sys.platform] = toolchain + 'gcc'

    if conf.env.PLATFORM.startswith('win32'):
        conf.env.cxxshlib_PATTERN = '%s.dll'
    elif conf.env.PLATFORM.startswith('linux'):
        conf.env.cxxshlib_PATTERN = '%s.so'

    try:
        conf.find_file('DistrhoPluginMain.cpp', ['DPF/distrho'])
    except conf.errors.ConfigurationError as err:
        raise conf.errors.ConfigurationError(
            "DPF submodule not found!\n"
            "Please run the following commands and try again:\n"
            "  git submodule init\n"
            "  git submodule update\n"
        )

def build(bld):
    bld.recurse('plugins')
