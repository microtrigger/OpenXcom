"""Pure screen-pixel evidence. Fixed density, no world/camera inference."""
def layout(view):
    if not isinstance(view,dict):return None
    r=view.get('viewport')
    if not isinstance(r,dict)or set(r)!={'x','y','width','height'}or any(type(v)is not int for v in r.values()):return None
    if r['x']!=0 or r['y']!=32 or not(1<=r['width']<=512 and 16<=r['height']<=368)or view.get('sample_step')!=4 or view.get('sample_offset')!=2 or view.get('content_status')!='shown':return None
    columns=(r['width']+1)//4;rows=(r['height']+1)//4
    if not(0<columns*rows<=4096):return None
    return r,columns,rows

def decode(view):
    shape=layout(view)
    if shape is None:return None
    r,columns,rows=shape;n=columns*rows;palette=view.get('palette');pixels=view.get('pixels');mask=view.get('mask')
    if not isinstance(palette,list)or not(1<=len(palette)<=256)or any(not isinstance(c,list)or len(c)!=3 or any(type(x)is not int or not 0<=x<=255 for x in c)for c in palette):return None
    if not isinstance(mask,str)or len(mask)!=n or set(mask)-{'0','1'}or not isinstance(pixels,str)or len(pixels)!=2*n or set(pixels)-set('0123456789abcdef'):return None
    result={}
    for i,m in enumerate(mask):
        index=int(pixels[2*i:2*i+2],16)
        if m=='1':
            if index>=len(palette):return None
            result[(i%columns,i//columns)]=tuple(palette[index])
        elif index!=0:return None
    return result

def physical(points,view):
    r=view['viewport'];return {(r['x']+2+x*4,r['y']+2+y*4)for x,y in points}
def spread(points,view):
    p=physical(points,view);xs={x for x,y in p};ys={y for x,y in p}
    return {'original16px_cells':len({(x//16,y//16)for x,y in p}),'original16px_columns':len({x//16 for x,y in p}),'original16px_rows':len({y//16 for x,y in p}),'extent_x_px':max(xs)-min(xs)if xs else 0,'extent_y_px':max(ys)-min(ys)if ys else 0}
def broad(s):return s['original16px_columns']>=3 and s['original16px_rows']>=3 and s['extent_x_px']>=32 and s['extent_y_px']>=32
def compatible(views):return bool(views)and all(layout(v)is not None and layout(v)==layout(views[0])for v in views)
def preflight(before):
    if len(before)!=2 or not compatible(before):return {'qualified':False,'reason':'TWO_COMPATIBLE_CURRENT_BEFORE_REQUIRED'}
    a,b=[decode(v)for v in before]
    if a is None or b is None:return {'qualified':False,'reason':'INVALID_BEFORE_PIXELS'}
    stable={p for p in a.keys()&b.keys()if a[p]==b[p]};s=spread(stable,before[0]);ok=len(stable)>=64 and s['original16px_cells']>=8 and broad(s)
    return {'qualified':ok,'stable_before_samples':len(stable),'stable_before_spread':s,'reason':'BEFORE_FEASIBLE_NOT_AFTER_GUARANTEE'if ok else'BEFORE_EVIDENCE_IMPOSSIBLE_NO_ACTION'}
def visible_change(before,after):
    prerequisite=preflight(before)
    if not prerequisite['qualified']:return {'qualified':False,'reason':prerequisite['reason'],'preflight':prerequisite}
    if len(after)!=2 or not compatible(before+after):return {'qualified':False,'reason':'TWO_COMPATIBLE_SETTLED_AFTER_REQUIRED'}
    a,b,c,d=[decode(v)for v in before+after]
    if any(v is None for v in (a,b,c,d)):return {'qualified':False,'reason':'INVALID_PIXELS'}
    stable={p for p in a.keys()&b.keys()&c.keys()&d.keys()if a[p]==b[p]and c[p]==d[p]};changed={p for p in stable if a[p]!=c[p]};ss=spread(stable,before[0]);cs=spread(changed,before[0])
    ok=len(stable)>=64 and len(changed)>=16 and ss['original16px_cells']>=8 and broad(cs)
    return {'qualified':ok,'common_stable_samples':len(stable),'changed_stable_samples':len(changed),'stable_spread':ss,'changed_spread':cs,'requires_independent_image_review':True,'motion_proven':False,'reason':'VISIBLE_CHANGE_CANDIDATE'if ok else'SETTLED_EVIDENCE_DEFICIENT_PARTIAL'}
