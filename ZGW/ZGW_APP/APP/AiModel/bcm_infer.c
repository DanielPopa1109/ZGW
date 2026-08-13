#include <math.h>
#include <stddef.h>
#include <stdint.h>
#define BCM_MODEL_ENABLE_WEIGHTS
#include "bcm_infer.h"

static float clip(float x,float lo,float hi){return x<lo?lo:(x>hi?hi:x);}
static float norm(float x,float lo,float hi){return clip((x-lo)/(hi-lo),0.0f,1.0f);}
static float relu(float x){return x>0.0f?x:0.0f;}
static float sigmoid(float x){if(x>=0.0f){float z=expf(-x);return 1.0f/(1.0f+z);}else{float z=expf(x);return z/(1.0f+z);}}

static float bcm_h1[BCM_H1_SIZE];
static float bcm_h2[BCM_H2_SIZE];
static float bcm_h3[BCM_H3_SIZE];

static void dense(const float *in,const float *w,const float *b,uint16_t ni,uint16_t no,float *out){
    uint16_t r,c; for(r=0;r<no;r++){float a=b[r];const float *rw=&w[(size_t)r*ni];for(c=0;c<ni;c++)a+=in[c]*rw[c];out[r]=a;}
}

/* Returns mean,max,min,RMS,variance and least-squares slope in A/sample. */
static void window_stats(const float *x,uint16_t n,float out[6]){
    uint16_t j; float sum=0.0f,sq=0.0f,mx=x[0],mn=x[0];
    for(j=0;j<n;j++){float v=x[j];sum+=v;sq+=v*v;if(v>mx)mx=v;if(v<mn)mn=v;}
    {float mean=sum/(float)n,var=sq/(float)n-mean*mean;float tc=((float)n-1.0f)*0.5f,num=0.0f,den=0.0f;
     for(j=0;j<n;j++){float d=(float)j-tc;num+=d*(x[j]-mean);den+=d*d;}
     out[0]=mean;out[1]=mx;out[2]=mn;out[3]=sqrtf(sq/(float)n);out[4]=var>0.0f?var:0.0f;out[5]=den>0.0f?num/den:0.0f;}
}

void BCM_BuildInput(const float v[BCM_SEQ_LEN],const float i[BCM_SEQ_LEN],float rating,float out[BCM_INPUT_SIZE]){
    static const uint16_t win[4]={5u,20u,100u,200u};
    float st[4][6]; uint16_t w,s,k=0u,j; float maxdi=-1.0e30f,mindi=1.0e30f;
    for(w=0;w<4u;w++) window_stats(&i[BCM_SEQ_LEN-win[w]],win[w],st[w]);
    out[k++]=clip(i[BCM_SEQ_LEN-1u]/250.0f,0.0f,1.0f);
    for(s=0;s<6u;s++) for(w=0;w<4u;w++){
        float x=st[w][s];
        if(s<4u)x=clip(x/250.0f,0.0f,1.0f);
        else if(s==4u)x=clip(x/(250.0f*250.0f),0.0f,1.0f);
        else x=clip(x/25.0f,-1.0f,1.0f);
        out[k++]=x;
    }
    for(j=1u;j<BCM_SEQ_LEN;j++){float d=i[j]-i[j-1u];if(d>maxdi)maxdi=d;if(d<mindi)mindi=d;}
    out[k++]=clip(maxdi/50.0f,-1.0f,1.0f); out[k++]=clip(mindi/50.0f,-1.0f,1.0f);
    if(rating>0.0f){out[k++]=clip((i[199]/rating)/2.0f,0,1);out[k++]=clip((st[1][0]/rating)/2.0f,0,1);out[k++]=clip((st[3][1]/rating)/2.0f,0,1);}else{out[k++]=out[k++]=out[k++]=0.0f;}
    {float vsum=0.0f,vm,tc=99.5f,num=0.0f,den=0.0f;for(j=0;j<BCM_SEQ_LEN;j++)vsum+=v[j];vm=vsum/200.0f;for(j=0;j<BCM_SEQ_LEN;j++){float d=(float)j-tc;num+=d*(v[j]-vm);den+=d*d;}
     out[k++]=norm(v[199],6.0f,32.0f);out[k++]=norm(vm,6.0f,32.0f);out[k++]=clip((num/den)/2.0f,-1.0f,1.0f);}
    out[k++]=norm(rating,5.0f,250.0f);
}

void BCM_Infer(const float in[BCM_INPUT_SIZE],float raw[BCM_OUTPUT_SIZE]){
    uint16_t n;
    dense(in,bcm_net_0_weight,bcm_net_0_bias,BCM_INPUT_SIZE,BCM_H1_SIZE,bcm_h1);for(n=0;n<BCM_H1_SIZE;n++)bcm_h1[n]=relu(bcm_h1[n]);
    dense(bcm_h1,bcm_net_2_weight,bcm_net_2_bias,BCM_H1_SIZE,BCM_H2_SIZE,bcm_h2);for(n=0;n<BCM_H2_SIZE;n++)bcm_h2[n]=relu(bcm_h2[n]);
    dense(bcm_h2,bcm_net_4_weight,bcm_net_4_bias,BCM_H2_SIZE,BCM_H3_SIZE,bcm_h3);for(n=0;n<BCM_H3_SIZE;n++)bcm_h3[n]=relu(bcm_h3[n]);
    dense(bcm_h3,bcm_net_6_weight,bcm_net_6_bias,BCM_H3_SIZE,BCM_OUTPUT_SIZE,raw);
}

void BCM_DecodeOutput(const float raw[BCM_OUTPUT_SIZE],BCM_InferenceResult *r){
    float mx=raw[BCM_OUTPUT_FAULT_OFFSET],den=0.0f;uint8_t best=0u,f;
    r->predicted_current_a=250.0f*sigmoid(raw[BCM_OUTPUT_PREDICTED_CURRENT]);r->fault_soon_probability=sigmoid(raw[BCM_OUTPUT_FAULT_SOON]);
    for(f=1;f<BCM_NUM_FAULT_CLASSES;f++)if(raw[BCM_OUTPUT_FAULT_OFFSET+f]>mx){mx=raw[BCM_OUTPUT_FAULT_OFFSET+f];best=f;}
    for(f=0;f<BCM_NUM_FAULT_CLASSES;f++){r->fault_probability[f]=expf(raw[BCM_OUTPUT_FAULT_OFFSET+f]-mx);den+=r->fault_probability[f];}
    for(f=0;f<BCM_NUM_FAULT_CLASSES;f++)r->fault_probability[f]/=den;r->fault_class=best;
}

void BCM_EvaluateProtectionStatus(float v,float i,float rating,BCM_ProtectionStatus *s){s->undervoltage_now=v<9.0f;s->overvoltage_now=v>15.0f;s->overcurrent_now=i>rating;s->current_utilization=rating>0.0f?i/rating:0.0f;}
