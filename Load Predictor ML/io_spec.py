"""Schema-v4: feature-based per-consumer predictive BCM monitoring."""
from __future__ import annotations
import numpy as np
MODEL_SCHEMA_VERSION=4
CYCLE_TIME_MS=5
SEQ_LEN=200
HISTORY_MS=SEQ_LEN*CYCLE_TIME_MS
INFERENCE_PERIOD_MS=20
PREDICTION_HORIZON_MS=500
PREDICTION_HORIZON_STEPS=PREDICTION_HORIZON_MS//CYCLE_TIME_MS
VOLTAGE_MIN_V=6.0; VOLTAGE_MAX_V=32.0
UNDERVOLTAGE_THRESHOLD_V=9.0; OVERVOLTAGE_THRESHOLD_V=15.0
CURRENT_MIN_A=0.0; CURRENT_MAX_A=250.0
CURRENT_RATIO_MAX = 2.0
CURRENT_RATING_MIN_A=5.0; CURRENT_RATING_MAX_A=250.0
CHANNEL_RATINGS_A=np.array([5.0, 7.5, 10.0, 12.5, 15.0, 17.5, 20.0, 22.5, 25.0, 27.5, 30.0, 32.5, 35.0, 37.5, 40.0, 45.0, 50.0, 55.0, 60.0, 65.0, 70.0, 75.0, 80.0, 85.0, 90.0, 95.0, 100.0, 105.0, 110.0, 115.0, 120.0, 125.0, 130.0, 135.0, 140.0, 145.0, 150.0, 155.0, 160.0, 165.0, 170.0, 175.0, 180.0, 185.0, 190.0, 195.0, 200.0, 205.0, 210.0, 215.0, 220.0, 225.0, 230.0, 235.0, 240.0, 245.0, 248.0, 7.5, 10.0, 15.0, 20.0, 25.0, 30.0, 40.0, 50.0, 60.0, 80.0, 100.0, 125.0, 150.0, 175.0, 200.0, 225.0, 230.0, 250.0],dtype=np.float32)
NUM_CHANNELS=int(CHANNEL_RATINGS_A.size)
WINDOW_SAMPLES=(5,20,100,200) # 25,100,500,1000 ms
# current_now + 6 statistics x 4 windows + max/min dI/dt + 3 utilization + 3 Vin + rating
INPUT_SIZE=34
FAULT_NORMAL=0; FAULT_IMPENDING_OVERCURRENT=1; FAULT_IMPENDING_OPEN_LOAD=2; FAULT_IMPENDING_INTERMITTENT=3
NUM_FAULT_CLASSES=4
FAULT_NAMES=['normal','impending_overcurrent','impending_open_load','impending_intermittent']
OUTPUT_PREDICTED_CURRENT=0; OUTPUT_FAULT_SOON=1; OUTPUT_FAULT_OFFSET=2; OUTPUT_SIZE=6

def _norm(x,lo,hi): return np.clip((np.asarray(x,dtype=np.float32)-lo)/(hi-lo),0,1)
def normalize_current(x): return _norm(x,CURRENT_MIN_A,CURRENT_MAX_A)
def _window_features(x,n,rating):
    a=np.asarray(x[-n:],dtype=np.float32); t=np.arange(n,dtype=np.float32)
    mean=float(a.mean()); centered=t-t.mean(); den=float(np.dot(centered,centered))
    slope=float(np.dot(centered,a-mean)/den) if den>0 else 0.0 # A/sample
    return [mean,float(a.max()),float(a.min()),float(np.sqrt(np.mean(a*a))),float(a.var()),slope]
def build_input(voltage_v,current_a,rating_a):
    v=np.asarray(voltage_v,dtype=np.float32); i=np.asarray(current_a,dtype=np.float32); r=float(rating_a)
    if v.shape!=(SEQ_LEN,) or i.shape!=(SEQ_LEN,): raise ValueError(f'histories must be {SEQ_LEN} samples')
    f=[float(i[-1])]
    stats=[]
    for n in WINDOW_SAMPLES: stats.append(_window_features(i,n,r))
    # statistic-major order: mean windows, max windows, min windows, rms windows, variance windows, slope windows
    for stat in range(6): f.extend(s[stat] for s in stats)
    di=np.diff(i); f += [float(di.max()) if di.size else 0.0,float(di.min()) if di.size else 0.0]
    f += [float(i[-1]/r),float(i[-20:].mean()/r),float(i.max()/r)]
    vt=np.arange(SEQ_LEN,dtype=np.float32); vc=vt-vt.mean(); vmean=float(v.mean())
    vslope=float(np.dot(vc,v-vmean)/np.dot(vc,vc))
    f += [float(v[-1]),vmean,vslope,r]
    f=np.asarray(f,dtype=np.float32); assert f.size==INPUT_SIZE
    # scale to compact roughly [-1,1]/[0,1] domains
    out=f.copy(); k=0
    out[k]=np.clip(out[k]/CURRENT_MAX_A,0,1); k+=1
    for _ in range(4): out[k]=np.clip(out[k]/CURRENT_MAX_A,0,1); k+=1 # means
    for _ in range(4): out[k]=np.clip(out[k]/CURRENT_MAX_A,0,1); k+=1 # max
    for _ in range(4): out[k]=np.clip(out[k]/CURRENT_MAX_A,0,1); k+=1 # min
    for _ in range(4): out[k]=np.clip(out[k]/CURRENT_MAX_A,0,1); k+=1 # rms
    for _ in range(4): out[k]=np.clip(out[k]/(CURRENT_MAX_A**2),0,1); k+=1
    for _ in range(4): out[k]=np.clip(out[k]/25.0,-1,1); k+=1 # A/sample slope
    out[k:k+2]=np.clip(out[k:k+2]/50.0,-1,1); k+=2
    out[k:k+3]=np.clip(out[k:k+3]/2.0,0,1); k+=3
    out[k]=np.clip((out[k]-VOLTAGE_MIN_V)/(VOLTAGE_MAX_V-VOLTAGE_MIN_V),0,1); k+=1
    out[k]=np.clip((out[k]-VOLTAGE_MIN_V)/(VOLTAGE_MAX_V-VOLTAGE_MIN_V),0,1); k+=1
    out[k]=np.clip(out[k]/2.0,-1,1); k+=1 # V/sample
    out[k]=np.clip((out[k]-CURRENT_RATING_MIN_A)/(CURRENT_RATING_MAX_A-CURRENT_RATING_MIN_A),0,1)
    return out.astype(np.float32)
