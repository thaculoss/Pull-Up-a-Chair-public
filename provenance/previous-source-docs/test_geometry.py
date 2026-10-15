from pathlib import Path
import json,math
BASE=Path(__file__).resolve().parent;ROOT=BASE.parent
data=json.loads((BASE/'evidence/scene-audit/seat-table-record-chains.json').read_text('utf-8'))
cases=[]
def check(v,m):
    if not v:raise AssertionError(m)
def geometry(table,player,facing,bounds,low):
    tx,ty,_,_,_,yaw=table['position_rotation'];scale=table.get('scale',1);px,py=player
    dx,dy=tx-px,ty-py;angle=math.degrees(yaw);frame=facing
    if low:
        si,co=math.sin(yaw),math.cos(yaw);cx=(bounds[0]+bounds[2])/2;cy=(bounds[1]+bounds[3])/2
        xx=dx+scale*(cx*co+cy*si);yy=dy+scale*(-cx*si+cy*co)
        along=xx*si+yy*co;across=xx*co-yy*si
        if abs(across)>abs(along):angle+=90 if across>=0 else -90
        elif along<0:angle+=180
        frame=angle%360
    else:
        while facing-angle>45:angle+=90
        while facing-angle< -45:angle-=90
        if abs(facing-angle)<12 and dx*math.sin(math.radians(facing))+dy*math.cos(math.radians(facing))>15:frame=angle
    pc,ps=math.cos(math.radians(frame)),math.sin(math.radians(frame));sn,cs=math.sin(yaw-math.radians(frame)),math.cos(yaw-math.radians(frame))
    cx,cy=(bounds[0]+bounds[2])/2,(bounds[1]+bounds[3])/2;hx,hy=(bounds[2]-bounds[0])/2,(bounds[3]-bounds[1])/2
    ox=hx*abs(cs)+hy*abs(sn);oy=hx*abs(sn)+hy*abs(cs);factor=min(hx/(ox*abs(cs)+oy*abs(sn)),hy/(ox*abs(sn)+oy*abs(cs)))
    centerS=dx*pc-dy*ps+scale*(cx*cs+cy*sn);centerF=dx*ps+dy*pc+scale*(-cx*sn+cy*cs)
    w,d=scale*ox*factor,scale*oy*factor
    return frame,(centerS-w,centerS+w,centerF-d,centerF+d)
def fits(region,dishes):
    left,right,near,far=region
    for layout in range(4):
        padded=dishes+[(0,0)]*(3-len(dishes));ww=[d[0] for d in padded];dd=[d[1] for d in padded]
        offsets=[(0,0),(ww[0]+ww[1]+3,14),(-ww[0]-ww[2]-3,8)]
        if layout==1:offsets=[(0,0),(-offsets[1][0],14),(-offsets[2][0],8)]
        elif layout>=2:
            row=dd[0]+max(dd[1:])+3
            if layout==3:row=-row
            offsets=[(0,0),(ww[2]+1.5,row),(-ww[1]-1.5,row)]
        mins=min(offsets[i][0]-d[0] for i,d in enumerate(dishes));maxs=max(offsets[i][0]+d[0] for i,d in enumerate(dishes))
        minf=min(offsets[i][1]-d[1] for i,d in enumerate(dishes));maxf=max(offsets[i][1]+d[1] for i,d in enumerate(dishes))
        slo=max(-56,left-mins+.5);shi=min(56,right-maxs-.5);flo=max(30,near-minf+.5);fhi=min(105,far-maxf-.5)
        if slo<=shi and flo<=fhi:return {'layout':layout,'side_interval':[slo,shi],'forward_interval':[flo,fhi]}
    return None
bounds=(-55,-35.5,55.3,34,29.44566)
table=data['skyrim.esm:0C8FC8'][-1];player=(41.506226,269.639618);facing=131.417999
oldframe,oldregion=geometry(table,player,facing,bounds,False);newframe,newregion=geometry(table,player,facing,bounds,True)
for name,items in [('small_drink',[(4,4)]),('large_drink',[(10,10)]),('apple',[(5,5)]),('plated_main',[(24,24)]),('three_courses',[(24,24),(8,8),(14,14)])]:
    before=fits(oldregion,items);after=fits(newregion,items)
    check(before is None,'Observed failure not reproduced: '+name);check(after is not None,'Low-table frame still rejects '+name)
    cases.append({'case':'HS observed '+name,'old_rejected':True,'new_fit':after})
for seat in ['0C8FCE','0C8FCA']:
    chair=data['skyrim.esm:'+seat][0];t=data['skyrim.esm:0C8FC8'][0]
    pp=tuple(chair['position_rotation'][:2]);ff=math.degrees(chair['position_rotation'][5])
    frame,reg=geometry(t,pp,ff,bounds,True)
    check(fits(reg,[(4,4)]) is not None,'Vanilla chair drink regressed')
    check(fits(reg,[(24,24),(8,8),(14,14)]) is not None,'Vanilla chair meal regressed')
    cases.append({'case':'vanilla '+seat,'drink_and_meal_fit':True,'frame':frame})
root=ROOT/'pull-up-a-chair-0.9.7-beta-lowtables2-20261014/evidence/low02-records-and-mesh.json'
low02=json.loads(root.read_text('utf-8'))['records'];t=low02['skyrim.esm:0CBA60'];chair=low02['skyrim.esm:0CB93E'];pp=tuple(chair['position_rotation'][:2]);ff=math.degrees(chair['position_rotation'][5])
frame,reg=geometry(t,pp,ff,(-39.5,-31.5,39.5,31.5,29.44566),True)
check(fits(reg,[(4,4)]) is not None,'Low02 drink regressed');cases.append({'case':'Low02 vanilla','drink_fit':True,'frame':frame})
result={'observed_player':player,'observed_facing':facing,'old_frame':oldframe,'old_support':oldregion,'new_frame':newframe,'new_support':newregion,'cases':cases,'gameplay_verified':False}
(BASE/'evidence/geometry-regressions.json').write_text(json.dumps(result,indent=2),'utf-8')
print(json.dumps(result,indent=2))
