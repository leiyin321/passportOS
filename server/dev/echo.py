#!/usr/bin/env python3
"""Owned bounded WS/WSS echo fixture; text/binary remain distinct."""
import argparse,asyncio,ssl
from websockets.asyncio.server import serve
async def echo(ws):
 async for message in ws:
  await ws.send(message)
async def main():
 p=argparse.ArgumentParser();p.add_argument('--host',default='0.0.0.0');p.add_argument('--port',type=int,default=8041);p.add_argument('--cert');p.add_argument('--key');a=p.parse_args();tls=None
 if a.cert:
  tls=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);tls.load_cert_chain(a.cert,a.key)
 async with serve(echo,a.host,a.port,ssl=tls,max_size=512,max_queue=4,ping_interval=20):
  print(f'Owned {"WSS" if tls else "WS"} echo ready on port {a.port}',flush=True)
  await asyncio.Future()
if __name__=='__main__':asyncio.run(main())
