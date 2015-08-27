import os, sys
import ConfigParser
import StringIO
sys.path.insert(0, 'src/tools')
import install

mach = os.environ.get('M3_MACHINE')
if mach != 'chip':
	mach = 'sim'

target = os.environ.get('M3_TARGET')
if target == 't2' or target == 't3':
	toolversion = 'RE-2014.5-linux' if target == 't3' else 'RD-2011.2-linux'

	# config (prefix it with [root] to make it usable from bash and python)
	ini_str = '[root]\n' + open('config.ini', 'r').read()
	config = ConfigParser.RawConfigParser()
	config.readfp(StringIO.StringIO(ini_str))

	cross = 'xtensa-buildroot-linux-uclibc'
	crossdir = Dir(config.get('root', 'buildroot')).abspath + '/host/usr/'
	crossver = '4.8.4'
	runtime = 'sim' if mach == 'sim' else 'min-rt'
	configpath = Dir(config.get('root', 'cfgpath'))
	xtroot = Dir(config.get('root', 'xtroot'))
	tooldir = Dir(xtroot.abspath + '/XtDevTools/install/tools/' + toolversion + '/XtensaTools/bin')
	crtdir = crossdir + '/lib/gcc/' + cross + '/' + crossver
elif target == 'gem5':
	cross = ''
	configpath = Dir('.')
else:
	# build for host by default
	target = 'host'
	cross = ''
	configpath = Dir('.')

core = os.environ.get('M3_CORE')
if core is None:
	if target == 't3':
		core = 'Pe_4MB_128k_4irq'
	elif target == 't2':
		core = 'oi_lx4_PE_6'
	elif target == 'gem5':
		core = 'x86_64'
	else:
		core = os.popen("uname -m").read().strip()

# build basic environment
baseenv = Environment(
	CPPFLAGS = '-D__' + target + '__ -D__' + target + '_' + mach + '__',
	CXXFLAGS = ' -std=c++11 -Wall -Wextra',
	CFLAGS = ' -std=c99 -Wall -Wextra',
	CPPPATH = ['#src/include'],
	ENV = {
		'PATH' : os.environ['PATH'],
		# required for colored outputs
		'HOME' : os.environ['HOME'],
		'TERM' : os.environ['TERM'],
	}
)

# for host compilation
hostenv = baseenv.Clone()

# for target compilation
env = baseenv.Clone()
env.Append(
	CXXFLAGS = ' -fno-strict-aliasing -fno-exceptions -fno-rtti -gdwarf-2 -fno-threadsafe-statics',
	CFLAGS = ' -gdwarf-2',
	ASFLAGS = ' -Wl,-W -Wall -Wextra',
	LINKFLAGS = ' -fno-exceptions -fno-rtti -Wl,--gc-sections',
)

# add target-dependent stuff to env
if target == 't2' or target == 't3':
	env.Append(
		# align it appropriately for the DTU
		LINKFLAGS = ' -nostdlib -Wl,-z,max-page-size=8 -Wl,-z,common-page-size=8',
		CPPPATH = [
			Dir(configpath.abspath + '/' + core + '/xtensa-elf/arch/include'),
			Dir(xtroot.abspath + '/XtDevTools/install/tools/' + toolversion + '/XtensaTools/xtensa-elf/include')
		],
		SUPDIR = Dir(configpath.abspath + '/' + core + '/xtensa-elf/arch/lib'),
		CRTDIR = Dir(crtdir)
	)
	env.Replace(ENV = {'PATH' : crossdir + '/bin:' + tooldir.abspath + ':' + os.environ['PATH']})
	env.Replace(CXX = cross + '-g++')
	env.Replace(AS = cross + '-gcc')
	env.Replace(FORTRAN = cross + '-gfortran')
	env.Replace(F90 = cross + '-gfortran')
	env.Replace(CC = cross + '-gcc')
	env.Replace(LD = cross + '-ld')
	env.Replace(AR = cross + '-ar')
	env.Replace(RANLIB = cross + '-ranlib')
else:
	if target == 'gem5':
		env.Append(LINKFLAGS = ' -static')
	env.Replace(CXX = 'g++')
	env.Replace(CC = 'gcc')
	env.Replace(AS = 'gcc')

# add build-dependent flags (debug/release)
btype = os.environ.get('M3_BUILD', 'release')
if btype == 'debug':
	if target == 'host' or target == 'gem5':
		env.Append(CXXFLAGS = ' -O0 -g')
		env.Append(CFLAGS = ' -O0 -g')
	else:
		# use -Os here because otherwise the binaries tend to get larger than 32k
		env.Append(CXXFLAGS = ' -Os -g')
		env.Append(CFLAGS = ' -Os -g')
	env.Append(ASFLAGS = ' -g')
	hostenv.Append(CXXFLAGS = ' -O0 -g')
	hostenv.Append(CFLAGS = ' -O0 -g')
else:
	if target == 't2':
		env.Append(CXXFLAGS = ' -Os -DNDEBUG -flto')
		env.Append(CFLAGS = ' -Os -DNDEBUG -flto')
		env.Append(LINKFLAGS = ' -Os -flto')
	elif target == 'host' or target == 'gem5':
		# no LTO for host
		env.Append(CXXFLAGS = ' -O2 -DNDEBUG')
		env.Append(CFLAGS = ' -O2 -DNDEBUG')
		env.Append(LINKFLAGS = ' -O2')
	else:
		env.Append(CXXFLAGS = ' -O2 -DNDEBUG -flto')
		env.Append(CFLAGS = ' -O2 -DNDEBUG -flto')
		env.Append(LINKFLAGS = ' -O2 -flto')
	hostenv.Append(CXXFLAGS = ' -O3')
	hostenv.Append(CFLAGS = ' -O3')
builddir = 'build/' + target + '-' + mach + '-' + btype

# print executed commands?
verbose = os.environ.get('M3_VERBOSE', 0)
if int(verbose) == 0:
	env['ASPPCOMSTR'] = "AS $TARGET"
	env['ASPPCOMSTR'] = "ASPP $TARGET"
	env['CCCOMSTR'] = "CC $TARGET"
	env['SHCCCOMSTR'] = "SHCC $TARGET"
	env['CXXCOMSTR'] = "CXX $TARGET"
	env['SHCXXCOMSTR'] = "SHCXX $TARGET"
	env['LINKCOMSTR'] = "LD $TARGET"
	env['SHLINKCOMSTR'] = "SHLD $TARGET"
	env['ARCOMSTR'] = "AR $TARGET"
	env['RANLIBCOMSTR'] = "RANLIB $TARGET"
	env['F90COMSTR'] = "FC $TARGET"

# add some important paths
env.Append(
	CROSS = cross,
	ARCH = target,
	MACH = mach,
	CORE = core,
	BUILD = btype,
	CFGS = configpath,
	BUILDDIR = Dir(builddir),
	BINARYDIR = Dir(builddir + '/bin'),
	LIBDIR = Dir(builddir + '/bin'),
	MEMDIR = Dir(builddir + '/mem'),
	FSDIR = Dir(builddir + '/fsdata')
)

def M3MemDump(env, target, source):
	dump = env.Command(
		target, source, 'xt-dumpelf --width=64 --offset=0 $SOURCE > $TARGET'
	)
	env.Install('$MEMDIR', dump)

def M3FileDump(env, target, source, addr, args = ''):
	dump = env.Command(
		target, source, '$BUILDDIR/src/tools/dumpfile/dumpfile $SOURCE 0x%x %s > $TARGET' % (addr, args)
	)
	env.Depends(dump, '$BUILDDIR/src/tools/dumpfile/dumpfile')
	env.Install('$MEMDIR', dump)

def M3Mkfs(env, target, source, blocks, inodes, blks_per_ext):
	fs = env.Command(
		target, source,
		'$BUILDDIR/src/tools/mkm3fs/mkm3fs $TARGET $SOURCE %d %d %d' % (blocks, inodes, blks_per_ext)
	)
	env.Depends(fs, '$BUILDDIR/src/tools/mkm3fs/mkm3fs')
	env.Install('$BUILDDIR', fs)

def M3Strip(env, target, source):
    return env.Command(
        target, source, cross + '-strip -o $TARGET $SOURCE'
    )

def M3Program(env, target, source, libs = [], libpaths = [], NoSup = False, core = None, ldscript = None):
	myenv = env.Clone()
	if myenv['ARCH'] == 't2' or myenv['ARCH'] == 't3':
		# set variables, depending on core
		if core is None:
			core = os.environ.get('M3_CORE')
		runtimedir = configpath.abspath + '/' + core + '/xtensa-elf/lib/' + runtime

		sources = []
		if not NoSup:
			sources += [
				myenv['LIBDIR'].abspath + '/crti.o',
				crtdir + '/crtbegin.o',
				crtdir + '/crtend.o',
				myenv['LIBDIR'].abspath + '/crtn.o',
				myenv['LIBDIR'].abspath + '/Window.o'
			]
			libs = ['hal', 'handlers-sim', 'c', 'm3', 'c', 'gcc'] + libs
		sources += source

		if ldscript is None:
			ldscript = File(runtimedir + '/ldscripts/elf32xtensa.x')
		myenv.Append(LINKFLAGS = ' -Wl,-T,' + ldscript.abspath)

		prog = myenv.Program(
			target,
			sources,
			LIBS = ['handler-reset'] + libs,
			LIBPATH = [myenv['LIBDIR'], myenv['SUPDIR']] + libpaths,
			CPPPATH = [
				'#src/include',
				Dir(configpath.abspath + '/' + core + '/xtensa-elf/arch/include'),
				Dir(xtroot.abspath + '/XtDevTools/install/tools/' + toolversion + '/XtensaTools/xtensa-elf/include')
			],
			SUPDIR = Dir(configpath.abspath + '/' + core + '/xtensa-elf/arch/lib')
		)
		myenv.M3MemDump(target + '.mem', prog)
		myenv.Depends(prog, ldscript)
		myenv.Depends(prog, File(runtimedir + '/specs'))
		myenv.Depends(prog, myenv['LIBDIR'].abspath + '/libm3.a')
	else:
		prog = myenv.Program(
			target, source,
			LIBS = ['m3', 'pthread'] + libs,
			LIBPATH = [myenv['LIBDIR']] + libpaths
		)

	myenv.Install(myenv['BINARYDIR'], prog)
	return prog

env.AddMethod(M3MemDump)
env.AddMethod(M3FileDump)
env.AddMethod(M3Mkfs)
env.AddMethod(M3Strip)
env.AddMethod(install.InstallFiles)
env.M3Program = M3Program

# always use grouping for static libraries, because they may depend on each other so that we want
# to cycle through them until all references are resolved.
env['_LIBFLAGS'] = '-Wl,--start-group ' + env['_LIBFLAGS'] + ' -Wl,--end-group'

env.SConscript('src/SConscript', exports = ['env', 'hostenv'], variant_dir = builddir, src_dir = '.', duplicate = 0)