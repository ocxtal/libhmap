#! /usr/bin/env python
# encoding: utf-8

def options(opt):
	opt.load('compiler_c')

def configure(conf):
	conf.load('ar')
	conf.load('compiler_c')

	conf.env.append_value('CFLAGS', '-O3')
	conf.env.append_value('CFLAGS', '-std=c99')
	conf.env.append_value('CFLAGS', '-march=native')

	conf.env.append_value('OBJ_HMAP', ['hmap.o'])


def build(bld):

	bld.objects(source = 'hmap.c', target = 'hmap.o')

	bld.stlib(
		source = ['unittest.c'],
		target = 'hmap',
		use = bld.env.OBJ_HMAP)

	bld.program(
		source = ['unittest.c'],
		target = 'unittest',
		use = bld.env.OBJ_HMAP,
		defines = ['TEST'])
