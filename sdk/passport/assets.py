"""Bounded deterministic offline image compiler for Passport/LVGL 9.5.
PAI1 is a Passport wrapper, never passed directly to LVGL.
"""
import hashlib,io,json,re,struct,warnings
from pathlib import Path
FORMATS={'A1':1,'A4':2,'I4':3,'I8':4,'RGB565':5}
MAX_DIM=96
MAX_SOURCE=262144
HEADER=struct.Struct('<4sBBBBHHHHII')
def resource_id(value):
    if not isinstance(value,str) or len(value)>48 or not re.fullmatch(r'[a-z0-9]+(?:[/_-][a-z0-9]+)*',value):raise ValueError('invalid asset ID')
    return value
def validate_blob(data):
    if not 24<=len(data)<=16384:raise ValueError('image size')
    magic,fmt,major,minor,flags,w,h,stride,colors,size,reserved=HEADER.unpack_from(data)
    if magic!=b'PAI1' or (major,minor)!=(9,5) or flags or reserved or fmt not in FORMATS.values():raise ValueError('image version/format/flags')
    if not 1<=w<=MAX_DIM or not 1<=h<=MAX_DIM or w*h>9216:raise ValueError('image dimensions')
    bpp={1:1,2:4,3:4,4:8,5:16}[fmt];expected=(w*bpp+7)//8;slots={3:16,4:256}.get(fmt,0)
    if stride!=expected or size!=slots*4+stride*h or len(data)!=24+size:raise ValueError('image size/stride/palette')
    if (slots and not 1<=colors<=slots) or (not slots and colors):raise ValueError('palette count')
    if slots:
        if data[24:28]!=b'\0\0\0\0':raise ValueError('transparent slot zero')
        pixels=data[24+slots*4:]
        for y in range(h):
            for x in range(w):
                v=pixels[y*stride+x] if fmt==4 else (pixels[y*stride+x//2]>>(4 if x%2==0 else 0))&15
                if v>=colors:raise ValueError('invalid palette index')
    return dict(format=next(k for k,v in FORMATS.items() if v==fmt),width=w,height=h,stride=stride,palette_count=colors,size=len(data),sha256=hashlib.sha256(data).hexdigest())
def _svg(data,w,h):
    from defusedxml import ElementTree
    root=ElementTree.fromstring(data)
    allowed={'svg','g','path','rect','circle','ellipse','line','polyline','polygon','title','desc','defs','linearGradient','radialGradient','stop'}
    for node in root.iter():
        if node.tag.split('}')[-1] not in allowed:raise ValueError('SVG element not permitted')
        for k,v in node.attrib.items():
            if 'href' in k.lower() or 'url(' in v.lower() or '://' in v or k.lower().startswith('on'):raise ValueError('SVG external resource/reference denied')
    import cairosvg
    return cairosvg.svg2png(bytestring=data,output_width=w,output_height=h,unsafe=False)
def compile_asset(source,*,width=None,height=None,encoding='auto',kind='sprite',no_dither=True):
    from PIL import Image
    source=Path(source)
    if not source.is_file() or not 0<source.stat().st_size<=MAX_SOURCE:raise ValueError("source file size")
    data=source.read_bytes()
    if not data or len(data)>MAX_SOURCE:raise ValueError('source file size')
    if source.suffix.lower()=='.pai':validate_blob(data);return data
    if source.suffix.lower() not in ('.png','.svg'):raise ValueError('only PNG/SVG/compiled PAI supported')
    if width is None:width=48 if kind=='app-icon' else 24
    if height is None:height=width
    if type(width)!=int or type(height)!=int or not 1<=width<=MAX_DIM or not 1<=height<=MAX_DIM:raise ValueError('requested dimensions')
    if source.suffix.lower()=='.svg':data=_svg(data,width,height)
    with warnings.catch_warnings():
        warnings.simplefilter('error',Image.DecompressionBombWarning)
        with Image.open(io.BytesIO(data)) as im:
            if im.width*im.height>262144 or getattr(im,'n_frames',1)!=1:raise ValueError('source pixels/frames')
            im=im.convert('RGBA').resize((width,height),Image.Resampling.LANCZOS)
    pixels=list(im.getdata());nontransparent={p for p in pixels if p[3]}
    if encoding=='auto':
        if kind=='mask':encoding='A1' if all(p[3] in (0,255) for p in pixels) else 'A4'
        else:encoding='I4' if len(nontransparent)<=15 else 'I8'
    if encoding not in FORMATS:raise ValueError('encoding')
    fmt=FORMATS[encoding];bpp={1:1,2:4,3:4,4:8,5:16}[fmt];stride=(width*bpp+7)//8;palette=b'';colors=0
    if encoding in ('I4','I8'):
        slots=16 if encoding=='I4' else 256
        if len(nontransparent)>slots-1:
            im=im.quantize(colors=slots-1,method=Image.Quantize.FASTOCTREE,dither=Image.Dither.NONE if no_dither else Image.Dither.FLOYDSTEINBERG).convert('RGBA');pixels=list(im.getdata());nontransparent={p for p in pixels if p[3]}
        entries=[(0,0,0,0)]+sorted(nontransparent);colors=len(entries);lookup={p:i for i,p in enumerate(entries)}
        palette=b''.join(bytes((b,g,r,a)) for r,g,b,a in entries)+b'\0'*((slots-colors)*4)
        indices=[0 if p[3]==0 else lookup[p] for p in pixels]
    else:indices=None
    payload=bytearray(stride*height)
    for y in range(height):
        for x in range(width):
            i=y*width+x;pos=y*stride
            if fmt==1:payload[pos+x//8]|=(1 if pixels[i][3]>=128 else 0)<<(7-x%8)
            elif fmt==2:payload[pos+x//2]|=((pixels[i][3]*15+127)//255)<<(4 if x%2==0 else 0)
            elif fmt==3:payload[pos+x//2]|=indices[i]<<(4 if x%2==0 else 0)
            elif fmt==4:payload[pos+x]=indices[i]
            else:
                r,g,b,a=pixels[i]
                if a!=255:raise ValueError('RGB565 cannot preserve alpha')
                struct.pack_into('<H',payload,pos+x*2,((r>>3)<<11)|((g>>2)<<5)|(b>>3))
    body=palette+payload;result=HEADER.pack(b'PAI1',fmt,9,5,0,width,height,stride,colors,len(body),0)+body
    validate_blob(result);return result
def build_manifest_assets(directory,assets):
    root=Path(directory).resolve();records=[];blob=bytearray();seen=set()
    if not isinstance(assets,list) or len(assets)>32:raise ValueError('asset count')
    for item in sorted(assets,key=lambda x:x.get('id','')):
        rid=resource_id(item.get('id'))
        if rid in seen:raise ValueError('duplicate asset ID')
        seen.add(rid);relative=item.get('source',item.get('file',''))
        if not isinstance(relative,str) or len(relative)>128 or not relative or Path(relative).is_absolute() or '..' in Path(relative).parts:raise ValueError('source path')
        source=(root/relative).resolve()
        if not source.is_relative_to(root):raise ValueError('source outside app')
        encoded=compile_asset(source,width=item.get('width'),height=item.get('height'),encoding=item.get('encoding','auto'),kind=item.get('kind','sprite'),no_dither=item.get('no_dither',True))
        info=validate_blob(encoded);records.append(dict(id=rid,offset=len(blob),**info));blob.extend(encoded)
        if len(blob)>196608:raise ValueError('asset bundle quota')
    return records,bytes(blob)
def validate_index(records,blob):
    if not isinstance(records,list) or len(records)>32:raise ValueError('asset count')
    seen=set();end=0
    for r in records:
        rid=resource_id(r.get('id'))
        if rid in seen:raise ValueError('duplicate asset ID')
        seen.add(rid);offset=r.get('offset');size=r.get('size')
        if type(offset)!=int or type(size)!=int or offset!=end or size<24 or offset+size>len(blob):raise ValueError('asset offsets')
        info=validate_blob(blob[offset:offset+size])
        if any(r.get(k)!=v for k,v in info.items()):raise ValueError('asset index/header/hash mismatch')
        end=offset+size
    if end!=len(blob):raise ValueError('unindexed asset bytes')
def decode_blob(data):
    from PIL import Image
    info=validate_blob(data);_,fmt,_,_,_,w,h,stride,_,_,_=HEADER.unpack_from(data);slots={3:16,4:256}.get(fmt,0);pixels=data[24+slots*4:];result=Image.new('RGBA',(w,h));out=[]
    for y in range(h):
        for x in range(w):
            if fmt in (3,4):
                idx=pixels[y*stride+x] if fmt==4 else (pixels[y*stride+x//2]>>(4 if x%2==0 else 0))&15
                b,g,r,a=data[24+idx*4:28+idx*4];out.append((r,g,b,a))
            elif fmt==1:out.append((255,255,255,255 if (pixels[y*stride+x//8]>>(7-x%8))&1 else 0))
            elif fmt==2:out.append((255,255,255,((pixels[y*stride+x//2]>>(4 if x%2==0 else 0))&15)*17))
            else:
                v=struct.unpack_from('<H',pixels,y*stride+x*2)[0];out.append((((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31,255))
    result.putdata(out);return result
