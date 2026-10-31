"""Wire-level CBR 20ms -> 60ms aggregation, with a real libopus decode when available."""
import ctypes as C
import ctypes.util
import math
import subprocess
import tempfile
import unittest
from pathlib import Path
class OpusPacket(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();lib=Path(cls.tmp.name)/'packet.dylib'
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC','-Ios/services/audio','os/services/audio/opus_packet.c','-o',str(lib)],check=True)
  cls.library=C.CDLL(str(lib));cls.join=cls.library.ps_opus_join_20ms
  cls.join.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p,C.c_size_t];cls.join.restype=C.c_size_t
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def assemble(self,frames,capacity=512):
  packets=((C.c_ubyte*256)*3)();sizes=(C.c_uint16*3)();output=(C.c_ubyte*512)()
  for i,frame in enumerate(frames):
   for j,b in enumerate(frame):packets[i][j]=b
   sizes[i]=len(frame)
  n=self.join(packets,sizes,output,capacity)
  return bytes(output[:n])
 def test_rfc_cbr_packet_and_rejections(self):
  silence=b'\xf8\xff\xfe'
  self.assertEqual(self.assemble([silence]*3),b'\xfb\x03'+b'\xff\xfe'*3)
  for frames in ([b'']*3,[silence,b'\xf8\xff',silence],[silence,b'\xb8\xff\xfe',silence],[b'\xf9\xff\xfe']*3,[b'\xfc\xff\xfe']*3,[b'\xf0\xff\xfe']*3):
   self.assertEqual(self.assemble(frames),b'')
  self.assertEqual(self.assemble([silence]*3,7),b'')
 def test_real_encoder_and_decoder_sixty_ms(self):
  name=C.util.find_library('opus')
  if not name:self.skipTest('libopus unavailable; deterministic RFC fixtures remain mandatory')
  opus=C.CDLL(name);err=C.c_int()
  opus.opus_encoder_create.argtypes=[C.c_int,C.c_int,C.c_int,C.POINTER(C.c_int)];opus.opus_encoder_create.restype=C.c_void_p
  opus.opus_encoder_destroy.argtypes=[C.c_void_p]
  opus.opus_encoder_ctl.argtypes=[C.c_void_p,C.c_int]
  opus.opus_encode.argtypes=[C.c_void_p,C.c_void_p,C.c_int,C.c_void_p,C.c_int];opus.opus_encode.restype=C.c_int
  opus.opus_decoder_create.argtypes=[C.c_int,C.c_int,C.POINTER(C.c_int)];opus.opus_decoder_create.restype=C.c_void_p
  opus.opus_decoder_destroy.argtypes=[C.c_void_p]
  opus.opus_decode.argtypes=[C.c_void_p,C.c_void_p,C.c_int,C.c_void_p,C.c_int,C.c_int];opus.opus_decode.restype=C.c_int
  opus.opus_packet_get_nb_samples.argtypes=[C.c_void_p,C.c_int,C.c_int]
  encoder=opus.opus_encoder_create(16000,1,2051,C.byref(err));self.assertEqual(err.value,0)
  decoder=opus.opus_decoder_create(16000,1,C.byref(err));self.assertEqual(err.value,0)
  try:
   for req,value in ((4002,24000),(4010,0),(4006,0)):self.assertEqual(opus.opus_encoder_ctl(encoder,req,C.c_int(value)),0)
   frames=[]
   for i in range(3):
    pcm=(C.c_int16*320)(*(int(6000*math.sin(2*math.pi*440*(i*320+j)/16000)) for j in range(320)))
    data=(C.c_ubyte*256)();n=opus.opus_encode(encoder,pcm,320,data,128);self.assertGreater(n,0);frames.append(bytes(data[:n]))
   combined=self.assemble(frames);self.assertTrue(combined)
   self.assertEqual(opus.opus_packet_get_nb_samples(combined,len(combined),16000),960)
   decoded=(C.c_int16*960)();self.assertEqual(opus.opus_decode(decoder,combined,len(combined),decoded,960,0),960)
   self.assertGreater(sum(v*v for v in decoded),1000000)
  finally:opus.opus_encoder_destroy(encoder);opus.opus_decoder_destroy(decoder)
if __name__=='__main__':unittest.main(verbosity=2)
