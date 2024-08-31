#!/usr/bin/env python3
import os, sys, subprocess


def c2o(file, out='/tmp/c2o.o', includes=None, defines=None, opt='-O0', bits=64 ):
	if 'fedora' in os.uname().nodename:
		cmd = ['riscv64-linux-gnu-gcc']
	else:
		cmd = ['riscv64-unknown-elf-gcc']

	if bits==32:
		cmd.append('-march=rv32i')
		cmd.append('-mabi=ilp32')

	cmd += [
		'-c', '-mcmodel=medany', '-fomit-frame-pointer', '-ffunction-sections',
		'-ffreestanding', '-nostdlib', '-nostartfiles', '-nodefaultlibs', 
		opt, '-g', '-o', out, file
	]
	if includes:
		for inc in includes:
			if not inc.startswith('-I'): inc = '-I'+inc
			cmd.append(inc)
	if defines:
		for d in defines:
			if not d.startswith('-D'): d = '-D'+d
			cmd.append(d)
	
	print(cmd)
	subprocess.check_call(cmd)
	return out


def mkkernel(output='/tmp/two11bsd.elf'):
	defines = ['KERNEL']
	includes = [
		'./include', #'./sys/h', 
		'/tmp/bsd',
	]
	if not os.path.isdir('/tmp/bsd'): os.mkdir('/tmp/bsd')
	if not os.path.isdir('/tmp/bsd/sys'): os.mkdir('/tmp/bsd/sys')
	if not os.path.isdir('/tmp/bsd/machine'): os.mkdir('/tmp/bsd/machine')
	os.system('cp -v ./sys/h/*.h /tmp/bsd/sys/.')
	os.system('cp -v ./sys/machine/*.h /tmp/bsd/machine/.')
	obs = []
	for name in os.listdir('./sys/sys/'):
		assert name.endswith('.c')
		print(name)
		o = c2o(
			os.path.join('./sys/sys', name), 
			out = '/tmp/%s.o' % name,
			includes=includes, defines=defines, bits=32)
		obs.append(o)

	macho = []
	for name in 'machdep'.split():
		o = c2o(
			'./sys/machine/%s.c' % name, 
			out = '/tmp/_riscv_%s.o' % name,
			includes=includes, defines=defines, bits=32)
		macho.append(o)

	if 'fedora' in os.uname().nodename:
		cmd = ['riscv64-linux-gnu-ld']
	else:
		cmd = ['riscv64-unknown-elf-ld']

	cmd += ['-march=rv32', '-m', 'elf32lriscv', '-o', output] + obs + macho
	print(cmd)
	subprocess.check_call(cmd)

if __name__=='__main__':
	mkkernel()