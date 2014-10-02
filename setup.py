from distutils.core import setup, Extension
from platform import system

if system() == 'Windows':
	module1 = Extension('bora',
                    define_macros = [('BORA_RETRANSMISSION', '1'),
                                     ('DEF2', '0')],
                    extra_compile_args = ['-g','-mno-ms-bitfields', '-Wall', '-Wextra', '-Wstrict-prototypes'],
                    include_dirs = ['./', '/usr/include/'],
                    libraries = ['pthreadgc2', 'Ws2_32'],
                    library_dirs = ['/usr/local/lib'],
                    sources = ['bora.c', 'blockcache.c', 'netencoder.c', 'ack.c', 'ack_received.c', 'packet_receiver.c', 'packet_sender.c', 'biter_bridge.c', 'bpuller_bridge.c', 'stats_bridge.c', 'recv_stats.c', 'bw_stats.c', 'bw_msgs.c', 'cookie_sender.c', 'bora_util.c'])
else:
	module1 = Extension('bora',
                    define_macros = [('BORA_RETRANSMISSION', '1'),
                                     ('DEF2', '0')],
                    extra_compile_args = ['-g','-ggdb', '-mno-ms-bitfields', '-Wall', '-Wextra', '-Wstrict-prototypes'],
                    include_dirs = ['./', '/usr/include/'],
                    libraries = ['pthread'],
                    library_dirs = ['/usr/local/lib'],
                    sources = ['bora.c', 'blockcache.c', 'netencoder.c', 'ack.c', 'ack_received.c', 'packet_receiver.c', 'packet_sender.c', 'biter_bridge.c', 'bpuller_bridge.c', 'stats_bridge.c', 'recv_stats.c', 'bw_stats.c', 'bw_msgs.c', 'cookie_sender.c', 'bora_util.c'])

setup (name = 'bora',
       version = '0.01',
       description = 'bora is P2NER blockcache and congestion control Python Extension',
       author = 'loox',
       author_email = 'loox@ece.upatras.gr',
       url = 'http://upg.iamnothere.org:8181/',
       long_description = '''
bora is P2NER blockcache and congestion control Python Extension.
''',
       ext_modules = [module1])


